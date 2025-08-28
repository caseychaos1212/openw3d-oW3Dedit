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
    static constexpr const char* k = {
        "X Translation", "Y Translation", "Z Translation",
        "X Rotation",    "Y Rotation",    "Z Rotation",
        "Quaternion Rotation", "Visibility"
    };
    return (flags < 8) ? k[flags] : nullptr;
}

// Timecoded vs AdaptiveDelta channel 
inline std::vector<ChunkField> InterpretCompressedAnimationChannel(const std::shared_ptr<ChunkItem>& chunk,
    uint16_t flavor /*0=timecoded, 1=adaptive*/) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    const size_t n = buf.size();
    ChunkFieldBuilder B(fields);

    if (flavor == 0) {
        // ---- Timecoded ----------------------------------------------------
        if (n < sizeof(W3dTimeCodedAnimChannelStruct)) {
            fields.emplace_back("error", "string", "Too small for TimeCodedAnimChannel");
            return fields;
        }
        W3dTimeCodedAnimChannelStruct hdr{};
        std::memcpy(&hdr, buf.data(), sizeof(hdr));

        // Payload words (uint32) after the header (header contains Data[1])
        const size_t headerBytes = sizeof(W3dTimeCodedAnimChannelStruct);
        const size_t availWords = (n - headerBytes) / sizeof(uint32_t) + 1; // include Data[0] in header
        const uint32_t declared = hdr.NumTimeCodes;
        const size_t actualWords = std::min<size_t>(declared, availWords);

        // Copy payload to owned storage
        std::vector<uint32_t> codes(actualWords);
        codes[0] = hdr.Data[0];
        if (actualWords > 1) {
            const auto* tail = reinterpret_cast<const uint32_t*>(buf.data() + headerBytes);
            std::memcpy(codes.data() + 1, tail, (actualWords - 1) * sizeof(uint32_t));
        }

        // Emit header
        B.UInt32("NumTimeCodes", hdr.NumTimeCodes);
        B.UInt16("Pivot", hdr.Pivot);
        B.UInt8("VectorLen", hdr.VectorLen);
        if (const char* nm = CompressedChannelTypeName(hdr.Flags)) {
            B.Push("ChannelType", "string", nm);
        }
        else {
            B.Push("ChannelType", "string", "Unknown(" + std::to_string(hdr.Flags) + ")");
        }

        // Truncation warning
        if (actualWords < hdr.NumTimeCodes) {
            B.Push("warning", "string",
                "Buffer holds only " + std::to_string(actualWords) +
                " timecodes; header declares " + std::to_string(hdr.NumTimeCodes));
        }

        // Emit data
        for (size_t i = 0; i < actualWords; ++i) {
            B.Int32("Data[" + std::to_string(i) + "]", static_cast<int32_t>(codes[i]));
        }
        return fields;
    }

    // ---- Adaptive Delta ---------------------------------------------------
    if (n < sizeof(W3dAdaptiveDeltaAnimChannelStruct)) {
        fields.emplace_back("error", "string", "Too small for AdaptiveDeltaAnimChannel");
        return fields;
    }
    W3dAdaptiveDeltaAnimChannelStruct hdr{};
    std::memcpy(&hdr, buf.data(), sizeof(hdr));

    const size_t headerBytes = sizeof(W3dAdaptiveDeltaAnimChannelStruct);
    const size_t availWords = (n - headerBytes) / sizeof(uint32_t) + 1; // include Data[0]
    // Some files don’t carry a declared count for deltas; we trust the buffer.
    const size_t actualWords = availWords;

    std::vector<uint32_t> words(actualWords);
    words[0] = hdr.Data[0];
    if (actualWords > 1) {
        const auto* tail = reinterpret_cast<const uint32_t*>(buf.data() + headerBytes);
        std::memcpy(words.data() + 1, tail, (actualWords - 1) * sizeof(uint32_t));
    }

    B.UInt32("NumFrames", hdr.NumFrames);
    B.UInt16("Pivot", hdr.Pivot);
    B.UInt8("VectorLen", hdr.VectorLen);
    if (const char* nm = CompressedChannelTypeName(hdr.Flags)) {
        B.Push("ChannelType", "string", nm);
    }
    else {
        B.Push("ChannelType", "string", "Unknown(" + std::to_string(hdr.Flags) + ")");
    }
    B.Float("Scale", hdr.Scale);

    for (size_t i = 0; i < actualWords; ++i) {
        B.Int32("Data[" + std::to_string(i) + "]", static_cast<int32_t>(words[i]));
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
        B.Int32("Data[" + std::to_string(i) + "]", static_cast<int32_t>(words[i]));
    }

    return fields;
}