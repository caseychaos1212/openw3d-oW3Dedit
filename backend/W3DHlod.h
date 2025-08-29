#pragma once
#include "W3DStructs.h"
#include <vector>
#include <algorithm>



inline std::vector<ChunkField> InterpretHLODHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dHLodHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed HLOD_HEADER: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dHLodHeaderStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Version("Version", h.Version);
    B.UInt32("LodCount", h.LodCount);
    B.Name("Name", h.Name);
    B.Name("HierarchyName", h.HierarchyName);
    return fields;
}


inline std::vector<ChunkField> InterpretHLODSubObjectArrayHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dHLodArrayHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed HLOD_SUBOBJECT_ARRAY_HEADER: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dHLodArrayHeaderStruct>(v);

    ChunkFieldBuilder B(fields);
    B.UInt32("ModelCount", h.ModelCount);
    B.Float("MaxScreenSize", h.MaxScreenSize);
    return fields;
}



inline std::vector<ChunkField> InterpretHLODSubObject_LodArray(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dHLodSubObjectStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed HLOD_SUBOBJECT: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dHLodSubObjectStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Name("Name", h.Name, 2 * W3D_NAME_LEN);
    B.UInt32("BoneIndex", h.BoneIndex);
    return fields;
}

