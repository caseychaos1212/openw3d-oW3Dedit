#pragma once
#include <unordered_map>
#include <string>

inline std::string GetChunkName(uint32_t id) {
    static const std::unordered_map<uint32_t, std::string> chunkNames = {
        { 0x0000, "W3D_CHUNK_MESH" },
        { 0x0001, "W3D_CHUNK_HEADER" },
        { 0x0002, "W3D_CHUNK_VERTICES" },
        { 0x0003, "W3D_CHUNK_VERTEX_NORMALS" },
        { 0x0004, "W3D_CHUNK_VERTEX_COLORS" },
        { 0x0005, "W3D_CHUNK_VERTEX_TEXTURE_COORDINATES" },
        { 0x001F, "W3D_CHUNK_MESH_HEADER3" },
        { 0x0020, "W3D_CHUNK_TRIANGLES" },
        { 0x0022, "W3D_CHUNK_VERTEX_SHADE_INDICES" },
        { 0x0028, "W3D_CHUNK_MATERIAL_INFO" },
        { 0x0029, "W3D_CHUNK_SHADERS" },
        { 0x002A, "W3D_CHUNK_VERTEX_MATERIALS" },
        { 0x0030, "W3D_CHUNK_TEXTURES" },
        { 0x0038, "W3D_CHUNK_MATERIAL_PASS" },
        { 0x0100, "W3D_CHUNK_HIERARCHY" },
        { 0x0101, "W3D_CHUNK_HIERARCHY_HEADER" },
        { 0x0102, "W3D_CHUNK_PIVOTS" },
        { 0x0103, "W3D_CHUNK_PIVOT_FIXUPS" },
        { 0x0200, "W3D_CHUNK_VERTEX_ATTRIBUTE_DECLARATION" },
        { 0x0300, "W3D_CHUNK_SHADER" },
        { 0x0410, "W3D_CHUNK_GEOMETRY" },
        { 0x0500, "W3D_CHUNK_VERTICES_X" },
        { 0x0501, "W3D_CHUNK_VERTEX_INFLUENCES" },
        { 0x0600, "W3D_CHUNK_MATERIAL" },
        { 0x0700, "W3D_CHUNK_TEXTURE" },
        { 0x0810, "W3D_CHUNK_MESH" },
        { 0x0900, "W3D_CHUNK_ANIMATION" },
        { 0x0910, "W3D_CHUNK_ANIMATION_CHANNEL" },
        { 0x0A00, "W3D_CHUNK_HIERARCHY_HEADER" },
        { 0x0A10, "W3D_CHUNK_TEXTURES" },
        { 0x0B10, "W3D_CHUNK_MATERIAL_PASS" },
        { 0x0090, "W3D_CHUNK_AABTREE" },
        { 0x002B, "W3D_CHUNK_VERTEX_MATERIAL" },
        { 0x002C, "W3D_CHUNK_VERTEX_MATERIAL_NAME" },
        { 0x002D, "W3D_CHUNK_VERTEX_MATERIAL_INFO" },
    };

    auto it = chunkNames.find(id);
    if (it != chunkNames.end()) return it->second;
    return "UNKNOWN";
}
