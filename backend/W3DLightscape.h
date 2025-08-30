#pragma once
#include "W3DStructs.h"
#include <vector>


inline std::vector<ChunkField> InterpretLightTransform(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk || chunk->data.size() < sizeof(W3dLightTransformStruct)) {
        fields.emplace_back("error", "string", "Invalid LIGHT_TRANSFORM chunk");
        return fields;
    }

    const auto* lt = reinterpret_cast<const W3dLightTransformStruct*>(chunk->data.data());
    ChunkFieldBuilder B(fields);

    for (int r = 0; r < 3; ++r) {
        const std::string base = "Transform[" + std::to_string(r) + "]";
        B.Float(base + ".X", lt->Transform[r][0]);
        B.Float(base + ".Y", lt->Transform[r][1]);
        B.Float(base + ".Z", lt->Transform[r][2]);
        B.Float(base + ".W", lt->Transform[r][3]); // translation component per row
    }

    return fields;
}
