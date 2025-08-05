#pragma once
#include "W3DStructs.h"
#include <vector>

inline std::vector<ChunkField> InterpretNullObject(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    // Must be at least the size of our struct
    if (!chunk || chunk->data.size() < sizeof(W3dNullObjectStruct)) {
        fields.emplace_back("Error", "string", "Invalid NULL_OBJECT chunk");
        return fields;
    }

    // Cast to our struct
    auto obj = reinterpret_cast<const W3dNullObjectStruct*>(chunk->data.data());

    // 1) Version
    {
        uint16_t major = uint16_t(obj->Version >> 16);
        uint16_t minor = uint16_t(obj->Version & 0xFFFF);
        fields.emplace_back(
            "Version", "uint32_t",
            std::to_string(major) + "." + std::to_string(minor)
        );
    }

    // 2) Attributes (hex + decimal)
    {
        std::ostringstream ss;
        ss << "0x" << std::hex << obj->Attributes
            << std::dec << " (" << obj->Attributes << ")";
        fields.emplace_back("Attributes", "uint32_t", ss.str());
    }

    // 3) Name (trim at first NUL)
    {
        const char* raw = obj->Name;
        size_t len = 0, max = sizeof(obj->Name);
        // scan up to the first NUL (or max)
        while (len < max && raw[len] != '\0') {
            ++len;
        }
        fields.emplace_back("Name", "string", std::string(raw, len));
    }

    return fields;
}

