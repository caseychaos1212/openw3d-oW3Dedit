#pragma once
#include "W3DStructs.h"
#include <vector>





inline std::vector<ChunkField> InterpretAnimationHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto buff = ParseChunkStruct<W3dAnimHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&buff)) {
        fields.emplace_back("error", "string", "Malformed Animation_Header chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<W3dAnimHeaderStruct>(buff);

    

    ChunkFieldBuilder B(fields);

    B.Version("Version", data.Version);
    B.Name("AnimationName", data.Name);
    B.Name("HierarchyName", data.HierarchyName);
    B.UInt32("NumFrames", data.NumFrames);
    B.UInt32("FrameRate", data.FrameRate);
    return fields;
}

inline const char* AnimationChannelName(uint16_t flags) {
    switch (flags) {
    case 0: return "X Translation";
    case 1: return "Y Translation";
    case 2: return "Z Translation";
    case 3: return "X Rotation";
    case 4: return "Y Rotation";
    case 5: return "Z Rotation";
    case 6: return "Quaternion Rotation";
    default: return nullptr;
    }
}

inline std::vector<ChunkField> InterpretAnimationChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;

    // Safety: need at least the fixed header (which includes Data[1])
    if (buf.size() < sizeof(W3dAnimChannelStruct)) {
        fields.emplace_back("error", "string", "Animation channel chunk too small");
        return fields;
    }

    // Copy header safely (avoid aliasing UB)
    W3dAnimChannelStruct hdr{};
    std::memcpy(&hdr, buf.data(), sizeof(W3dAnimChannelStruct));

    // Compute payload size (floats)
    // total values = (#frames) * VectorLen
    const int frameCount = int(hdr.LastFrame) - int(hdr.FirstFrame) + 1;
    if (frameCount <= 0 || hdr.VectorLen == 0) {
        fields.emplace_back("error", "string", "Invalid channel header (frame count or vector length)");
        return fields;
    }

    const size_t valueCount = size_t(frameCount) * size_t(hdr.VectorLen);
    const size_t headerBytes = sizeof(W3dAnimChannelStruct); // includes first float
    const size_t neededBytes = headerBytes + (valueCount - 1) * sizeof(float);
    if (buf.size() < neededBytes) {
        fields.emplace_back(
            "error", "string",
            "Animation channel data truncated; expected " + std::to_string(neededBytes) +
            " bytes, have " + std::to_string(buf.size())
        );
        return fields;
    }

    // Gather all floats into a vector<float>
    std::vector<float> values(valueCount);
    // First float is inside the header’s Data[0]
    values[0] = hdr.Data[0];
    if (valueCount > 1) {
        const auto* tail = reinterpret_cast<const float*>(buf.data() + headerBytes);
        std::memcpy(values.data() + 1, tail, (valueCount - 1) * sizeof(float));
    }

    // Emit fields
    ChunkFieldBuilder B(fields);
    B.UInt16("FirstFrame", hdr.FirstFrame);
    B.UInt16("LastFrame", hdr.LastFrame);


    if (const char* nm = AnimationChannelName(hdr.Flags)) {
        B.Push("ChannelType", "string", nm);
    }
    else {
        B.Push("ChannelType", "string", "Unknown(" + std::to_string(hdr.Flags) + ")");
    }

    B.UInt16("Pivot", hdr.Pivot);
    B.UInt16("VectorLen", hdr.VectorLen);

    // Dump values as Data[f][v]
    for (int f = 0; f < frameCount; ++f) {
        for (int v = 0; v < int(hdr.VectorLen); ++v) {
            const float val = values[size_t(f) * hdr.VectorLen + v];
            B.Float("Data[" + std::to_string(f) + "][" + std::to_string(v) + "]", val);
        }
    }

    

    return fields;
}

inline const char* BitChannelName(uint16_t f) {
    switch (f) {
    case 0: return "Visibility";
    case 1: return "Timecoded Visibility";
    default: return nullptr;
    }
}

inline std::vector<ChunkField> InterpretBitChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;

    // Need at least the fixed header (includes Data[1] sentinel byte)
    if (buf.size() < sizeof(W3dBitChannelStruct)) {
        fields.emplace_back("error", "string", "Bit channel chunk too small");
        return fields;
    }

    // Copy header safely (avoid aliasing UB)
    W3dBitChannelStruct hdr{};
    std::memcpy(&hdr, buf.data(), sizeof(W3dBitChannelStruct));

    const int first = static_cast<int>(hdr.FirstFrame);
    const int last = static_cast<int>(hdr.LastFrame);
    const int count = last - first + 1;

    if (count <= 0) {
        fields.emplace_back("error", "string", "Invalid bit channel frame range");
        return fields;
    }

    // Payload sizing: header has Data[1], so total bytes = (sizeof - 1) + bitBytes
    const size_t bitBytes = static_cast<size_t>((count + 7) / 8);
    const size_t headerBytes = sizeof(W3dBitChannelStruct);
    const size_t neededBytes = (headerBytes - 1) + bitBytes;

    if (buf.size() < neededBytes) {
        fields.emplace_back(
            "error", "string",
            "Bit channel data truncated; expected " + std::to_string(neededBytes) +
            " bytes, have " + std::to_string(buf.size())
        );
        return fields;
    }

    // Gather bit payload into owned storage
    std::vector<uint8_t> bits(bitBytes);
    // First byte is hdr.Data[0] (inside the header)
    bits[0] = hdr.Data[0];
    if (bitBytes > 1) {
        const uint8_t* tail = reinterpret_cast<const uint8_t*>(buf.data() + headerBytes);
        std::memcpy(bits.data() + 1, tail, bitBytes - 1);
    }

    // Emit fields
    ChunkFieldBuilder B(fields);
    B.UInt16("FirstFrame", hdr.FirstFrame);
    B.UInt16("LastFrame", hdr.LastFrame);
    

    if (const char* nm = BitChannelName(hdr.Flags)) {
        B.Push("ChannelType", "string", nm);
    }
    else {
        B.Push("ChannelType", "string", "Unknown(" + std::to_string(hdr.Flags) + ")");
    }

    B.UInt16("Pivot", hdr.Pivot);
    B.UInt8("DefaultVal", hdr.DefaultVal);

    // Unpack bits
    const int label_base = std::max(0, first );  // convention: start at FirstFrame-1
    for (int i = 0; i < count; ++i) {
        const bool val = (bits[static_cast<size_t>(i / 8)] >> (i % 8)) & 1;
        const int label = label_base + i;
        B.Push("Data[" + std::to_string(label) + "]", "bool", val ? "true" : "false");
    }

    return fields;
}