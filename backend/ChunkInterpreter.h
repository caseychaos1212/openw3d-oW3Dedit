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

inline std::vector<ChunkField> InterpretShaderName(const std::shared_ptr<ChunkItem>& chunk) {
    std::string name(reinterpret_cast<const char*>(chunk->data.data()));
    return { { "Shader.Name", "string", name } };
}
inline std::vector<ChunkField> InterpretShaderDetail(const std::shared_ptr<ChunkItem>& chunk) {
    std::ostringstream hexDump;
    for (size_t i = 0; i < std::min<size_t>(chunk->data.size(), 16); ++i) {
        hexDump << std::hex << (int)chunk->data[i] << " ";
    }
    return { { "Shader.Detail", "hex", hexDump.str() } };
}
std::vector<ChunkField> InterpretShaders(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    size_t size = chunk->data.size();

    static const char* depthCompare[] = { "Pass Never","Pass Less","Pass Equal","Pass Less or Equal", "Pass Greater","Pass Not Equal","Pass Greater or Equal","Pass Always" };
    static const char* depthMask[] = { "Write Disable", "Write Enable" };
    static const char* destBlend[] = { "Zero","One","Src Color","One Minus Src Color","Src Alpha","One Minus Src Alpha","Src Color Prefog" };
    static const char* priGradient[] = { "Disable","Modulate","Add","Bump-Environment" };
    static const char* secGradient[] = { "Disable","Enable" };
    static const char* srcBlend[] = { "Zero","One","Src Alpha","One Minus Src Alpha" };
    static const char* texturing[] = { "Disable","Enable" };
    static const char* detailColor[] = { "Disable","Detail","Scale","InvScale","Add","Sub","SubR","Blend","DetailBlend" };
    static const char* detailAlpha[] = { "Disable","Detail","Scale","InvScale" };
    static const char* alphaTest[] = { "Alpha Test Disable", "Alpha Test Enable" };
    static const char* PostDetailColor[] = { "Disable", "Detail", "Scale", "InvScale", "Add", "Sub", "SubR", "Blend", "DetailBlend" };
    static const char* PostDetailAlpha[] = { "Disable", "Detail", "Scale", "InvScale" };


    const size_t shaderSize = 16;
    size_t count = size / shaderSize;

    for (size_t i = 0; i + shaderSize <= chunk->data.size(); i += shaderSize) {
        std::string prefix = "shader[" + std::to_string(i / shaderSize) + "]";

        auto safeLookup = [](const char* const* arr, size_t len, uint8_t index) -> std::string {
            return index < len ? arr[index] : ("Unknown(" + std::to_string(index) + ")");
            };

        fields.push_back({ prefix + ".DepthCompare", "string", depthCompare[data[i + 0]] });
        fields.push_back({ prefix + ".DepthMask",    "string", depthMask[data[i + 1]] });
        // i + 2 is unused
        fields.push_back({ prefix + ".DestBlend",    "string", destBlend[data[i + 3]] });
        // i + 4 is unused
        fields.push_back({ prefix + ".PriGradient",  "string", priGradient[data[i + 5]] });
        fields.push_back({ prefix + ".SecGradient",  "string", secGradient[data[i + 6]] });
        fields.push_back({ prefix + ".SrcBlend",     "string", srcBlend[data[i + 7]] });
        fields.push_back({ prefix + ".Texturing",    "string", texturing[data[i + 8]] });
        fields.push_back({ prefix + ".DetailColor",  "string", detailColor[data[i + 9]] });
        fields.push_back({ prefix + ".DetailAlpha",  "string", detailAlpha[data[i + 10]] });
        // i + 11 is unused
        fields.push_back({ prefix + ".AlphaTest",    "string", alphaTest[data[i + 12]] });
        fields.push_back({ prefix + ".PostDetailColor",    "string", PostDetailColor[data[i + 13]] });
        fields.push_back({ prefix + ".PostDetailAlpha",    "string", PostDetailAlpha[data[i + 14]] });
        // i + 15 is padding
    }

    return fields;
}


std::vector<ChunkField> InterpretTextureName(const std::shared_ptr<ChunkItem>& chunk) {
    std::string name(reinterpret_cast<const char*>(chunk->data.data()));
    return { { "Texture Name", "string", name } };
}

std::vector<ChunkField> InterpretTextureInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    if (chunk->data.size() < 12) {
        fields.push_back({ "Error", "string", "Texture info too short" });
        return fields;
    }

    uint32_t attributes = *reinterpret_cast<const uint32_t*>(&data[0]);
    uint32_t frameCount = *reinterpret_cast<const uint32_t*>(&data[4]);
    float frameRate = *reinterpret_cast<const float*>(&data[8]);

    fields.push_back({ "Texture.Attributes", "int32", std::to_string(attributes) });

    if (attributes & 0x01) fields.push_back({ "Texture.Flag", "flag", "W3DTEXTURE_PUBLISH" });
    if (attributes & 0x02) fields.push_back({ "Texture.Flag", "flag", "W3DTEXTURE_NO_LOD" });
    if (attributes & 0x04) fields.push_back({ "Texture.Flag", "flag", "W3DTEXTURE_CLAMP_U" });
    if (attributes & 0x08) fields.push_back({ "Texture.Flag", "flag", "W3DTEXTURE_CLAMP_V" });
    if (attributes & 0x10) fields.push_back({ "Texture.Flag", "flag", "W3DTEXTURE_ALPHA_BITMAP" });

    fields.push_back({ "Texture.FrameCount", "int32", std::to_string(frameCount) });
    fields.push_back({ "Texture.FrameRate", "float", std::to_string(frameRate) });

    return fields;
}

inline std::vector<ChunkField> InterpretVertexMaterialIDs(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint32_t* data = reinterpret_cast<const uint32_t*>(chunk->data.data());
    size_t count = chunk->data.size() / sizeof(uint32_t);

    for (size_t i = 0; i < count; ++i) {
        std::string label = "Vertex[" + std::to_string(i) + "] Vertex Material Index";
        fields.push_back({ label, "int32", std::to_string(data[i]) });
    }


    return fields;
}

inline std::vector<ChunkField> InterpretShaderIDs(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    size_t size = chunk->data.size();

    for (size_t i = 0; i + 4 <= size; i += 4) {
        uint32_t shaderID = *reinterpret_cast<const uint32_t*>(&data[i]);
        fields.push_back({ "Face[" + std::to_string(i / 4) + "] Shader Index", "int32", std::to_string(shaderID) });

    }

    return fields;
}

inline std::vector<ChunkField> InterpretTextureStage(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    if (chunk->data.size() < 4) {
        fields.push_back({ "Error", "string", "Stage data too short" });
        return fields;
    }

    uint32_t stageNumber = *reinterpret_cast<const uint32_t*>(data);
    fields.push_back({ "StageNumber", "uint32", std::to_string(stageNumber) });

    return fields;
}

inline std::vector<ChunkField> InterpretStageTexCoords(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    size_t size = chunk->data.size();

    for (size_t i = 0; i + 8 <= size; i += 8) {
        float u = *reinterpret_cast<const float*>(&data[i]);
        float v = *reinterpret_cast<const float*>(&data[i + 4]);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << u << ", " << v;
        fields.push_back({ "Vertex[" + std::to_string(i) + "].UV", "vec2", oss.str() });

    }

    return fields;
}

inline std::vector<ChunkField> InterpretTextureIDs(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint32_t* data = reinterpret_cast<const uint32_t*>(chunk->data.data());
    size_t count = chunk->data.size() / sizeof(uint32_t);

    for (size_t i = 0; i < count; ++i) {
        fields.push_back({ "Face[" + std::to_string(i) + "] Texture Index", "int32", std::to_string(data[i]) });
    }

    return fields;
}

inline std::vector<ChunkField> InterpretAABTreeHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint32_t* data = reinterpret_cast<const uint32_t*>(chunk->data.data());

    if (chunk->data.size() < 8) { // Only 2 uint32s
        fields.push_back({ "Error", "string", "Header too short" });
        return fields;
    }

    fields.push_back({ "NodeCount", "uint32", std::to_string(data[0]) });
    fields.push_back({ "PolyCount", "uint32", std::to_string(data[1]) });

    return fields;
}


inline std::vector<ChunkField> InterpretAABTreePolyIndices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint16_t* data = reinterpret_cast<const uint16_t*>(chunk->data.data());
    size_t count = chunk->data.size() / sizeof(uint16_t);

    for (size_t i = 0, logicalIndex = 0; i + 1 < count; i += 2) {
        // Skip every second index (padding)
        fields.push_back({
            "Polygon Index[" + std::to_string(logicalIndex++) + "]",
            "int32",
            std::to_string(data[i])
            });
    }

    return fields;
}



