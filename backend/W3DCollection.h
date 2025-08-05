#pragma once
#include "W3DStructs.h"
#include <vector>
#include <cstring>
#include <algorithm>


inline std::vector<ChunkField> InterpretCollectionHeader(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;

    // Make sure we have at least a full header
    if (!chunk || chunk->data.size() < sizeof(W3dCollectionHeaderStruct)) {
        fields.push_back({ "Error", "string", "Invalid COLLECTION_HEADER chunk" });
        return fields;
    }

    // Reinterpret the raw bytes as our header struct
    auto const* hdr = reinterpret_cast<const W3dCollectionHeaderStruct*>(chunk->data.data());

    // 1 Version  Major.Minor
    uint16_t major = static_cast<uint16_t>((hdr->Version >> 16) & 0xFFFF);
    uint16_t minor = static_cast<uint16_t>(hdr->Version & 0xFFFF);
    fields.push_back({
        "Version", "string",
        std::to_string(major) + "." + std::to_string(minor)
        });

    // 2 Name null?terminated within W3D_NAME_LEN bytes
    auto nameStart = hdr->Name;
    auto nameEnd = std::find(nameStart, nameStart + W3D_NAME_LEN, '\0');
    std::string name(nameStart, nameEnd);
    fields.push_back({ "Name", "string", name });

    // 3 RenderObjectCount
    fields.push_back({
        "RenderObjectCount", "uint32_t",
        std::to_string(hdr->RenderObjectCount)
        });

    // pad[] is ignored
    return fields;
}

inline std::vector<ChunkField> InterpretCollectionObjName(
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
      "ObjectName",
      "string",
      nts.value
        });
    return fields;
}

inline std::vector<ChunkField> InterpretTransformNode(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;

    // Minimum size = struct + at least 1 byte of name
    if (!chunk || chunk->data.size() < sizeof(W3dTransformNodeStruct) + 1) {
        fields.emplace_back("error", "string", "Invalid TRANSFORM_NODE chunk");
        return fields;
    }

    // Cast the first part of data to our struct
    auto const* hdr = reinterpret_cast<const W3dTransformNodeStruct*>(chunk->data.data());

    // 1 Version  Major.Minor
    uint32_t rawVer = hdr->version;
    uint16_t major = static_cast<uint16_t>(rawVer >> 16);
    uint16_t minor = static_cast<uint16_t>(rawVer & 0xFFFF);
    fields.emplace_back(
        "Version",
        "uint32_t",
        std::to_string(major) + "." + std::to_string(minor)
    );

    // 2 4×3 transform rows
    for (int row = 0; row < 4; ++row) {
        auto& t = hdr->transform[row];
        std::ostringstream oss;
        oss << "("
            << t[0] << " "
            << t[1] << " "
            << t[2] << ")";
        fields.emplace_back(
            "Transform[" + std::to_string(row) + "]",
            "vector3",   // or "Vector3" if you prefer
            oss.str()
        );
    }

    // 3 Name length + the name itself
    size_t headerSize = sizeof(W3dTransformNodeStruct);
    uint32_t nameLen = hdr->name_len;

    // clamp to available bytes
    size_t available = chunk->data.size() - headerSize;
    if (nameLen > available) nameLen = static_cast<uint32_t>(available);

    fields.emplace_back(
        "NameLength",
        "uint32_t",
        std::to_string(hdr->name_len)
    );

    // 4 Read the name and trim trailing NULs/spaces
    const char* nameStart = reinterpret_cast<const char*>(chunk->data.data() + headerSize);
    std::string name(nameStart, nameLen);
    while (!name.empty() && (name.back() == '\0' || std::isspace((unsigned char)name.back())))
        name.pop_back();

    fields.emplace_back("NodeName", "string", name);

    return fields;
}
inline std::vector<ChunkField> InterpretPlaceHolder(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        return { { "error", "string", "Empty COLLECTION_PLACEHOLDER chunk" } };
    }

    const auto& data = chunk->data;
    const size_t len = data.size();

    // Must hold at least our struct header
    if (len < sizeof(W3dPlaceholderStruct)) {
        return { { "error", "string", "Unexpected COLLECTION_PLACEHOLDER size: " + std::to_string(len) } };
    }

    // Map the first bytes to our struct
    auto const* hdr = reinterpret_cast<const W3dPlaceholderStruct*>(data.data());

    // 1) Version
    fields.push_back({
        "Version",
        "uint32_t",
        std::to_string(hdr->version)
        });

    // 2) 4×3 transform matrix
    for (int row = 0; row < 4; ++row) {
        std::ostringstream oss;
        oss << "("
            << hdr->transform[row][0] << " "
            << hdr->transform[row][1] << " "
            << hdr->transform[row][2]
            << ")";
        fields.push_back({
            "Transform[" + std::to_string(row) + "]",
            "Vector3",
            oss.str()
            });
    }

    // 3) Name length
    uint32_t nameLen = hdr->name_len;
    fields.push_back({
        "NameLength",
        "uint32_t",
        std::to_string(nameLen)
        });

    // 4) Name string immediately follows the struct header
    size_t offset = sizeof(W3dPlaceholderStruct);
    if (offset + nameLen > len) {
        nameLen = static_cast<uint32_t>(len - offset);
    }
    std::string name(reinterpret_cast<const char*>(data.data() + offset), nameLen);
    // trim trailing NUL/whitespace
    while (!name.empty() && (name.back() == '\0' || std::isspace((unsigned char)name.back()))) {
        name.pop_back();
    }
    fields.push_back({ "Name", "string", name });

    return fields;
}
