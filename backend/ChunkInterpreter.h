#pragma once
#include "ChunkData.h"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip> // for std::setw
#include <iostream>

static const std::unordered_map<uint32_t, std::string> vertexMatFlags = {
    {0x00000001, "W3DVERTMAT_USE_DEPTH_CUE"},
    {0x00000002, "W3DVERTMAT_ARGB_EMISSIVE_ONLY"},
    {0x00000004, "W3DVERTMAT_COPY_SPECULAR_TO_DIFFUSE"},
    {0x00000008, "W3DVERTMAT_DEPTH_CUE_TO_ALPHA"},

    // STAGE0_MAPPING_MASK group
    {0x00000100, "W3DVERTMAT_STAGE0_MAPPING_UV"},
    {0x00000200, "W3DVERTMAT_STAGE0_MAPPING_ENVIRONMENT"},
    {0x00000300, "W3DVERTMAT_STAGE0_MAPPING_CHEAP_ENVIRONMENT"},
    {0x00000400, "W3DVERTMAT_STAGE0_MAPPING_SCREEN"},
    {0x00000500, "W3DVERTMAT_STAGE0_MAPPING_LINEAR_OFFSET"},
    {0x00000600, "W3DVERTMAT_STAGE0_MAPPING_SILHOUETTE"},
    {0x00000700, "W3DVERTMAT_STAGE0_MAPPING_SCALE"},
    {0x00000800, "W3DVERTMAT_STAGE0_MAPPING_GRID"},
    {0x00000900, "W3DVERTMAT_STAGE0_MAPPING_ROTATE"},
    {0x00000A00, "W3DVERTMAT_STAGE0_MAPPING_SINE_LINEAR_OFFSET"},
    {0x00000B00, "W3DVERTMAT_STAGE0_MAPPING_STEP_LINEAR_OFFSET"},
    {0x00000C00, "W3DVERTMAT_STAGE0_MAPPING_ZIGZAG_LINEAR_OFFSET"},
    {0x00000D00, "W3DVERTMAT_STAGE0_MAPPING_WS_CLASSIC_ENV"},
    {0x00000E00, "W3DVERTMAT_STAGE0_MAPPING_WS_ENVIRONMENT"},
    {0x00000F00, "W3DVERTMAT_STAGE0_MAPPING_GRID_CLASSIC_ENV"},
    {0x00001000, "W3DVERTMAT_STAGE0_MAPPING_GRID_ENVIRONMENT"},
    {0x00001100, "W3DVERTMAT_STAGE0_MAPPING_RANDOM"},
    {0x00001200, "W3DVERTMAT_STAGE0_MAPPING_BUMPENV"},

    // STAGE1_MAPPING_MASK group
    {0x00010000, "W3DVERTMAT_STAGE1_MAPPING_UV"},
    {0x00020000, "W3DVERTMAT_STAGE1_MAPPING_ENVIRONMENT"},
    {0x00030000, "W3DVERTMAT_STAGE1_MAPPING_CHEAP_ENVIRONMENT"},
    {0x00040000, "W3DVERTMAT_STAGE1_MAPPING_SCREEN"},
    {0x00050000, "W3DVERTMAT_STAGE1_MAPPING_LINEAR_OFFSET"},
    {0x00060000, "W3DVERTMAT_STAGE1_MAPPING_SILHOUETTE"},
    {0x00070000, "W3DVERTMAT_STAGE1_MAPPING_SCALE"},
    {0x00080000, "W3DVERTMAT_STAGE1_MAPPING_GRID"},
    {0x00090000, "W3DVERTMAT_STAGE1_MAPPING_ROTATE"},
    {0x000A0000, "W3DVERTMAT_STAGE1_MAPPING_SINE_LINEAR_OFFSET"},
    {0x000B0000, "W3DVERTMAT_STAGE1_MAPPING_STEP_LINEAR_OFFSET"},
    {0x000C0000, "W3DVERTMAT_STAGE1_MAPPING_ZIGZAG_LINEAR_OFFSET"},
    {0x000D0000, "W3DVERTMAT_STAGE1_MAPPING_WS_CLASSIC_ENV"},
    {0x000E0000, "W3DVERTMAT_STAGE1_MAPPING_WS_ENVIRONMENT"},
    {0x000F0000, "W3DVERTMAT_STAGE1_MAPPING_GRID_CLASSIC_ENV"},
    {0x00100000, "W3DVERTMAT_STAGE1_MAPPING_GRID_ENVIRONMENT"},
    {0x00110000, "W3DVERTMAT_STAGE1_MAPPING_RANDOM"},
    {0x00120000, "W3DVERTMAT_STAGE1_MAPPING_BUMPENV"},

    // PSX (platform-specific) bits
    {0x80000000, "W3DVERTMAT_PSX_NO_RT_LIGHTING"},
    {0x00000000, "W3DVERTMAT_PSX_TRANS_NONE"},
    {0x10000000, "W3DVERTMAT_PSX_TRANS_100"},
    {0x20000000, "W3DVERTMAT_PSX_TRANS_50"},
    {0x30000000, "W3DVERTMAT_PSX_TRANS_25"},
    {0x40000000, "W3DVERTMAT_PSX_TRANS_MINUS_100"}
};



