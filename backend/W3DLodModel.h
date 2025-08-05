#pragma once
#include "W3DStructs.h"
#include <vector>

inline std::vector<ChunkField> InterpretLODModelHeader(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;

    
    if (!chunk || chunk->data.size() < sizeof(W3dLODModelHeaderStruct)) {
        fields.push_back({ "Error", "string", "Invalid LODMODEL_HEADER chunk" });
        return fields;
    }

    auto const* hdr =
        reinterpret_cast<const W3dLODModelHeaderStruct*>(chunk->data.data());

    
    uint16_t major = static_cast<uint16_t>(hdr->Version >> 16);
    uint16_t minor = static_cast<uint16_t>(hdr->Version & 0xFFFF);
    fields.push_back({
        "Version",
        "string",
        std::to_string(major) + "." + std::to_string(minor)
        });

    
    size_t nameLen = 0;
    while (nameLen < W3D_NAME_LEN && hdr->Name[nameLen] != '\0') {
        ++nameLen;
    }
    std::string name(hdr->Name, nameLen);
    fields.emplace_back("Name", "string", name);
    
    fields.push_back({
        "NumLODs",
        "uint16",
        std::to_string(hdr->NumLODs)
        });

    return fields;
}

inline std::vector<ChunkField> InterpretLOD(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;

    
    if (!chunk || chunk->data.size() < sizeof(W3dLODStruct)) {
        fields.push_back({ "Error", "string", "Invalid LOD chunk" });
        return fields;
    }

    
    auto const* hdr = reinterpret_cast<const W3dLODStruct*>(chunk->data.data());

    
    {
        constexpr size_t MAX_NAME = 2 * W3D_NAME_LEN;
        const char* src = hdr->RenderObjName;
        size_t nameLen = 0;
        while (nameLen < MAX_NAME && src[nameLen] != '\0') {
            ++nameLen;
        }
        std::string name(src, nameLen);
        fields.emplace_back("RenderObjName", "string", name);
    }

   
    fields.push_back({
        "LODMin",
        "float",
        std::to_string(hdr->LODMin)
        });

    
    fields.push_back({
        "LODMax",
        "float",
        std::to_string(hdr->LODMax)
        });

    return fields;
}