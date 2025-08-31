#pragma once
#include "W3DStructs.h"
#include "FormatUtils.h"
#include "ParseUtils.h"
#include <vector>

// --- Shader Mesh level ------------------------------------------------------

// SHDMESH_NAME
inline std::vector<ChunkField> InterpretShdMeshName(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    ChunkFieldBuilder B(fields);
    B.NullTerm("Name", reinterpret_cast<const char*>(chunk->data.data()), chunk->data.size());
    return fields;
}



inline std::vector<ChunkField> InterpretShdMeshHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dShdMeshHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDMESH_HEADER chunk: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dShdMeshHeaderStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Version("Version", h.Version);
    B.UInt32("MeshFlags", h.MeshFlags);
    B.UInt32("NumTriangles", h.NumTriangles);
    B.UInt32("NumVertices", h.NumVertices);
    B.UInt32("NumSubMeshes", h.NumSubMeshes);
    B.UInt32Array("FutureCounts", h.FutureCounts, 5);
    B.Vec3("BoxMin", h.BoxMin);
    B.Vec3("BoxMax", h.BoxMax);
    B.Vec3("SphCenter", h.SphCenter);
    B.Float("SphRadius", h.SphRadius);
    return fields;
}

inline std::vector<ChunkField> InterpretShdMeshUserText(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    ChunkFieldBuilder B(fields);
    B.NullTerm("UserText", reinterpret_cast<const char*>(chunk->data.data()), chunk->data.size());
    return fields;
}



inline std::vector<ChunkField> InterpretShdSubMeshHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    auto v = ParseChunkStruct<W3dShdSubMeshHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_HEADER chunk: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dShdSubMeshHeaderStruct>(v);
    ChunkFieldBuilder B(fields);
    B.UInt32("NumTriangles", h.NumTriangles);
    B.UInt32("NumVertices", h.NumVertices);
    B.UInt32Array("FutureCounts", h.FutureCounts, 2);
    B.Vec3("BoxMin", h.BoxMin);
    B.Vec3("BoxMax", h.BoxMax);
    B.Vec3("SphCenter", h.SphCenter);
    B.Float("SphRadius", h.SphRadius);
    return fields;
}





inline std::vector<ChunkField>
InterpretShdSubMeshShaderClassId(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto v = ParseChunkStruct<W3dShdSubMeshShaderClassIdStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_SHADER_CLASSID: " + *err);
        return fields;
    }
    const auto& s = std::get<W3dShdSubMeshShaderClassIdStruct>(v);
    ChunkFieldBuilder B(fields);
    B.UInt32("ClassID", s.ShaderClass);

    static const char* k[] = {
        "SHDDEF_CLASSID_DUMMY",
        "SHDDEF_CLASSID_SIMPLE",
        "SHDDEF_CLASSID_GLOSSMASK",
        "SHDDEF_CLASSID_BUMPSPEC",
        "SHDDEF_CLASSID_BUMPDIFF",
        "SHDDEF_CLASSID_CUBEMAP",
        "SHDDEF_CLASSID_LEGACYW3D"
    };
    if (s.ShaderClass < std::size(k)) B.Push("ClassName", "enum", k[s.ShaderClass]);
    return fields;
}

// VARIABLES wrapper used by ShdDef (Generals):
static constexpr uint32_t SHDDEF_CHUNKID_VARIABLES = 0x16490430u;

// SurfaceType mapping (matches your enum list)
inline const char* SurfaceTypeName(uint32_t v) {
    switch (v) {
    case 0:  return "Light Metal";
    case 1:  return "Heavy Metal";
    case 2:  return "Water";
    case 3:  return "Sand";
    case 4:  return "Dirt";
    case 5:  return "Mud";
    case 6:  return "Grass";
    case 7:  return "Wood";
    case 8:  return "Concrete";
    case 9:  return "Flesh";
    case 10: return "Rock";
    case 11: return "Snow";
    case 12: return "Ice";
    case 13: return "Default";
    case 14: return "Glass";
    case 15: return "Cloth";
    case 16: return "Tiberium Field";
    case 17: return "Foliage Permeable";
    case 18: return "Glass Permeable";
    case 19: return "Ice Permeable";
    case 20: return "Cloth Permeable";
    case 21: return "Electrical";
    case 22: return "Electrical Permeable";
    case 23: return "Flammable";
    case 24: return "Flammable Permeable";
    case 25: return "Steam";
    case 26: return "Steam Permeable";
    case 27: return "Water Permeable";
    case 28: return "Tiberium Water";
    case 29: return "Tiberium Water Permeable";
    case 30: return "Underwater Dirt";
    case 31: return "Underwater Tiberium Dirt";
    default: return nullptr;
    }
}

