#pragma once
#include "W3DStructs.h"
#include <vector>
#include "ChunkItem.h"


inline const char* CompressedAnimFlavorName(uint16_t f) {
    switch (f) {
    case 0: return "Timecoded";
    case 1: return "Adaptive Delta";
    default: return nullptr;
    }
}

inline std::vector<ChunkField> InterpretCompressedAnimationHeader(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    if (buf.size() < sizeof(W3dCompressedAnimHeaderStruct)) {
        fields.emplace_back("Error", "string", "Compressed Animation Header too small");
        return fields;
    }

    auto hdr = reinterpret_cast<const W3dCompressedAnimHeaderStruct*>(buf.data());

    // Version as major.minor
    uint16_t major = uint16_t(hdr->Version >> 16);
    uint16_t minor = uint16_t(hdr->Version & 0xFFFF);
    fields.emplace_back("Version", "string",
        std::to_string(major) + "." + std::to_string(minor));

    
    fields.emplace_back("Name", "string", std::string(hdr->Name));
    fields.emplace_back("HierarchyName", "string", std::string(hdr->HierarchyName));

    fields.emplace_back("NumFrames", "uint32", std::to_string(hdr->NumFrames));
    fields.emplace_back("FrameRate", "uint16", std::to_string(hdr->FrameRate));

    if (auto fn = CompressedAnimFlavorName(hdr->Flavor)) {
        fields.emplace_back("Flavor", "string", fn);
    }
    else {
        fields.emplace_back("Flavor", "string",
            "Unknown(" + std::to_string(hdr->Flavor) + ")");
    }

    return fields;
}

inline std::vector<ChunkField> InterpretCompressedAnimationChannel(
    const std::shared_ptr<ChunkItem>& chunk,
    uint16_t flavor = 0
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    const size_t sz = buf.size();
    const uint8_t* ptr = buf.data();

    static constexpr const char* ChannelTypes[] = {
        "X Translation", "Y Translation", "Z Translation",
        "X Rotation",    "Y Rotation",    "Z Rotation",
        "Quaternion Rotation", "Visibility"
    };

    if (flavor == 0) {
        
        if (sz < sizeof(W3dTimeCodedAnimChannelStruct)) {
            fields.emplace_back("Error", "string", "Too small for TimeCodedAnimChannel");
            return fields;
        }
        auto hdr = reinterpret_cast<const W3dTimeCodedAnimChannelStruct*>(ptr);
        fields.emplace_back("NumTimeCodes", "uint32", std::to_string(hdr->NumTimeCodes));
        fields.emplace_back("Pivot", "uint16", std::to_string(hdr->Pivot));
        fields.emplace_back("VectorLen", "uint8", std::to_string(hdr->VectorLen));
        {
            const char* ct = (hdr->Flags < 8 ? ChannelTypes[hdr->Flags] : "Unknown");
            fields.emplace_back("ChannelType", "string", ct);
        }
        size_t count = (sz - sizeof(*hdr)) / sizeof(uint32_t);
        for (size_t i = 0; i < count; ++i) {
            fields.emplace_back(
                "Data[" + std::to_string(i) + "]",
                "int32",
                std::to_string(hdr->Data[i])
            );
        }
    }
    else {
        
        if (sz < sizeof(W3dAdaptiveDeltaAnimChannelStruct)) {
            fields.emplace_back("Error", "string", "Too small for AdaptiveDeltaAnimChannel");
            return fields;
        }
        auto hdr = reinterpret_cast<const W3dAdaptiveDeltaAnimChannelStruct*>(ptr);
        fields.emplace_back("NumFrames", "uint32", std::to_string(hdr->NumFrames));
        fields.emplace_back("Pivot", "uint16", std::to_string(hdr->Pivot));
        fields.emplace_back("VectorLen", "uint8", std::to_string(hdr->VectorLen));
        {
            const char* ct = (hdr->Flags < 8 ? ChannelTypes[hdr->Flags] : "Unknown");
            fields.emplace_back("ChannelType", "string", ct);
        }
        fields.emplace_back("Scale", "float", std::to_string(hdr->Scale));
        size_t count = (sz - sizeof(*hdr)) / sizeof(uint32_t);
        for (size_t i = 0; i < count; ++i) {
            fields.emplace_back(
                "Data[" + std::to_string(i) + "]",
                "int32",
                std::to_string(hdr->Data[i])
            );
        }
    }

    return fields;
}

inline std::vector<ChunkField> InterpretCompressedBitChannel(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    const auto& buf = chunk->data;
    size_t bufSize = buf.size();

    // Must at least hold the header up through Data[0]
    if (bufSize < offsetof(W3dTimeCodedBitChannelStruct, Data) + sizeof(uint32_t)) {
        fields.push_back({ "Error", "string", "Compressed Bit Channel too small" });
        return fields;
    }

    // Reinterpret the start of the buffer as our struct
    auto hdr = reinterpret_cast<const W3dTimeCodedBitChannelStruct*>(buf.data());
    uint32_t nCodes = hdr->NumTimeCodes;

    // Compute how many bytes we *should* have
    size_t needed = offsetof(W3dTimeCodedBitChannelStruct, Data)
        + nCodes * sizeof(uint32_t);

    // Header fields
    fields.push_back({ "NumTimeCodes", "uint32", std::to_string(nCodes) });
    fields.push_back({ "Pivot",       "uint16", std::to_string(hdr->Pivot) });

    static constexpr const char* BitChannelTypes[] = {
        "Visibility",
        "Timecoded Visibility"
    };
    uint8_t flags = hdr->Flags;
    std::string typeName = (flags < 2)
        ? BitChannelTypes[flags]
        : ("Unknown (" + std::to_string(flags) + ")");
    fields.push_back({ "ChannelType", "string", typeName });

    fields.push_back({ "DefaultVal", "uint8", std::to_string(hdr->DefaultVal) });

    if (bufSize < needed) {
        fields.push_back({ "Warning", "string",
            "Buffer too small for declared NumTimeCodes — only "
            + std::to_string((bufSize - offsetof(W3dTimeCodedBitChannelStruct, Data))
                             / sizeof(uint32_t))
            + " codes available" });
    }

    // Only iterate as many as actually fit in the buffer
    size_t actual = std::min<size_t>(
        nCodes,
        (bufSize - offsetof(W3dTimeCodedBitChannelStruct, Data)) / sizeof(uint32_t)
    );

    for (size_t i = 0; i < actual; ++i) {
        fields.push_back({
            "Data[" + std::to_string(i) + "]",
            "int32",
            std::to_string(hdr->Data[i])
            });
    }

    return fields;
}