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
    B.Name("MeshName", data.Name);
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
        const std::string pfx = "Pivot[" + std::to_string(i) + "]";
        for (int j = 0; j < 2; ++j) {
            B.Name(pfx + ".Name[" + std::to_string(j) + "]", piv.Name[j]);
            B.UInt16(pfx + ".ParentIDx[" + std::to_string(j) + "]", piv.ParentIDx[j]);
            B.Vec3(pfx + ".Translation[" + std::to_string(j) + "]", piv.Translation[j]);
            B.Vec3(pfx + ".EulerAngles[" + std::to_string(j) + "]", piv.EulerAngles[j]);
            B.Quat(pfx + ".Rotation[" + std::to_string(j) + "]", piv.Rotation[j]);
        }
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
        for (int j = 0; j < 4; ++j) {
            const std::string pfx = "Transform" + std::to_string(j) + "," + "Row";

            B.Vec3(pfx + [ + std::to_string(i) + "]", pf.TM[i]);

        }
    }

    return fields;
}