// decode a WWSTRING (UTF-16LE bytes, size includes NUL) to UTF-8 std::string
inline std::string DecodeWWString(const uint8_t* p, size_t sizeBytes) {
    if (sizeBytes < 2) return {};
    // drop trailing u16 NUL if present
    size_t u16len = sizeBytes / 2;
    const char16_t* w = reinterpret_cast<const char16_t*>(p);
    if (u16len && w[u16len - 1] == u'\0') --u16len;

    std::u16string u16(w, w + u16len);
    // simple UTF-16LE -> UTF-8 conversion (fallback for non-Qt core code):
    std::string out;
    out.reserve(u16len);
    for (char16_t ch : u16) {
        if (ch < 0x80) out.push_back(static_cast<char>(ch));
        else {
            // minimal utf-8 encoder for BMP range
            if (ch < 0x800) {
                out.push_back(char(0xC0 | (ch >> 6)));
                out.push_back(char(0x80 | (ch & 0x3F)));
            }
            else {
                out.push_back(char(0xE0 | (ch >> 12)));
                out.push_back(char(0x80 | ((ch >> 6) & 0x3F)));
                out.push_back(char(0x80 | (ch & 0x3F)));
            }
        }
    }
    return out;
}


inline std::vector<ChunkField>
InterpretShdSubMeshShaderDef(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    ChunkFieldBuilder B(fields);

    // Old files sometimes have a plain u32 in the DEF chunk itself
    if (!chunk->data.empty()) {
        if (chunk->data.size() >= sizeof(W3dShdSubMeshShaderDefStruct)) {
            auto v = ParseChunkStruct<W3dShdSubMeshShaderDefStruct>(chunk);
            if (auto err = std::get_if<std::string>(&v)) {
                fields.emplace_back("warning", "string", "ShaderDef u32 parse failed: " + *err);
            }
            else {
                const auto& s = std::get<W3dShdSubMeshShaderDefStruct>(v);
                B.UInt32("ShaderDefinition", s.ShaderDefinition);
            }
        }
    }

    // Generals-style: one or more nested VARIABLES chunks (0x16490430)
    for (const auto& child : chunk->children) {
        if (child->id != SHDDEF_CHUNKID_VARIABLES) continue;

        const uint8_t* cur = child->data.data();
        size_t          rem = child->data.size();
        size_t          idx = 0;

        while (rem >= 2) {
            const uint8_t id = cur[0];
            const uint8_t size = cur[1];
            cur += 2; rem -= 2;
            if (size > rem) {
                B.Push("error", "string", "Truncated shader VAR micro-chunk");
                break;
            }

            switch (id) {
            case 0x00: { // VARID_NAME (WWSTRING)
                const std::string name = DecodeWWString(cur, size);
                B.Push("Name", "string", name);
                break;
            }
            case 0x01: { // VARID_SURFACETYPE (uint32)
                if (size == 4) {
                    uint32_t st{};
                    std::memcpy(&st, cur, 4);
                    if (const char* sname = SurfaceTypeName(st)) {
                        B.UInt32("SurfaceType", st);
                        B.Push("SurfaceTypeName", "enum", sname);
                    }
                    else {
                        B.UInt32("SurfaceType", st);
                        B.Push("SurfaceTypeName", "enum", "Unknown");
                    }
                }
                else {
                    B.Push("error", "string", "SurfaceType micro-chunk wrong size");
                }
                break;
            }
            default: {
                // Keep unknowns visible
                B.Push("UnknownVar(0x" + ToHex(id) + ")",
                    "bytes[" + std::to_string(size) + "]",
                    std::to_string(size) + " bytes");
                break;
            }
            }

            cur += size; rem -= size; ++idx;
        }
    }

    return fields;
}

