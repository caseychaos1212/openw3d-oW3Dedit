#pragma once
#include "W3DStructs.h"
#include <vector>
#include "ChunkItem.h"


// Compressed flavor pretty-name
inline const char* CompressedAnimFlavorName(uint16_t f) {
    switch (f) {
    case 0: return "Timecoded";
    case 1: return "Adaptive Delta";
    default: return nullptr;
    }
}

// Compressed Animation Header (fixed-size -> use ParseChunkStruct)
inline std::vector<ChunkField> InterpretCompressedAnimationHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dCompressedAnimHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed Compressed_Animation_Header: " + *err);
        return fields;
    }
    const auto& hdr = std::get<W3dCompressedAnimHeaderStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Version("Version", hdr.Version);
    B.Name("Name", hdr.Name);
    B.Name("HierarchyName", hdr.HierarchyName);
    B.UInt32("NumFrames", hdr.NumFrames);
    B.UInt16("FrameRate", hdr.FrameRate);

    if (const char* nm = CompressedAnimFlavorName(hdr.Flavor)) {
        B.Push("Flavor", "string", nm);
    }
    else {
        B.Push("Flavor", "string", "Unknown(" + std::to_string(hdr.Flavor) + ")");
    }
    return fields;
}

// Helper for channel type name (compressed channels share the same mapping)
inline const char* CompressedChannelTypeName(uint8_t flags) {
    static constexpr const char* k[] = {
        "X Translation", "Y Translation", "Z Translation",
        "X Rotation",    "Y Rotation",    "Z Rotation",
        "Quaternion Rotation", "Visibility"
    };
    return (flags < 8) ? k[flags] : nullptr;
}

// Timecoded vs AdaptiveDelta channel 
inline std::vector<ChunkField>
InterpretCompressedAnimationChannel(const std::shared_ptr<ChunkItem>& chunk,
    uint16_t flavor /*0=timecoded, 1=adaptive*/) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    const auto& buf = chunk->data;
    const size_t n = buf.size();
    const uint8_t* p = buf.data();

    auto rd_u32 = [&](size_t off) { uint32_t v; std::memcpy(&v, p + off, 4); return v; };
    auto rd_u16 = [&](size_t off) { uint16_t v; std::memcpy(&v, p + off, 2); return v; };
    auto rd_f32 = [&](size_t off) { float v;    std::memcpy(&v, p + off, 4); return v; };

    ChunkFieldBuilder B(fields);

    if (flavor == 0) {
        // ---- Timecoded (NumTimeCodes u32, Pivot u16, VectorLen u8, Flags u8, Data[0] u32...) ----
        if (n < 8 + 4) { // need header (8) + first Data word
            fields.emplace_back("error", "string", "Too small for TimeCodedAnimChannel");
            return fields;
        }

        const uint32_t numTimeCodes = rd_u32(0);
        const uint16_t pivot = rd_u16(4);
        const uint8_t  vecLen = p[6];
        const uint8_t  flags = p[7];
        const size_t   data_off = 8;                // Data[0] is here
        const size_t   words = (n - data_off) / 4; 

        // header
        B.UInt32("NumTimeCodes", numTimeCodes);
        B.UInt16("Pivot", pivot);
        B.UInt8("VectorLen", vecLen);
        if (const char* nm = CompressedChannelTypeName(flags)) {
            B.Push("ChannelType", "string", nm);
        }
        else {
            B.Push("ChannelType", "string", "Unknown(" + std::to_string(flags) + ")");
        }
  //      if (words != numTimeCodes) {
 //           B.Push("note", "string",
 //               "Header declares " + std::to_string(numTimeCodes) +
 //               " timecodes; buffer contains " + std::to_string(words));
  //      }

   //     )
        const auto* w = reinterpret_cast<const uint32_t*>(p + data_off);
        for (size_t i = 0; i < words; ++i) {
            B.UInt32("Data[" + std::to_string(i) + "]", w[i]);
        }
        return fields;
    }

    // ---- Adaptive Delta (NumFrames u32, Pivot u16, VectorLen u8, Flags u8, Scale f32, Data[0] u32...) ----
    if (n < 12 + 4) { // need header (12) + first Data word
        fields.emplace_back("error", "string", "Too small for AdaptiveDeltaAnimChannel");
        return fields;
    }

    const uint32_t numFrames = rd_u32(0);
    const uint16_t pivot = rd_u16(4);
    const uint8_t  vecLen = p[6];
    const uint8_t  flags = p[7];
    const float    scale = rd_f32(8);
    const size_t   data_off = 12;                       // Data[0] starts here
    const size_t   words = (n - data_off) / 4;       

    B.UInt32("NumFrames", numFrames);
    B.UInt16("Pivot", pivot);
    B.UInt8("VectorLen", vecLen);
    if (const char* nm = CompressedChannelTypeName(flags)) {
        B.Push("ChannelType", "string", nm);
    }
    else {
        B.Push("ChannelType", "string", "Unknown(" + std::to_string(flags) + ")");
    }
    B.Float("Scale", scale);

    const auto* w = reinterpret_cast<const uint32_t*>(p + data_off);
    for (size_t i = 0; i < words; ++i) {
        B.UInt32("Data[" + std::to_string(i) + "]", w[i]);
    }
    return fields;
}