inline std::vector<ChunkField> InterpretAABTreeNodes(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(chunk->data.data());
    const size_t stride = 32;

    if (chunk->data.size() % stride != 0) {
        fields.push_back({ "Error", "string", "Node size mismatch — expected 32-byte aligned data" });
        return fields;
    }

    size_t count = chunk->data.size() / stride;

    auto readF32 = [&](size_t offset) {
        return *reinterpret_cast<const float*>(raw + offset);
        };
    auto readU32 = [&](size_t offset) {
        return *reinterpret_cast<const uint32_t*>(raw + offset);
        };

    for (size_t i = 0; i < count; ++i) {
        size_t base = i * stride;
        std::string prefix = "Node[" + std::to_string(i) + "]";

        std::ostringstream minStr, maxStr;
        minStr << std::fixed << std::setprecision(6)
            << readF32(base + 0) << " "
            << readF32(base + 4) << " "
            << readF32(base + 8);
        maxStr << std::fixed << std::setprecision(6)
            << readF32(base + 12) << " "
            << readF32(base + 16) << " "
            << readF32(base + 20);

        fields.push_back({ prefix + ".Min", "vector", minStr.str() });
        fields.push_back({ prefix + ".Max", "vector", maxStr.str() });

        uint32_t frontOrPoly0 = readU32(base + 24);
        uint32_t backOrPolyCount = readU32(base + 28);

        if (frontOrPoly0 & 0x80000000) {
            // Leaf node
            fields.push_back({ prefix + ".Poly0", "uint32", std::to_string(frontOrPoly0 & 0x7FFFFFFF) });
            fields.push_back({ prefix + ".PolyCount", "uint32", std::to_string(backOrPolyCount) });
        }
        else {
            // Interior node
            fields.push_back({ prefix + ".Front", "int32", std::to_string(static_cast<int32_t>(frontOrPoly0)) });
            fields.push_back({ prefix + ".Back", "int32", std::to_string(static_cast<int32_t>(backOrPolyCount)) });
        }
    }

    return fields;
}

inline std::vector<ChunkField> InterpretHLOD(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    fields.push_back({ "Info", "string", "W3D_CHUNK_HLOD (wrapper)" });
    return fields;
}

inline std::vector<ChunkField> InterpretHLODHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    if (chunk->data.size() < 0x28) {
        fields.push_back({ "Error", "string", "HLOD header too short" });
        return fields;
    }

    const uint8_t* raw = reinterpret_cast<const uint8_t*>(chunk->data.data());
    uint8_t version = raw[2]; // Corrected offset
    const uint32_t lodCount = *reinterpret_cast<const uint32_t*>(&raw[4]);
    const char* namePtr = reinterpret_cast<const char*>(&raw[8]);
    const char* htreeNamePtr = reinterpret_cast<const char*>(&raw[24]);

    std::string name(namePtr, strnlen(namePtr, 16));
    std::string htreeName(htreeNamePtr, strnlen(htreeNamePtr, 16));

    fields.push_back({ "Version", "string", std::to_string(version) });
    fields.push_back({ "LodCount", "int32", std::to_string(lodCount) });
    fields.push_back({ "Name", "string", name });
    fields.push_back({ "HTree Name", "string", htreeName });

    return fields;
}
inline std::vector<ChunkField> InterpretHLODLODArray(const std::shared_ptr<ChunkItem>& chunk) {
    return {}; // no fields, wrapper only
}

inline std::vector<ChunkField> InterpretHLODSubObjectArrayHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 8) {
        fields.push_back({ "Error", "string", "Header too short" });
        return fields;
    }

    int32_t modelCount = *reinterpret_cast<const int32_t*>(&data[0]);
    float maxScreenSize = *reinterpret_cast<const float*>(&data[4]);

    fields.push_back({ "ModelCount", "int32", std::to_string(modelCount) });

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6) << maxScreenSize;
    fields.push_back({ "MaxScreenSize", "float", ss.str() });

    return fields;
}

inline std::vector<ChunkField> InterpretHLODSubObject_LodArray(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    size_t size = chunk->data.size();

    if (size < 5) {
        fields.push_back({ "Error", "string", "Too small for HLOD Sub Object" });
        return fields;
    }

    // First 4 bytes: BoneIndex
    int32_t boneIndex = *reinterpret_cast<const int32_t*>(data);

    // Remaining bytes: name string
    const char* namePtr = reinterpret_cast<const char*>(data + 4);
    std::string name(namePtr);

    fields.push_back({ "Name", "string", name });
    fields.push_back({ "BoneIndex", "int32", std::to_string(boneIndex) });
    

    return fields;
}
inline std::vector<ChunkField> InterpretAnimationHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 44) {
        fields.push_back({ "Error", "string", "Animation header too small" });
        return fields;
    }

    uint16_t minor = *reinterpret_cast<const uint16_t*>(&data[0]);
    uint16_t major = *reinterpret_cast<const uint16_t*>(&data[2]);

    std::ostringstream versionStream;
    versionStream << major << "." << minor;
    fields.push_back({ "Version", "string", versionStream.str() });




    std::string animName(reinterpret_cast<const char*>(&data[4]), 16);
    fields.push_back({ "AnimationName", "string", animName.c_str() });

    std::string hierarchyName(reinterpret_cast<const char*>(&data[20]), 16);
    fields.push_back({ "HierarchyName", "string", hierarchyName.c_str() });

    uint32_t numFrames = *reinterpret_cast<const uint32_t*>(&data[36]);
    fields.push_back({ "NumFrames", "int32", std::to_string(numFrames) });

    uint32_t frameRate = *reinterpret_cast<const uint32_t*>(&data[40]);
    fields.push_back({ "FrameRate", "int32", std::to_string(frameRate) });

    return fields;
}



inline std::vector<ChunkField> InterpretAnimationChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 12) {
        fields.push_back({ "Error", "string", "Animation channel chunk too small" });
        return fields;
    }

    uint16_t firstFrame = *reinterpret_cast<const uint16_t*>(&data[0]);
    uint16_t lastFrame = *reinterpret_cast<const uint16_t*>(&data[2]);
    uint16_t vectorLen = *reinterpret_cast<const uint16_t*>(&data[4]);
    uint16_t flags = *reinterpret_cast<const uint16_t*>(&data[6]);
    uint16_t pivot = *reinterpret_cast<const uint16_t*>(&data[8]);
    // uint16_t pad = *reinterpret_cast<const uint16_t*>(&data[10]); // skip padding

    const float* animData = reinterpret_cast<const float*>(&data[12]);

    fields.push_back({ "FirstFrame", "uint16", std::to_string(firstFrame) });
    fields.push_back({ "LastFrame", "uint16", std::to_string(lastFrame) });

    // --- Here we map Flags to a readable ChannelType name ---
    std::string channelTypeName;
    switch (flags) {
    case 0: channelTypeName = "X Translation"; break;
    case 1: channelTypeName = "Y Translation"; break;
    case 2: channelTypeName = "Z Translation"; break;
    case 3: channelTypeName = "X Rotation"; break;
    case 4: channelTypeName = "Y Rotation"; break;
    case 5: channelTypeName = "Z Rotation"; break;
    case 6: channelTypeName = "Quaternion Rotation"; break;
    default: channelTypeName = "Unknown (" + std::to_string(flags) + ")"; break;
    }
    fields.push_back({ "ChannelType", "string", channelTypeName });

    fields.push_back({ "Pivot", "uint16", std::to_string(pivot) });
    fields.push_back({ "VectorLen", "uint16", std::to_string(vectorLen) });

    // Now the floats:
    int numFrames = lastFrame - firstFrame + 1;
    for (int frameIdx = 0; frameIdx < numFrames; ++frameIdx) {
        for (int vecIdx = 0; vecIdx < vectorLen; ++vecIdx) {
            std::string name = "Data[" + std::to_string(frameIdx) + "][" + std::to_string(vecIdx) + "]";
            float value = animData[frameIdx * vectorLen + vecIdx];
            fields.push_back({ name, "float", std::to_string(value) });
        }
    }

    return fields;
}

inline std::vector<ChunkField> InterpretBitChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 9) {
        fields.push_back({ "Error", "string", "Bit channel chunk too small" });
        return fields;
    }

    uint16_t firstFrame = *reinterpret_cast<const uint16_t*>(&data[0]);
    uint16_t lastFrame = *reinterpret_cast<const uint16_t*>(&data[2]);
    uint16_t flags = *reinterpret_cast<const uint16_t*>(&data[4]);
    uint16_t pivot = *reinterpret_cast<const uint16_t*>(&data[6]);
    uint8_t  defaultVal = data[8];

    fields.push_back({ "FirstFrame", "uint16", std::to_string(firstFrame) });
    fields.push_back({ "LastFrame", "uint16", std::to_string(lastFrame) });

    // Flag mapping:
    static const char* flagNames[] = {
        "Visibility",
        "Timecoded Visibility"
    };
    std::string flagName = (flags < 2) ? flagNames[flags] : ("Unknown (" + std::to_string(flags) + ")");
    fields.push_back({ "ChannelType", "string", flagName });

    fields.push_back({ "Pivot", "uint16", std::to_string(pivot) });
    fields.push_back({ "DefaultVal", "uint8", std::to_string(defaultVal) });

    // Now the packed bit data:
    const uint8_t* bitData = &data[9];
    int numFrames = lastFrame - firstFrame + 1;
    for (int i = 0; i < numFrames; ++i) {
        int byteIdx = i / 8;
        int bitIdx = i % 8;
        bool isVisible = (bitData[byteIdx] >> bitIdx) & 1;

        std::string name = "Data[" + std::to_string(i) + "]";
        fields.push_back({ name, "bool", isVisible ? "true" : "false" });
    }

    return fields;
}