inline std::vector<ChunkField>
InterpretShdSubMeshShaderDefVariables(const std::shared_ptr<ChunkItem>& chunk)
{
    std::vector<ChunkField> F; if (!chunk) return F;
    const auto& buf = chunk->data;
    const uint8_t* p = buf.data();
    const uint8_t* end = p + buf.size();

    ChunkFieldBuilder B(F);

    // Micro-chunk ids (from Generals code)
    constexpr uint8_t VARID_NAME = 0x00; // 
    constexpr uint8_t VARID_SURFACETYPE = 0x01; // uint32

    while (p + 2 <= end) {
        uint8_t id = p[0];
        uint8_t size = p[1];
        p += 2;
        if (p + size > end) break; // truncated

        switch (id) {
        case VARID_NAME: {
                    
            B.NullTerm("Name", reinterpret_cast<const char*>(p), size);
            break;
        }
        case VARID_SURFACETYPE: {
            if (size >= 4) {
                uint32_t st = (uint32_t)p[0] | ((uint32_t)p[1] << 8)
                    | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
                B.UInt32("SurfaceType", st);
                
                static const char* kSurfaceNames[] = {
                    "Light Metal","Heavy Metal","Water","Sand","Dirt","Mud","Grass","Wood",
                    "Concrete","Flesh","Rock","Snow","Ice","Default","Glass","Cloth",
                    "Tiberium Field","Foliage Permeable","Glass Permeable","Ice Permeable",
                    "Cloth Permeable","Electrical","Electrical Permeable","Flammable",
                    "Flammable Permeable","Steam","Steam Permeable","Water Permeable",
                    "Tiberium Water","Tiberium Water Permeable","Underwater Dirt",
                    "Underwater Tiberium Dirt"
                };
                if (st < (uint32_t)(sizeof(kSurfaceNames) / sizeof(kSurfaceNames[0]))) {
                    B.Push("SurfaceTypeName", "enum", kSurfaceNames[st]);
                }
            }
            break;
        }
        default:
            // unknown variable, keep bytes count so users can spot them
            B.Push("UnknownVar[" + std::to_string(id) + "]",
                "bytes", std::to_string(size));
            break;
        }
        p += size;
    }

    return F;
}

inline std::vector<ChunkField>
InterpretShdSubMeshShaderDefVariables50(const std::shared_ptr<ChunkItem>& chunk)
{
    std::vector<ChunkField> F; if (!chunk) return F;
    const uint8_t* cur = chunk->data.data();
    const uint8_t* end = cur + chunk->data.size();
    ChunkFieldBuilder B(F);

    enum : uint8_t {
        VARID_TEXTURE_NAME = 0x00, // wwstring; treat as C-string bytes
        VARID_BUMP_MAP_NAME = 0x01, // only in Bump* variants
        VARID_AMBIENT_COLOR = 0x02, // float[3]
        VARID_DIFFUSE_COLOR = 0x03, // float[3]
        VARID_SPECULAR_COLOR = 0x04, // only BumpSpec; float[3]
        VARID_DIFFUSE_BUMPINESS = 0x05, // float[2] (X=Scale, Y=Bias)
        VARID_SPECULAR_BUMPINESS = 0x06  // only BumpSpec; float[2]
    };

    auto read_f32 = [](const uint8_t* p) { float f; std::memcpy(&f, p, 4); return f; };

    while (cur + 2 <= end) {
        const uint8_t id = cur[0];
        const uint8_t size = cur[1];
        cur += 2;
        if (cur + size > end) break; // truncated

        switch (id) {
        case VARID_TEXTURE_NAME:
            B.NullTerm("TextureName", reinterpret_cast<const char*>(cur), size);
            break;

        case VARID_BUMP_MAP_NAME:
            B.NullTerm("BumpMapName", reinterpret_cast<const char*>(cur), size);
            break;

        case VARID_AMBIENT_COLOR:
            if (size >= 12) B.Push("Ambient", "vector3",
                FormatUtils::FormatVec3(read_f32(cur), read_f32(cur + 4), read_f32(cur + 8)));
            break;

        case VARID_DIFFUSE_COLOR:
            if (size >= 12) B.Push("Diffuse", "vector3",
                FormatUtils::FormatVec3(read_f32(cur), read_f32(cur + 4), read_f32(cur + 8)));
            break;

        case VARID_SPECULAR_COLOR:
            if (size >= 12) B.Push("Specular", "vector3",
                FormatUtils::FormatVec3(read_f32(cur), read_f32(cur + 4), read_f32(cur + 8)));
            break;

        case VARID_DIFFUSE_BUMPINESS:
            if (size >= 8) {
                const float sx = read_f32(cur), sy = read_f32(cur + 4);
                B.Float("DiffuseBumpiness.Scale", sx);
                B.Float("DiffuseBumpiness.Bias", sy);
            }
            break;

        case VARID_SPECULAR_BUMPINESS:
            if (size >= 8) {
                const float sx = read_f32(cur), sy = read_f32(cur + 4);
                B.Float("SpecularBumpiness.Scale", sx);
                B.Float("SpecularBumpiness.Bias", sy);
            }
            break;

        default:
            // Unknown micro-var: show its id and length so we don't hide data
            B.Push("UnknownVar[" + std::to_string(id) + "]", "bytes",
                std::to_string(size));
            break;
        }

        cur += size;
    }

    return F;
}


