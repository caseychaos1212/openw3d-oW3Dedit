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

    uint32_t attr = data.Attributes;

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

inline std::vector<ChunkField> InterpretAnimationChannel(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    // Must have at least the fixed header (12 bytes) + one float
    if (buf.size() < sizeof(W3dAnimChannelStruct)) {
        fields.emplace_back("Error", "string", "Animation channel chunk too small");
        return fields;
    }

    // Cast into our POD
    auto hdr = reinterpret_cast<const W3dAnimChannelStruct*>(buf.data());

    // compute how many frames we actually have
    int frameCount = int(hdr->LastFrame) - int(hdr->FirstFrame) + 1;
    size_t expectedFloats = size_t(frameCount) * hdr->VectorLen;
    size_t expectedBytes = sizeof(W3dAnimChannelStruct) + expectedFloats * sizeof(float) - sizeof(float);
    // subtract one float because struct.Data[1] already counts one

    if (buf.size() < expectedBytes) {
        fields.emplace_back(
            "Error", "string",
            "Animation channel data truncated; expected " +
            std::to_string(expectedBytes) + " bytes"
        );
        return fields;
    }

    // Header fields
    fields.emplace_back("FirstFrame", "uint16", std::to_string(hdr->FirstFrame));
    fields.emplace_back("LastFrame", "uint16", std::to_string(hdr->LastFrame));
    fields.emplace_back("VectorLen", "uint16", std::to_string(hdr->VectorLen));
    fields.emplace_back("Pivot", "uint16", std::to_string(hdr->Pivot));

    // ChannelType
    if (auto name = AnimationChannelName(hdr->Flags)) {
        fields.emplace_back("ChannelType", "string", name);
    }
    else {
        fields.emplace_back("ChannelType", "string",
            "Unknown(" + std::to_string(hdr->Flags) + ")");
    }

    // Now dump the float data
    const float* data = hdr->Data;
    for (int f = 0; f < frameCount; ++f) {
        for (int v = 0; v < int(hdr->VectorLen); ++v) {
            float val = data[size_t(f) * hdr->VectorLen + v];
            fields.emplace_back(
                "Data[" + std::to_string(f) + "][" + std::to_string(v) + "]",
                "float",
                std::to_string(val)
            );
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

inline std::vector<ChunkField> InterpretBitChannel(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    // Must have at least the fixed header (9 bytes)
    if (buf.size() < sizeof(W3dBitChannelStruct)) {
        fields.emplace_back("Error", "string", "Bit channel chunk too small");
        return fields;
    }

    // Cast into our POD
    auto hdr = reinterpret_cast<const W3dBitChannelStruct*>(buf.data());
    int   first = hdr->FirstFrame;
    int   last = hdr->LastFrame;
    int   count = last - first + 1;
    size_t bitBytes = (count + 7) / 8;
    size_t expected = sizeof(W3dBitChannelStruct) - 1 + bitBytes;
    if (buf.size() < expected) {
        fields.emplace_back(
            "Error", "string",
            "Bit channel data truncated; expected " +
            std::to_string(expected) + " bytes"
        );
        return fields;
    }

    // Header fields
    fields.emplace_back("FirstFrame", "uint16", std::to_string(first));
    fields.emplace_back("LastFrame", "uint16", std::to_string(last));
    fields.emplace_back("Pivot", "uint16", std::to_string(hdr->Pivot));

    // ChannelType
    if (auto name = BitChannelName(hdr->Flags)) {
        fields.emplace_back("ChannelType", "string", name);
    }
    else {
        fields.emplace_back("ChannelType", "string",
            "Unknown(" + std::to_string(hdr->Flags) + ")");
    }

    fields.emplace_back("DefaultVal", "uint8", std::to_string(hdr->DefaultVal));

    // Unpack bits
    const uint8_t* bits = hdr->Data;
    for (int i = 0; i < count; ++i) {
        bool val = (bits[i / 8] >> (i % 8)) & 1;
        fields.emplace_back(
            "Data[" + std::to_string(i) + "]",
            "bool",
            val ? "true" : "false"
        );
    }

    return fields;
}