inline std::vector<ChunkField> InterpretCompressedAnimationHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 44) {
        fields.push_back({ "Error", "string", "Compressed Animation Header too small" });
        return fields;
    }

    uint32_t version = *reinterpret_cast<const uint32_t*>(&data[0]);
    const char* name = reinterpret_cast<const char*>(&data[4]);
    const char* hierarchyName = reinterpret_cast<const char*>(&data[20]);
    uint32_t numFrames = *reinterpret_cast<const uint32_t*>(&data[36]);
    uint16_t frameRate = *reinterpret_cast<const uint16_t*>(&data[40]);
    uint16_t flavor = *reinterpret_cast<const uint16_t*>(&data[42]);

    fields.push_back({ "Version", "uint32", std::to_string(version >> 16) + "." + std::to_string(version & 0xFFFF) });
    fields.push_back({ "Name", "string", std::string(name) });
    fields.push_back({ "HierarchyName", "string", std::string(hierarchyName) });
    fields.push_back({ "NumFrames", "uint32", std::to_string(numFrames) });
    fields.push_back({ "FrameRate", "uint16", std::to_string(frameRate) });

    static const char* flavorNames[] = {
        "Timecoded",
        "Adaptive Delta"
    };
    std::string flavorName = (flavor < 2) ? flavorNames[flavor] : ("Unknown (" + std::to_string(flavor) + ")");
    fields.push_back({ "Flavor", "string", flavorName });

    return fields;
}

inline std::vector<ChunkField> InterpretVertexInfluences(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() % 8 != 0) {
        fields.push_back({ "Error", "string", "VertexInfluences chunk size is not a multiple of 8" });
        return fields;
    }

    size_t numEntries = chunk->data.size() / 8;
    for (size_t i = 0; i < numEntries; ++i) {
        const uint8_t* entry = &data[i * 8];

        uint16_t boneIdx0 = *reinterpret_cast<const uint16_t*>(&entry[0]);
        uint16_t weight0 = *reinterpret_cast<const uint16_t*>(&entry[2]);
        uint16_t boneIdx1 = *reinterpret_cast<const uint16_t*>(&entry[4]);
        uint16_t weight1 = *reinterpret_cast<const uint16_t*>(&entry[6]);

        fields.push_back({ "VertexInfluence[" + std::to_string(i) + "].BoneIdx[0]", "uint16", std::to_string(boneIdx0) });
        fields.push_back({ "VertexInfluence[" + std::to_string(i) + "].Weight[0]", "uint16", std::to_string(weight0) });
        fields.push_back({ "VertexInfluence[" + std::to_string(i) + "].BoneIdx[1]", "uint16", std::to_string(boneIdx1) });
        fields.push_back({ "VertexInfluence[" + std::to_string(i) + "].Weight[1]", "uint16", std::to_string(weight1) });
    }

    return fields;
}

inline std::string ToHex(uint32_t value, int width = 8) {
    std::stringstream stream;
    stream << std::uppercase << std::setfill('0') << std::setw(width) << std::hex << value;
    return stream.str();
}

