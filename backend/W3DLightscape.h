#pragma once
#include "W3DStructs.h"
#include <vector>

inline std::vector<ChunkField> InterpretLightTransform(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;

    // Must be at least as big as our struct
    if (!chunk || chunk->data.size() < sizeof(W3dLightTransformStruct)) {
        fields.emplace_back("error", "string", "Invalid LIGHT_TRANSFORM chunk");
        return fields;
    }

    // Cast straight into our struct
    auto const* lt = reinterpret_cast<const W3dLightTransformStruct*>(chunk->data.data());

    // Each row is a float[4]
    for (int row = 0; row < 3; ++row) {
        std::ostringstream oss;
        oss << "("
            << lt->Transform[row][0] << " "
            << lt->Transform[row][1] << " "
            << lt->Transform[row][2] << " "
            << lt->Transform[row][3] << ")";
        fields.emplace_back(
            "Transform[" + std::to_string(row) + "]",
            "float[4]",
            oss.str()
        );
    }

    return fields;
}
