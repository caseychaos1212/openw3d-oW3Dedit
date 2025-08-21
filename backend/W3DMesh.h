#pragma once
#include "W3DStructs.h"
#include "FormatUtils.h"
#include "ParseUtils.h"

inline std::vector<ChunkField> InterpretMeshHeader3(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    if (buf.size() < sizeof(W3dMeshHeader3Struct)) {
        fields.emplace_back("error", "string", "Header too small");
        return fields;
    }

    const auto* hdr = reinterpret_cast<const W3dMeshHeader3Struct*>(chunk->data.data());
    uint32_t attr = hdr->Attributes;

    ChunkFieldBuilder B(fields);

    // --- Version -----------------------------------------------------------
    B.Version("Version", hdr->Version);

    //--- Names ----------------------------------------------------------
    B.Name("MeshName", hdr->MeshName);
    B.Name("ContainerName", hdr->ContainerName);

    //--- Raw Attributes -----------------------------------------------
	B.UInt32("Attributes", hdr->Attributes);


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
    B.UInt32("NumTris", hdr->NumTris);
    B.UInt32("NumVertices", hdr->NumVertices);
    B.UInt32("NumMaterials", hdr->NumMaterials);
    B.UInt32("NumDamageStages", hdr->NumDamageStages);

    // SortLevel
    if (hdr->SortLevel == static_cast<int32_t>(SortLevel::SORT_LEVEL_NONE)) {
        B.Push("SortLevel", "string", "NONE");
    }
    else {
        B.Push("SortLevel", "int32", std::to_string(hdr->SortLevel));
    }

    // PrelitVersion
    constexpr auto PRELIT_MASK = static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_PRELIT_MASK);
    B.Versioned("PrelitVersion", attr, PRELIT_MASK, hdr->PrelitVersion);

    // FutureCounts
	B.UInt32("FutureCounts[0]", hdr->FutureCounts[0]);

    //--- VertexChannels ---------------------------------------------
    uint32_t vc = hdr->VertexChannels;
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
    B.UInt32("FaceChannels", hdr->FaceChannels);

    // We only have one right now, but this pattern is extensible
    static constexpr std::pair<uint32_t, const char*> FCFlags[] = {
        { W3D_FACE_CHANNEL_FACE, "W3D_FACE_CHANNEL_FACE" }
    };

    for (auto [mask, name] : FCFlags) {
        B.Flag(hdr->FaceChannels, mask, name);
    }

    //--- Bounds --------------------------------------------------------
    B.Vec3("Min", hdr->Min);
    B.Vec3("Max", hdr->Max);
    B.Vec3("SphCenter", hdr->SphCenter);
    B.Float("SphRadius", hdr->SphRadius);

    return fields;
}



inline std::vector<ChunkField> InterpretVertices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string",
            "Malformed VERTICES chunk: " + *err);
        return fields;
    }

    auto verts = std::get<std::span<const W3dVectorStruct>>(parsed);
    
    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < verts.size(); ++i) {
        B.Vec3(
            "Vertex[" + std::to_string(i) + "]",
            verts[i]
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretVertexNormals(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dVectorStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string",
            "Malformed Normal chunk: " + *err);
        return fields;
    }

    auto verts = std::get<std::span<const W3dVectorStruct>>(parsed);

    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < verts.size(); ++i) {
        B.Vec3(
            "Normal[" + std::to_string(i) + "]",
            verts[i]
        );
    }

    return fields;
}

//TODO: TEST

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
        fields.emplace_back("error", "string",
            "Malformed VERTEX_INFLUENCES chunk: " + *err);
        return fields;
    }

    auto infs = std::get<std::span<const W3dVertInfStruct>>(parsed);

    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < infs.size(); ++i) {
        const auto& inf = infs[i];
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
        fields.emplace_back("error", "string",
            "Malformed TRIANGLES chunk: " + *err);
        return fields;
    }

    auto tris = std::get<std::span<const W3dTriStruct>>(parsed);

    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < tris.size(); ++i) {
        const auto& T = tris[i];
        std::string pfx = "Triangle[" + std::to_string(i) + "]";

        // int32[3] array of indices
        B.UInt32Array(pfx + ".Vertexindices", T.Vindex, 3);

        // attributes
        B.UInt32(pfx + ".Attributes", T.Attributes);

        // normal
        B.Vec3(pfx + ".Normal", T.Normal);

        // dist
        // BUG: Something is off here example: 0.441293 reads as 1054994752.000000
        B.Float(pfx + ".Dist", T.Dist);
    }

    return fields;
}

