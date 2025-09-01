#pragma once
#include "W3DStructs.h"
#include "FormatUtils.h"
#include "ParseUtils.h"
#include <iostream>

inline std::vector<ChunkField> InterpretMeshHeader3(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto buff = ParseChunkStruct<W3dMeshHeader3Struct>(chunk);
    if (auto err = std::get_if<std::string>(&buff)) {
        fields.emplace_back("error", "string", "Malformed Mesh_Header chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<W3dMeshHeader3Struct>(buff);

    uint32_t attr = data.Attributes;

    ChunkFieldBuilder B(fields);

    // --- Version -----------------------------------------------------------
    B.Version("Version", data.Version);

    //--- Names ----------------------------------------------------------
    B.Name("MeshName", data.MeshName);
    B.Name("ContainerName", data.ContainerName);

    //--- Raw Attributes -----------------------------------------------
	B.UInt32("Attributes", data.Attributes);


    //--- Geometry type (masked equality) ---------------------------------
    constexpr uint32_t GEOM_MASK = static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_MASK);

    // table of concrete values inside the masked field
    static constexpr std::pair<uint32_t, const char*> geomVals[] = {
        { static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_NORMAL),         "W3D_MESH_FLAG_GEOMETRY_TYPE_NORMAL" },
        { static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_CAMERA_ALIGNED), "W3D_MESH_FLAG_GEOMETRY_TYPE_CAMERA_ALIGNED" },
        { static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_SKIN),           "W3D_MESH_FLAG_GEOMETRY_TYPE_SKIN" },
        { static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_AABOX),          "W3D_MESH_FLAG_GEOMETRY_TYPE_AABOX" },
        { static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_OBBOX),          "W3D_MESH_FLAG_GEOMETRY_TYPE_OBBOX" },
        { static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_CAMERA_ORIENTED), "W3D_MESH_FLAG_GEOMETRY_TYPE_CAMERA_ORIENTED" },
    };

    //--- Collision types ---------------------------------------------
    static constexpr std::pair<MeshAttr, const char*> collTypes[] = {
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_PHYSICAL,   "W3D_MESH_FLAG_COLLISION_TYPE_PHYSICAL" },
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_PROJECTILE, "W3D_MESH_FLAG_COLLISION_TYPE_PROJECTILE" },
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_VIS,        "W3D_MESH_FLAG_COLLISION_TYPE_VIS" },
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_CAMERA,     "W3D_MESH_FLAG_COLLISION_TYPE_CAMERA" },
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_VEHICLE,    "W3D_MESH_FLAG_COLLISION_TYPE_VEHICLE" },
    };

    //--- Other flags -------------------------------------------------
    static constexpr std::pair<MeshAttr, const char*> otherTypes[] = {
        { MeshAttr::W3D_MESH_FLAG_HIDDEN,                         "W3D_MESH_FLAG_HIDDEN" },
        { MeshAttr::W3D_MESH_FLAG_TWO_SIDED,                      "W3D_MESH_FLAG_TWO_SIDED" },
        { MeshAttr::W3D_MESH_FLAG_CAST_SHADOW,                    "W3D_MESH_FLAG_CAST_SHADOW" },
        { MeshAttr::W3D_MESH_FLAG_SHATTERABLE,                    "W3D_MESH_FLAG_SHATTERABLE" },
        { MeshAttr::W3D_MESH_FLAG_NPATCHABLE,                     "W3D_MESH_FLAG_NPATCHABLE" },
        { MeshAttr::W3D_MESH_FLAG_PRELIT_UNLIT,                   "W3D_MESH_FLAG_PRELIT_UNLIT" },
        { MeshAttr::W3D_MESH_FLAG_PRELIT_VERTEX,                  "W3D_MESH_FLAG_PRELIT_VERTEX" },
        { MeshAttr::W3D_MESH_FLAG_PRELIT_LIGHTMAP_MULTI_PASS,     "W3D_MESH_FLAG_PRELIT_LIGHTMAP_MULTI_PASS" },
        { MeshAttr::W3D_MESH_FLAG_PRELIT_LIGHTMAP_MULTI_TEXTURE,  "W3D_MESH_FLAG_PRELIT_LIGHTMAP_MULTI_TEXTURE" },
    };

    // Loop each group:
    uint32_t gsel = attr & GEOM_MASK;
    for (auto [val, name] : geomVals) {
        if (gsel == val) {
            B.Push("Attributes", "flag", name);
            break;
        }
    }
    for (auto [maskEnum, name] : collTypes) {
        B.Flag(attr, uint32_t(maskEnum), name);
    }
    for (auto [maskEnum, name] : otherTypes) {
        B.Flag(attr, uint32_t(maskEnum), name);
    }


    //--- Counts, Sorting & Bounds ------------------------------------
    B.UInt32("NumTris", data.NumTris);
    B.UInt32("NumVertices", data.NumVertices);
    B.UInt32("NumMaterials", data.NumMaterials);
    B.UInt32("NumDamageStages", data.NumDamageStages);

    // SortLevel
    if (data.SortLevel == static_cast<int32_t>(SortLevel::SORT_LEVEL_NONE)) {
        B.Push("SortLevel", "string", "NONE");
    }
    else {
        B.Push("SortLevel", "int32", std::to_string(data.SortLevel));
    }

    // PrelitVersion
    constexpr auto PRELIT_MASK = static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_PRELIT_MASK);
    B.Versioned("PrelitVersion", attr, PRELIT_MASK, data.PrelitVersion);

    // FutureCounts
	B.UInt32("FutureCounts[0]", data.FutureCounts[0]);

    //--- VertexChannels ---------------------------------------------
    uint32_t vc = data.VertexChannels;
    B.UInt32("VertexChannels", vc);

    // for each bit in the enum, if set push a flag
    static constexpr std::pair<VertexChannelAttr, const char*> VCFlags[] = {
        { VertexChannelAttr::W3D_VERTEX_CHANNEL_LOCATION, "W3D_VERTEX_CHANNEL_LOCATION" },
        { VertexChannelAttr::W3D_VERTEX_CHANNEL_NORMAL,   "W3D_VERTEX_CHANNEL_NORMAL"   },
        { VertexChannelAttr::W3D_VERTEX_CHANNEL_TEXCOORD, "W3D_VERTEX_CHANNEL_TEXCOORD" },
        { VertexChannelAttr::W3D_VERTEX_CHANNEL_COLOR,    "W3D_VERTEX_CHANNEL_COLOR"    },
        { VertexChannelAttr::W3D_VERTEX_CHANNEL_BONEID,   "W3D_VERTEX_CHANNEL_BONEID"   },
    };

    for (auto [e, name] : VCFlags) {
        B.Flag(vc, static_cast<uint32_t>(e), name);
    }

    //--- FaceChannels -----------------------------------------------
    B.UInt32("FaceChannels", data.FaceChannels);

    // We only have one right now, but this pattern is extensible
    static constexpr std::pair<uint32_t, const char*> FCFlags[] = {
        { W3D_FACE_CHANNEL_FACE, "W3D_FACE_CHANNEL_FACE" }
    };

    for (auto [mask, name] : FCFlags) {
        B.Flag(data.FaceChannels, mask, name);
    }

    //--- Bounds --------------------------------------------------------
    B.Vec3("Min", data.Min);
    B.Vec3("Max", data.Max);
    B.Vec3("SphCenter", data.SphCenter);
    B.Float("SphRadius", data.SphRadius);

    return fields;
}



