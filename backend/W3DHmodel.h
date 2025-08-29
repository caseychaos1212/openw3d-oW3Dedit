#pragma once
#include "W3DStructs.h"
#include <vector>


inline std::vector<ChunkField> InterpretHModelHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dHModelHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed HMODEL_HEADER: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dHModelHeaderStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Version("Version", h.Version);
    B.Name("Name", h.Name);
    B.Name("HierarchyName", h.HierarchyName);
    B.UInt16("NumConnections", h.NumConnections);
    return fields;
}


inline std::vector<ChunkField> InterpretHModelAuxData(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dHModelAuxDataStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed HMODEL_AUXDATA: " + *err);
        return fields;
    }
    const auto& a = std::get<W3dHModelAuxDataStruct>(v);

    ChunkFieldBuilder B(fields);
    B.UInt32("Attributes", a.Attributes);
    B.UInt32("MeshCount", a.MeshCount);
    B.UInt32("CollisionCount", a.CollisionCount);
    B.UInt32("SkinCount", a.SkinCount);
    B.UInt32("ShadowCount", a.ShadowCount);
    B.UInt32("NullCount", a.NullCount);

    for (size_t i = 0; i < std::size(a.FutureCounts); ++i) {
        B.UInt32("FutureCounts[" + std::to_string(i) + "]", a.FutureCounts[i]);
    }

    B.Float("LODMin", a.LODMin);
    B.Float("LODMax", a.LODMax);

    for (size_t i = 0; i < std::size(a.FutureUse); ++i) {
        B.UInt32("FutureUse[" + std::to_string(i) + "]", a.FutureUse[i]);
    }

    return fields;
}


inline std::vector<ChunkField> InterpretHModelNode(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dHModelNodeStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed HMODEL_NODE: " + *err);
        return fields;
    }
    const auto& n = std::get<W3dHModelNodeStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Name("RenderObjName", n.RenderObjName);
    B.UInt16("PivotIdx", n.PivotIdx);
    return fields;
}

// Helper to reuse node parsing but rename the first field label
static inline std::vector<ChunkField> InterpretNamedNode(
    const std::shared_ptr<ChunkItem>& chunk,
    std::string_view firstFieldName
) {
    auto f = InterpretHModelNode(chunk);
    if (!f.empty()) {
        
        f[0].field = std::string(firstFieldName);
    }
    return f;
}


inline std::vector<ChunkField> InterpretNode(const std::shared_ptr<ChunkItem>& chunk) {
    return InterpretNamedNode(chunk, "RenderObjName");
}

inline std::vector<ChunkField> InterpretCollisionNode(const std::shared_ptr<ChunkItem>& chunk) {
    return InterpretNamedNode(chunk, "CollisionMeshName");
}

inline std::vector<ChunkField> InterpretSkinNode(const std::shared_ptr<ChunkItem>& chunk) {
    return InterpretNamedNode(chunk, "SkinMeshName");
}

inline std::vector<ChunkField> InterpretShadowNode(const std::shared_ptr<ChunkItem>& chunk) {
    return InterpretNamedNode(chunk, "ShadowMeshName");
}




