#pragma once
#include "W3DStructs.h"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip> 
#include <cstring>


inline std::vector<ChunkField> InterpretHierarchyHeader(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t MIN_SZ = sizeof(W3dHierarchyStruct);
    if (buf.size() < MIN_SZ) {
        
        return fields;
    }

   
    auto hdr = reinterpret_cast<const W3dHierarchyStruct*>(buf.data());

    fields.emplace_back("Version", "string", FormatVersion(hdr->Version));
    fields.emplace_back("Name", "string", FormatName(hdr->Name, W3D_NAME_LEN));
    fields.emplace_back("NumPivots", "uint32", std::to_string(hdr->NumPivots));
    fields.emplace_back("Center", "vector", FormatVec3(hdr->Center));

    return fields;
}




inline std::string FormatQuat(const W3dQuaternionStruct& q) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(6)
        << q.Q[0] << " " << q.Q[1] << " " << q.Q[2] << " " << q.Q[3];
    return o.str();
}

inline std::vector<ChunkField> InterpretPivots(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dPivotStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed Pivots chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    auto pivots = reinterpret_cast<const W3dPivotStruct*>(buf.data());
    size_t count = buf.size() / REC;
    for (size_t i = 0; i < count; ++i) {
        const auto& p = pivots[i];
        std::string pfx = "Pivot[" + std::to_string(i) + "]";

        fields.emplace_back(
            pfx + ".Name", "string",
            FormatName(p.Name, W3D_NAME_LEN)
        );
        fields.emplace_back(
            pfx + ".Parentidx", "int32",
            std::to_string(static_cast<int32_t>(p.ParentIdx))
        );
        fields.emplace_back(
            pfx + ".Translation", "vector",
            FormatVec3(p.Translation)
        );
        fields.emplace_back(
            pfx + ".EulerAngles", "vector",
            FormatVec3(p.EulerAngles)
        );
        fields.emplace_back(
            pfx + ".Rotation", "quaternion",
            FormatQuat(p.Rotation)
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretPivotFixups(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dPivotFixupStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed PivotFixups; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    auto fixes = reinterpret_cast<const W3dPivotFixupStruct*>(buf.data());
    size_t count = buf.size() / REC;

    for (size_t i = 0; i < count; ++i) {
        const auto& PF = fixes[i];
        std::string pfx = "PivotFixup[" + std::to_string(i) + "]";

        for (size_t row = 0; row < 4; ++row) {
            W3dVectorStruct v{
                PF.TM[row][0],
                PF.TM[row][1],
                PF.TM[row][2]
            };
            fields.emplace_back(
                pfx + ".Row" + std::to_string(row),
                "vector3",
                FormatVec3(v)
            );
        }
    }

    return fields;
}