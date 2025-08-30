#pragma once
#include "W3DStructs.h"
#include "FormatUtils.h"
#include "ParseUtils.h"
#include <iostream>
#include <vector>

//These Chunks were depreciated before renegade released.

inline std::vector<ChunkField> ObsoleteStub(const char* name) {
    return { { "info", "string", std::string("OBSOLETE: ") + name + " (no structure)" } };
}

#pragma pack(push,1)
struct W3dMeshHeader1 {
    uint32_t Version;
    char MeshName[16];
    uint32_t Attributes;
    uint32_t NumTriangles;
    uint32_t NumQuads;
    uint32_t NumSrTris;
    uint32_t NumPovQuads;
    uint32_t NumVertices;
    uint32_t NumNormals;
    uint32_t NumSrNormals;
    uint32_t NumTexCoords;
    uint32_t NumMaterials;
    uint32_t NumVertColors;
    uint32_t NumVertInfluences;
    uint32_t NumDamageStages;
    uint32_t FutureCounts[8];
    float    LODMin;
    float    LODMax;
    W3dVectorStruct Min;
    W3dVectorStruct Max;
    W3dVectorStruct SphCenter;
    float    SphRadius;
    W3dVectorStruct Translation;
    float    Rotation[9];
    W3dVectorStruct MassCenter;
    float    Inertia[9];
    float    Volume;
    char     HierarchyTreeName[16];
    char     HierarchyModelName[16];
    uint32_t FutureUse[24];
};

struct W3dMaterial1Struct {
    char     MaterialName[16];
    char     PrimaryName[16];
    char     SecondaryName[16];
    uint32_t RenderFlags;
    uint8_t  Red;
    uint8_t  Green;
    uint8_t  Blue;
    uint8_t  Pad;
};

struct W3dSurrenderTriStruct {
    uint32_t          VIndex[3];
    W3dTexCoordStruct TexCoord[3];
    uint32_t          MaterialIDx;
    W3dVectorStruct   Normal;
    uint32_t          Attributes;
    W3dRGBStruct      Gouraud[3];
};

struct W3dDamageHeaderStruct {
    uint32_t NumDamageMaterials;
    uint32_t NumDamageVerts;
    uint32_t NumDamageColors;
    uint32_t DamageIndex;
    uint32_t FutureUse[4];
};

struct W3dMeshDamageVertexStruct {
    uint32_t        VertexIndex;
    W3dVectorStruct NewVertex;
};

struct W3dMeshDamageColorStruct {
    uint32_t     VertexIndex;
    W3dRGBStruct NewColor;
};

struct W3dMaterial2Struct {
    char     MaterialName[16];
    char     PrimaryName[16];
    char     SecondaryName[16];
    uint32_t RenderFlags;
    uint8_t  Red;
    uint8_t  Green;
    uint8_t  Blue;
    uint8_t  Alpha;
    uint16_t PrimaryNumFrames;
    uint16_t SecondaryNumFrames;
    char     Padding[12];
};

struct W3dMaterial3Struct {
    uint32_t     Material3Flags;
    W3dRGBStruct DiffuseColor;
    W3dRGBStruct SpecularColor;
    W3dRGBStruct EmissiveCoefficients;
    W3dRGBStruct AmbientCoefficients;
    W3dRGBStruct DiffuseCoefficients;
    W3dRGBStruct SpecularCoefficients;
    float        Shininess;
    float        Opacity;
    float        Translucency;
    float        FogCoeff;
};

struct W3dMap3Struct {
    uint16_t MappingType;
    uint16_t FrameCount;
    uint32_t FrameRate;
};

struct W3dHModelAuxDataStruct {
    uint32_t Attributes;
    uint32_t MeshCount;
    uint32_t CollisionCount;
    uint32_t SkinCount;
    uint32_t ShadowCount;
    uint32_t NullCount;
    uint32_t FutureCounts[6];
    uint32_t LODMin;
    uint32_t LODMax;
    uint32_t FutureUse[32];
};

struct W3dEmitterColorKeyframeStruct {
    float        Time;
    W3dRGBAStruct Color;
};

struct W3dEmitterOpacityKeyframeStruct {
    float Time;
    float Opacity;
};

struct W3dEmitterSizeKeyframeStruct {
    float Time;
    float Size;
};
#pragma pack(pop)