inline std::vector<ChunkField> InterpretVertexShadeIndices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkArray<uint32_t>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string",
            "Malformed VERTEX_SHADE_INDICES chunk: " + *err);
        return fields;
    }

    auto data = std::get<std::span<const uint32_t>>(parsed);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < data.size(); ++i) {
        B.UInt32("Index[" + std::to_string(i) + "]", static_cast<uint32_t>(data[i]));
    }

    return fields;
}

inline std::vector<ChunkField> InterpretMaterialInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;

    if (buf.size() < sizeof(W3dMaterialInfoStruct)) {
        fields.emplace_back(
            "error", "string",
            "Malformed MATERIAL_INFO chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    auto const* hdr = reinterpret_cast<const W3dMaterialInfoStruct*>(buf.data());
    ChunkFieldBuilder B(fields);

   
    B.UInt32("PassCount", hdr->PassCount);
    B.UInt32("VertexMaterialCount", hdr->VertexMaterialCount);
    B.UInt32("ShaderCount", hdr->ShaderCount);
    B.UInt32("TextureCount", hdr->TextureCount);

    return fields;
}

inline std::vector<ChunkField> InterpretVertexMaterialName(
    const std::shared_ptr<ChunkItem>& chunk
) {
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

inline std::vector<ChunkField> InterpretVertexMaterialInfo(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dVertexMaterialStruct);
    if (buf.size() < REC) {
        fields.emplace_back("error", "string", "Material info too short");
        return fields;
    }

    auto vm = reinterpret_cast<const W3dVertexMaterialStruct*>(buf.data());
    ChunkFieldBuilder B(fields);

    uint32_t attr = vm->Attributes;
    

    // basic flags
    for (auto [mask, name] : VERTMAT_BASIC_FLAGS) {
        B.Flag(attr, mask, name);
    }

    // stage mappings (4-bit indices at shifts 8 and 12)
    for (int stage = 0; stage < 2; ++stage) {
        uint32_t shift = 8 + 4 * stage;
        uint8_t idx = (attr >> shift) & 0xF;
        if (idx < VERTMAT_STAGE_MAPPING.size()) {
            std::string s(VERTMAT_STAGE_MAPPING[idx].second);     // make a string
            if (auto pos = s.find('?'); pos != std::string::npos)  // swap '?' for stage index
                s[pos] = char('0' + stage);
            B.Push("Material.Attributes", "flag", std::move(s));
        }
    }
    B.UInt32("Material.Attributes", attr);
    // PSX flags
    for (auto [mask, name] : VERTMAT_PSX_FLAGS) {
        B.Flag(attr, mask, name);
        if (mask == 0x0010'0000) break; // NO_RT_LIGHTING early‐out
    }

    // finally the colors & floats
    B.RGB("Material.Ambient", vm->Ambient.R, vm->Ambient.G, vm->Ambient.B);
    B.RGB("Material.Diffuse", vm->Diffuse.R, vm->Diffuse.G, vm->Diffuse.B);
    B.RGB("Material.Specular", vm->Specular.R, vm->Specular.G, vm->Specular.B);
    B.RGB("Material.Emissive", vm->Emissive.R, vm->Emissive.G, vm->Emissive.B);
    B.Float("Material.Shininess", vm->Shininess);
    B.Float("Material.Opacity", vm->Opacity);
    B.Float("Material.Translucency", vm->Translucency);

    return fields;
}





inline std::vector<ChunkField> InterpretShaders(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dShaderStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed SHADERS chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    auto shaders = reinterpret_cast<const W3dShaderStruct*>(buf.data());
    size_t count = buf.size() / REC;

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < count; ++i) {
        const auto& s = shaders[i];
        std::string pfx = "Shader[" + std::to_string(i) + "]";

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




//TODO: TEST
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

//TODO: TEST
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

inline std::vector<ChunkField> InterpretTextureInfo(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    if (buf.size() < sizeof(W3dTextureInfoStruct)) {
        fields.emplace_back("error", "string", "Malformed TEXTURE_INFO chunk; size=" + std::to_string(buf.size()));
        return fields;
    }

    const auto* info = reinterpret_cast<const W3dTextureInfoStruct*>(buf.data());
    ChunkFieldBuilder B(fields);

    // Attributes (raw)
    if constexpr (requires { B.UInt16(std::string{}, uint16_t{}); }) {
        B.UInt16("Texture.Attributes", info->Attributes);
    }
    else {
        B.Push("Texture.Attributes", "uint16", std::to_string(info->Attributes));
    }

    // Attribute flags
    for (auto [flag, name] : TextureAttrNames) {
        auto mask = static_cast<uint16_t>(flag);
        if (info->Attributes & mask) {
            B.Push("Texture.Attributes", "flag", std::string(name));
        }
    }

    // Anim type
    B.Push("Texture.AnimType", "string", std::to_string(info->AnimType));


    // Frame count / rate
    B.UInt32("Texture.FrameCount", info->FrameCount);
    B.Float("Texture.FrameRate", info->FrameRate);

    return fields;
}


inline std::vector<ChunkField> InterpretVertexMaterialIDs(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    if (buf.size() % sizeof(int32_t) != 0) {
        fields.emplace_back("error", "string",
            "Malformed VERTEX_MATERIAL_IDS chunk; size=" + std::to_string(buf.size()));
        return fields;
    }

    const auto* ids = reinterpret_cast<const int32_t*>(buf.data());
    const size_t count = buf.size() / sizeof(int32_t);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < count; ++i) {
        B.UInt32("Vertex[" + std::to_string(i) + "] Vertex Material Index", ids[i]);
    }
    return fields;
}

inline std::vector<ChunkField> InterpretShaderIDs(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    if (buf.size() % sizeof(int32_t) != 0) {
        fields.emplace_back("error", "string",
            "Malformed Shader_Index chunk; size=" + std::to_string(buf.size()));
        return fields;
    }

    const auto* ids = reinterpret_cast<const int32_t*>(buf.data());
    const size_t count = buf.size() / sizeof(int32_t);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < count; ++i) {
        B.UInt32("Face[" + std::to_string(i) + "] Shader Index", ids[i]);
    }
    return fields;
}
// TODO: TEST
inline std::vector<ChunkField> InterpretDCG(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dRGBAStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back("error", "string", "Malformed DCG chunk; size=" + std::to_string(buf.size()));
        return fields;
    }

    const auto* arr = reinterpret_cast<const W3dRGBAStruct*>(buf.data());
    const size_t count = buf.size() / REC;

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < count; ++i) {
        B.RGBA("Vertex[" + std::to_string(i) + "].DCG",
            arr[i].R, arr[i].G, arr[i].B, arr[i].A);
    }
    return fields;
}
// TODO: TEST
inline std::vector<ChunkField> InterpretDIG(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dRGBStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back("error", "string", "Malformed DIG chunk; size=" + std::to_string(buf.size()));
        return fields;
    }

    const auto* arr = reinterpret_cast<const W3dRGBStruct*>(buf.data());
    const size_t count = buf.size() / REC;

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < count; ++i) {
        B.RGB("Vertex[" + std::to_string(i) + "].DIG",
            arr[i].R, arr[i].G, arr[i].B);
    }
    return fields;
}
// TODO: TEST
inline std::vector<ChunkField> InterpretSCG(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dRGBStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back("error", "string", "Malformed SCG chunk; size=" + std::to_string(buf.size()));
        return fields;
    }

    const auto* arr = reinterpret_cast<const W3dRGBStruct*>(buf.data());
    const size_t count = buf.size() / REC;

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < count; ++i) {
        B.RGB("Vertex[" + std::to_string(i) + "].SCG",
            arr[i].R, arr[i].G, arr[i].B);
    }
    return fields;
}
// TODO: TEST
inline std::vector<ChunkField> InterpretTextureIDs(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    if (buf.size() % sizeof(int32_t) != 0) {
        fields.emplace_back("error", "string",
            "Malformed Texture_Index chunk; size=" + std::to_string(buf.size()));
        return fields;
    }

    const auto* ids = reinterpret_cast<const int32_t*>(buf.data());
    const size_t count = buf.size() / sizeof(int32_t);

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < count; ++i) {
        B.Int32("Face[" + std::to_string(i) + "] Texture Index", ids[i]);
    }
    return fields;
}

