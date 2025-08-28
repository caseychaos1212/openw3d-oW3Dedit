#pragma once
#include "W3DStructs.h"
#include <vector>
#include "ChunkItem.h"

inline std::vector<ChunkField> InterpretMorphAnimHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dMorphAnimHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed MORPHANIM_HEADER: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dMorphAnimHeaderStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Version("Version", h.Version);
    B.Name("Name", h.Name);
    B.Name("HierarchyName", h.HierarchyName);
    B.UInt32("FrameCount", h.FrameCount);
    B.Float("FrameRate", h.FrameRate);
    B.UInt32("ChannelCount", h.ChannelCount);
    return fields;
}

inline std::vector<ChunkField> InterpretMorphAnimPoseName(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    ChunkFieldBuilder B(fields);
    B.NullTerm("Pose Name",
        reinterpret_cast<const char*>(chunk->data.data()),
        chunk->data.size(),
        W3D_NAME_LEN);
    return fields;
}


inline std::vector<ChunkField> InterpretMorphAnimKeyData(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dMorphAnimKeyStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed MORPHANIM_KEYDATA: " + *err);
        return fields;
    }
    const auto& keys = std::get<std::vector<W3dMorphAnimKeyStruct>>(parsed);

    ChunkFieldBuilder B(fields);
    B.UInt32("Count", static_cast<uint32_t>(keys.size()));
    for (size_t i = 0; i < keys.size(); ++i) {
        const auto& e = keys[i];
        const std::string pfx = "Key[" + std::to_string(i) + "]";
        B.UInt32(pfx + ".MorphFrame", e.MorphFrame);
        B.UInt32(pfx + ".PoseFrame", e.PoseFrame);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretMorphAnimPivotChannelData(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<uint32_t>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed MORPHANIM_PIVOTCHANNEL: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<uint32_t>>(parsed);

    ChunkFieldBuilder B(fields);
    B.UInt32("Count", static_cast<uint32_t>(data.size()));
    for (size_t i = 0; i < data.size(); ++i) {
        B.Int32("PivotChannel[" + std::to_string(i) + "]", static_cast<int32_t>(data[i]));
    }
    return fields;
}