inline std::vector<ChunkField> InterpretVertices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed VERTICES chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dVectorStruct>>(parsed);

    
    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < data.size(); ++i) {
        B.Vec3(
            "Vertex[" + std::to_string(i) + "]",
            data[i]
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretVertexNormals(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed Normal chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<W3dVectorStruct>>(parsed);

    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < data.size(); ++i) {
        B.Vec3(
            "Normal[" + std::to_string(i) + "]",
            data[i]
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretTangents(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed TANGENTS chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<W3dVectorStruct>>(parsed);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.Vec3(
            "Tangent[" + std::to_string(i) + "]",
            data[i]
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretBinormals(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed BINORMALS chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<W3dVectorStruct>>(parsed);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.Vec3(
            "Binormal[" + std::to_string(i) + "]",
            data[i]
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretSecondaryVertices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed SECONDARY_VERTICES chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<W3dVectorStruct>>(parsed);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.Vec3(
            "SecondaryVertex[" + std::to_string(i) + "]",
            data[i]
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretSecondaryVertexNormals(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed SECONDARY_VERTEX_NORMALS chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<W3dVectorStruct>>(parsed);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.Vec3(
            "SecondaryNormal[" + std::to_string(i) + "]",
            data[i]
        );
    }

    return fields;
}



//leaving trailing ...
inline std::vector<ChunkField> InterpretMeshUserText(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    ChunkFieldBuilder B(fields);
    
    B.NullTerm(
        "UserText",
        reinterpret_cast<const char*>(chunk->data.data()),
        chunk->data.size()
    );

    return fields;
}



inline std::vector<ChunkField> InterpretVertexInfluences(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dVertInfStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed VERTEX_INFLUENCES chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<W3dVertInfStruct>>(parsed);

    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < data.size(); ++i) {
        const auto& inf = data[i];
        const std::string pfx = "VertexInfluence[" + std::to_string(i) + "]";
        for (int j = 0; j < 2; ++j) {
            B.UInt16(pfx + ".BoneIdx[" + std::to_string(j) + "]", inf.BoneIdx[j]);
            B.UInt16(pfx + ".Weight[" + std::to_string(j) + "]", inf.Weight[j]);
        }
    }

    return fields;
}


inline std::vector<ChunkField> InterpretTriangles(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dTriStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed TRIANGLES chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<W3dTriStruct>>(parsed);

    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < data.size(); ++i) {
        const auto& T = data[i];
        std::string pfx = "Triangle[" + std::to_string(i) + "]";

        // int32[3] array of indices
        B.UInt32Array(pfx + ".VertexIndices", T.Vindex, 3);

        // attributes
        B.UInt32(pfx + ".Attributes", T.Attributes);

        // normal
        B.Vec3(pfx + ".Normal", T.Normal);

        // dist
        B.Float(pfx + ".Dist", T.Dist);
    }

    return fields;
}

inline std::vector<ChunkField> InterpretVertexShadeIndices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<uint32_t>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed VERTEX_SHADE_INDICES chunk: " + *err);
        return fields;
    }

    auto data = std::get<std::vector<uint32_t>>(parsed);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.UInt32("Index[" + std::to_string(i) + "]", static_cast<uint32_t>(data[i]));
    }

    return fields;
}

inline std::vector<ChunkField> InterpretMaterialInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto buff = ParseChunkStruct<W3dMaterialInfoStruct>(chunk);
    if (auto err = std::get_if<std::string>(&buff)) {
        fields.emplace_back("error", "string", "Malformed Material Info chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<W3dMaterialInfoStruct>(buff);

    ChunkFieldBuilder B(fields);

   
    B.UInt32("PassCount", data.PassCount);
    B.UInt32("VertexMaterialCount", data.VertexMaterialCount);
    B.UInt32("ShaderCount", data.ShaderCount);
    B.UInt32("TextureCount", data.TextureCount);

    return fields;
}

inline std::vector<ChunkField> InterpretVertexMaterialName(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    ChunkFieldBuilder B(fields);
    // read a null-terminated string out of chunk->data (up to chunk size)
    B.NullTerm(
        "MaterialName",
        reinterpret_cast<const char*>(chunk->data.data()),
        chunk->data.size()
    );

    return fields;
}

inline std::vector<ChunkField> InterpretVertexMaterialInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto buff = ParseChunkStruct<W3dVertexMaterialStruct>(chunk);
    if (auto err = std::get_if<std::string>(&buff)) {
        fields.emplace_back("error", "string", "Malformed Material Info chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<W3dVertexMaterialStruct>(buff);

    ChunkFieldBuilder B(fields);

    const uint32_t attr = data.Attributes;

    // Raw attributes first (handy for debugging)
    B.UInt32("Material.Attributes", attr);

    // Basic low-bit flags
    for (auto [mask, name] : VERTMAT_BASIC_FLAGS) {
        B.Flag(attr, mask, name);
    }

    // Stage mappings are full bytes now:
    {
        uint8_t s0 = ExtractStageMapping(attr, /*stage*/0);
        uint8_t s1 = ExtractStageMapping(attr, /*stage*/1);

        B.UInt8("Material.Stage0Mapping.Code", s0);
        B.Push("Material.Stage0Mapping.Name", "flag", StageMappingName(s0, 0));

        B.UInt8("Material.Stage1Mapping.Code", s1);
        B.Push("Material.Stage1Mapping.Name", "flag", StageMappingName(s1, 1));
    }

    // (Optional) PSX transparency flags:
    // These collide with Stage0’s mapping byte; only decode if you KNOW you’re on that platform/asset.
    // if (attr & 0x00F00000u) {
    //     for (auto [mask, name] : VERTMAT_PSX_FLAGS) {
    //         B.Flag(attr, mask, name);
    //     }
    // }

    // Colors & floats
    B.RGB("Material.Ambient", data.Ambient.R, data.Ambient.G, data.Ambient.B);
    B.RGB("Material.Diffuse", data.Diffuse.R, data.Diffuse.G, data.Diffuse.B);
    B.RGB("Material.Specular", data.Specular.R, data.Specular.G, data.Specular.B);
    B.RGB("Material.Emissive", data.Emissive.R, data.Emissive.G, data.Emissive.B);
    B.Float("Material.Shininess", data.Shininess);
    B.Float("Material.Opacity", data.Opacity);
    B.Float("Material.Translucency", data.Translucency);

    return fields;
}



inline std::vector<ChunkField> InterpretShaders(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;


    auto parsed = ParseChunkArray<W3dShaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) { 
        fields.emplace_back("error", "string", "Malformed SHADERS chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dShaderStruct>>(parsed);


    ChunkFieldBuilder B(fields);
   

    for (size_t i = 0; i < data.size(); ++i) {
        const auto& s = data[i];
        const std::string pfx = "Shader[" + std::to_string(i) + "]";

        B.DepthCompareField(pfx + ".DepthCompare", s.DepthCompare);
        B.DepthMaskField(pfx + ".DepthMask", s.DepthMask);
        B.DestBlendField(pfx + ".DestBlend", s.DestBlend);
        B.PriGradientField(pfx + ".PriGradient", s.PriGradient);
        B.SecGradientField(pfx + ".SecGradient", s.SecGradient);
        B.SrcBlendField(pfx + ".SrcBlend", s.SrcBlend);
        B.TexturingField(pfx + ".Texturing", s.Texturing);
        B.DetailColorFuncField(pfx + ".DetailColorFunc", s.DetailColorFunc);
        B.DetailAlphaFuncField(pfx + ".DetailAlphaFunc", s.DetailAlphaFunc);
        B.AlphaTestField(pfx + ".AlphaTest", s.AlphaTest);
        B.DetailColorFuncField(pfx + ".PostDetailColorFunc", s.PostDetailColorFunc);
        B.DetailAlphaFuncField(pfx + ".PostDetailAlphaFunc", s.PostDetailAlphaFunc);
        // we ignore the 1-byte pad and any now-obsolete slots
    }

    return fields;
}





inline std::vector<ChunkField> InterpretARG0(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    ChunkFieldBuilder B(fields);
    // read a null-terminated string out of chunk->data (up to chunk size)
    B.NullTerm(
        "Stage0 Mapper Args",
        reinterpret_cast<const char*>(chunk->data.data()),
        chunk->data.size()
    );

    return fields;
}


inline std::vector<ChunkField> InterpretARG1(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    ChunkFieldBuilder B(fields);
    // read a null-terminated string out of chunk->data (up to chunk size)
    B.NullTerm(
        "Stage1 Mapper Args",
        reinterpret_cast<const char*>(chunk->data.data()),
        chunk->data.size()
    );

    return fields;
}

inline std::vector<ChunkField> InterpretTextureName(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    ChunkFieldBuilder B(fields);
    // read a null-terminated string out of chunk->data (up to chunk size)
    B.NullTerm(
        "Texture Name:",
        reinterpret_cast<const char*>(chunk->data.data()),
        chunk->data.size()
    );

    return fields;
}


inline std::vector<ChunkField> InterpretTextureInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto buff = ParseChunkStruct<W3dTextureInfoStruct>(chunk);
    if (auto err = std::get_if<std::string>(&buff)) {
        fields.emplace_back("error", "string", "Malformed Texture_Info chunk: " + *err);
        return fields;
    }
    const auto& ti = std::get<W3dTextureInfoStruct>(buff);

    ChunkFieldBuilder B(fields);

    const uint16_t attr16 = ti.Attributes;
    const uint32_t attr = static_cast<uint32_t>(attr16);

    // Raw attributes
    B.UInt16("Texture.Attributes", attr16);


    // Simple one-bit flags (publish, no_lod, clamp_u/v, alpha_bitmap, etc.)
    
    B.TextureFlag("Texture.Attributes", attr, static_cast<uint32_t>(TextureAttr::PUBLISH), "W3DTEXTURE_PUBLISH");
    B.TextureFlag("Texture.Attributes", attr, static_cast<uint32_t>(TextureAttr::RESIZE_OBSOLETE), "W3DTEXTURE_RESIZE_OBSOLETE");
    B.TextureFlag("Texture.Attributes", attr, static_cast<uint32_t>(TextureAttr::NO_LOD), "W3DTEXTURE_NO_LOD");
    B.TextureFlag("Texture.Attributes", attr, static_cast<uint32_t>(TextureAttr::CLAMP_U), "W3DTEXTURE_CLAMP_U");
    B.TextureFlag("Texture.Attributes", attr, static_cast<uint32_t>(TextureAttr::CLAMP_V), "W3DTEXTURE_CLAMP_V");
    B.TextureFlag("Texture.Attributes", attr, static_cast<uint32_t>(TextureAttr::ALPHA_BITMAP), "W3DTEXTURE_ALPHA_BITMAP");

    // MIP levels (masked equality on MIP_MASK)
    {
        constexpr uint16_t MIP_MASK = static_cast<uint16_t>(TextureAttr::MIP_MASK);
        const uint16_t mip = static_cast<uint16_t>(attr16 & MIP_MASK);
 
        if (mip == static_cast<uint16_t>(TextureAttr::MIP_ALL)) {
            B.Push("Texture.Attributes", "flag", "W3DTEXTURE_MIP_LEVELS_ALL");
        }
        else if (mip == static_cast<uint16_t>(TextureAttr::MIP_2)) {
            B.Push("Texture.Attributes", "flag", "W3DTEXTURE_MIP_LEVELS_2");
        }
        else if (mip == static_cast<uint16_t>(TextureAttr::MIP_3)) {
            B.Push("Texture.Attributes", "flag", "W3DTEXTURE_MIP_LEVELS_3");
        }
        else if (mip == static_cast<uint16_t>(TextureAttr::MIP_4)) {
            B.Push("Texture.Attributes", "flag", "W3DTEXTURE_MIP_LEVELS_4");
        }
    }

    // Hint (masked equality).
    {
        constexpr uint16_t HINT_MASK = static_cast<uint16_t>(TextureAttr::HINT_MASK);
        const uint16_t hint = static_cast<uint16_t>(attr16 & HINT_MASK);
       
        if (hint == static_cast<uint16_t>(TextureAttr::HINT_BASE)) {
            B.Push("Texture.Attributes", "flag", "W3DTEXTURE_HINT_BASE");
        }
        else if (hint == static_cast<uint16_t>(TextureAttr::HINT_EMISSIVE)) {
            B.Push("Texture.Attributes", "flag", "W3DTEXTURE_HINT_EMISSIVE");
        }
        else if (hint == static_cast<uint16_t>(TextureAttr::HINT_ENVIRONMENT)) {
            B.Push("Texture.Attributes", "flag", "W3DTEXTURE_HINT_ENVIRONMENT");
        }
        else if (hint == static_cast<uint16_t>(TextureAttr::HINT_SHINY_MASK)) {
            B.Push("Texture.Attributes", "flag", "W3DTEXTURE_HINT_SHINY_MASK");
        }
        else if (hint != 0) {
            B.Push("Texture.Attributes", "string", "Unknown");
        }
    }

    // Type (bit test)
    {
        constexpr uint16_t TYPE_MASK = static_cast<uint16_t>(TextureAttr::TYPE_MASK);
        const uint16_t type = attr16 & TYPE_MASK;
       
        if (type != 0) {
            B.Push("Texture.Attributes", "flag", "W3DTEXTURE_TYPE_BUMPMAP");
        }
        else {
            B.Push("Texture.Attributes", "flag", "W3DTEXTURE_TYPE_COLORMAP");
        }
    }
    

    // Unknown high bits
    if ((attr & 0xE000u) != 0) {
        B.Push("Texture.Attributes", "string", "Unknown");
    }


    // Anim type 
    B.UInt16("Texture.AnimType", ti.AnimType);
    switch (static_cast<TextureAttr>(ti.AnimType)) {
    case TextureAttr::ANIM_LOOP:     B.Push("AnimType", "flag", "W3DTEXTURE_ANIM_LOOP"); break;
    case TextureAttr::ANIM_PINGPONG: B.Push("AnimType", "flag", "W3DTEXTURE_ANIM_PINGPONG"); break;
    case TextureAttr::ANIM_ONCE:     B.Push("AnimType", "flag", "W3DTEXTURE_ANIM_ONCE"); break;
    case TextureAttr::ANIM_MANUAL:   B.Push("AnimType", "flag", "W3DTEXTURE_ANIM_MANUAL"); break;
    default:                         B.Push("AnimType", "string", "Unknown"); break;
    }

    // Frame params
    B.UInt32("Texture.FrameCount", ti.FrameCount);
    B.Float("Texture.FrameRate", ti.FrameRate);

    return fields;
}


inline std::vector<ChunkField> InterpretVertexMaterialIDs(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<uint32_t>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string",
            "Malformed VERTEX_SHADE_INDICES chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<uint32_t>>(parsed);


    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.UInt32("Vertex[" + std::to_string(i) + "] Vertex Material Index", static_cast<uint32_t>(data[i]));
    }
    return fields;
}

inline std::vector<ChunkField> InterpretShaderIDs(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<uint32_t>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string",
            "Malformed Shader_Index chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<uint32_t>>(parsed);


    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.UInt32("Face[" + std::to_string(i) + "] Shader Index", static_cast<uint32_t>(data[i]));
    }
    return fields;
}

inline std::vector<ChunkField> InterpretShaderMaterialId(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<uint32_t>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string",
            "Malformed Shader_Material_ID chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<uint32_t>>(parsed);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.UInt32("Face[" + std::to_string(i) + "] FX Shader Index", static_cast<uint32_t>(data[i]));
    }
    return fields;
}

inline std::vector<ChunkField> InterpretDCG(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dRGBAStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed DCG chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dRGBAStruct>>(parsed);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        const auto& c = data[i];
        B.RGBA("Vertex[" + std::to_string(i) + "].DCG", c.R, c.G, c.B, c.A);
    }
    return fields;
}
//TODO: Either this is never used or I'm unable to parse it.
inline std::vector<ChunkField> InterpretDIG(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dRGBStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed DIG chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dRGBStruct>>(parsed);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        const auto& c = data[i];
        B.RGB("Vertex[" + std::to_string(i) + "].DIG", c.R, c.G, c.B );
    }
    return fields;
}
//TODO: Either this is never used or I'm unable to parse it.
inline std::vector<ChunkField> InterpretSCG(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dRGBStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed SCG chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dRGBStruct>>(parsed);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        const auto& c = data[i];
        B.RGB("Vertex[" + std::to_string(i) + "].SCG", c.R, c.G, c.B);
    }
    return fields;
}