//void DumpHex(const uint8_t* data, size_t length) {
 //   std::ostringstream oss;
//    for (size_t i = 0; i < length; i += 16) {
//        oss << std::hex << std::setw(8) << std::setfill('0') << i << "  ";
 //       for (size_t j = 0; j < 16; ++j) {
//            if (i + j < length) {
 //               oss << std::setw(2) << std::setfill('0') << (int)data[i + j] << " ";
 //           }
//            else {
//                oss << "   ";
 //           }
//        }
 //       oss << " | ";
//        for (size_t j = 0; j < 16 && (i + j < length); ++j) {
 //           char c = data[i + j];
//            oss << (isprint(static_cast<unsigned char>(c)) ? c : '.');
//        }
//        oss << "\n";
 //   }
 //   std::cout << "[DEBUG] Raw W3D_CHUNK_MESH_HEADER3:\n" << oss.str() << std::endl;
//}

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

        std::string prefix = "Pivot[" + std::to_string(i) + "]";

        fields.push_back({ prefix + ".Name", "char[16]", name });
        fields.push_back({ prefix + ".Parent", "int32", std::to_string(parentIdx) });
        fields.push_back({ prefix + ".Translation", "vector", tStr.str() });
        fields.push_back({ prefix + ".EulerAngles", "vector", eStr.str() });
        fields.push_back({ prefix + ".Rotation", "quaternion", qStr.str() });
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

inline std::vector<ChunkField> InterpretVertices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    size_t vertexSize = sizeof(float) * 3;
    size_t count = chunk->data.size() / vertexSize;
    const float* ptr = reinterpret_cast<const float*>(chunk->data.data());

    for (size_t i = 0; i < count; ++i) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6)
            << ptr[i * 3 + 0] << " "
            << ptr[i * 3 + 1] << " "
            << ptr[i * 3 + 2];

        fields.push_back({ "Vertex[" + std::to_string(i) + "]", "vector3", oss.str() });
    }

    return fields;
}



#include <unordered_map>
#include <iostream>

