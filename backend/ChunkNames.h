#pragma once
#include <unordered_map>
#include <string>

inline std::string GetChunkName(uint32_t id) {
    static const std::unordered_map<uint32_t, std::string> chunkNames = {
        { 0x0100, "HIERARCHY" },
        { 0x0101, "HIERARCHY_HEADER" },
        { 0x0102, "PIVOTS" },
        { 0x0103, "PIVOT_FIXUPS" },
        { 0x0200, "VERTEX_ATTRIBUTE_DECLARATION" },
        { 0x0300, "SHADER" },
        { 0x0410, "GEOMETRY" },
        { 0x0500, "VERTICES" },
        { 0x0501, "VERTEX_INFLUENCES" },
        { 0x0600, "MATERIAL" },
        { 0x0700, "TEXTURE" },
        { 0x0810, "MESH" },
        { 0x0900, "ANIMATION" },
        { 0x0910, "ANIMATION_CHANNEL" },
        { 0x0A00, "HIERARCHY_HEADER" },
        // Add more as needed...
    };

    auto it = chunkNames.find(id);
    if (it != chunkNames.end()) return it->second;
    return "UNKNOWN";
}