inline std::vector<ChunkField> InterpretTextureIDs(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<uint32_t>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed Texture_Index chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<uint32_t>>(parsed);

    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < data.size(); ++i) {
        B.UInt32(
            "Face[" + std::to_string(i) + "] Texture Index", static_cast<uint32_t>(data[i]));
    }

    return fields;
}


inline std::vector<ChunkField> InterpretStageTexCoords(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dTexCoordStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed TEXCOORDS chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<W3dTexCoordStruct>>(parsed);

    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < data.size(); ++i) {
        B.TexCoord(
            "Vertex[" + std::to_string(i) + "].UV", data[i]);
    }

    return fields;
}

//TODO: Either this is never used or I'm unable to parse it.
inline std::vector<ChunkField> InterpretPerFaceTexcoordIds(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<Vector3i>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed PER_FACE_TEX_COORD_IDs chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<Vector3i>>(parsed);

    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < data.size(); ++i) {
        B.Vec3i(
            "Face[" + std::to_string(i) + "].UVIndices", data[i]);
    }

    return fields;
}


inline std::vector<ChunkField> InterpretShaderMaterialHeader(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkStruct<W3dShaderMaterialHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string",
            "Malformed Shader_Material_Header chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<W3dShaderMaterialHeaderStruct>(parsed);

    ChunkFieldBuilder B(fields);
    B.UInt8("Version", data.Version);
    B.Name("ShaderName", data.ShaderName, 32);
    B.UInt8("Technique", data.Technique);
  //  B.UInt8("Padding[0]", data.Padding[0]);
  //  B.UInt8("Padding[1]", data.Padding[1]);
  //  B.UInt8("Padding[2]", data.Padding[2]);

    return fields;
}


