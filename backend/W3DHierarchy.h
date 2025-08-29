#pragma once
#include "W3DStructs.h"
#include "FormatUtils.h"
#include "ParseUtils.h"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip> 
#include <cstring>


inline std::vector<ChunkField> InterpretHierarchyHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto buff = ParseChunkStruct<W3dHierarchyStruct>(chunk);
    if (auto err = std::get_if<std::string>(&buff)) {
        fields.emplace_back("error", "string", "Malformed Hierarchy_Header chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<W3dHierarchyStruct>(buff);
    ChunkFieldBuilder B(fields);

    B.Version("Version", data.Version);
    B.Name("Name", data.Name);
    B.UInt32("NumPivots", data.NumPivots);
    B.Vec3("Center", data.Center);
    return fields;
}





inline std::vector<ChunkField> InterpretPivots(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dPivotStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed Pivot chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dPivotStruct>>(parsed);

    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < data.size(); ++i) {
        const auto& piv = data[i];
        const std::string base = "Pivot[" + std::to_string(i) + "]";

        // Name
        {
            const std::string label = base + ".Name";
            B.Name(label.c_str(), piv.Name);
        }

        // ParentIdx: show -1 when it's the root sentinel (0xFFFFFFFF)
        if (piv.ParentIdx == 0xFFFFFFFFu) {
            B.Int32(base + ".ParentIdx", -1);
        }
        else {
            B.UInt32(base + ".ParentIdx", piv.ParentIdx);
        }

        // Transforms
        B.Vec3(base + ".Translation", piv.Translation);
        B.Vec3(base + ".EulerAngles", piv.EulerAngles);
        B.Quat(base + ".Rotation", piv.Rotation);
    }

    return fields;
}




inline std::vector<ChunkField> InterpretPivotFixups(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dPivotFixupStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed PivotFixups chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dPivotFixupStruct>>(parsed);

    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < data.size(); ++i) {
        const auto& pf = data[i];

        // 4 rows of 3 floats each (Max 3x4)
        for (int r = 0; r < 4; ++r) {
            const std::string label = "Transform[" + std::to_string(i) + "].Row[" + std::to_string(r) + "]";
            // Build a W3dVectorStruct from TM[r][0..2]
            W3dVectorStruct row{ pf.TM[r][0], pf.TM[r][1], pf.TM[r][2] };
            B.Vec3(label, row);
        }
    }

    return fields;
}