inline std::vector<ChunkField> InterpretBox(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    if (chunk->data.size() < sizeof(uint32_t) * 2 + 2 * 16 + 3 * sizeof(float) * 2) {
        fields.push_back({ "Error", "string", "Box chunk too small" });
        return fields;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    uint32_t version = *reinterpret_cast<const uint32_t*>(&data[0]);
    uint32_t attributes = *reinterpret_cast<const uint32_t*>(&data[4]);
    const char* name = reinterpret_cast<const char*>(&data[8]);
    const float* floats = reinterpret_cast<const float*>(&data[8 + 2 * 16]); // Skip 2*16 = 32 bytes of name

    float colorR = floats[0];
    float colorG = floats[1];
    float colorB = floats[2];
    float centerX = floats[3];
    float centerY = floats[4];
    float centerZ = floats[5];
    float extentX = floats[6];
    float extentY = floats[7];
    float extentZ = floats[8];

    fields.push_back({ "Version", "uint32", std::to_string(version) });
    fields.push_back({ "Attributes", "hex", "0x" + ToHex(attributes, 8) });

    // Attribute flags (as individual "flags")
    if (attributes & 0x00000001) fields.push_back({ "AttributeFlag", "string", "W3D_BOX_ATTRIBUTE_ORIENTED" });
    if (attributes & 0x00000002) fields.push_back({ "AttributeFlag", "string", "W3D_BOX_ATTRIBUTE_ALIGNED" });
    if (attributes & 0x00000010) fields.push_back({ "AttributeFlag", "string", "W3D_BOX_COLLISION_TYPE_PHYSICAL" });
    if (attributes & 0x00000020) fields.push_back({ "AttributeFlag", "string", "W3D_BOX_COLLISION_TYPE_PROJECTILE" });
    if (attributes & 0x00000040) fields.push_back({ "AttributeFlag", "string", "W3D_BOX_COLLISION_TYPE_VIS" });
    if (attributes & 0x00000080) fields.push_back({ "AttributeFlag", "string", "W3D_BOX_COLLISION_TYPE_CAMERA" });
    if (attributes & 0x00000100) fields.push_back({ "AttributeFlag", "string", "W3D_BOX_COLLISION_TYPE_VEHICLE" });

    fields.push_back({ "Name", "string", std::string(name) });

    // Color
    fields.push_back({ "ColorR", "float", std::to_string(colorR) });
    fields.push_back({ "ColorG", "float", std::to_string(colorG) });
    fields.push_back({ "ColorB", "float", std::to_string(colorB) });

    // Center
    fields.push_back({ "CenterX", "float", std::to_string(centerX) });
    fields.push_back({ "CenterY", "float", std::to_string(centerY) });
    fields.push_back({ "CenterZ", "float", std::to_string(centerZ) });

    // Extent
    fields.push_back({ "ExtentX", "float", std::to_string(extentX) });
    fields.push_back({ "ExtentY", "float", std::to_string(extentY) });
    fields.push_back({ "ExtentZ", "float", std::to_string(extentZ) });

    return fields;
}


inline std::vector<ChunkField> InterpretMeshUserText(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    size_t size = chunk->data.size();
    std::string fullText;
    std::string currentString;

    for (size_t i = 0; i < size; ++i) {
        if (std::isprint(data[i]) || data[i] == '\t') {
            // Printable character or tab - add to current string
            currentString += static_cast<char>(data[i]);
        }
        else {
            // Non-printable - flush currentString if not empty
            if (!currentString.empty()) {
                fullText += currentString;
                fullText += ' '; // Optional: insert space between strings
                currentString.clear();
            }
        }
    }

    // If leftover printable text at the end
    if (!currentString.empty()) {
        fullText += currentString;
    }

    // Trim any trailing space
    if (!fullText.empty() && fullText.back() == ' ') {
        fullText.pop_back();
    }

    fields.push_back({ "UserText", "string", fullText });

    return fields;
}


inline std::vector<ChunkField> InterpretPrelitVertexWrapper(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    fields.push_back({ "Info", "string", "W3D_CHUNK_PRELIT_VERTEX (wrapper)" });
    fields.push_back({ "ChildCount", "int", std::to_string(chunk->children.size()) });
    return fields;
}


inline std::vector<ChunkField> InterpretLightmapMultiPass(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    fields.push_back({ "Info", "string", "W3D_CHUNK_LIGHTMAP_MULTI_PASS (wrapper)" });
    fields.push_back({ "ChildCount", "int", std::to_string(chunk->children.size()) });
    return fields;
}


inline std::vector<ChunkField> InterpretLightmapMultiTexture(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    fields.push_back({ "Info", "string", "W3D_CHUNK_LIGHTMAP_MULTI_TEXTURE (wrapper)" });
    fields.push_back({ "ChildCount", "int", std::to_string(chunk->children.size()) });
    return fields;
}

inline std::vector<ChunkField> InterpretDiffuseColorChunk(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    size_t count = chunk->data.size() / 4;

    for (size_t i = 0; i < count; ++i) {
        uint8_t r = data[i * 4 + 0];
        uint8_t g = data[i * 4 + 1];
        uint8_t b = data[i * 4 + 2];
        uint8_t a = data[i * 4 + 3];

        std::ostringstream value;
        value << (int)r << " " << (int)g << " " << (int)b << " " << (int)a;

        fields.push_back({ "Vertex[" + std::to_string(i) + "].Color", "RGBA", value.str() });
    }

    return fields;
}

inline std::vector<ChunkField> InterpretDeform(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    fields.push_back({ "Info", "string", "W3D_CHUNK_DEFORM (wrapper for damage/deform data)" });
    fields.push_back({ "ChildCount", "int", std::to_string(chunk->children.size()) });
    return fields;
}

inline std::vector<ChunkField> InterpretDeformSet(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    if (chunk->data.size() < 8) {
        fields.push_back({ "Error", "string", "Deform Set chunk too short" });
        return fields;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    uint32_t version = *reinterpret_cast<const uint32_t*>(&data[0]);
    uint32_t keyframeCount = *reinterpret_cast<const uint32_t*>(&data[4]);

    fields.push_back({ "Version", "uint32", std::to_string(version) });
    fields.push_back({ "KeyframeCount", "uint32", std::to_string(keyframeCount) });

    return fields;
}

inline std::vector<ChunkField> InterpretDeformKeyframe(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    if (chunk->data.size() < 4) {
        fields.push_back({ "Error", "string", "Deform Keyframe chunk too short" });
        return fields;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    uint32_t time = *reinterpret_cast<const uint32_t*>(&data[0]);

    fields.push_back({ "Time", "uint32", std::to_string(time) });

    return fields;
}


inline std::vector<ChunkField> InterpretDeformData(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    size_t entrySize = sizeof(uint32_t) + sizeof(float) * 3;

    if (chunk->data.size() % entrySize != 0) {
        fields.push_back({ "Error", "string", "Deform Data chunk misaligned" });
        return fields;
    }

    const uint8_t* raw = reinterpret_cast<const uint8_t*>(chunk->data.data());
    size_t count = chunk->data.size() / entrySize;

    for (size_t i = 0; i < count; ++i) {
        const uint32_t index = *reinterpret_cast<const uint32_t*>(raw + i * entrySize);
        const float* vec = reinterpret_cast<const float*>(raw + i * entrySize + 4);

        std::ostringstream displacement;
        displacement << std::fixed << std::setprecision(6)
            << vec[0] << " " << vec[1] << " " << vec[2];

        fields.push_back({ "Vertex[" + std::to_string(i) + "].Index", "uint32", std::to_string(index) });
        fields.push_back({ "Vertex[" + std::to_string(i) + "].Displacement", "vector3", displacement.str() });
    }

    return fields;
}

inline std::vector<ChunkField> InterpretCompressedAnimationChannel(const std::shared_ptr<ChunkItem>& chunk, uint16_t flavor = 0) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    size_t size = chunk->data.size();

    static const char* ChannelTypes[] = {
        "X Translation", "Y Translation", "Z Translation",
        "X Rotation", "Y Rotation", "Z Rotation",
        "Quaternion Rotation", "Visibility"
    };

    if (flavor == 0) { // Timecoded
        if (size < 12) {
            fields.push_back({ "Error", "string", "Too small for W3dTimeCodedAnimChannelStruct" });
            return fields;
        }

        uint32_t numTimeCodes = *reinterpret_cast<const uint32_t*>(&data[0]);
        uint16_t pivot = *reinterpret_cast<const uint16_t*>(&data[4]);
        uint8_t vectorLen = data[6];
        uint8_t flags = data[7];

        fields.push_back({ "NumTimeCodes", "uint32", std::to_string(numTimeCodes) });
        fields.push_back({ "Pivot", "uint16", std::to_string(pivot) });
        fields.push_back({ "VectorLen", "uint8", std::to_string(vectorLen) });
        fields.push_back({ "ChannelType", "string", flags < 8 ? ChannelTypes[flags] : "Unknown" });

        size_t datalen = (size - 8 + 3) / 4;  // packed int32s
        const uint32_t* packed = reinterpret_cast<const uint32_t*>(data + 8);
        for (size_t i = 0; i < datalen; ++i) {
            fields.push_back({ "Data[" + std::to_string(i) + "]", "int32", std::to_string(packed[i]) });
        }

    }
    else { // Adaptive Delta
        if (size < 20) {
            fields.push_back({ "Error", "string", "Too small for W3dAdaptiveDeltaAnimChannelStruct" });
            return fields;
        }

        uint32_t numFrames = *reinterpret_cast<const uint32_t*>(&data[0]);
        uint16_t pivot = *reinterpret_cast<const uint16_t*>(&data[4]);
        uint8_t vectorLen = data[6];
        uint8_t flags = data[7];
        float scale = *reinterpret_cast<const float*>(&data[8]);

        fields.push_back({ "NumFrames", "uint32", std::to_string(numFrames) });
        fields.push_back({ "Pivot", "uint16", std::to_string(pivot) });
        fields.push_back({ "VectorLen", "uint8", std::to_string(vectorLen) });
        fields.push_back({ "ChannelType", "string", flags < 8 ? ChannelTypes[flags] : "Unknown" });
        fields.push_back({ "Scale", "float", std::to_string(scale) });

        size_t datalen = (size - 12 + 3) / 4;
        const uint32_t* packed = reinterpret_cast<const uint32_t*>(data + 12);
        for (size_t i = 0; i < datalen; ++i) {
            fields.push_back({ "Data[" + std::to_string(i) + "]", "int32", std::to_string(packed[i]) });
        }
    }

    return fields;
}

inline std::vector<ChunkField> InterpretCompressedBitChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    size_t size = chunk->data.size();

    if (size < 8) {
        fields.push_back({ "Error", "string", "Compressed Bit Channel too small" });
        return fields;
    }

    uint32_t numTimeCodes = *reinterpret_cast<const uint32_t*>(&data[0]);
    uint16_t pivot = *reinterpret_cast<const uint16_t*>(&data[4]);
    uint8_t  flags = data[6];
    uint8_t  defaultVal = data[7];

    fields.push_back({ "NumTimeCodes", "uint32", std::to_string(numTimeCodes) });
    fields.push_back({ "Pivot", "uint16", std::to_string(pivot) });

    static const char* BitChannelTypes[] = {
        "Visibility",
        "Timecoded Visibility"
    };
    std::string channelTypeStr = (flags < 2) ? BitChannelTypes[flags] : ("Unknown (" + std::to_string(flags) + ")");
    fields.push_back({ "ChannelType", "string", channelTypeStr });

    fields.push_back({ "Default Value", "uint8", std::to_string(defaultVal) });

    // Interpret bit-packed stream as uint32 blocks
    size_t dataStart = 8;
    size_t expectedSize = dataStart + numTimeCodes * 4;

    if (size < expectedSize) {
        fields.push_back({ "Warning", "string", "Chunk smaller than expected for declared NumTimeCodes" });
    }

    size_t actualCodes = std::min<size_t>(numTimeCodes, (size - dataStart) / 4);
    const uint32_t* codeData = reinterpret_cast<const uint32_t*>(data + dataStart);

    for (size_t i = 0; i < actualCodes; ++i) {
        fields.push_back({ "Data[" + std::to_string(i) + "]", "int32", std::to_string(codeData[i]) });
    }

    return fields;
}

inline std::vector<ChunkField> InterpretHModelHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 38) {
        fields.push_back({ "Error", "string", "HMODEL_HEADER too small" });
        return fields;
    }

    uint32_t version = *reinterpret_cast<const uint32_t*>(&data[0]);
    const char* namePtr = reinterpret_cast<const char*>(&data[4]);
    const char* hierPtr = reinterpret_cast<const char*>(&data[20]);
    uint16_t numConnections = *reinterpret_cast<const uint16_t*>(&data[36]);

    std::string name(namePtr, strnlen(namePtr, 16));
    std::string hier(hierPtr, strnlen(hierPtr, 16));

    std::ostringstream vstr;
    vstr << ((version >> 16) & 0xFFFF) << "." << (version & 0xFFFF);

    fields.push_back({ "Version", "string", vstr.str() });
    fields.push_back({ "Name", "string", name });
    fields.push_back({ "HierarchyName", "string", hier });
    fields.push_back({ "NumConnections", "uint16", std::to_string(numConnections) });

    return fields;
}

inline std::vector<ChunkField> InterpretHModelAuxData(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    if (chunk->data.size() < 180) {
        fields.push_back({ "Error", "string", "Aux data chunk too small" });
        return fields;
    }

    uint32_t attributes = *reinterpret_cast<const uint32_t*>(&data[0]);
    uint32_t meshCount = *reinterpret_cast<const uint32_t*>(&data[4]);
    uint32_t collisionCount = *reinterpret_cast<const uint32_t*>(&data[8]);
    uint32_t skinCount = *reinterpret_cast<const uint32_t*>(&data[12]);

    fields.push_back({ "Attributes", "uint32", std::to_string(attributes) });
    fields.push_back({ "MeshCount", "uint32", std::to_string(meshCount) });
    fields.push_back({ "CollisionCount", "uint32", std::to_string(collisionCount) });
    fields.push_back({ "SkinCount", "uint32", std::to_string(skinCount) });

    for (int i = 0; i < 8; ++i) {
        uint32_t val = *reinterpret_cast<const uint32_t*>(&data[16 + i * 4]);
        fields.push_back({ "FutureCounts[" + std::to_string(i) + "]", "uint32", std::to_string(val) });
    }

    float lodMin = *reinterpret_cast<const float*>(&data[48]);
    float lodMax = *reinterpret_cast<const float*>(&data[52]);

    fields.push_back({ "LODMin", "float", std::to_string(lodMin) });
    fields.push_back({ "LODMax", "float", std::to_string(lodMax) });

    for (int i = 0; i < 32; ++i) {
        uint32_t val = *reinterpret_cast<const uint32_t*>(&data[56 + i * 4]);
        fields.push_back({ "FutureUse[" + std::to_string(i) + "]", "uint32", std::to_string(val) });
    }

    return fields;
}


inline std::vector<ChunkField> InterpretEmitter(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    fields.push_back({ "Info", "string", "W3D_CHUNK_EMITTER (wrapper for particle emitter data)" });
    fields.push_back({ "ChildCount", "int", std::to_string(chunk->children.size()) });
    return fields;
}