inline uint32_t ReadLEU32(const uint8_t* p) {
    uint32_t v; std::memcpy(&v, p, 4); return v;
}
inline float ReadLEF32(const uint8_t* p) {
    float f; std::memcpy(&f, p, 4); return f;
}
inline std::string CleanCString(const char* p, size_t maxlen) {
    // Respect first NUL and strip trailing control bytes (defensive)
    size_t n = 0;
    while (n < maxlen && p[n] != '\0') ++n;
    std::string s(p, p + n);
    while (!s.empty() && static_cast<unsigned char>(s.back()) < 0x20) s.pop_back();
    return s;
}

inline std::vector<ChunkField> InterpretShaderMaterialProperty(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    if (buf.size() < 8) {
        fields.emplace_back("error", "string",
            "Malformed FX_SHADER_CONSTANT: size < 8");
        return fields;
    }

    const uint8_t* base = buf.data();
    uint32_t type = ReadLEU32(base + 0);
    uint32_t nameLen = ReadLEU32(base + 4);

    if (buf.size() < 8ull + nameLen) {
        fields.emplace_back("error", "string",
            "Malformed FX_SHADER_CONSTANT: nameLen exceeds chunk size");
        return fields;
    }

    // Read constant name as C-string (trim at first NUL)
    std::string name = CleanCString(reinterpret_cast<const char*>(base + 8), nameLen);

    ChunkFieldBuilder B(fields);
    B.UInt32("Type", type);
    switch (static_cast<ShaderMaterialFlag>(type)) {
    case ShaderMaterialFlag::CONSTANT_TYPE_TEXTURE: B.Push("TypeName", "flag", "CONSTANT_TYPE_TEXTURE"); break;
    case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT1:  B.Push("TypeName", "flag", "CONSTANT_TYPE_FLOAT1");  break;
    case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT2:  B.Push("TypeName", "flag", "CONSTANT_TYPE_FLOAT2");  break;
    case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT3:  B.Push("TypeName", "flag", "CONSTANT_TYPE_FLOAT3");  break;
    case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT4:  B.Push("TypeName", "flag", "CONSTANT_TYPE_FLOAT4");  break;
    case ShaderMaterialFlag::CONSTANT_TYPE_INT:     B.Push("TypeName", "flag", "CONSTANT_TYPE_INT");     break;
    case ShaderMaterialFlag::CONSTANT_TYPE_BOOL:    B.Push("TypeName", "flag", "CONSTANT_TYPE_BOOL");    break;
    default: break;
    }
    B.UInt32("NameLength", nameLen);
    B.Push("ConstantName", "string", name);

    size_t pos = 8 + static_cast<size_t>(nameLen);

    // Parse payload like wdump
    switch (static_cast<ShaderMaterialFlag>(type)) {
    case ShaderMaterialFlag::CONSTANT_TYPE_TEXTURE: {
        if (buf.size() < pos + 4) {
            B.Push("error", "string", "Truncated: missing texture length");
            break;
        }
        uint32_t texLen = ReadLEU32(base + pos);
        pos += 4;
        if (buf.size() < pos + texLen) {
            B.Push("error", "string", "Truncated: texture length exceeds chunk");
            break;
        }
        std::string tex = CleanCString(reinterpret_cast<const char*>(base + pos), texLen);
        B.UInt32("TextureLength", texLen);
        B.Push("Texture", "string", tex);
        break;
    }

    case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT1:
    case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT2:
    case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT3:
    case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT4: {
        int count = static_cast<int>(type) - 1; // matches wdump
        size_t need = static_cast<size_t>(count) * 4;
        if (buf.size() < pos + need) {
            B.Push("error", "string", "Truncated: not enough float payload");
            break;
        }
        for (int i = 0; i < count; ++i) {
            float f = ReadLEF32(base + pos + i * 4);
            B.Float("Floats[" + std::to_string(i) + "]", f);
        }
        break;
    }

    case ShaderMaterialFlag::CONSTANT_TYPE_INT: {
        if (buf.size() < pos + 4) {
            B.Push("error", "string", "Truncated: missing int payload");
            break;
        }
        uint32_t v = ReadLEU32(base + pos);
        B.UInt32("Int", v);
        break;
    }

    case ShaderMaterialFlag::CONSTANT_TYPE_BOOL: {
        if (buf.size() < pos + 1) {
            B.Push("error", "string", "Truncated: missing bool payload");
            break;
        }
        uint8_t v = *(base + pos);
        B.UInt8("Bool", v);
        break;
    }

    default:
        B.Push("note", "string", "Unknown constant type; payload not parsed");
        break;
    }

    return fields;
}


