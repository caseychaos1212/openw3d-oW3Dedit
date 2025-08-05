#pragma once
#include "W3DStructs.h"
#include <vector>

inline std::vector<ChunkField> InterpretHModelHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk || chunk->data.size() < sizeof(W3dHModelHeaderStruct)) {
        return { { "Error", "string", "HMODEL_HEADER too small" } };
    }

    auto hdr = reinterpret_cast<const W3dHModelHeaderStruct*>(chunk->data.data());

    // Helper to trim at the first NUL
    auto trim = [&](const char* s, size_t n) {
        return std::string(s, strnlen(s, n));
        };

    // Version = high word.major low word.minor
    uint16_t major = static_cast<uint16_t>(hdr->Version >> 16);
    uint16_t minor = static_cast<uint16_t>(hdr->Version & 0xFFFF);
    std::ostringstream vstr;
    vstr << major << "." << minor;

    fields.reserve(4);
    fields.push_back({ "Version",       "string", vstr.str() });
    fields.push_back({ "Name",          "string", trim(hdr->Name,          sizeof hdr->Name) });
    fields.push_back({ "HierarchyName", "string", trim(hdr->HierarchyName, sizeof hdr->HierarchyName) });
    fields.push_back({ "NumConnections","uint16", std::to_string(hdr->NumConnections) });

    return fields;
}

inline std::vector<ChunkField> InterpretHModelAuxData(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk || chunk->data.size() < sizeof(W3dHModelAuxDataStruct)) {
        fields.push_back({ "Error", "string", "Aux data chunk too small" });
        return fields;
    }

    auto const* aux = reinterpret_cast<const W3dHModelAuxDataStruct*>(chunk->data.data());

    fields.push_back({ "Attributes",     "uint32", std::to_string(aux->Attributes) });
    fields.push_back({ "MeshCount",      "uint32", std::to_string(aux->MeshCount) });
    fields.push_back({ "CollisionCount", "uint32", std::to_string(aux->CollisionCount) });
    fields.push_back({ "SkinCount",      "uint32", std::to_string(aux->SkinCount) });
    fields.push_back({ "ShadowCount",    "uint32", std::to_string(aux->ShadowCount) });
    fields.push_back({ "NullCount",      "uint32", std::to_string(aux->NullCount) });

    for (size_t i = 0; i < std::size(aux->FutureCounts); ++i) {
        fields.push_back({
            "FutureCounts[" + std::to_string(i) + "]",
            "uint32",
            std::to_string(aux->FutureCounts[i])
            });
    }

    fields.push_back({ "LODMin", "float", std::to_string(aux->LODMin) });
    fields.push_back({ "LODMax", "float", std::to_string(aux->LODMax) });

    for (size_t i = 0; i < std::size(aux->FutureUse); ++i) {
        fields.push_back({
            "FutureUse[" + std::to_string(i) + "]",
            "uint32",
            std::to_string(aux->FutureUse[i])
            });
    }

    return fields;
}


inline std::vector<ChunkField> InterpretHModelNode(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    
    if (!chunk || chunk->data.size() < sizeof(W3dHModelNodeStruct)) {
        fields.push_back({ "Error", "string", "HModelNode chunk too small" });
        return fields;
    }

    
    auto const* node = reinterpret_cast<const W3dHModelNodeStruct*>(chunk->data.data());

    
    std::string name(node->RenderObjName,
        strnlen(node->RenderObjName, W3D_NAME_LEN));
    fields.push_back({ "RenderObjName", "string", name });

    
    fields.push_back({ "PivotIdx", "uint16", std::to_string(node->PivotIdx) });

    return fields;
}

static inline std::vector<ChunkField> InterpretNamedNode(
    const std::shared_ptr<ChunkItem>& chunk,
    std::string_view firstFieldName
) {
    auto f = InterpretHModelNode(chunk);
    if (!f.empty()) f[0].field = std::string(firstFieldName);
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