inline std::vector<ChunkField> InterpretEmitterHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 20) {
        fields.push_back({ "Error", "string", "Emitter header too short" });
        return fields;
    }

    uint32_t versionRaw = *reinterpret_cast<const uint32_t*>(&data[0]);
    uint16_t major = (versionRaw >> 16) & 0xFFFF;
    uint16_t minor = versionRaw & 0xFFFF;

    std::ostringstream versionStr;
    versionStr << major << "." << minor;

    std::string name(reinterpret_cast<const char*>(&data[4]), strnlen(reinterpret_cast<const char*>(&data[4]), 16));

    fields.push_back({ "Version", "string", versionStr.str() });
    fields.push_back({ "Name", "string", name });

    return fields;
}


inline std::vector<ChunkField> InterpretEmitterUserData(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    if (chunk->data.empty()) {
        fields.push_back({ "User Data", "string", "" });
        return fields;
    }

    const char* str = reinterpret_cast<const char*>(chunk->data.data());
    size_t len = strnlen(str, chunk->data.size());  // Safe bounded read

    std::string result(str, len);
    fields.push_back({ "User Data", "string", result });
    return fields;
}



inline std::vector<ChunkField> InterpretEmitterInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    constexpr size_t TEXTURE_NAME_SIZE = 260;

    if (chunk->data.size() < TEXTURE_NAME_SIZE + 60) {
        fields.push_back({ "Error", "string", "Emitter Info chunk too small (expected at least 320 bytes)" });
        return fields;
    }

    // Read null-terminated string from fixed-size buffer
    std::string texture(reinterpret_cast<const char*>(&data[0]), strnlen(reinterpret_cast<const char*>(&data[0]), TEXTURE_NAME_SIZE));
    fields.push_back({ "Texture Name", "string", texture });

    auto readF32Str = [&](size_t offset, int precision = 6) -> std::string {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision)
            << *reinterpret_cast<const float*>(&data[offset]);
        return oss.str();
        };

    size_t offset = TEXTURE_NAME_SIZE;

    fields.push_back({ "StartSize", "float", readF32Str(offset + 0x00) });
    fields.push_back({ "EndSize", "float", readF32Str(offset + 0x04) });
    fields.push_back({ "Lifetime", "float", readF32Str(offset + 0x08) });
    fields.push_back({ "EmissionRate", "float", readF32Str(offset + 0x0C) });
    fields.push_back({ "MaxEmissions", "float", readF32Str(offset + 0x10) });
    fields.push_back({ "VelocityRandom", "float", readF32Str(offset + 0x14) });
    fields.push_back({ "PositionRandom", "float", readF32Str(offset + 0x18) });
    fields.push_back({ "FadeTime", "float", readF32Str(offset + 0x1C) });
    fields.push_back({ "Gravity", "float", readF32Str(offset + 0x20) });
    fields.push_back({ "Elasticity", "float", readF32Str(offset + 0x24) });

    std::ostringstream velStr, accStr;
    for (int i = 0; i < 3; ++i) velStr << readF32Str(offset + 0x28 + i * 4) << (i < 2 ? " " : "");
    for (int i = 0; i < 3; ++i) accStr << readF32Str(offset + 0x34 + i * 4) << (i < 2 ? " " : "");

    fields.push_back({ "Velocity", "vector3", velStr.str() });
    fields.push_back({ "Acceleration", "vector3", accStr.str() });

    auto rgbaToString = [&](size_t localOffset) {
        const uint8_t* rgba = &data[localOffset];
        std::ostringstream s;
        s << (int)rgba[0] << " " << (int)rgba[1] << " " << (int)rgba[2] << " " << (int)rgba[3];
        return s.str();
        };

    fields.push_back({ "StartColor", "RGBA", rgbaToString(offset + 0x40) });
    fields.push_back({ "EndColor", "RGBA", rgbaToString(offset + 0x44) });

    return fields;
}


inline void InterpretShaderStruct(std::vector<ChunkField>& fields, const uint8_t* data, size_t baseOffset) {
    const char* DepthCompareValues[] = { "Pass Never", "Pass Less", "Pass Equal", "Pass Less or Equal", "Pass Greater", "Pass Not Equal", "Pass Greater or Equal", "Pass Always" };
    const char* DepthMaskValues[] = { "Write Disable", "Write Enable" };
    const char* DestBlendValues[] = { "Zero", "One", "Src Color", "One Minus Src Color", "Src Alpha", "One Minus Src Alpha", "Src Color Prefog", "Disable", "Enable", "Scale Fragment", "Replace Fragment" };
    const char* PriGradientValues[] = { "Disable", "Modulate", "Add", "Bump-Environment", "Bump-Environment Luminance", "Modulate 2x" };
    const char* SecGradientValues[] = { "Disable", "Enable" };
    const char* SrcBlendValues[] = { "Zero", "One", "Src Alpha", "One Minus Src Alpha" };
    const char* TexturingValues[] = { "Disable", "Enable" };
    const char* DetailColorValues[] = { "Disable", "Detail", "Scale", "InvScale", "Add", "Sub", "SubR", "Blend", "DetailBlend", "Add Signed", "Add Signed 2x", "Scale 2x", "Mod Alpha Add Color" };
    const char* DetailAlphaValues[] = { "Disable", "Detail", "Scale", "InvScale", "Disable", "Enable", "Smooth", "Flat" };
    const char* AlphaTestValues[] = { "Alpha Test Disable", "Alpha Test Enable" };

    auto readEnum = [&](size_t offset, const char* fieldName, const char** values, size_t valueCount) {
        uint8_t value = data[baseOffset + offset];
        std::string strValue = (value < valueCount) ? values[value] : ("Unknown (" + std::to_string(value) + ")");
        fields.push_back({ std::string("Shader.") + fieldName, "string", strValue });
        };

    readEnum(0, "DepthCompare", DepthCompareValues, std::size(DepthCompareValues));
    readEnum(1, "DepthMask", DepthMaskValues, std::size(DepthMaskValues));
    // skip 2 = ColorMask
    readEnum(3, "DestBlend", DestBlendValues, std::size(DestBlendValues));
    // skip 4 = FogFunc
    readEnum(5, "PriGradient", PriGradientValues, std::size(PriGradientValues));
    readEnum(6, "SecGradient", SecGradientValues, std::size(SecGradientValues));
    readEnum(7, "SrcBlend", SrcBlendValues, std::size(SrcBlendValues));
    readEnum(8, "Texturing", TexturingValues, std::size(TexturingValues));
    readEnum(9, "DetailColor", DetailColorValues, std::size(DetailColorValues));
    readEnum(10, "DetailAlpha", DetailAlphaValues, std::size(DetailAlphaValues));
    // skip 11 = ShaderPreset
    readEnum(12, "AlphaTest", AlphaTestValues, std::size(AlphaTestValues));
    // skip 13 = PostDetailColorFunc
    // skip 14 = PostDetailAlphaFunc
    // skip 15 = pad
}






inline std::vector<ChunkField> InterpretEmitterInfoV2(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 124) {
        fields.push_back({ "Error", "string", "EmitterInfoV2 chunk too small (expected at least 124 bytes)" });
        return fields;
    }

    auto readU32 = [&](size_t offset) -> uint32_t {
        return *reinterpret_cast<const uint32_t*>(&data[offset]);
        };
    auto readF32 = [&](size_t offset) -> float {
        return *reinterpret_cast<const float*>(&data[offset]);
        };

    size_t offset = 0;
    fields.push_back({ "BurstSize", "uint32", std::to_string(readU32(offset)) }); offset += 4;

    fields.push_back({ "CreationVolume.ClassID", "uint32", std::to_string(readU32(offset)) }); offset += 4;
    fields.push_back({ "CreationVolume.Value1", "float", std::to_string(readF32(offset)) }); offset += 4;
    fields.push_back({ "CreationVolume.Value2", "float", std::to_string(readF32(offset)) }); offset += 4;
    fields.push_back({ "CreationVolume.Value3", "float", std::to_string(readF32(offset)) }); offset += 4;
    offset += 4 * 4; // skip CreationVolume.Reserved[4]

    fields.push_back({ "VelRandom.ClassID", "uint32", std::to_string(readU32(offset)) }); offset += 4;
    fields.push_back({ "VelRandom.Value1", "float", std::to_string(readF32(offset)) }); offset += 4;
    fields.push_back({ "VelRandom.Value2", "float", std::to_string(readF32(offset)) }); offset += 4;
    fields.push_back({ "VelRandom.Value3", "float", std::to_string(readF32(offset)) }); offset += 4;
    offset += 4 * 4; // skip VelRandom.Reserved[4]

    fields.push_back({ "OutwardVel", "float", std::to_string(readF32(offset)) }); offset += 4;
    fields.push_back({ "VelInherit", "float", std::to_string(readF32(offset)) }); offset += 4;

    InterpretShaderStruct(fields, data, offset); offset += 16;

    fields.push_back({ "RenderMode", "uint32", std::to_string(readU32(offset)) }); offset += 4;
    fields.push_back({ "FrameMode", "uint32", std::to_string(readU32(offset)) }); offset += 4;

    // skip reserved[6]
    offset += 4 * 6;

    return fields;
}








