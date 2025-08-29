#pragma once
#include "W3DStructs.h"
#include <vector>


inline std::vector<ChunkField> InterpretDazzleName(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    ChunkFieldBuilder B(fields);
    B.NullTerm(
        "DazzleName",
        reinterpret_cast<const char*>(chunk->data.data()),
        chunk->data.size()
    );
    return fields;
}

inline std::vector<ChunkField> InterpretDazzleTypeName(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    ChunkFieldBuilder B(fields);
    B.NullTerm(
        "DazzleTypeName",
        reinterpret_cast<const char*>(chunk->data.data()),
        chunk->data.size()
    );
    return fields;
}