inline std::vector<ChunkField> InterpretMeshHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dMeshHeader1>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed MESH_HEADER: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dMeshHeader1>(v);
    ChunkFieldBuilder B(fields);
    B.Version("Version", h.Version);
    B.Name("MeshName", h.MeshName);
    B.UInt32("Attributes", h.Attributes);
    B.UInt32("NumTriangles", h.NumTriangles);
    B.UInt32("NumQuads", h.NumQuads);
    B.UInt32("NumSrTris", h.NumSrTris);
    B.UInt32("NumPovQuads", h.NumPovQuads);
    B.UInt32("NumVertices", h.NumVertices);
    B.UInt32("NumNormals", h.NumNormals);
    B.UInt32("NumSrNormals", h.NumSrNormals);
    B.UInt32("NumTexCoords", h.NumTexCoords);
    B.UInt32("NumMaterials", h.NumMaterials);
    B.UInt32("NumVertColors", h.NumVertColors);
    B.UInt32("NumVertInfluences", h.NumVertInfluences);
    B.UInt32("NumDamageStages", h.NumDamageStages);
    B.UInt32Array("FutureCounts", h.FutureCounts, 8);
    B.Float("LODMin", h.LODMin);
    B.Float("LODMax", h.LODMax);
    B.Vec3("Min", h.Min);
    B.Vec3("Max", h.Max);
    B.Vec3("SphCenter", h.SphCenter);
    B.Float("SphRadius", h.SphRadius);
    B.Vec3("Translation", h.Translation);
    for (int i = 0; i < 9; ++i) B.Float("Rotation[" + std::to_string(i) + "]", h.Rotation[i]);
    B.Vec3("MassCenter", h.MassCenter);
    for (int i = 0; i < 9; ++i) B.Float("Inertia[" + std::to_string(i) + "]", h.Inertia[i]);
    B.Float("Volume", h.Volume);
    B.Name("HierarchyTreeName", h.HierarchyTreeName);
    B.Name("HierarchyModelName", h.HierarchyModelName);
    B.UInt32Array("FutureUse", h.FutureUse, 24);
    return fields;
}

inline std::vector<ChunkField> InterpretSurrenderNormals(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto parsed = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed SURRENDER_NORMALS: " + *err);
        return fields;
    }
    const auto& arr = std::get<std::vector<W3dVectorStruct>>(parsed);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < arr.size(); ++i) {
        B.Vec3("SrNormal[" + std::to_string(i) + "]", arr[i]);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretTexcoords(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto parsed = ParseChunkArray<W3dTexCoordStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed TEXCOORDS: " + *err);
        return fields;
    }
    const auto& arr = std::get<std::vector<W3dTexCoordStruct>>(parsed);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < arr.size(); ++i) {
        B.TexCoord("TexCoord[" + std::to_string(i) + "]", arr[i]);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretMaterials1(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto parsed = ParseChunkArray<W3dMaterial1Struct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed MATERIALS: " + *err);
        return fields;
    }
    const auto& mats = std::get<std::vector<W3dMaterial1Struct>>(parsed);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < mats.size(); ++i) {
        const auto& m = mats[i];
        std::string pfx = "Material[" + std::to_string(i) + "]";
        B.Name((pfx + ".MaterialName").c_str(), m.MaterialName);
        B.Name((pfx + ".PrimaryName").c_str(), m.PrimaryName);
        B.Name((pfx + ".SecondaryName").c_str(), m.SecondaryName);
        B.UInt32(pfx + ".RenderFlags", m.RenderFlags);
        B.RGB(pfx + ".Color", m.Red, m.Green, m.Blue);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretTrianglesO(const std::shared_ptr<ChunkItem>&) {
    return ObsoleteStub("O_W3D_CHUNK_TRIANGLES");
}

inline std::vector<ChunkField> InterpretQuadranglesO(const std::shared_ptr<ChunkItem>&) {
    return ObsoleteStub("O_W3D_CHUNK_QUADRANGLES");
}

inline std::vector<ChunkField> InterpretSurrenderTriangles(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto parsed = ParseChunkArray<W3dSurrenderTriStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed SURRENDER_TRIANGLES: " + *err);
        return fields;
    }
    const auto& tris = std::get<std::vector<W3dSurrenderTriStruct>>(parsed);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < tris.size(); ++i) {
        const auto& t = tris[i];
        std::string pfx = "Triangle[" + std::to_string(i) + "]";
        B.UInt32(pfx + ".VIndex0", t.VIndex[0]);
        B.UInt32(pfx + ".VIndex1", t.VIndex[1]);
        B.UInt32(pfx + ".VIndex2", t.VIndex[2]);
        for (int j = 0; j < 3; ++j) {
            B.TexCoord((pfx + ".TexCoord[" + std::to_string(j) + "]").c_str(), t.TexCoord[j]);
        }
        B.UInt32(pfx + ".MaterialIDx", t.MaterialIDx);
        B.Vec3(pfx + ".Normal", t.Normal);
        B.UInt32(pfx + ".Attributes", t.Attributes);
        for (int j = 0; j < 3; ++j) {
            B.RGB(pfx + ".Gouraud[" + std::to_string(j) + "]", t.Gouraud[j].Red, t.Gouraud[j].Green, t.Gouraud[j].Blue);
        }
    }
    return fields;
}