inline std::vector<ChunkField> InterpretMeshHeader3(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    size_t size = chunk->data.size();

    auto readF32 = [&](size_t offset) { return (offset + 4 <= size) ? *reinterpret_cast<const float*>(data + offset) : 0.0f; };
    auto readU32 = [&](size_t offset) { return (offset + 4 <= size) ? *reinterpret_cast<const uint32_t*>(data + offset) : 0u; };
    auto readU8 = [&](size_t offset) { return (offset + 1 <= size) ? data[offset] : 0u; };
    auto readStr = [&](size_t offset, size_t max = 16) {
        if (offset + max > size) return std::string("out-of-bounds");
        const char* raw = reinterpret_cast<const char*>(data + offset);
        return std::string(raw, strnlen(raw, max));
        };
    auto readVec3 = [&](size_t off) {
        if (off + 12 > size) return std::string("out-of-bounds");
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6)
            << readF32(off) << " "
            << readF32(off + 4) << " "
            << readF32(off + 8);
        return oss.str();
        };

    std::unordered_map<uint8_t, std::string> sortLevelNames = { {0, "NONE"} };
    std::unordered_map<uint8_t, std::string> prelitNames = { {0, "N/A"} };
    std::unordered_map<uint32_t, std::string> vertexChannelFlags = {
        {0x01, "W3D_VERTEX_CHANNEL_LOCATION"},
        {0x02, "W3D_VERTEX_CHANNEL_NORMAL"},
        {0x04, "W3D_VERTEX_CHANNEL_COLOR"},
        {0x08, "W3D_VERTEX_CHANNEL_TEXCOORD"}
    };
    std::unordered_map<uint32_t, std::string> faceChannelFlags = {
        {0x01, "W3D_FACE_CHANNEL_FACE"}
    };
    std::unordered_map<uint32_t, std::string> meshFlagBits = {
        {0, "W3D_MESH_FLAG_GEOMETRY_TYPE_NORMAL"}
    };

    uint32_t rawVer = readU32(0x00);
    int major = (rawVer >> 16) & 0xFFFF;
    int minor = rawVer & 0xFFFF;
    fields.push_back({ "Version", "string", std::to_string(major) + "." + std::to_string(minor) });

    std::string meshName = readStr(0x08);
    std::string containerName = readStr(0x18);
    std::cout << "[DEBUG] MeshName: " << meshName << "\n";
    std::cout << "[DEBUG] ContainerName: " << containerName << "\n";
    fields.push_back({ "MeshName", "string", meshName });
    fields.push_back({ "ContainerName", "string", containerName });

    uint32_t attributes = readU32(0x20);
    fields.push_back({ "Raw Attributes", "int32", std::to_string(attributes) });

    std::string attrDesc = meshFlagBits.count(attributes) ? meshFlagBits[attributes] : std::to_string(attributes);
    fields.push_back({ "Attributes (Flags)", "flag", attrDesc });


    fields.push_back({ "NumTris", "int32", std::to_string(readU32(0x28)) });
    fields.push_back({ "NumVertices", "int32", std::to_string(readU32(0x2C)) });
    fields.push_back({ "NumMaterials", "int32", std::to_string(readU32(0x30)) });
    fields.push_back({ "NumDamageStages", "int32", std::to_string(readU32(0x34)) });

    uint8_t sortLevel = readU8(0x38);
    fields.push_back({ "SortLevel", "uint8", sortLevelNames.count(sortLevel) ? sortLevelNames[sortLevel] : std::to_string(sortLevel) });

    uint8_t prelitVersion = readU8(0x39);
    fields.push_back({ "PrelitVersion", "uint8", prelitNames.count(prelitVersion) ? prelitNames[prelitVersion] : std::to_string(prelitVersion) });

    uint32_t future = readU32(0x40);
    fields.push_back({ "FutureCounts[0]", "int32", std::to_string(future) });

    uint32_t vertexChannels = readU32(0x44);
    std::ostringstream vcStr;
    for (const auto& [bit, name] : vertexChannelFlags) {
        if (vertexChannels & bit) vcStr << name << " ";
    }
    fields.push_back({ "VertexChannels", "int32", std::to_string(vertexChannels) });

    for (const auto& [bit, name] : vertexChannelFlags) {
        if (vertexChannels & bit) {
            fields.push_back({ "VertexChannels", "flag", name });
        }
    }

    uint32_t faceChannels = readU32(0x48);
    fields.push_back({ "FaceChannels", "int32", std::to_string(faceChannels) });
    for (const auto& [bit, name] : faceChannelFlags) {
        if (faceChannels & bit) {
            fields.push_back({ "FaceChannels", "flag", name });
        }
    }
    size_t offset = 0x4C;
    if (offset + 36 + 4 <= size) {
        fields.push_back({ "Min", "vector", readVec3(offset) }); offset += 12;
        fields.push_back({ "Max", "vector", readVec3(offset) }); offset += 12;
        fields.push_back({ "SphCenter", "vector", readVec3(offset) }); offset += 12;
        fields.push_back({ "SphRadius", "float", std::to_string(readF32(offset)) }); offset += 4;
    }

    return fields;
}
inline std::vector<ChunkField> InterpretVertexNormals(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const float* raw = reinterpret_cast<const float*>(chunk->data.data());
    size_t count = chunk->data.size() / 12;

    for (size_t i = 0; i < count; ++i) {
        std::ostringstream value;
        value << std::fixed << std::setprecision(6)
            << raw[i * 3 + 0] << " "
            << raw[i * 3 + 1] << " "
            << raw[i * 3 + 2];

        std::ostringstream label;
        label << "Normal[" << i << "]";

        fields.push_back({ label.str(), "vector", value.str() });
    }

    return fields;
}