inline std::vector<ChunkField> InterpretDeform(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto buff = ParseChunkStruct<W3dMeshDeform>(chunk);
    if (auto err = std::get_if<std::string>(&buff)) {
        fields.emplace_back("error", "string", *err);
        return fields;
    }

    const auto& data = std::get<W3dMeshDeform>(buff);


    ChunkFieldBuilder B(fields);

    B.UInt32("SetCount", data.SetCount);
    B.UInt32("AlphaPasses", data.AlphaPasses);

    return fields;
}

//TODO: Either this is never used or I'm unable to parse it.
inline std::vector<ChunkField> InterpretDeformSet(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dDeformSetInfo>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed DEFORM_SET chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<W3dDeformSetInfo>>(parsed);

    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < data.size(); ++i) {
        const auto& ds = data[i];
        const std::string pfx = "DeformSet[" + std::to_string(i) + "]";

        B.UInt32(pfx + ".KeyframeCount", ds.KeyframeCount);
        B.UInt32(pfx + ".Flags", ds.flags);
    }

    return fields;
}

//TODO: Either this is never used or I'm unable to parse it.
inline std::vector<ChunkField> InterpretDeformKeyframes(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dDeformKeyframeInfo>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed DEFORM_KEYFRAME chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<W3dDeformKeyframeInfo>>(parsed);

    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < data.size(); ++i) {
        const auto& df = data[i];
        const std::string pfx = "DeformFrame[" + std::to_string(i) + "]";

        B.Float(pfx + ".DeformPercent", df.DeformPercent);
        B.UInt32(pfx + ".DataCount", df.DataCount);
    }

    return fields;
}

