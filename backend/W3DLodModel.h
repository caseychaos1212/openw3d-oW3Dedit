#pragma once
#include "W3DStructs.h"
#include <vector>

inline std::vector<ChunkField> InterpretLODModelHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dLODModelHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed LODMODEL_HEADER: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dLODModelHeaderStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Version("Version", h.Version);
    B.Name("Name", h.Name);
    B.UInt16("NumLODs", h.NumLODs);
    return fields;
}

inline std::vector<ChunkField> InterpretLOD(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dLODStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed LOD: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dLODStruct>(v);

    ChunkFieldBuilder B(fields);
    // RenderObjName is 2 * W3D_NAME_LEN in this struct
    B.Name("RenderObjName", h.RenderObjName, 2 * W3D_NAME_LEN);
    B.Float("LODMin", h.LODMin);
    B.Float("LODMax", h.LODMax);
    return fields;
}