inline std::vector<ChunkField> InterpretTriangles(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    size_t count = chunk->data.size() / 32;

    for (size_t i = 0; i < count; ++i) {
        size_t offset = i * 32;

        // Vertex indices
        std::ostringstream vi;
        const uint32_t* indices = reinterpret_cast<const uint32_t*>(data + offset);
        vi << indices[0] << " " << indices[1] << " " << indices[2];
        fields.push_back({ "Triangle[" + std::to_string(i) + "].VertexIndices", "int32[3]", vi.str() });

        // Attributes
        uint32_t attrib = *reinterpret_cast<const uint32_t*>(data + offset + 12);
        fields.push_back({ "Triangle[" + std::to_string(i) + "].Attributes", "int32", std::to_string(attrib) });

        // Normal
        const float* normal = reinterpret_cast<const float*>(data + offset + 16);
        std::ostringstream normalStr;
        normalStr << std::fixed << std::setprecision(6)
            << normal[0] << " " << normal[1] << " " << normal[2];
        fields.push_back({ "Triangle[" + std::to_string(i) + "].Normal", "vector", normalStr.str() });

        // Distance
        float dist = *reinterpret_cast<const float*>(data + offset + 28);
        fields.push_back({ "Triangle[" + std::to_string(i) + "].Dist", "float", std::to_string(dist) });
    }

    return fields;
}

inline std::vector<ChunkField> InterpretVertexShadeIndices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    const uint32_t* data = reinterpret_cast<const uint32_t*>(chunk->data.data());
    size_t count = chunk->data.size() / 4;

    for (size_t i = 0; i < count; ++i) {
        std::ostringstream label;
        label << "Index[" << i << "]";
        fields.push_back({ label.str(), "int32", std::to_string(data[i]) });
    }

    return fields;
}

inline std::vector<ChunkField> InterpretMaterialInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    const uint32_t* raw = reinterpret_cast<const uint32_t*>(chunk->data.data());
    size_t count = chunk->data.size() / 16;

    for (size_t i = 0; i < count; ++i) {
        size_t base = i * 4;

        fields.push_back({ "Material[" + std::to_string(i) + "].PassCount", "int32", std::to_string(raw[base + 0]) });
        fields.push_back({ "Material[" + std::to_string(i) + "].VertexMaterialCount", "int32", std::to_string(raw[base + 1]) });
        fields.push_back({ "Material[" + std::to_string(i) + "].ShaderCount", "int32", std::to_string(raw[base + 2]) });
        fields.push_back({ "Material[" + std::to_string(i) + "].TextureCount", "int32", std::to_string(raw[base + 3]) });
    }

    return fields;
}