// TODO: TEST
inline std::vector<ChunkField> InterpretStageTexCoords(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dTexCoordStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back("error", "string",
            "Malformed TEXCOORDS chunk; size=" + std::to_string(buf.size()));
        return fields;
    }

    const auto* uv = reinterpret_cast<const W3dTexCoordStruct*>(buf.data());
    const size_t count = buf.size() / REC;

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < count; ++i) {
        B.TexCoord("Vertex[" + std::to_string(i) + "].UV", uv[i]); 
    }

    return fields;
}

// TODO: TEST
inline std::vector<ChunkField> InterpretPerFaceTexcoordIds(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t ElemSize = sizeof(Vector3i);
    if (buf.size() < ElemSize || (buf.size() % ElemSize) != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed PER_FACE_TEX_COORD_IDs chunk (size=" + std::to_string(buf.size()) + ")"
        );
        return fields;
    }

    const auto* verts = reinterpret_cast<const Vector3i*>(buf.data());
    const size_t count = buf.size() / ElemSize;

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < count; ++i) {
        B.Vec3i("Face[" + std::to_string(i) + "].UVIndices", verts[i]);
    }

    return fields;
}


inline std::vector<ChunkField> InterpretDeform(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dMeshDeform);
    if (buf.size() < REC) {
        fields.emplace_back("error", "string",
            "Malformed DEFORM chunk; size=" + std::to_string(buf.size()));
        return fields;
    }

    const auto* d = reinterpret_cast<const W3dMeshDeform*>(buf.data());
    ChunkFieldBuilder B(fields);

    B.UInt32("SetCount", d->SetCount);
    B.UInt32("AlphaPasses", d->AlphaPasses);

    // Report trailing reserved bytes (if any)
    const size_t extra = buf.size() - REC;
    if (extra > 0) {
        B.Push("ReservedBytes", "bytes", std::to_string(extra));
    }

    return fields;
}


