#pragma once
#include "W3DStructs.h"
#include <vector>
#include <cstring>
#include <algorithm>


inline std::vector<ChunkField> InterpretCollectionHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dCollectionHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed COLLECTION_HEADER: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dCollectionHeaderStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Version("Version", h.Version);
    B.Name("Name", h.Name);
    B.UInt32("RenderObjectCount", h.RenderObjectCount);
    return fields;
}

inline std::vector<ChunkField> InterpretCollectionObjName(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    ChunkFieldBuilder B(fields);
    B.NullTerm("ObjectName",
        reinterpret_cast<const char*>(chunk->data.data()),
        chunk->data.size(),
        2 * W3D_NAME_LEN);
    return fields;
}
//TODO: FIND EXAMPLE
inline std::vector<ChunkField> InterpretTransformNode(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    // Need at least the fixed header (which contains name_len but not the trailing name bytes)
    auto v = ParseChunkStruct<W3dTransformNodeStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed TRANSFORM_NODE: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dTransformNodeStruct>(v);

    const size_t headerBytes = sizeof(W3dTransformNodeStruct);
    size_t available = (chunk->data.size() > headerBytes) ? (chunk->data.size() - headerBytes) : 0;
    uint32_t nameLen = h.name_len;
    if (nameLen > available) nameLen = static_cast<uint32_t>(available);

    ChunkFieldBuilder B(fields);
    B.Version("Version", h.version);

    // 4x3 transform rows
    for (int row = 0; row < 4; ++row) {
        B.Push("Transform[" + std::to_string(row) + "]",
            "vector3",
            FormatUtils::FormatVec3(h.transform[row][0], h.transform[row][1], h.transform[row][2]));
    }

    B.UInt32("NameLength", h.name_len);

    // trailing name
    const char* nameStart = reinterpret_cast<const char*>(chunk->data.data() + headerBytes);
    std::string name(nameStart, nameLen);
    while (!name.empty() && (name.back() == '\0' || std::isspace(static_cast<unsigned char>(name.back()))))
        name.pop_back();
    B.Push("NodeName", "string", std::move(name));

    return fields;
}

inline std::vector<ChunkField> InterpretPlaceHolder(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dPlaceholderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed COLLECTION_PLACEHOLDER: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dPlaceholderStruct>(v);

    const size_t headerBytes = sizeof(W3dPlaceholderStruct);
    size_t available = (chunk->data.size() > headerBytes) ? (chunk->data.size() - headerBytes) : 0;
    uint32_t nameLen = h.name_len;
    if (nameLen > available) nameLen = static_cast<uint32_t>(available);

    ChunkFieldBuilder B(fields);
    B.Version("Version", h.version);

    for (int row = 0; row < 4; ++row) {
        B.Push("Transform[" + std::to_string(row) + "]",
            "vector3",
            FormatUtils::FormatVec3(h.transform[row][0], h.transform[row][1], h.transform[row][2]));
    }

    B.UInt32("NameLength", h.name_len);

    const char* nameStart = reinterpret_cast<const char*>(chunk->data.data() + headerBytes);
    std::string name(nameStart, nameLen);
    while (!name.empty() && (name.back() == '\0' || std::isspace(static_cast<unsigned char>(name.back()))))
        name.pop_back();
    B.Push("Name", "string", std::move(name));

    return fields;
}