//TODO: Either this is never used or I'm unable to parse it.
inline std::vector<ChunkField> InterpretDeformData(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dDeformData>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed DEFORM_DATA chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<std::vector<W3dDeformData>>(parsed);

    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < data.size(); ++i) {
        const auto& dd = data[i];
        const std::string pfx = "DeformData[" + std::to_string(i) + "]";

        B.UInt32(pfx + ".VertexIndex", dd.VertexIndex);
        B.Vec3(pfx + ".Position", dd.Position);
        B.RGBA(pfx + ".Color", dd.Color.R, dd.Color.G, dd.Color.B, dd.Color.A);
    }

    return fields;
}

inline std::vector<ChunkField> InterpretPS2Shaders(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;


    auto parsed = ParseChunkArray<W3dPS2ShaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed PS2_SHADERS chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<W3dPS2ShaderStruct>>(parsed);


    ChunkFieldBuilder B(fields);
    

    for (size_t i = 0; i < data.size(); ++i) {
        const auto& s = data[i];
        const std::string pfx = "Shader[" + std::to_string(i) + "]";

        B.Ps2DepthCompareField(pfx + ".DepthCompare", s.DepthCompare);
        B.Ps2DepthMaskField(pfx + ".DepthMask", s.DepthMask);
        B.Ps2PriGradientField(pfx + ".PriGradient", s.PriGradient);
        B.Ps2TexturingField(pfx + ".Texturing", s.Texturing);
        B.Ps2AlphaTestField(pfx + ".AlphaTest", s.AlphaTest);
        B.UInt8(pfx + ".AParam", s.AParam);
        B.UInt8(pfx + ".BParam", s.BParam);
        B.UInt8(pfx + ".CParam", s.CParam);
        B.UInt8(pfx + ".DParam", s.DParam);
        // we ignore the padding
    }

    return fields;
}