inline std::vector<ChunkField> InterpretDeformSet(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dDeformSetInfo);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed DEFORM_SET chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    const auto* sets = reinterpret_cast<const W3dDeformSetInfo*>(buf.data());
    const size_t count = buf.size() / REC;

    for (size_t i = 0; i < count; ++i) {
        const auto& s = sets[i];
        ChunkFieldBuilder B(fields);
        const std::string pfx = "DeformSet[" + std::to_string(i) + "]";

        B.UInt32(pfx + ".KeyframeCount", s.KeyframeCount);
        B.UInt32(pfx + ".Flags", s.flags);

        // Emit non-zero reserved values (works for any reserved array size).
        constexpr size_t RES_N =
            sizeof(sets[0].reserved) / sizeof(sets[0].reserved[0]);
        for (size_t r = 0; r < RES_N; ++r) {
            if (s.reserved[r] != 0) {
                B.UInt32(pfx + ".Reserved[" + std::to_string(r) + "]",
                    s.reserved[r]);
            }
        }
    }

    return fields;
}


inline std::vector<ChunkField> InterpretDeformKeyframes(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dDeformKeyframeInfo);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed DEFORM_KEYFRAME chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    const auto* arr = reinterpret_cast<const W3dDeformKeyframeInfo*>(buf.data());
    const size_t count = buf.size() / REC;

    for (size_t i = 0; i < count; ++i) {
        const auto& kf = arr[i];
        const std::string pfx = "Keyframe[" + std::to_string(i) + "]";
        ChunkFieldBuilder B(fields);

        B.Float(pfx + ".DeformPercent", kf.DeformPercent);
        B.UInt32(pfx + ".DataCount", kf.DataCount);

        // emit non-zero reserved words (generic for any array size)
        constexpr size_t RES_N = sizeof(arr[0].reserved) / sizeof(arr[0].reserved[0]);
        for (size_t r = 0; r < RES_N; ++r) {
            if (kf.reserved[r] != 0) {
                B.UInt32(pfx + ".Reserved[" + std::to_string(r) + "]", kf.reserved[r]);
            }
        }
    }

    return fields;
}