// Compressed Bit Channel (timecoded bit events)
// Fixed header with Data[0] present -> use offsetof to size payload safely
inline std::vector<ChunkField> InterpretCompressedBitChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    const size_t n = buf.size();

    // Require header up through first uint32 in Data
    const size_t minHeader = offsetof(W3dTimeCodedBitChannelStruct, Data) + sizeof(uint32_t);
    if (n < minHeader) {
        fields.emplace_back("error", "string", "Compressed Bit Channel too small");
        return fields;
    }

    W3dTimeCodedBitChannelStruct hdr{};
    std::memcpy(&hdr, buf.data(), sizeof(hdr));

    const uint32_t declared = hdr.NumTimeCodes;
    const size_t payloadBytes = (n - offsetof(W3dTimeCodedBitChannelStruct, Data));
    const size_t availWords = payloadBytes / sizeof(uint32_t);
    const size_t actualWords = std::min<size_t>(declared, availWords);

    // Collect the timecode words
    std::vector<uint32_t> words(actualWords);
    const auto* src = reinterpret_cast<const uint32_t*>(buf.data() + offsetof(W3dTimeCodedBitChannelStruct, Data));
    std::memcpy(words.data(), src, actualWords * sizeof(uint32_t));

    ChunkFieldBuilder B(fields);
    B.UInt32("NumTimeCodes", hdr.NumTimeCodes);
    B.UInt16("Pivot", hdr.Pivot);

    // 0 = Visibility, 1 = Timecoded Visibility (per your original)
    static constexpr const char* kTypes[] = { "Visibility", "Timecoded Visibility" };
    const char* nm = (hdr.Flags < 2) ? kTypes[hdr.Flags] : nullptr;
    B.Push("ChannelType", "string", nm ? nm : ("Unknown(" + std::to_string(hdr.Flags) + ")"));

    B.UInt8("DefaultVal", hdr.DefaultVal);

    if (actualWords < hdr.NumTimeCodes) {
        B.Push("warning", "string",
            "Buffer holds only " + std::to_string(actualWords) +
            " codes; header declares " + std::to_string(hdr.NumTimeCodes));
    }

    for (size_t i = 0; i < actualWords; ++i) {
        B.UInt32("Data[" + std::to_string(i) + "]", static_cast<uint32_t>(words[i]));
    }

    return fields;
}