inline std::vector<ChunkField> InterpretEmitterProps(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    auto readU32 = [&](size_t offset) -> uint32_t {
        return *reinterpret_cast<const uint32_t*>(&data[offset]);
        };
    auto readF32 = [&](size_t offset) -> float {
        return *reinterpret_cast<const float*>(&data[offset]);
        };
    auto readRGBA = [&](size_t offset) -> std::string {
        std::ostringstream ss;
        ss << (int)data[offset] << " " << (int)data[offset + 1] << " "
            << (int)data[offset + 2] << " " << (int)data[offset + 3];
        return ss.str();
        };

    size_t offset = 0;
    if (chunk->data.size() < 44) {
        fields.push_back({ "Error", "string", "EmitterProps chunk too small (expected at least 44 bytes)" });
        return fields;
    }

    // Keyframe counts
    uint32_t colorCount = readU32(offset); offset += 4;
    uint32_t opacityCount = readU32(offset); offset += 4;
    uint32_t sizeCount = readU32(offset); offset += 4;

    fields.push_back({ "ColorKeyframes", "uint32", std::to_string(colorCount) });
    fields.push_back({ "OpacityKeyframes", "uint32", std::to_string(opacityCount) });
    fields.push_back({ "SizeKeyframes", "uint32", std::to_string(sizeCount) });

    // ColorRandom (RGBA stored as 4 bytes)
    fields.push_back({ "ColorRandom", "rgba", readRGBA(offset) });
    offset += 4;

    // Scalars
    fields.push_back({ "OpacityRandom", "float", std::to_string(readF32(offset)) }); offset += 4;
    fields.push_back({ "SizeRandom", "float", std::to_string(readF32(offset)) }); offset += 4;

    // Skip 4x reserved
    offset += 4 * 4;

    // --- Color keyframes ---
    for (uint32_t i = 0; i < colorCount; ++i) {
        if (offset + 8 > chunk->data.size()) break;
        float time = readF32(offset); offset += 4;
        uint8_t r = data[offset++];
        uint8_t g = data[offset++];
        uint8_t b = data[offset++];
        uint8_t a = data[offset++];

        fields.push_back({ "ColorTime[" + std::to_string(i) + "]", "float", std::to_string(time) });
        std::ostringstream ss;
        ss << (int)r << " " << (int)g << " " << (int)b << " " << (int)a;
        fields.push_back({ "Color[" + std::to_string(i) + "]", "rgba", ss.str() });
    }

    // --- Opacity keyframes ---
    for (uint32_t i = 0; i < opacityCount; ++i) {
        if (offset + 8 > chunk->data.size()) break;
        float time = readF32(offset); offset += 4;
        float val = readF32(offset); offset += 4;
        fields.push_back({ "OpacityTime[" + std::to_string(i) + "]", "float", std::to_string(time) });
        fields.push_back({ "Opacity[" + std::to_string(i) + "]", "float", std::to_string(val) });
    }

    // --- Size keyframes ---
    for (uint32_t i = 0; i < sizeCount; ++i) {
        if (offset + 8 > chunk->data.size()) break;
        float time = readF32(offset); offset += 4;
        float val = readF32(offset); offset += 4;
        fields.push_back({ "SizeTime[" + std::to_string(i) + "]", "float", std::to_string(time) });
        fields.push_back({ "Size[" + std::to_string(i) + "]", "float", std::to_string(val) });
    }

    return fields;
}


inline std::vector<ChunkField> InterpretEmitterRotationKeys(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 16) {
        fields.push_back({ "Error", "string", "Chunk too small for RotationHeader (expected at least 16 bytes)" });
        return fields;
    }

    auto readU32 = [&](size_t offset) -> uint32_t {
        return *reinterpret_cast<const uint32_t*>(&data[offset]);
    };
    auto readF32 = [&](size_t offset) -> float {
        return *reinterpret_cast<const float*>(&data[offset]);
    };

    size_t offset = 0;
    uint32_t keyframeCount = readU32(offset); offset += 4;
    float randomVelocity = readF32(offset); offset += 4;
    float orientationRandom = readF32(offset); offset += 4;
    offset += 4; // Skip reserved

    fields.push_back({ "KeyframeCount", "uint32", std::to_string(keyframeCount) });
    fields.push_back({ "RandomVelocity", "float", std::to_string(randomVelocity) });
    fields.push_back({ "OrientationRandom", "float", std::to_string(orientationRandom) });

    for (uint32_t i = 0; i < keyframeCount; ++i) {
        if (offset + 8 > chunk->data.size()) break;
        float time = readF32(offset); offset += 4;
        float rotation = readF32(offset); offset += 4;

        fields.push_back({ "Time[" + std::to_string(i) + "]", "float", std::to_string(time) });
        fields.push_back({ "Rotation[" + std::to_string(i) + "]", "float", std::to_string(rotation) });
    }

    return fields;
}

inline std::vector<ChunkField> InterpretEmitterFrameKeys(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 16) {
        fields.push_back({ "Error", "string", "Chunk too small for FrameHeader (expected at least 16 bytes)" });
        return fields;
    }

    auto readU32 = [&](size_t offset) -> uint32_t {
        return *reinterpret_cast<const uint32_t*>(&data[offset]);
        };
    auto readF32 = [&](size_t offset) -> float {
        return *reinterpret_cast<const float*>(&data[offset]);
        };

    size_t offset = 0;

    // --- Header ---
    uint32_t keyframeCount = readU32(offset); offset += 4;
    float random = readF32(offset); offset += 4;
    offset += 4 * 2; // Reserved[2]

    fields.push_back({ "KeyframeCount", "uint32", std::to_string(keyframeCount) });
    fields.push_back({ "Random", "float", std::to_string(random) });

    // --- Keyframes ---
    for (uint32_t i = 0; i < keyframeCount + 1; ++i) {
        if (offset + 8 > chunk->data.size()) break;

        float time = readF32(offset); offset += 4;
        float frame = readF32(offset); offset += 4;

        fields.push_back({ "Time[" + std::to_string(i) + "]", "float", std::to_string(time) });
        fields.push_back({ "Frame[" + std::to_string(i) + "]", "float", std::to_string(frame) });
    }

    return fields;
}

inline std::vector<ChunkField> InterpretAggregateHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 20) {
        return { { "Error", "string", "Chunk too small for AggregateHeader (expected 20 bytes)" } };
    }

    uint16_t major = *reinterpret_cast<const uint16_t*>(&data[0]);
    uint16_t minor = *reinterpret_cast<const uint16_t*>(&data[2]);
    std::ostringstream versionStr;
    versionStr << minor << "." << major;
    
    std::string name(reinterpret_cast<const char*>(&data[4]), 16);
    name = name.c_str(); // truncate at null

    fields.push_back({ "Version", "uint32", versionStr.str() });
    fields.push_back({ "Name", "string", name });

    return fields;
}


inline std::vector<ChunkField> InterpretAggregateInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 36) {
        return { { "Error", "string", "Chunk too small for AggregateInfo (expected at least 36 bytes)" } };
    }

    std::string baseModel(reinterpret_cast<const char*>(&data[0]), 32);
    baseModel = baseModel.c_str();
    uint32_t subCount = *reinterpret_cast<const uint32_t*>(&data[32]);

    fields.push_back({ "BaseModelName", "string", baseModel });
    fields.push_back({ "SubobjectCount", "uint32", std::to_string(subCount) });

    size_t offset = 36;
    for (uint32_t i = 0; i < subCount; ++i) {
        if (offset + 64 > chunk->data.size()) break;

        std::string subName(reinterpret_cast<const char*>(&data[offset]), 32);
        std::string boneName(reinterpret_cast<const char*>(&data[offset + 32]), 32);

        subName = subName.c_str();
        boneName = boneName.c_str();

        fields.push_back({ "SubObject[" + std::to_string(i) + "].SubobjectName", "string", subName });
        fields.push_back({ "SubObject[" + std::to_string(i) + "].BoneName", "string", boneName });

        offset += 64;
    }

    return fields;
}


inline std::vector<ChunkField> InterpretAggregateClassInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    if (chunk->data.size() < 8) {
        return { { "Error", "string", "Chunk too small for AggregateClassInfo (expected 8 bytes)" } };
    }

    uint32_t classID = *reinterpret_cast<const uint32_t*>(&data[0]);
    uint32_t flags = *reinterpret_cast<const uint32_t*>(&data[4]);

    fields.push_back({ "OriginalClassID", "uint32", std::to_string(classID) });
    fields.push_back({ "Flags", "uint32", std::to_string(flags) });

    return fields;
}