inline std::vector<ChunkField> InterpretVertexMaterialName(const std::shared_ptr<ChunkItem>& chunk) {
    std::string name(reinterpret_cast<const char*>(chunk->data.data()));
    std::cout << "[DEBUG] Material Name chunk size: " << chunk->data.size() << "\n";
    std::cout << "[DEBUG] Chunk ID: 0x" << std::hex << chunk->id << std::dec << "\n";

    for (size_t i = 0; i < chunk->data.size(); ++i) {
        std::cout << std::hex << (int)chunk->data[i] << " ";
    }
    std::cout << std::dec << "\n";
    return { { "Material Name", "string", name } };
}

struct Vector3 {
    float x, y, z;
};

struct W3dVertexMaterialInfo {
    uint32_t Attributes;
    Vector3 Ambient;
    Vector3 Diffuse;
    Vector3 Specular;
    Vector3 Emissive;
    float Shininess;
    float Opacity;
    float Translucency;
};
std::vector<ChunkField> InterpretVertexMaterialInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    auto readF32 = [&](size_t offset) {
        return (offset + 4 <= chunk->data.size())
            ? *reinterpret_cast<const float*>(&data[offset])
            : 0.0f;
        };

    auto readRGB = [&](size_t offset) {
        if (offset + 3 > chunk->data.size()) return std::string("out-of-bounds");
        std::ostringstream oss;
        oss << "(" << std::to_string(data[offset]) << " "
            << std::to_string(data[offset + 1]) << " "
            << std::to_string(data[offset + 2]) << ")";
        return oss.str();
        };

    if (chunk->data.size() < 32) {
        fields.push_back({ "Error", "string", "Material info too short" });
        return fields;
    }

    uint32_t attr = *reinterpret_cast<const uint32_t*>(&data[0]);

    std::ostringstream attrFlags;

    // Bitwise flags
    if (attr & 0x00000001) attrFlags << "W3DVERTMAT_USE_DEPTH_CUE ";
    if (attr & 0x00000002) attrFlags << "W3DVERTMAT_ARGB_EMISSIVE_ONLY ";
    if (attr & 0x00000004) attrFlags << "W3DVERTMAT_COPY_SPECULAR_TO_DIFFUSE ";
    if (attr & 0x00000008) attrFlags << "W3DVERTMAT_DEPTH_CUE_TO_ALPHA ";

    // Stage 0 Mapping
    switch (attr & 0x00000F00) {
    case 0x00000000: attrFlags << "W3DVERTMAT_STAGE0_MAPPING_UV "; break;
    case 0x00000100: attrFlags << "W3DVERTMAT_STAGE0_MAPPING_ENVIRONMENT "; break;
    case 0x00000200: attrFlags << "W3DVERTMAT_STAGE0_MAPPING_CHEAP_ENVIRONMENT "; break;
    case 0x00000300: attrFlags << "W3DVERTMAT_STAGE0_MAPPING_SCREEN "; break;
    case 0x00000400: attrFlags << "W3DVERTMAT_STAGE0_MAPPING_LINEAR_OFFSET "; break;
    case 0x00000500: attrFlags << "W3DVERTMAT_STAGE0_MAPPING_SILHOUETTE "; break;
    }

    // Stage 1 Mapping
    switch (attr & 0x0000F000) {
    case 0x00000000: attrFlags << "W3DVERTMAT_STAGE1_MAPPING_UV "; break;
    case 0x00001000: attrFlags << "W3DVERTMAT_STAGE1_MAPPING_ENVIRONMENT "; break;
    case 0x00002000: attrFlags << "W3DVERTMAT_STAGE1_MAPPING_CHEAP_ENVIRONMENT "; break;
    case 0x00003000: attrFlags << "W3DVERTMAT_STAGE1_MAPPING_SCREEN "; break;
    case 0x00004000: attrFlags << "W3DVERTMAT_STAGE1_MAPPING_LINEAR_OFFSET "; break;
    case 0x00005000: attrFlags << "W3DVERTMAT_STAGE1_MAPPING_SILHOUETTE "; break;
    }

    // PSX flags
    if (attr & 0x00F00000) {
        if (attr & 0x00100000) {
            attrFlags << "W3DVERTMAT_PSX_NO_RT_LIGHTING ";
        }
        else {
            switch (attr & 0x00F00000) {
            case 0x00200000: attrFlags << "W3DVERTMAT_PSX_TRANS_NONE "; break;
            case 0x00300000: attrFlags << "W3DVERTMAT_PSX_TRANS_100 "; break;
            case 0x00400000: attrFlags << "W3DVERTMAT_PSX_TRANS_50 "; break;
            case 0x00500000: attrFlags << "W3DVERTMAT_PSX_TRANS_25 "; break;
            case 0x00600000: attrFlags << "W3DVERTMAT_PSX_TRANS_MINUS_100 "; break;
            }
        }
    }

    // Add raw integer value first
    fields.push_back({ "Material.Attributes", "int32", std::to_string(attr) });

    // Individual flags as separate rows
    if (attr & 0x00000001) fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_USE_DEPTH_CUE" });
    if (attr & 0x00000002) fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_ARGB_EMISSIVE_ONLY" });
    if (attr & 0x00000004) fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_COPY_SPECULAR_TO_DIFFUSE" });
    if (attr & 0x00000008) fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_DEPTH_CUE_TO_ALPHA" });

    // Stage 0 Mapping
    switch (attr & 0x00000F00) {
    case 0x00000000: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_STAGE0_MAPPING_UV" }); break;
    case 0x00000100: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_STAGE0_MAPPING_ENVIRONMENT" }); break;
    case 0x00000200: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_STAGE0_MAPPING_CHEAP_ENVIRONMENT" }); break;
    case 0x00000300: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_STAGE0_MAPPING_SCREEN" }); break;
    case 0x00000400: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_STAGE0_MAPPING_LINEAR_OFFSET" }); break;
    case 0x00000500: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_STAGE0_MAPPING_SILHOUETTE" }); break;
    }

    // Stage 1 Mapping
    switch (attr & 0x0000F000) {
    case 0x00000000: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_STAGE1_MAPPING_UV" }); break;
    case 0x00001000: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_STAGE1_MAPPING_ENVIRONMENT" }); break;
    case 0x00002000: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_STAGE1_MAPPING_CHEAP_ENVIRONMENT" }); break;
    case 0x00003000: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_STAGE1_MAPPING_SCREEN" }); break;
    case 0x00004000: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_STAGE1_MAPPING_LINEAR_OFFSET" }); break;
    case 0x00005000: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_STAGE1_MAPPING_SILHOUETTE" }); break;
    }

    // PSX block
    if (attr & 0x00F00000) {
        if (attr & 0x00100000) {
            fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_PSX_NO_RT_LIGHTING" });
        }
        else {
            switch (attr & 0x00F00000) {
            case 0x00200000: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_PSX_TRANS_NONE" }); break;
            case 0x00300000: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_PSX_TRANS_100" }); break;
            case 0x00400000: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_PSX_TRANS_50" }); break;
            case 0x00500000: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_PSX_TRANS_25" }); break;
            case 0x00600000: fields.push_back({ "Material.Attributes", "flag", "W3DVERTMAT_PSX_TRANS_MINUS_100" }); break;
            }
        }
    }

    fields.push_back({ "Material.Ambient", "RGB", readRGB(4) });
    fields.push_back({ "Material.Diffuse", "RGB", readRGB(8) });
    fields.push_back({ "Material.Specular", "RGB", readRGB(12) });
    fields.push_back({ "Material.Emissive", "RGB", readRGB(16) });

    fields.push_back({ "Material.Shininess", "float", std::to_string(readF32(20)) });
    fields.push_back({ "Material.Opacity", "float", std::to_string(readF32(24)) });
    fields.push_back({ "Material.Translucency", "float", std::to_string(readF32(28)) });

    return fields;
}