// BFME2: Compressed Motion Channel (header = 8 bytes)
inline std::vector<ChunkField>
InterpretCompressedMotionChannel(const std::shared_ptr<ChunkItem>& chunk)
{
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    if (buf.size() < 8) {
        fields.emplace_back("error", "string", "Too small for MotionChannel header (need 8 bytes)");
        return fields;
    }

    // Header layout
    const uint8_t  Zero = buf[0];
    const uint8_t  Flavor = buf[1]; // 0=TIMECODED, 1=ADAPTIVE_DELTA_4, 2=ADAPTIVE_DELTA_8
    const uint8_t  VectorLen = buf[2];
    const uint8_t  Flags = buf[3];
    const uint16_t NumTimeCodes = *reinterpret_cast<const uint16_t*>(&buf[4]);
    const uint16_t Pivot = *reinterpret_cast<const uint16_t*>(&buf[6]);

    ChunkFieldBuilder B(fields);
    B.UInt8("Zero", Zero);
    // Flavor name
    const char* flavorName = nullptr;
    switch (Flavor) {
    case 0: flavorName = "TIMECODED"; break;
    case 1: flavorName = "ADAPTIVE_DELTA_4"; break;
    case 2: flavorName = "ADAPTIVE_DELTA_8"; break;
    default: break;
    }
    B.Push("Flavor", "string", flavorName ? flavorName : ("Unknown(" + std::to_string(Flavor) + ")"));
    B.UInt8("VectorLen", VectorLen);

    // Channel type name (re-uses your helper from elsewhere)
    if (const char* nm = CompressedChannelTypeName(Flags)) {
        B.Push("ChannelType", "string", nm);
    }
    else {
        B.Push("ChannelType", "string", "Unknown(" + std::to_string(Flags) + ")");
    }

    // wdump prints NumTimeCodes as Int32; we keep it as 16-bit but show both for clarity if you like:
    B.UInt16("NumTimeCodes", NumTimeCodes);
    B.UInt16("Pivot", Pivot);

    const size_t N = buf.size();
    const size_t HDR = 8;

    if (Flavor == 0) {
        // -------- Timecoded -------------------------------------------------
        // keyframes: NumTimeCodes * uint16 immediately after header
        const size_t keyBytes = static_cast<size_t>(NumTimeCodes) * 2;
        if (HDR + keyBytes > N) {
            B.Push("warning", "string", "Truncated before keyframes");
            return fields;
        }

        // Emit keyframes
        const auto* key16 = reinterpret_cast<const uint16_t*>(buf.data() + HDR);
        for (uint32_t i = 0; i < NumTimeCodes; ++i) {
            B.UInt16("KeyFrames[" + std::to_string(i) + "]", key16[i]);
        }

        // Values (uint32): VectorLen * NumTimeCodes words after keyframes (+2 pad if NumTimeCodes is odd)
        size_t pos = HDR + keyBytes;
        if (NumTimeCodes & 1) {
            if (pos + 2 > N) {
                B.Push("warning", "string", "Missing 2-byte pad after odd keyframe count");
                return fields;
            }
            pos += 2;
        }

        const size_t neededWords = static_cast<size_t>(VectorLen) * static_cast<size_t>(NumTimeCodes);
        const size_t availWords = (pos <= N) ? ((N - pos) / 4) : 0;
        const size_t words = std::min(neededWords, availWords);

        if (words < neededWords) {
            B.Push("warning", "string",
                "Buffer holds only " + std::to_string(words) +
                " data words; expected " + std::to_string(neededWords));
        }

        const auto* w = reinterpret_cast<const uint32_t*>(buf.data() + pos);
        for (size_t i = 0; i < words; ++i) {
            B.UInt32("Data[" + std::to_string(i) + "]", w[i]);
        }
        return fields;
    }

    // -------- Adaptive Delta (Flavor 1 or 2) -------------------------------
    // Layout:
    //   float Scale;
    //   float Initial[VectorLen];
    //   uint32 Data[ ... rest ... ]
    if (N < HDR + 4) {
        B.Push("warning", "string", "Truncated before Scale");
        return fields;
    }
    float scale = 0.0f;
    std::memcpy(&scale, buf.data() + HDR, 4);
    B.Float("Scale", scale);

    const size_t initBytes = static_cast<size_t>(VectorLen) * 4;
    const size_t initStart = HDR + 4;
    if (initStart + initBytes > N) {
        B.Push("warning", "string", "Truncated before Initial[]");
        return fields;
    }

    const float* init = reinterpret_cast<const float*>(buf.data() + initStart);
    for (uint32_t i = 0; i < VectorLen; ++i) {
        B.Float("Initial[" + std::to_string(i) + "]", init[i]);
    }

    const size_t dataStart = initStart + initBytes;
    if (dataStart > N) return fields;
    const size_t dataWords = (N - dataStart) / 4;

    const auto* w = reinterpret_cast<const uint32_t*>(buf.data() + dataStart);
    for (size_t i = 0; i < dataWords; ++i) {
        B.UInt32("Data[" + std::to_string(i) + "]", w[i]);
    }

    return fields;
}