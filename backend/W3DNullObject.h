#pragma once
#include "W3DStructs.h"
#include <vector>

//TODO: Find Example
inline std::vector<ChunkField> InterpretNullObject(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    // Parse once, validate size
    auto v = ParseChunkStruct<W3dNullObjectStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed NULL_OBJECT chunk: " + *err);
        return fields;
    }
    const auto& obj = std::get<W3dNullObjectStruct>(v);

    ChunkFieldBuilder B(fields);

    // Version (packed major.minor, consistent with your other headers)
    B.Version("Version", obj.Version);

    // Raw attributes first (prefix matches your convention, e.g. Texture.Attributes / Box.Attributes)
    B.UInt32("Null.Attributes", obj.Attributes);

    // Name (fixed-size, NUL-terminated)
    B.Name("Name", obj.Name, 2 * W3D_NAME_LEN);

    return fields;
}