inline std::vector<ChunkField> InterpretTextureReplacerInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());
    size_t offset = 0;

    const size_t headerSize = 4;
    const size_t meshPathCount = 15;
    const size_t pathEntryLength = 16 * 2;
    const size_t texNameLength = 260;
    const size_t replacerSize = (meshPathCount * pathEntryLength) * 2 + (texNameLength * 2) + sizeof(uint16_t) * 2 + sizeof(uint32_t) + sizeof(float);

    if (chunk->data.size() < headerSize) {
        fields.push_back({ "Error", "string", "TextureReplacerHeader too small" });
        return fields;
    }

    uint32_t count = *reinterpret_cast<const uint32_t*>(&data[offset]); offset += 4;
    fields.push_back({ "ReplacedTexturesCount", "uint32", std::to_string(count) });

    for (uint32_t i = 0; i < count; ++i) {
        if (offset + replacerSize > chunk->data.size()) {
            fields.push_back({ "Error", "string", "TextureReplacerStruct out of bounds" });
            break;
        }

        // Mesh Paths
        for (int j = 0; j < meshPathCount; ++j) {
            std::string str(reinterpret_cast<const char*>(&data[offset]), pathEntryLength);
            str = str.c_str();
            fields.push_back({ "Replacer[" + std::to_string(i) + "].MeshPath[" + std::to_string(j) + "]", "string", str });
            offset += pathEntryLength;
        }

        // Bone Paths
        for (int j = 0; j < meshPathCount; ++j) {
            std::string str(reinterpret_cast<const char*>(&data[offset]), pathEntryLength);
            str = str.c_str();
            fields.push_back({ "Replacer[" + std::to_string(i) + "].BonePath[" + std::to_string(j) + "]", "string", str });
            offset += pathEntryLength;
        }

        // Texture names
        std::string oldTex(reinterpret_cast<const char*>(&data[offset]), texNameLength);
        oldTex = oldTex.c_str(); fields.push_back({ "Replacer[" + std::to_string(i) + "].OldTextureName", "string", oldTex });
        offset += texNameLength;

        std::string newTex(reinterpret_cast<const char*>(&data[offset]), texNameLength);
        newTex = newTex.c_str(); fields.push_back({ "Replacer[" + std::to_string(i) + "].NewTextureName", "string", newTex });
        offset += texNameLength;

        // Texture Params
        uint16_t attr = *reinterpret_cast<const uint16_t*>(&data[offset]); offset += 2;
        uint16_t anim = *reinterpret_cast<const uint16_t*>(&data[offset]); offset += 2;
        uint32_t frameCount = *reinterpret_cast<const uint32_t*>(&data[offset]); offset += 4;
        float frameRate = *reinterpret_cast<const float*>(&data[offset]); offset += 4;

        fields.push_back({ "Replacer[" + std::to_string(i) + "].TextureParams.Attributes", "uint16", std::to_string(attr) });
        fields.push_back({ "Replacer[" + std::to_string(i) + "].TextureParams.AnimType", "uint16", std::to_string(anim) });
        fields.push_back({ "Replacer[" + std::to_string(i) + "].TextureParams.FrameCount", "uint32", std::to_string(frameCount) });
        fields.push_back({ "Replacer[" + std::to_string(i) + "].TextureParams.FrameRate", "float", std::to_string(frameRate) });
    }

    return fields;
}

inline void InterpretSphereShaderStruct(std::vector<ChunkField>& fields, const uint8_t* data, size_t baseOffset) {
    const char* DepthCompareValues[] = {
        "Pass Never", "Pass Less", "Pass Equal", "Pass Less or Equal",
        "Pass Greater", "Pass Not Equal", "Pass Greater or Equal", "Pass Always"
    };
    const char* DepthMaskValues[] = { "Disable", "Enable" };
    const char* DestBlendValues[] = {
        "Zero", "One", "Src Color", "One Minus Src Color", "Src Alpha", "One Minus Src Alpha",
        "Src Color Prefog", "Disable", "Enable", "Scale Fragment", "Replace Fragment"
    };
    const char* PriGradientValues[] = {
        "Disable", "Modulate", "Add", "Bump-Environment", "Bump-Environment Luminance", "Modulate 2x"
    };
    const char* SecGradientValues[] = { "Disable", "Enable" };
    const char* SrcBlendValues[] = { "Zero", "One", "Src Alpha", "One Minus Src Alpha" };
    const char* TexturingValues[] = { "Disable", "Enable" };
    const char* DetailColorValues[] = {
        "Disable", "Detail", "Scale", "InvScale", "Add", "Sub", "SubR", "Blend", "DetailBlend",
        "Add Signed", "Add Signed 2x", "Scale 2x", "Mod Alpha Add Color"
    };
    const char* DetailAlphaValues[] = {
        "Disable", "Detail", "Scale", "InvScale", "Disable", "Enable", "Smooth", "Flat"
    };
    const char* AlphaTestValues[] = { "Alpha Test Disable", "Alpha Test Enable" };

    auto readEnum = [&](const char* label, uint8_t value, const char* const* values, size_t count) {
        std::string val = (value < count) ? values[value] : ("Unknown (" + std::to_string(value) + ")");
        fields.push_back({ std::string("Shader.") + label, "enum", val });
        };

    readEnum("DepthCompare", data[baseOffset + 0], DepthCompareValues, std::size(DepthCompareValues));
    readEnum("DepthMask", data[baseOffset + 1], DepthMaskValues, std::size(DepthMaskValues));
    readEnum("DestBlend", data[baseOffset + 3], DestBlendValues, std::size(DestBlendValues)); // skip ColorMask
    readEnum("PriGradient", data[baseOffset + 5], PriGradientValues, std::size(PriGradientValues)); // skip FogFunc
    readEnum("SecGradient", data[baseOffset + 6], SecGradientValues, std::size(SecGradientValues));
    readEnum("SrcBlend", data[baseOffset + 7], SrcBlendValues, std::size(SrcBlendValues));
    readEnum("Texturing", data[baseOffset + 8], TexturingValues, std::size(TexturingValues));
    readEnum("DetailColor", data[baseOffset + 9], DetailColorValues, std::size(DetailColorValues));
    readEnum("DetailAlpha", data[baseOffset + 10], DetailAlphaValues, std::size(DetailAlphaValues));
    readEnum("AlphaTest", data[baseOffset + 12], AlphaTestValues, std::size(AlphaTestValues)); // skip ShaderPreset
}


inline std::vector<ChunkField> InterpretSphereHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(chunk->data.data());

    // Version (2x uint32)
    uint32_t versionMinor = *reinterpret_cast<const uint32_t*>(&data[0x00]);
    uint32_t versionMajor = *reinterpret_cast<const uint32_t*>(&data[0x04]);
    fields.push_back({ "Version", "version", std::to_string(versionMajor) + "." + std::to_string(versionMinor) });

    // Name (null-terminated string, max 256 bytes)
    std::string name(reinterpret_cast<const char*>(&data[0x08]), strnlen(reinterpret_cast<const char*>(&data[0x08]), 256));
    fields.push_back({ "Name", "string", name });

    // Center
    fields.push_back({ "Center.X", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x28])) });
    fields.push_back({ "Center.Y", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x2C])) });
    fields.push_back({ "Center.Z", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x30])) });

    // Extent
    fields.push_back({ "Extent.X", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x34])) });
    fields.push_back({ "Extent.Y", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x38])) });
    fields.push_back({ "Extent.Z", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x3C])) });

    // AnimationDuration
    fields.push_back({ "AnimationDuration", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x40])) });

    // Color
    fields.push_back({ "Color.R", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x44])) });
    fields.push_back({ "Color.G", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x48])) });
    fields.push_back({ "Color.B", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x4C])) });

    // Alpha
    fields.push_back({ "Alpha", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x50])) });

    // Scale
    fields.push_back({ "Scale.X", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x54])) });
    fields.push_back({ "Scale.Y", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x58])) });
    fields.push_back({ "Scale.Z", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x5C])) });

    // Vector Quat
    fields.push_back({ "Vector.Quat.X", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x60])) });
    fields.push_back({ "Vector.Quat.Y", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x64])) });
    fields.push_back({ "Vector.Quat.Z", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x68])) });
    fields.push_back({ "Vector.Quat.W", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x6C])) });

    // Vector Magnitude
    fields.push_back({ "Vector.Magnitude", "float", std::to_string(*reinterpret_cast<const float*>(&data[0x70])) });

    // TextureName (null-terminated, max 64 bytes)
    std::string tex(reinterpret_cast<const char*>(&data[0x74]), strnlen(reinterpret_cast<const char*>(&data[0x74]), 64));
    fields.push_back({ "TextureName", "string", tex });
    
    InterpretSphereShaderStruct(fields, data, /* shaderOffset */ 0x94);
   
    return fields;
}

inline void InterpretMicroChunkKeyframes(
    std::vector<ChunkField>& fields,
    const std::vector<uint8_t>& data,
    const std::string& prefix,
    int valueSize)
{
    size_t offset = 0;
    size_t index = 0;

    while (offset + 2 + valueSize <= data.size()) {
        uint8_t id = data[offset++];
        uint8_t size = data[offset++];

        std::ostringstream valStream;
        for (int i = 0; i < valueSize / 4 - 1; ++i) {
            float val = *reinterpret_cast<const float*>(&data[offset]);
            offset += 4;
            valStream << std::fixed << std::setprecision(6) << val;
            if (i < (valueSize / 4 - 2)) valStream << " ";
        }

        float time = *reinterpret_cast<const float*>(&data[offset]);
        offset += 4;

        fields.push_back({ prefix + "[" + std::to_string(index) + "].Value", (valueSize == 16 ? "vec3" : "float"), valStream.str() });
        fields.push_back({ prefix + "[" + std::to_string(index) + "].Time", "float", std::to_string(time) });

        index++;
    }
}
inline void InterpretMicroChunkKeyframes(
    std::vector<ChunkField>& fields,
    const std::vector<uint8_t>& data,
    const std::string& prefix,
    int framePayloadSize,
    std::function<void(const uint8_t*, size_t, size_t, std::vector<ChunkField>&)> decoder)
{
    size_t offset = 0;
    size_t index = 0;

    while (offset + 2 <= data.size()) {
        uint8_t id = data[offset++];
        uint8_t size = data[offset++];

        if (size != framePayloadSize || offset + size > data.size()) {
            fields.push_back({ prefix + "[" + std::to_string(index) + "]", "error", "Unexpected size or out-of-bounds" });
            break;
        }

        decoder(data.data() + offset, size, index, fields);
        offset += size;
        index++;
    }
}

