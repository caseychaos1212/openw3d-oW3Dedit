#pragma once
#include "W3DStructs.h"
#include <vector>


inline std::vector<ChunkField> InterpretPoints(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty POINTS chunk");
        return fields;
    }

    auto const& buf = chunk->data;
    size_t totalBytes = buf.size();
    constexpr size_t stride = sizeof(W3dVectorStruct);

    // must be an exact multiple of our struct size
    if (totalBytes % stride != 0) {
        fields.emplace_back(
            "error", "string",
            "Unexpected POINTS chunk size: " + std::to_string(totalBytes)
        );
        return fields;
    }

    size_t count = totalBytes / stride;
    auto const* pts = reinterpret_cast<const W3dVectorStruct*>(buf.data());

    for (size_t i = 0; i < count; ++i) {
        auto const& v = pts[i];
        std::ostringstream oss;
        oss << "("
            << std::fixed << std::setprecision(6)
            << v.X << " "
            << v.Y << " "
            << v.Z << ")";

        fields.emplace_back(
            "Point[" + std::to_string(i) + "]",
            "vector3",
            oss.str()
        );
    }

    return fields;
}