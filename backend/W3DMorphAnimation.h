#pragma once
#include "W3DStructs.h"
#include <vector>
#include "ChunkItem.h"

inline std::vector<ChunkField> InterpretMorphAnimHeader(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;

    // Must at least hold the full header
    if (!chunk || chunk->data.size() < sizeof(W3dMorphAnimHeaderStruct)) {
        fields.push_back({ "error", "string", "Invalid MORPHANIM_HEADER" });
        return fields;
    }

    // Reinterpret as our struct
    auto hdr = reinterpret_cast<const W3dMorphAnimHeaderStruct*>(chunk->data.data());

    // 1) Version
    uint32_t rawVer = hdr->Version;
    uint16_t major = static_cast<uint16_t>(rawVer >> 16);
    uint16_t minor = static_cast<uint16_t>(rawVer & 0xFFFF);
    {
        std::ostringstream ver;
        ver << major << "." << minor;
        fields.push_back({ "Version", "string", ver.str() });
    }

    // 2) Name (null-terminated within 16 bytes)
    {
        size_t len = strnlen(hdr->Name, W3D_NAME_LEN);
        fields.push_back({ "Name", "string", std::string(hdr->Name, len) });
    }

    // 3) HierarchyName
    {
        size_t len = strnlen(hdr->HierarchyName, W3D_NAME_LEN);
        fields.push_back({ "HierarchyName", "string", std::string(hdr->HierarchyName, len) });
    }

    // 4) FrameCount
    fields.push_back({
      "FrameCount", "uint32_t",
      std::to_string(hdr->FrameCount)
        });

    // 5) FrameRate
    fields.push_back({
      "FrameRate", "float",
      std::to_string(hdr->FrameRate)
        });

    // 6) ChannelCount
    fields.push_back({
      "ChannelCount", "uint32_t",
      std::to_string(hdr->ChannelCount)
        });

    return fields;
}

inline std::vector<ChunkField> InterpretMorphAnimPoseName(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    // build the helper from the raw bytes
    W3dNullTermString nts(
        chunk->data.data(),
        chunk->data.size()
    );

    fields.push_back({
      "Pose Name",
      "string",
      nts.value
        });
    return fields;
}

inline std::vector<ChunkField> InterpretMorphAnimKeyData(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;

    if (!chunk) {
        return { { "error", "string", "Empty MORPHANIM_KEYDATA chunk" } };
    }

    const auto& data = chunk->data;
    constexpr size_t ENTRY_SIZE = sizeof(W3dMorphAnimKeyStruct);

    if (data.size() % ENTRY_SIZE != 0) {
        return { { "error", "string",
                  "Unexpected MORPHANIM_KEYDATA size: " + std::to_string(data.size()) } };
    }

    size_t count = data.size() / ENTRY_SIZE;
    auto ptr = reinterpret_cast<const W3dMorphAnimKeyStruct*>(data.data());

    fields.reserve(count * 2);
    for (size_t i = 0; i < count; ++i) {
        const auto& e = ptr[i];
        std::string base = "Key[" + std::to_string(i) + "]";
        fields.push_back({ base + ".MorphFrame", "uint32", std::to_string(e.MorphFrame) });
        fields.push_back({ base + ".PoseFrame",  "uint32", std::to_string(e.PoseFrame) });
    }

    return fields;
}

inline std::vector<ChunkField> InterpretMorphAnimPivotChannelData(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint32_t* data = reinterpret_cast<const uint32_t*>(chunk->data.data());
    size_t count = chunk->data.size() / sizeof(uint32_t);

    for (size_t i = 0; i < count; ++i) {
        fields.push_back({ "PivotChannel[" + std::to_string(i) + "]", "int32", std::to_string(data[i]) });
    }

    return fields;
}