inline std::vector<ChunkField> InterpretSphereColorChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    InterpretMicroChunkKeyframes(fields, chunk->data, "ColorKey", 16,
        [](const uint8_t* p, size_t, size_t i, std::vector<ChunkField>& f) {
            float x = *reinterpret_cast<const float*>(p + 0);
            float y = *reinterpret_cast<const float*>(p + 4);
            float z = *reinterpret_cast<const float*>(p + 8);
            float t = *reinterpret_cast<const float*>(p + 12);
            f.push_back({ "ColorKey[" + std::to_string(i) + "].Value.X", "float", std::to_string(x) });
            f.push_back({ "ColorKey[" + std::to_string(i) + "].Value.Y", "float", std::to_string(y) });
            f.push_back({ "ColorKey[" + std::to_string(i) + "].Value.Z", "float", std::to_string(z) });
            f.push_back({ "ColorKey[" + std::to_string(i) + "].Time",    "float", std::to_string(t) });
        });
    return fields;
}


inline std::vector<ChunkField> InterpretSphereAlphaChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    InterpretMicroChunkKeyframes(fields, chunk->data, "AlphaKey", 8,
        [](const uint8_t* p, size_t, size_t i, std::vector<ChunkField>& f) {
            float value = *reinterpret_cast<const float*>(p + 0);
            float t = *reinterpret_cast<const float*>(p + 4);
            f.push_back({ "AlphaKey[" + std::to_string(i) + "].Value", "float", std::to_string(value) });
            f.push_back({ "AlphaKey[" + std::to_string(i) + "].Time",  "float", std::to_string(t) });
        });
    return fields;
}



inline std::vector<ChunkField> InterpretSphereScaleChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    InterpretMicroChunkKeyframes(fields, chunk->data, "ScaleKey", 16,
        [](const uint8_t* p, size_t, size_t i, std::vector<ChunkField>& f) {
            float x = *reinterpret_cast<const float*>(p + 0);
            float y = *reinterpret_cast<const float*>(p + 4);
            float z = *reinterpret_cast<const float*>(p + 8);
            float t = *reinterpret_cast<const float*>(p + 12);
            f.push_back({ "ScaleKey[" + std::to_string(i) + "].Value.X", "float", std::to_string(x) });
            f.push_back({ "ScaleKey[" + std::to_string(i) + "].Value.Y", "float", std::to_string(y) });
            f.push_back({ "ScaleKey[" + std::to_string(i) + "].Value.Z", "float", std::to_string(z) });
            f.push_back({ "ScaleKey[" + std::to_string(i) + "].Time",    "float", std::to_string(t) });
        });
    return fields;
}


inline std::vector<ChunkField> InterpretSphereVectorChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    InterpretMicroChunkKeyframes(fields, chunk->data, "VectorKey", 24,
        [](const uint8_t* p, size_t, size_t i, std::vector<ChunkField>& f) {
            float x = *reinterpret_cast<const float*>(p + 0);
            float y = *reinterpret_cast<const float*>(p + 4);
            float z = *reinterpret_cast<const float*>(p + 8);
            float w = *reinterpret_cast<const float*>(p + 12);
            float mag = *reinterpret_cast<const float*>(p + 16);
            float t = *reinterpret_cast<const float*>(p + 20);
            std::ostringstream quatStr;
            quatStr << x << " " << y << " " << z << " " << w;
            f.push_back({ "VectorKey[" + std::to_string(i) + "].Quat",      "quaternion", quatStr.str() });
            f.push_back({ "VectorKey[" + std::to_string(i) + "].Magnitude", "float",      std::to_string(mag) });
            f.push_back({ "VectorKey[" + std::to_string(i) + "].Time",      "float",      std::to_string(t) });
        });
    return fields;
}


inline std::vector<ChunkField> InterpretSphereMicrochunk(const std::shared_ptr<ChunkItem>& chunk, uint32_t parentChunkId) {
    std::vector<ChunkField> fields;

    if (!chunk || chunk->id != 0x01 || !chunk->parent) {
        fields.push_back({ "Frame[0]", "error", "Invalid chunk or missing parent" });
        return fields;
    }

    const std::vector<uint8_t>& data = chunk->data;
    size_t size = data.size();

    // Compute frameIndex by locating position within parent's children
    int frameIndex = 0;
    const auto& siblings = chunk->parent->children;
    auto it = std::find_if(siblings.begin(), siblings.end(),
        [&](const std::shared_ptr<ChunkItem>& sibling) { return sibling.get() == chunk.get(); });
    if (it != siblings.end()) {
        frameIndex = static_cast<int>(std::distance(siblings.begin(), it));
    }

    std::string prefix;
    switch (parentChunkId) {
    case 0x0002: prefix = "ColorKey"; break;
    case 0x0003: prefix = "AlphaKey"; break;
    case 0x0004: prefix = "ScaleKey"; break;
    case 0x0005: prefix = "VectorKey"; break;
    default:
        fields.push_back({ "Frame[" + std::to_string(frameIndex) + "]", "error", "Unknown parent chunk ID: 0x" + ToHex(parentChunkId) });
        return fields;
    }

    const uint8_t* payload = data.data();

    if ((parentChunkId == 0x0002 || parentChunkId == 0x0004) && size == 16) {
        float x = *reinterpret_cast<const float*>(&payload[0]);
        float y = *reinterpret_cast<const float*>(&payload[4]);
        float z = *reinterpret_cast<const float*>(&payload[8]);
        float t = *reinterpret_cast<const float*>(&payload[12]);

        fields.push_back({ prefix + "[" + std::to_string(frameIndex) + "].Value.X", "float", std::to_string(x) });
        fields.push_back({ prefix + "[" + std::to_string(frameIndex) + "].Value.Y", "float", std::to_string(y) });
        fields.push_back({ prefix + "[" + std::to_string(frameIndex) + "].Value.Z", "float", std::to_string(z) });
        fields.push_back({ prefix + "[" + std::to_string(frameIndex) + "].Time",    "float", std::to_string(t) });

    }
    else if (parentChunkId == 0x0003 && size == 8) {
        float value = *reinterpret_cast<const float*>(&payload[0]);
        float t = *reinterpret_cast<const float*>(&payload[4]);

        fields.push_back({ prefix + "[" + std::to_string(frameIndex) + "].Value", "float", std::to_string(value) });
        fields.push_back({ prefix + "[" + std::to_string(frameIndex) + "].Time",  "float", std::to_string(t) });

    }
    else if (parentChunkId == 0x0005 && size == 24) {
        float x = *reinterpret_cast<const float*>(&payload[0]);
        float y = *reinterpret_cast<const float*>(&payload[4]);
        float z = *reinterpret_cast<const float*>(&payload[8]);
        float w = *reinterpret_cast<const float*>(&payload[12]);
        float mag = *reinterpret_cast<const float*>(&payload[16]);
        float t = *reinterpret_cast<const float*>(&payload[20]);

        std::ostringstream quatStr;
        quatStr << x << " " << y << " " << z << " " << w;

        fields.push_back({ prefix + "[" + std::to_string(frameIndex) + "].Quat",      "quaternion", quatStr.str() });
        fields.push_back({ prefix + "[" + std::to_string(frameIndex) + "].Magnitude", "float",      std::to_string(mag) });
        fields.push_back({ prefix + "[" + std::to_string(frameIndex) + "].Time",      "float",      std::to_string(t) });

    }
    else {
        fields.push_back({ "Frame[" + std::to_string(frameIndex) + "]", "error", "Unexpected size (" + std::to_string(size) + ") for parent ID 0x" + ToHex(parentChunkId) });
    }

    return fields;
}








    


inline std::vector<ChunkField> InterpretSphereChannelChunk(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> result;

    // Defensive: if no parent, bail
    if (!chunk->parent) {
        result.push_back({ "Error", "string", "Missing parent chunk" });
        return result;
    }

    // Check if parent is a known Sphere object (e.g., 0x0001 = Sphere Header)
    bool isUnderSphere = (chunk->parent->id == 0x0001);  // W3D_CHUNK_SPHERE_HEADER

    if (!isUnderSphere) {
        result.push_back({ "Error", "string", "Ambiguous chunk ID outside of sphere context" });
        return result;
    }

    static const std::unordered_map<uint32_t, std::string> chunkNames = {
        {0x00000002, "Color Channel"},
        {0x00000003, "Alpha Channel"},
        {0x00000004, "Scale Channel"},
        {0x00000005, "Vector Channel"},
    };

    std::string label = chunkNames.count(chunk->id)
        ? chunkNames.at(chunk->id)
        : "Unknown Sphere Channel";

    result.push_back({ "ChannelType", "string", label });

    for (const auto& sub : chunk->children) {
        if (sub->id == 0x09081503) {
            auto fields = InterpretSphereMicrochunk(sub, chunk->id);
            result.insert(result.end(), fields.begin(), fields.end());
        }
    }

    return result;
}




























































