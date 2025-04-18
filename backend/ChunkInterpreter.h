#pragma once
#include "ChunkData.h"
#include <vector>
#include <string>
#include <sstream>

struct ChunkField {
    std::string field;
    std::string type;
    std::string value;
};

inline std::vector<ChunkField> InterpretHierarchyHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    if (chunk->data.size() < 0x24) // 4 + 16 + 4 + 12
        return fields;

    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    auto readU32 = [&](size_t offset) -> uint32_t {
        return *reinterpret_cast<const uint32_t*>(data + offset);
        };
    auto readS32 = [&](size_t offset) -> int32_t {
        return *reinterpret_cast<const int32_t*>(data + offset);
        };
    auto readF32 = [&](size_t offset) -> float {
        return *reinterpret_cast<const float*>(data + offset);
        };

    uint32_t version = readU32(0x00);
    std::string name(reinterpret_cast<const char*>(data + 0x04), 16);
    name = name.c_str();  // trim garbage after null
    int32_t numPivots = readS32(0x14);
    float cx = readF32(0x18);
    float cy = readF32(0x1C);
    float cz = readF32(0x20);

    uint16_t major = (version >> 16) & 0xFFFF;
    uint16_t minor = version & 0xFFFF;
    std::ostringstream vstr;
    vstr << major << "." << minor;
    fields.push_back({ "Version", "string", vstr.str() });
    fields.push_back({ "Name", "char[16]", name });
    fields.push_back({ "NumPivots", "int32_t", std::to_string(numPivots) });

    std::ostringstream center;
    center << std::fixed << std::setprecision(6);
    center << cx << " " << cy << " " << cz;
    fields.push_back({ "Center", "vector", center.str() });

    return fields;
}

inline std::vector<ChunkField> InterpretPivots(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    size_t pivotSize = 60;
    size_t count = chunk->data.size() / pivotSize;

    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    for (size_t i = 0; i < count; ++i) {
        size_t offset = i * pivotSize;
        const char* namePtr = reinterpret_cast<const char*>(data + offset);
        std::string name(namePtr, 16);
        name = name.c_str(); // trim nulls

        int32_t parentIdx = *reinterpret_cast<const int32_t*>(data + offset + 0x10);

        const float* t = reinterpret_cast<const float*>(data + offset + 0x14); // translation
        const float* e = reinterpret_cast<const float*>(data + offset + 0x20); // euler
        const float* q = reinterpret_cast<const float*>(data + offset + 0x2C); // quaternion

        std::ostringstream tStr, eStr, qStr;
        tStr << std::fixed << std::setprecision(6) << t[0] << " " << t[1] << " " << t[2];
        eStr << std::fixed << std::setprecision(6) << e[0] << " " << e[1] << " " << e[2];
        qStr << std::fixed << std::setprecision(6) << q[0] << " " << q[1] << " " << q[2] << " " << q[3];

        fields.push_back({ name, "Pivot", "Parent: " + std::to_string(parentIdx) });
        fields.push_back({ "Translation", "vector", tStr.str() });
        fields.push_back({ "EulerAngles", "vector", eStr.str() });
        fields.push_back({ "Rotation", "quaternion", qStr.str() });
    }

    return fields;
}






inline std::vector<ChunkField> InterpretPivotFixups(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    size_t floatCount = chunk->data.size() / sizeof(float);
    const float* floats = reinterpret_cast<const float*>(chunk->data.data());

    size_t pivotIndex = 0;
    for (size_t i = 0; i + 11 < floatCount; i += 12, pivotIndex++) {
        for (int row = 0; row < 4; ++row) {
            std::ostringstream label;
            label << "Transform " << pivotIndex << ", Row[" << row << "]";
            std::ostringstream value;
            value << std::fixed;
            value.precision(6);  // or 4 for less precision
            value << floats[i + row * 3 + 0] << " "
                << floats[i + row * 3 + 1] << " "
                << floats[i + row * 3 + 2];

            fields.push_back({ label.str(), "float[3]", value.str() });
        }
    }

    return fields;
}