inline std::vector<ChunkField> InterpretAABTreeHeader(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto buff = ParseChunkStruct<W3dMeshAABTreeHeader>(chunk);
    if (auto err = std::get_if<std::string>(&buff)) {
        fields.emplace_back("error", "string", "Malformed AABB_HEADER chunk: " + *err);
        return fields;
    }

    const auto& data = std::get<W3dMeshAABTreeHeader>(buff);


    ChunkFieldBuilder B(fields);

    B.UInt32("NodeCount", data.NodeCount);
    B.UInt32("PolyCount", data.PolyCount);

    return fields;
}


inline std::vector<ChunkField> InterpretAABTreePolyIndices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<uint32_t>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed AAB_TREE_POLY_INDICES chunk: " + *err);
        return fields;
    }
    const auto& data = std::get<std::vector<uint32_t>>(parsed);

    ChunkFieldBuilder B(fields);
    

    for (size_t i = 0; i < data.size(); ++i) {
        const uint32_t a = data[i];
        const std::string name = "PolygonIndex[" + std::to_string(i) + "]";
        B.UInt32(name, a);
    }

    return fields;
}




inline std::vector<ChunkField> InterpretAABTreeNodes(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dMeshAABTreeNode>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed AAB_TREE_NODE chunk: " + *err);
        return fields;
    }
    const auto& nodes = std::get<std::vector<W3dMeshAABTreeNode>>(parsed);

    ChunkFieldBuilder B(fields);
    B.UInt32("Count", static_cast<uint32_t>(nodes.size()));

    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        const std::string pfx = "Node[" + std::to_string(i) + "]";

        // bounds
        B.Vec3(pfx + ".Min", n.Min);
        B.Vec3(pfx + ".Max", n.Max);

        // decode leaf/internal by top bit of FrontOrPoly0
        const uint32_t frontRaw = static_cast<uint32_t>(n.FrontOrPoly0);
        const uint32_t backRaw = static_cast<uint32_t>(n.BackOrPolyCount);
        const bool isLeaf = (frontRaw & 0x80000000u) != 0;

        if (!isLeaf) {
            // internal node: children indices
            B.UInt32(pfx + ".Front", frontRaw);
            B.UInt32(pfx + ".Back", backRaw);
        }
        else {
            // leaf node: polygon range
            B.UInt32(pfx + ".Poly0", (frontRaw & 0x7FFFFFFFu));
            B.UInt32(pfx + ".PolyCount", backRaw);
        }
    }

    return fields;
}


//TODO: Either this is never used or I'm unable to parse it.
inline std::vector<ChunkField> InterpretLightMapUV(const std::shared_ptr<ChunkItem>&) {
    return Undefined("InterpretLightMapUV");
}