inline std::vector<ChunkField> InterpretPovTriangles(const std::shared_ptr<ChunkItem>&) {
    return ObsoleteStub("O_W3D_CHUNK_POV_TRIANGLES");
}

inline std::vector<ChunkField> InterpretPovQuadrangles(const std::shared_ptr<ChunkItem>&) {
    return ObsoleteStub("O_W3D_CHUNK_POV_QUADRANGLES");
}

inline std::vector<ChunkField> InterpretVertexColors(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto parsed = ParseChunkArray<W3dRGBStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed VERTEX_COLORS: " + *err);
        return fields;
    }
    const auto& colors = std::get<std::vector<W3dRGBStruct>>(parsed);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < colors.size(); ++i) {
        const auto& c = colors[i];
        B.RGB("VertexColor[" + std::to_string(i) + "]", c.Red, c.Green, c.Blue);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretDamage(const std::shared_ptr<ChunkItem>&) {
    return ObsoleteStub("W3D_CHUNK_DAMAGE");
}

inline std::vector<ChunkField> InterpretDamageHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto v = ParseChunkStruct<W3dDamageHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed DAMAGE_HEADER: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dDamageHeaderStruct>(v);
    ChunkFieldBuilder B(fields);
    B.UInt32("NumDamageMaterials", h.NumDamageMaterials);
    B.UInt32("NumDamageVerts", h.NumDamageVerts);
    B.UInt32("NumDamageColors", h.NumDamageColors);
    B.UInt32("DamageIndex", h.DamageIndex);
    B.UInt32Array("FutureUse", h.FutureUse, 4);
    return fields;
}