inline std::vector<ChunkField> InterpretDeformData(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty DEFORM_DATA chunk");
        return fields;
    }

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dDeformData);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed DEFORM_DATA chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    auto entries = reinterpret_cast<const W3dDeformData*>(buf.data());
    size_t count = buf.size() / REC;
    for (size_t i = 0; i < count; ++i) {
        const auto& d = entries[i];
        std::string pfx = "DeformData[" + std::to_string(i) + "]";

        fields.emplace_back(
            pfx + ".VertexIndex",
            "uint32",
            std::to_string(d.VertexIndex)
        );
        fields.emplace_back(
            pfx + ".Position",
            "vector3",
            FormatUtils::FormatVector(d.Position.X, d.Position.Y, d.Position.Z)
        );
        fields.emplace_back(
            pfx + ".Color",
            "RGBA",
            "(" + FormatUtils::FormatVector(int(d.Color.R), int(d.Color.G), int(d.Color.B), int(d.Color.A)) + ")"
        );

        // if those reserved words ever matter:
        if (d.reserved[0] || d.reserved[1]) {
            fields.emplace_back(
                pfx + ".Reserved0",
                "uint32",
                std::to_string(d.reserved[0])
            );
            fields.emplace_back(
                pfx + ".Reserved1",
                "uint32",
                std::to_string(d.reserved[1])
            );
        }
    }

    return fields;
}

inline std::vector<ChunkField> InterpretPS2Shaders(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dPS2ShaderStruct);
    if (buf.empty() || buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed PS2 SHADERS chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    const auto* shaders = reinterpret_cast<const W3dPS2ShaderStruct*>(buf.data());
    const size_t count = buf.size() / REC;

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < count; ++i) {
        const auto& s = shaders[i];
        const std::string pfx = "Shader[" + std::to_string(i) + "]";

        B.Ps2DepthCompareField((pfx + ".DepthCompare").c_str(), s.DepthCompare);
        B.Ps2DepthMaskField((pfx + ".DepthMask").c_str(), s.DepthMask);
        B.Ps2PriGradientField((pfx + ".PriGradient").c_str(), s.PriGradient);
        B.Ps2TexturingField((pfx + ".Texturing").c_str(), s.Texturing);
        B.Ps2AlphaTestField((pfx + ".AlphaTest").c_str(), s.AlphaTest);

        // No named enums for these; just dump the raw bytes.
        B.UInt8(pfx + ".AParam", s.AParam);
        B.UInt8(pfx + ".BParam", s.BParam);
        B.UInt8(pfx + ".CParam", s.CParam);
        B.UInt8(pfx + ".DParam", s.DParam);

        // (ignore s.pad[3])
    }

    return fields;
}


inline std::vector<ChunkField> InterpretAABTreeHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;

    if (buf.size() < sizeof(W3dMeshAABTreeHeader)) {
        fields.emplace_back(
            "error", "string",
            "Malformed AAB_Tree_Header chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    auto const* hdr = reinterpret_cast<const W3dMeshAABTreeHeader*>(buf.data());
    ChunkFieldBuilder B(fields);


    B.UInt32("NodeCount", hdr->NodeCount);
    B.UInt32("PolyCount", hdr->PolyCount);
   

    return fields;
}


inline std::vector<ChunkField> InterpretAABTreePolyIndices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;

    if (buf.size() % sizeof(uint32_t) != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed AAB_TREE_POLY_INDICES chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    size_t count = buf.size() / sizeof(uint32_t);
    auto const* data = reinterpret_cast<const uint32_t*>(buf.data());

    ChunkFieldBuilder B(fields);
    for (size_t i = 0; i < count; ++i) {
        B.Int32("Polygon Index[" + std::to_string(i) + "]", static_cast<int32_t>(data[i]));
    }

    return fields;
}


inline std::vector<ChunkField> InterpretAABTreeNodes(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    // --- Sanity check: buffer length must be a multiple of our AAB struct size
    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dMeshAABTreeNode);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed AAB_TREE_NODE chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

	// --- Reinterpret as an array of AAB nodes
    auto const* aab = reinterpret_cast<const W3dMeshAABTreeNode*>(buf.data());
    size_t count = buf.size() / REC;

    // --- Builder
    ChunkFieldBuilder B(fields);

    for (size_t i = 0; i < count; ++i) {
        const auto& T = aab[i];
        std::string pfx = "Node[" + std::to_string(i) + "]";

        // min
        B.Vec3(pfx + ".Min", T.Min);
        
        // min
        B.Vec3(pfx + ".Max", T.Max);
        
        // front
        B.UInt32(pfx + ".Front", T.FrontOrPoly0);

        // back
        B.UInt32(pfx + ".Back", T.BackOrPolyCount);



    }

    return fields;
}



