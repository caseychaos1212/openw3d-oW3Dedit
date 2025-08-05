#pragma once
#include "W3DStructs.h"
#include <vector>

inline std::vector<ChunkField> InterpretDazzleName(
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
      "Dazzle Name",
      "string",
      nts.value
        });
    return fields;
}

inline std::vector<ChunkField> InterpretDazzleTypeName(
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
      "Dazzle Type Name",
      "string",
      nts.value
        });
    return fields;
}