inline std::vector<ChunkField> InterpretShdSubMeshVertices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    auto v = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_VERTICES chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dVectorStruct>>(v);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.Vec3("Vertex[" + std::to_string(i) + "]", data[i]);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretShdSubMeshVertexNormals(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    auto v = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_VERTEX_NORMALS chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dVectorStruct>>(v);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.Vec3("Normal[" + std::to_string(i) + "]", data[i]);
    }
    return fields;
}

struct W3dTri16Struct { uint16_t I, J, K; };

inline std::vector<ChunkField> InterpretShdSubMeshTriangles(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    auto v = ParseChunkArray<W3dTri16Struct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_TRIANGLES chunk: " + *err);
        return fields;
    }
    const auto& tris = std::get<std::vector<W3dTri16Struct>>(v);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < tris.size(); ++i) {
        const auto& t = tris[i];
        const std::string pfx = "Triangle[" + std::to_string(i) + "]";
        B.UInt16(pfx + ".V0", t.I);
        B.UInt16(pfx + ".V1", t.J);
        B.UInt16(pfx + ".V2", t.K);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretShdSubMeshVertexShadeIndices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    auto v = ParseChunkArray<uint32_t>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_VERTEX_SHADE_INDICES chunk: " + *err);
        return fields;
    }
    const auto& idx = std::get<std::vector<uint32_t>>(v);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < idx.size(); ++i) {
        B.UInt32("Index[" + std::to_string(i) + "]", idx[i]);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretShdSubMeshUV0(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    auto v = ParseChunkArray<W3dTexCoordStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_UV0 chunk: " + *err);
        return fields;
    }
    const auto& uvs = std::get<std::vector<W3dTexCoordStruct>>(v);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < uvs.size(); ++i) {
        B.TexCoord("UV0[" + std::to_string(i) + "]", uvs[i]);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretShdSubMeshUV1(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    auto v = ParseChunkArray<W3dTexCoordStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_UV1 chunk: " + *err);
        return fields;
    }
    const auto& uvs = std::get<std::vector<W3dTexCoordStruct>>(v);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < uvs.size(); ++i) {
        B.TexCoord("UV1[" + std::to_string(i) + "]", uvs[i]);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretShdSubMeshTangentBasisS(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    auto v = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_TANGENT_BASIS_S chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dVectorStruct>>(v);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.Vec3("TangentS[" + std::to_string(i) + "]", data[i]);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretShdSubMeshTangentBasisT(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    auto v = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_TANGENT_BASIS_T chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dVectorStruct>>(v);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.Vec3("TangentT[" + std::to_string(i) + "]", data[i]);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretShdSubMeshTangentBasisSXT(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    auto v = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_TANGENT_BASIS_SXT chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dVectorStruct>>(v);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.Vec3("TangentSxT[" + std::to_string(i) + "]", data[i]);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretShdSubMeshColor(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    auto v = ParseChunkArray<W3dRGBStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_COLOR chunk: " + *err);
        return fields;
    }
    const auto& cols = std::get<std::vector<W3dRGBStruct>>(v);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < cols.size(); ++i) {
        const auto& c = cols[i];
        B.RGB("Color[" + std::to_string(i) + "]", c.R, c.G, c.B);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretShdSubMeshVertexInfluences(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    auto v = ParseChunkArray<W3dVertInfStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SHDSUBMESH_VERTEX_INFLUENCES chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dVertInfStruct>>(v);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        const auto& inf = data[i];
        const std::string pfx = "Influence[" + std::to_string(i) + "]";
        for (int j = 0; j < 2; ++j) {
            B.UInt16(pfx + ".BoneIdx[" + std::to_string(j) + "]", inf.BoneIdx[j]);
            B.UInt16(pfx + ".Weight[" + std::to_string(j) + "]", inf.Weight[j]);
        }
    }
    return fields;
}

