#pragma once
#include "W3DStructs.h"
#include <vector>


inline std::vector<ChunkField> InterpretPoints(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed POINTS chunk: " + *err);
        return fields;
    }
    const auto& pts = std::get<std::vector<W3dVectorStruct>>(parsed);

    ChunkFieldBuilder B(fields);
    B.UInt32("Count", static_cast<uint32_t>(pts.size()));

    for (size_t i = 0; i < pts.size(); ++i) {
        B.Vec3("Point[" + std::to_string(i) + "]", pts[i]);
    }

    return fields;
}