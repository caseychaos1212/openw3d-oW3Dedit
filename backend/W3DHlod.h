#pragma once
#include "W3DStructs.h"
#include <vector>
#include <algorithm>



inline std::vector<ChunkField> InterpretHLODHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    // Must have at least the size of your header struct
    if (!chunk || chunk->data.size() < sizeof(W3dHLodHeaderStruct)) {
        fields.emplace_back("Error", "string", "HLOD header too short");
        return fields;
    }

    // Cast once
    auto hdr = reinterpret_cast<const W3dHLodHeaderStruct*>(chunk->data.data());

    // 1) version packed as Major<<16 | Minor
    uint16_t major = static_cast<uint16_t>((hdr->Version >> 16) & 0xFFFF);
    uint16_t minor = static_cast<uint16_t>(hdr->Version & 0xFFFF);
    fields.emplace_back(
        "Version", "string",
        std::to_string(major) + "." + std::to_string(minor)
    );

    // 2) LOD count
    fields.emplace_back(
        "LodCount", "uint32",
        std::to_string(hdr->LodCount)
    );

    // 3) Name (trim at first NUL)
    {
        std::string name(hdr->Name, strnlen(hdr->Name, W3D_NAME_LEN));
        fields.emplace_back("Name", "string", name);
    }

    // 4) HierarchyName (trim at first NUL)
    {
        std::string ht(hdr->HierarchyName, strnlen(hdr->HierarchyName, W3D_NAME_LEN));
        fields.emplace_back("HierarchyName", "string", ht);
    }

    return fields;
}


inline std::vector<ChunkField> InterpretHLODSubObjectArrayHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    // The header must be at least the size of your struct
    if (!chunk || chunk->data.size() < sizeof(W3dHLodArrayHeaderStruct)) {
        fields.emplace_back("Error", "string", "HLOD SubObjectArrayHeader too short");
        return fields;
    }

    // Cast into your POD struct
    auto hdr = reinterpret_cast<const W3dHLodArrayHeaderStruct*>(chunk->data.data());

    // 1) ModelCount
    fields.emplace_back(
        "ModelCount", "uint32",
        std::to_string(hdr->ModelCount)
    );

    // 2) MaxScreenSize, formatted
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(6) << hdr->MaxScreenSize;
        fields.emplace_back("MaxScreenSize", "float", ss.str());
    }

    return fields;
}


inline std::vector<ChunkField> InterpretHLODSubObject_LodArray(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    // Must be at least as big as our struct
    if (!chunk || chunk->data.size() < sizeof(W3dHLodSubObjectStruct)) {
        fields.emplace_back("Error", "string", "HLOD sub object too small");
        return fields;
    }

    // Cast to our header struct
    auto hdr = reinterpret_cast<const W3dHLodSubObjectStruct*>(chunk->data.data());

    // Read bone index
    fields.emplace_back(
        "BoneIndex", "uint32",
        std::to_string(hdr->BoneIndex)
    );

    //  Name find the first NUL in the fixed length array
    size_t maxLen = sizeof hdr->Name;
    size_t len = 0;
    while (len < maxLen && hdr->Name[len] != '\0') {
        ++len;
    }
    std::string name(hdr->Name, len);
    fields.emplace_back("Name", "string", name);

    return fields;
}