inline std::vector<ChunkField> InterpretDamageVertices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto parsed = ParseChunkArray<W3dMeshDamageVertexStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed DAMAGE_VERTICES: " + *err);
        return fields;
    }
    const auto& verts = std::get<std::vector<W3dMeshDamageVertexStruct>>(parsed);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < verts.size(); ++i) {
        const auto& vtx = verts[i];
        std::string pfx = "DamageVertex[" + std::to_string(i) + "]";
        B.UInt32(pfx + ".VertexIndex", vtx.VertexIndex);
        B.Vec3(pfx + ".NewVertex", vtx.NewVertex);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretDamageColors(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto parsed = ParseChunkArray<W3dMeshDamageColorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed DAMAGE_COLORS: " + *err);
        return fields;
    }
    const auto& cols = std::get<std::vector<W3dMeshDamageColorStruct>>(parsed);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < cols.size(); ++i) {
        const auto& c = cols[i];
        std::string pfx = "DamageColor[" + std::to_string(i) + "]";
        B.UInt32(pfx + ".VertexIndex", c.VertexIndex);
        B.RGB(pfx + ".NewColor", c.NewColor.Red, c.NewColor.Green, c.NewColor.Blue);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretDamageMaterials(const std::shared_ptr<ChunkItem>&) {
    return ObsoleteStub("W3D_CHUNK_DAMAGE_MATERIALS");
}

inline std::vector<ChunkField> InterpretMaterials2(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto parsed = ParseChunkArray<W3dMaterial2Struct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed MATERIALS2: " + *err);
        return fields;
    }
    const auto& mats = std::get<std::vector<W3dMaterial2Struct>>(parsed);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < mats.size(); ++i) {
        const auto& m = mats[i];
        std::string pfx = "Material2[" + std::to_string(i) + "]";
        B.Name((pfx + ".MaterialName").c_str(), m.MaterialName);
        B.Name((pfx + ".PrimaryName").c_str(), m.PrimaryName);
        B.Name((pfx + ".SecondaryName").c_str(), m.SecondaryName);
        B.UInt32(pfx + ".RenderFlags", m.RenderFlags);
        B.RGBA(pfx + ".Color", m.Red, m.Green, m.Blue, m.Alpha);
        B.UInt16(pfx + ".PrimaryNumFrames", m.PrimaryNumFrames);
        B.UInt16(pfx + ".SecondaryNumFrames", m.SecondaryNumFrames);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretMaterials3(const std::shared_ptr<ChunkItem>&) {
    return ObsoleteStub("W3D_CHUNK_MATERIALS3");
}

inline std::vector<ChunkField> InterpretMaterial3(const std::shared_ptr<ChunkItem>&) {
    return ObsoleteStub("W3D_CHUNK_MATERIAL3");
}

inline std::vector<ChunkField> InterpretMaterial3Name(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    ChunkFieldBuilder B(fields);
    B.NullTerm("Material3Name", reinterpret_cast<const char*>(chunk->data.data()), chunk->data.size(), 256);
    return fields;
}

inline std::vector<ChunkField> InterpretMaterial3Info(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto v = ParseChunkStruct<W3dMaterial3Struct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed MATERIAL3_INFO: " + *err);
        return fields;
    }
    const auto& m = std::get<W3dMaterial3Struct>(v);
    ChunkFieldBuilder B(fields);
    B.UInt32("Material3Flags", m.Material3Flags);
    B.RGB("DiffuseColor", m.DiffuseColor.Red, m.DiffuseColor.Green, m.DiffuseColor.Blue);
    B.RGB("SpecularColor", m.SpecularColor.Red, m.SpecularColor.Green, m.SpecularColor.Blue);
    B.RGB("EmissiveCoefficients", m.EmissiveCoefficients.Red, m.EmissiveCoefficients.Green, m.EmissiveCoefficients.Blue);
    B.RGB("AmbientCoefficients", m.AmbientCoefficients.Red, m.AmbientCoefficients.Green, m.AmbientCoefficients.Blue);
    B.RGB("DiffuseCoefficients", m.DiffuseCoefficients.Red, m.DiffuseCoefficients.Green, m.DiffuseCoefficients.Blue);
    B.RGB("SpecularCoefficients", m.SpecularCoefficients.Red, m.SpecularCoefficients.Green, m.SpecularCoefficients.Blue);
    B.Float("Shininess", m.Shininess);
    B.Float("Opacity", m.Opacity);
    B.Float("Translucency", m.Translucency);
    B.Float("FogCoeff", m.FogCoeff);
    return fields;
}

inline std::vector<ChunkField> InterpretMaterial3DcMap(const std::shared_ptr<ChunkItem>&) {
    return ObsoleteStub("W3D_CHUNK_MATERIAL3_DC_MAP");
}

inline std::vector<ChunkField> InterpretMap3Filename(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    ChunkFieldBuilder B(fields);
    B.NullTerm("Map3Filename", reinterpret_cast<const char*>(chunk->data.data()), chunk->data.size(), 256);
    return fields;
}

inline std::vector<ChunkField> InterpretMap3Info(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto v = ParseChunkStruct<W3dMap3Struct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed MAP3_INFO: " + *err);
        return fields;
    }
    const auto& m = std::get<W3dMap3Struct>(v);
    ChunkFieldBuilder B(fields);
    B.UInt16("MappingType", m.MappingType);
    B.UInt16("FrameCount", m.FrameCount);
    B.UInt32("FrameRate", m.FrameRate);
    return fields;
}

inline std::vector<ChunkField> InterpretMaterial3DiMap(const std::shared_ptr<ChunkItem>&) {
    return ObsoleteStub("W3D_CHUNK_MATERIAL3_DI_MAP");
}

inline std::vector<ChunkField> InterpretMaterial3ScMap(const std::shared_ptr<ChunkItem>&) {
    return ObsoleteStub("W3D_CHUNK_MATERIAL3_SC_MAP");
}

inline std::vector<ChunkField> InterpretMaterial3SiMap(const std::shared_ptr<ChunkItem>&) {
    return ObsoleteStub("W3D_CHUNK_MATERIAL3_SI_MAP");
}

inline std::vector<ChunkField> InterpretPerTriMaterials(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    auto parsed = ParseChunkArray<uint16_t>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed PER_TRI_MATERIALS: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<uint16_t>>(parsed);
    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.UInt16("TriMaterialIdx[" + std::to_string(i) + "]", data[i]);
    }
    return fields;
}


