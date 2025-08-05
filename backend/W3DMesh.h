#pragma once
#include "W3DStructs.h"

inline std::vector<ChunkField> InterpretMeshHeader3(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    if (buf.size() < sizeof(W3dMeshHeader3Struct)) {
        fields.emplace_back("error", "string", "Header too small");
        return fields;
    }

    auto hdr = reinterpret_cast<const W3dMeshHeader3Struct*>(buf.data());
    uint32_t attr = hdr->Attributes;


    //--- Helpers --------------------------------------------------------
    auto Push = [&](const char* k, const char* t, std::string v) {
        fields.emplace_back(k, t, std::move(v));
        };
    auto Flag = [&](uint32_t m, const char* name) {
        if (attr & m) Push("Attributes", "flag", name);
        };


    // --- Version -----------------------------------------------------------
    Push("Version", "string", FormatVersion(hdr->Version));

    //--- Names ----------------------------------------------------------
    Push("MeshName", "string", FormatName(hdr->MeshName, W3D_NAME_LEN));
    Push("ContainerName", "string", FormatName(hdr->ContainerName, W3D_NAME_LEN));

    //--- Raw Attributes -----------------------------------------------
    Push("Attributes", "uint32", FormatUInt32(attr));


    //--- Geometry type -----------------------------------------------
    switch (attr & static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_MASK)) {
    case static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_NORMAL):
        Push("Attributes", "flag", "W3D_MESH_FLAG_GEOMETRY_TYPE_NORMAL"); break;
    case static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_CAMERA_ALIGNED):
        Push("Attributes", "flag", "W3D_MESH_FLAG_GEOMETRY_TYPE_CAMERA_ALIGNED"); break;
    case static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_SKIN):
        Push("Attributes", "flag", "W3D_MESH_FLAG_GEOMETRY_TYPE_SKIN"); break;
    case static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_AABOX):
        Push("Attributes", "flag", "W3D_MESH_FLAG_GEOMETRY_TYPE_AABOX"); break;
    case static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_OBBOX):
        Push("Attributes", "flag", "W3D_MESH_FLAG_GEOMETRY_TYPE_OBBOX"); break;
    default:
        break;
    }

    //--- Collision types ---------------------------------------------
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_PHYSICAL), "W3D_MESH_FLAG_COLLISION_TYPE_PHYSICAL");
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_PROJECTILE), "W3D_MESH_FLAG_COLLISION_TYPE_PROJECTILE");
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_VIS), "W3D_MESH_FLAG_COLLISION_TYPE_VIS");
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_CAMERA), "W3D_MESH_FLAG_COLLISION_TYPE_CAMERA");
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_VEHICLE), "W3D_MESH_FLAG_COLLISION_TYPE_VEHICLE");

    //--- Other flags -------------------------------------------------
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_HIDDEN), "W3D_MESH_FLAG_HIDDEN");
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_TWO_SIDED), "W3D_MESH_FLAG_TWO_SIDED");
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_CAST_SHADOW), "W3D_MESH_FLAG_CAST_SHADOW");
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_SHATTERABLE), "W3D_MESH_FLAG_SHATTERABLE");
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_NPATCHABLE), "W3D_MESH_FLAG_NPATCHABLE");
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_PRELIT_UNLIT), "W3D_MESH_FLAG_PRELIT_UNLIT");
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_PRELIT_VERTEX), "W3D_MESH_FLAG_PRELIT_VERTEX");
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_PRELIT_LIGHTMAP_MULTI_PASS), "W3D_MESH_FLAG_PRELIT_LIGHTMAP_MULTI_PASS");
    Flag(static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_PRELIT_LIGHTMAP_MULTI_TEXTURE), "W3D_MESH_FLAG_PRELIT_LIGHTMAP_MULTI_TEXTURE");

    //--- Counts, Sorting & Bounds ------------------------------------
    Push("NumTris", "uint32", std::to_string(hdr->NumTris));
    Push("NumVertices", "uint32", std::to_string(hdr->NumVertices));
    Push("NumMaterials", "uint32", std::to_string(hdr->NumMaterials));
    Push("NumDamageStages", "uint32", std::to_string(hdr->NumDamageStages));

    // SortLevel
    if (hdr->SortLevel == static_cast<int32_t>(SortLevel::SORT_LEVEL_NONE))
        Push("SortLevel", "string", "NONE");
    else
        Push("SortLevel", "int32", std::to_string(hdr->SortLevel));

    // PrelitVersion
    constexpr auto PRELIT_MASK = static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_PRELIT_MASK);
    std::string prelit = (attr & PRELIT_MASK)
        ? (hdr->PrelitVersion ? FormatVersion(hdr->PrelitVersion) : "UNKNOWN")
        : "N/A";
    Push("PrelitVersion", "string", prelit);

    // FutureCounts

    Push("FutureCounts", "uint32", std::to_string(hdr->FutureCounts[0]));

    //--- VertexChannels ---------------------------------------------
    uint32_t vc = hdr->VertexChannels;
    Push("VertexChannels", "uint32", std::to_string(vc));

    auto FlagVC = [&](uint32_t mask, const char* name) {
        if (vc & mask) Push("VertexChannels", "flag", name);
        };

    auto VC = [](VertexChannelAttr e) { return static_cast<uint32_t>(e); };
    FlagVC(VC(VertexChannelAttr::W3D_VERTEX_CHANNEL_LOCATION), "W3D_VERTEX_CHANNEL_LOCATION");
    FlagVC(VC(VertexChannelAttr::W3D_VERTEX_CHANNEL_NORMAL), "W3D_VERTEX_CHANNEL_NORMAL");
    FlagVC(VC(VertexChannelAttr::W3D_VERTEX_CHANNEL_TEXCOORD), "W3D_VERTEX_CHANNEL_TEXCOORD");
    FlagVC(VC(VertexChannelAttr::W3D_VERTEX_CHANNEL_COLOR), "W3D_VERTEX_CHANNEL_COLOR");
    FlagVC(VC(VertexChannelAttr::W3D_VERTEX_CHANNEL_BONEID), "W3D_VERTEX_CHANNEL_BONEID");

    //--- FaceChannels -----------------------------------------------
    Push("FaceChannels", "uint32", std::to_string(hdr->FaceChannels));
    if (hdr->FaceChannels & W3D_FACE_CHANNEL_FACE)
        Push("FaceChannels", "flag", "W3D_FACE_CHANNEL_FACE");

    //--- Bounds --------------------------------------------------------
    Push("Min", "vector3", FormatVec3(hdr->Min));
    Push("Max", "vector3", FormatVec3(hdr->Max));
    Push("SphCenter", "vector3", FormatVec3(hdr->SphCenter));
    Push("SphRadius", "float", std::to_string(hdr->SphRadius));

    return fields;
}



inline std::vector<ChunkField> InterpretVertices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto const& buf = chunk->data;
    constexpr size_t ElemSize = sizeof(W3dVectorStruct);
    if (buf.size() % ElemSize != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed chunk: size=" + std::to_string(buf.size())
        );
        return fields;
    }

    size_t count = buf.size() / ElemSize;
    auto const* verts = reinterpret_cast<const W3dVectorStruct*>(buf.data());

    
    auto Push = [&](auto&&... args) {
        fields.emplace_back(std::forward<decltype(args)>(args)...);
        };

    for (size_t i = 0; i < count; ++i) {
        Push(
            "Vertex[" + std::to_string(i) + "]",
            "vector",
            FormatVec3(verts[i])
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretVertexNormals(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto const& buf = chunk->data;
    constexpr size_t ElemSize = sizeof(W3dVectorStruct);
    if (buf.size() % ElemSize != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed chunk: size=" + std::to_string(buf.size())
        );
        return fields;
    }

    size_t count = buf.size() / ElemSize;
    auto const* verts = reinterpret_cast<const W3dVectorStruct*>(buf.data());

    auto Push = [&](auto&&... args) {
        fields.emplace_back(std::forward<decltype(args)>(args)...);
        };

    for (size_t i = 0; i < count; ++i) {
        Push(
            "Normal[" + std::to_string(i) + "]",
            "vector",
            FormatVec3(verts[i])
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretMeshUserText(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    // build the helper from the raw bytes
    W3dNullTermString nts(
        chunk->data.data(),
        chunk->data.size()
    );

    fields.push_back({
      "UserText",
      "string",
      nts.value
        });
    return fields;
}

inline std::vector<ChunkField> InterpretVertexInfluences(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& data = chunk->data;
    constexpr size_t ElemSize = sizeof(W3dVertInfStruct);
    if (data.size() % ElemSize != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed VERTEX_INFLUENCES chunk; size=" + std::to_string(data.size())
        );
        return fields;
    }

    //--- helper lambdas --------------------------------------------------
    // Push: emplace a new ChunkField
    auto Push = [&](std::string fld, std::string type, std::string val) {
        fields.emplace_back(std::move(fld), std::move(type), std::move(val));
        };
    //----------------------------------------------------------------------

    auto const* arr = reinterpret_cast<const W3dVertInfStruct*>(data.data());
    size_t count = data.size() / ElemSize;

    for (size_t i = 0; i < count; ++i) {
        const auto& inf = arr[i];
        std::string pfx = "VertexInfluence[" + std::to_string(i) + "]";

        Push(pfx + ".BoneIdx[0]", "uint16", FormatUInt16(inf.BoneIdx[0]));
        Push(pfx + ".Weight[0]", "uint16", FormatUInt16(inf.Weight[0]));
        Push(pfx + ".BoneIdx[1]", "uint16", FormatUInt16(inf.BoneIdx[1]));
        Push(pfx + ".Weight[1]", "uint16", FormatUInt16(inf.Weight[1]));
    }

    return fields;
}


inline std::vector<ChunkField> InterpretTriangles(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dTriStruct);
    if (buf.size() % REC != 0) {
        fields.push_back({ "error", "string",
            "Malformed TRIANGLES chunk; size=" +
            std::to_string(buf.size()) });
        return fields;
    }

    size_t count = buf.size() / REC;
    auto tris = reinterpret_cast<const W3dTriStruct*>(buf.data());

    for (size_t i = 0; i < count; ++i) {
        const auto& T = tris[i];
        std::string pfx = "Triangle[" + std::to_string(i) + "]";

        // Vertex indices
        {
            std::ostringstream oss;
            oss << T.Vindex[0] << " "
                << T.Vindex[1] << " "
                << T.Vindex[2];
            fields.emplace_back(pfx + ".Vindex", "int32[3]", oss.str());
        }

        // Attributes
        fields.emplace_back(
            pfx + ".Attributes",
            "uint32",
            std::to_string(T.Attributes)
        );

        // Normal
        fields.emplace_back(
            pfx + ".Normal",
            "vector3",
            FormatVec3(T.Normal)
        );

        // Dist
        fields.emplace_back(
            pfx + ".Dist",
            "uint32",
            std::to_string(T.Dist)
        );
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

inline std::vector<ChunkField> InterpretMaterialInfo(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dMaterialInfoStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed MATERIAL_INFO chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    size_t count = buf.size() / REC;
    auto ptr = reinterpret_cast<const W3dMaterialInfoStruct*>(buf.data());

    for (size_t i = 0; i < count; ++i) {
        const auto& m = ptr[i];
        std::string pfx = "Material[" + std::to_string(i) + "]";

        fields.emplace_back(pfx + ".PassCount", "uint32", std::to_string(m.PassCount));
        fields.emplace_back(pfx + ".VertexMaterialCount", "uint32", std::to_string(m.VertexMaterialCount));
        fields.emplace_back(pfx + ".ShaderCount", "uint32", std::to_string(m.ShaderCount));
        fields.emplace_back(pfx + ".TextureCount", "uint32", std::to_string(m.TextureCount));
    }

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
        fields.emplace_back("error", "string",
            "Malformed SHADERS chunk; size=" + std::to_string(buf.size()));
        return fields;
    }

    auto shaders = reinterpret_cast<const W3dShaderStruct*>(buf.data());
    size_t count = buf.size() / REC;
    for (size_t i = 0; i < count; ++i) {
        const auto& s = shaders[i];
        std::string pfx = "shader[" + std::to_string(i) + "]";

        fields.emplace_back(pfx + ".DepthCompare", "string", ToString(DepthCompare(s.DepthCompare)));
        fields.emplace_back(pfx + ".DepthMask", "string", ToString(DepthMask(s.DepthMask)));
        fields.emplace_back(pfx + ".DestBlend", "string", ToString(DestBlend(s.DestBlend)));
        fields.emplace_back(pfx + ".PriGradient", "string", ToString(PriGradient(s.PriGradient)));
        fields.emplace_back(pfx + ".SecGradient", "string", ToString(SecGradient(s.SecGradient)));
        fields.emplace_back(pfx + ".SrcBlend", "string", ToString(SrcBlend(s.SrcBlend)));
        fields.emplace_back(pfx + ".Texturing", "string", ToString(Texturing(s.Texturing)));
        fields.emplace_back(pfx + ".DetailColorFunc", "string", ToString(DetailColorFunc(s.DetailColorFunc)));
        fields.emplace_back(pfx + ".DetailAlphaFunc", "string", ToString(DetailAlphaFunc(s.DetailAlphaFunc)));
        fields.emplace_back(pfx + ".AlphaTest", "string", ToString(AlphaTest(s.AlphaTest)));
        fields.emplace_back(pfx + ".PostDetailColor", "string", ToString(DetailColorFunc(s.PostDetailColorFunc)));
        fields.emplace_back(pfx + ".PostDetailAlpha", "string", ToString(DetailAlphaFunc(s.PostDetailAlphaFunc)));
        // padding and obsolete fields are simply ignored
    }
    return fields;
}

inline std::vector<ChunkField> InterpretVertexMaterialName(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    // build the helper from the raw bytes
    W3dNullTermString nts(
        chunk->data.data(),
        chunk->data.size()
    );

    fields.push_back({
      "Material Name",
      "string",
      nts.value
        });
    return fields;
}
static std::string FormatColor(const W3dRGBStruct& c) {
    return "(" +
        std::to_string(c.R) + " " +
        std::to_string(c.G) + " " +
        std::to_string(c.B) + ")";
}

inline std::vector<ChunkField> InterpretVertexMaterialInfo(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dVertexMaterialStruct);
    if (buf.size() < REC) {
        fields.push_back({ "error","string","Material info too short" });
        return fields;
    }
    // cast once
    auto vm = reinterpret_cast<const W3dVertexMaterialStruct*>(buf.data());

    // raw Attributes
    uint32_t attr = vm->Attributes;
    fields.emplace_back("Material.Attributes", "uint32", std::to_string(attr));

    // basic flags
    for (auto [mask, name] : VERTMAT_BASIC_FLAGS) {
        if (attr & mask) {
            fields.emplace_back("Material.Attributes", "flag", std::string(name));
        }
    }

    // stage mappings
    auto doStage = [&](int stage, uint32_t shift) {
        uint8_t idx = (attr >> shift) & 0xF;
        if (idx < VERTMAT_STAGE_MAPPING.size()) {
            auto name = VERTMAT_STAGE_MAPPING[idx].second;
            // replace '?' with stage number
            std::string s{ name };
            s[s.find('?')] = char('0' + stage);
            fields.emplace_back("Material.Attributes", "flag", s);
        }
        };
    doStage(0, 8);   // bits 0x00000F00
    doStage(1, 12);  // bits 0x0000F000

    // PSX flags
    for (auto [mask, name] : VERTMAT_PSX_FLAGS) {
        if (attr & mask) {
            fields.emplace_back("Material.Attributes", "flag", std::string(name));
            // if NO_RT_LIGHTING (0x0010_0000) is set, skip the rest
            if (mask == 0x0010'0000) break;
        }
    }

    // now the 4 colors & floats, in struct order
    fields.emplace_back("Material.Ambient", "RGB", FormatColor(vm->Ambient));
    fields.emplace_back("Material.Diffuse", "RGB", FormatColor(vm->Diffuse));
    fields.emplace_back("Material.Specular", "RGB", FormatColor(vm->Specular));
    fields.emplace_back("Material.Emissive", "RGB", FormatColor(vm->Emissive));
    fields.emplace_back("Material.Shininess", "float", std::to_string(vm->Shininess));
    fields.emplace_back("Material.Opacity", "float", std::to_string(vm->Opacity));
    fields.emplace_back("Material.Translucency", "float", std::to_string(vm->Translucency));

    return fields;
}



inline std::vector<ChunkField> InterpretARG0(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    // build the helper from the raw bytes
    W3dNullTermString nts(
        chunk->data.data(),
        chunk->data.size()
    );

    fields.push_back({
      "Stage0 Mapper Args",
      "string",
      nts.value
        });
    return fields;
}

inline std::vector<ChunkField> InterpretARG1(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    // build the helper from the raw bytes
    W3dNullTermString nts(
        chunk->data.data(),
        chunk->data.size()
    );

    fields.push_back({
      "Stage1 Mapper Args",
      "string",
      nts.value
        });
    return fields;
}

inline std::vector<ChunkField> InterpretTextureName(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    // build the helper from the raw bytes
    W3dNullTermString nts(
        chunk->data.data(),
        chunk->data.size()
    );

    fields.push_back({
      "Texture Name:",
      "string",
      nts.value
        });
    return fields;
}

inline std::vector<ChunkField> InterpretTextureInfo(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    if (buf.size() < sizeof(W3dTextureInfoStruct)) {
        fields.emplace_back("error", "string", "Texture info too short");
        return fields;
    }

    // Cast once
    auto info = reinterpret_cast<const W3dTextureInfoStruct*>(buf.data());

    // Raw attributes
    uint16_t rawAttr = info->Attributes;
    fields.emplace_back("Texture.Attributes", "uint16", std::to_string(rawAttr));

    // Decode each flag
    for (auto [flag, name] : TextureAttrNames) {
        if (rawAttr & static_cast<uint16_t>(flag)) {
            fields.emplace_back("Texture.Flag", "flag", std::string(name));
        }
    }

    // Animation type
    TextureAnim anim = static_cast<TextureAnim>(info->AnimType);
    fields.emplace_back(
        "Texture.AnimType",
        "string",
        std::string(ToString(anim))
    );

    // Frame count + rate
    fields.emplace_back(
        "Texture.FrameCount",
        "uint32",
        std::to_string(info->FrameCount)
    );
    fields.emplace_back(
        "Texture.FrameRate",
        "float",
        std::to_string(info->FrameRate)
    );

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

    const uint32_t* data = reinterpret_cast<const uint32_t*>(chunk->data.data());
    size_t count = chunk->data.size() / sizeof(uint32_t);

    for (size_t i = 0; i < count; ++i) {
        std::string label = "Vertex[" + std::to_string(i) + "] Shader Index";
        fields.push_back({ label, "int32", std::to_string(data[i]) });
    }


    return fields;
}

inline std::vector<ChunkField> InterpretDCG(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty DCG chunk");
        return fields;
    }

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dRGBAStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Unexpected DCG chunk size: " + std::to_string(buf.size())
        );
        return fields;
    }

    auto entries = reinterpret_cast<const W3dRGBAStruct*>(buf.data());
    size_t count = buf.size() / REC;

    for (size_t i = 0; i < count; ++i) {
        const auto& c = entries[i];
        std::string pfx = "Vertex[" + std::to_string(i) + "].DIG";
        fields.emplace_back(pfx + ".R", "uint8", std::to_string(c.R));
        fields.emplace_back(pfx + ".G", "uint8", std::to_string(c.G));
        fields.emplace_back(pfx + ".B", "uint8", std::to_string(c.B));
        fields.emplace_back(pfx + ".A", "uint8", std::to_string(c.A));
        
    }

    return fields;
}


inline std::vector<ChunkField> InterpretDIG(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty DIG chunk");
        return fields;
    }

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dRGBStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Unexpected DIG chunk size: " + std::to_string(buf.size())
        );
        return fields;
    }

    auto colors = reinterpret_cast<const W3dRGBStruct*>(buf.data());
    size_t count = buf.size() / REC;

    for (size_t i = 0; i < count; ++i) {
        const auto& c = colors[i];
        std::string pfx = "Vertex[" + std::to_string(i) + "].DIG";
        fields.emplace_back(pfx + ".R", "uint8", std::to_string(c.R));
        fields.emplace_back(pfx + ".G", "uint8", std::to_string(c.G));
        fields.emplace_back(pfx + ".B", "uint8", std::to_string(c.B));
    }

    return fields;
}


inline std::vector<ChunkField> InterpretSCG(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty SCG chunk");
        return fields;
    }

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dRGBStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Unexpected SCG chunk size: " + std::to_string(buf.size())
        );
        return fields;
    }

    auto colors = reinterpret_cast<const W3dRGBStruct*>(buf.data());
    size_t count = buf.size() / REC;

    for (size_t i = 0; i < count; ++i) {
        const auto& c = colors[i];
        std::string pfx = "Vertex[" + std::to_string(i) + "].SCG";
        fields.emplace_back(pfx + ".R", "uint8", std::to_string(c.R));
        fields.emplace_back(pfx + ".G", "uint8", std::to_string(c.G));
        fields.emplace_back(pfx + ".B", "uint8", std::to_string(c.B));
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

static std::string FormatUV(const W3dTexCoordStruct& t) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6)
        << t.U << ", " << t.V;
    return oss.str();
}

inline std::vector<ChunkField> InterpretStageTexCoords(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dTexCoordStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed TEXCOORDS chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    auto uvArray = reinterpret_cast<const W3dTexCoordStruct*>(buf.data());
    size_t count = buf.size() / REC;

    for (size_t i = 0; i < count; ++i) {
        std::string label = "Vertex[" + std::to_string(i) + "].UV";
        fields.emplace_back(label, "vec2", FormatUV(uvArray[i]));
    }

    return fields;
}

static std::string FormatVec3i(const Vector3i& v) {
    return "("
        + std::to_string(v.I) + " "
        + std::to_string(v.J) + " "
        + std::to_string(v.K) + ")";
}

inline std::vector<ChunkField> InterpretPerFaceTexcoordIds(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty PER_FACE_TEXCOORD_IDS chunk");
        return fields;
    }

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(Vector3i);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Unexpected PER_FACE_TEXCOORD_IDS chunk size: " + std::to_string(buf.size())
        );
        return fields;
    }

    auto faces = reinterpret_cast<const Vector3i*>(buf.data());
    size_t count = buf.size() / REC;
    for (size_t i = 0; i < count; ++i) {
        std::string label = "Face[" + std::to_string(i) + "] UV Indices";
        fields.emplace_back(label, "vector3i", FormatVec3i(faces[i]));
    }

    return fields;
}

inline std::vector<ChunkField> InterpretDeform(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty DEFORM chunk");
        return fields;
    }

    const auto& buf = chunk->data;
    constexpr size_t MIN_SZ = sizeof(W3dMeshDeform);
    if (buf.size() < MIN_SZ) {
        fields.emplace_back(
            "error", "string",
            "DEFORM chunk too short (" + std::to_string(buf.size()) + " bytes)"
        );
        return fields;
    }

    
    auto hdr = reinterpret_cast<const W3dMeshDeform*>(buf.data());
    fields.emplace_back("Deform.SetCount", "uint32", std::to_string(hdr->SetCount));
    fields.emplace_back("Deform.AlphaPasses", "uint32", std::to_string(hdr->AlphaPasses));

    // If you care about the reserved trailing bytes, you can report how many:
    size_t extra = buf.size() - MIN_SZ;
    if (extra > 0) {
        fields.emplace_back(
            "Deform.ReservedBytes",
            "bytes",
            std::to_string(extra)
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretDeformSet(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty DEFORM_SET chunk");
        return fields;
    }

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dDeformSetInfo);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed DEFORM_SET chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    auto sets = reinterpret_cast<const W3dDeformSetInfo*>(buf.data());
    size_t count = buf.size() / REC;

    for (size_t i = 0; i < count; ++i) {
        const auto& s = sets[i];
        std::string pfx = "DeformSet[" + std::to_string(i) + "]";

        fields.emplace_back(
            pfx + ".KeyframeCount",
            "uint32",
            std::to_string(s.KeyframeCount)
        );
        fields.emplace_back(
            pfx + ".Flags",
            "uint32",
            std::to_string(s.flags)
        );
        // reserved is an array, so index it:
        if (s.reserved[0] != 0) {
            fields.emplace_back(
                pfx + ".Reserved[0]",
                "uint32",
                std::to_string(s.reserved[0])
            );
        }
    }

    return fields;
}

inline std::vector<ChunkField> InterpretDeformKeyframes(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty DEFORM_KEYFRAME chunk");
        return fields;
    }

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dDeformKeyframeInfo);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed DEFORM_KEYFRAME chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    auto kfArray = reinterpret_cast<const W3dDeformKeyframeInfo*>(buf.data());
    size_t count = buf.size() / REC;

    for (size_t i = 0; i < count; ++i) {
        const auto& kf = kfArray[i];
        std::string pfx = "Keyframe[" + std::to_string(i) + "]";

        fields.emplace_back(
            pfx + ".DeformPercent",
            "float",
            std::to_string(kf.DeformPercent)
        );
        fields.emplace_back(
            pfx + ".DataCount",
            "uint32",
            std::to_string(kf.DataCount)
        );
        // if you care about the reserved words:
        if (kf.reserved[0] || kf.reserved[1]) {
            fields.emplace_back(
                pfx + ".Reserved0",
                "uint32",
                std::to_string(kf.reserved[0])
            );
            fields.emplace_back(
                pfx + ".Reserved1",
                "uint32",
                std::to_string(kf.reserved[1])
            );
        }
    }

    return fields;
}



inline std::string FormatRGBA(const W3dRGBAStruct& c) {
    return "("
        + std::to_string(c.R) + " "
        + std::to_string(c.G) + " "
        + std::to_string(c.B) + " "
        + std::to_string(c.A) + ")";
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
            FormatVec3(d.Position)
        );
        fields.emplace_back(
            pfx + ".Color",
            "RGBA",
            FormatRGBA(d.Color)
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
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty PS2_SHADERS chunk");
        return fields;
    }

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dPS2ShaderStruct);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Unexpected PS2_SHADERS size: " + std::to_string(buf.size())
        );
        return fields;
    }

    auto arr = reinterpret_cast<const W3dPS2ShaderStruct*>(buf.data());
    size_t count = buf.size() / REC;
    for (size_t i = 0; i < count; ++i) {
        const auto& s = arr[i];
        std::string pfx = "shader[" + std::to_string(i) + "]";

        fields.emplace_back(pfx + ".DepthCompare", "uint8", std::to_string(s.DepthCompare));
        fields.emplace_back(pfx + ".DepthMask", "uint8", std::to_string(s.DepthMask));
        fields.emplace_back(pfx + ".PriGradient", "uint8", std::to_string(s.PriGradient));
        fields.emplace_back(pfx + ".Texturing", "uint8", std::to_string(s.Texturing));
        fields.emplace_back(pfx + ".AlphaTest", "uint8", std::to_string(s.AlphaTest));
        fields.emplace_back(pfx + ".AParam", "uint8", std::to_string(s.AParam));
        fields.emplace_back(pfx + ".BParam", "uint8", std::to_string(s.BParam));
        fields.emplace_back(pfx + ".CParam", "uint8", std::to_string(s.CParam));
        fields.emplace_back(pfx + ".DParam", "uint8", std::to_string(s.DParam));
    }

    return fields;
}

inline std::vector<ChunkField> InterpretAABTreeHeader(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("Error", "string", "Empty AABTreeHeader chunk");
        return fields;
    }

    const auto& buf = chunk->data;
    constexpr size_t MIN_SZ = sizeof(uint32_t) * 2;  // NodeCount + PolyCount
    if (buf.size() < MIN_SZ) {
        fields.emplace_back(
            "Error", "string",
            "AABTreeHeader too short: " + std::to_string(buf.size()) + " bytes"
        );
        return fields;
    }

    // Cast to our struct (we only need the first two fields)
    auto hdr = reinterpret_cast<const W3dMeshAABTreeHeader*>(buf.data());

    fields.emplace_back("NodeCount", "uint32", std::to_string(hdr->NodeCount));
    fields.emplace_back("PolyCount", "uint32", std::to_string(hdr->PolyCount));
    return fields;
}

inline std::vector<ChunkField> InterpretAABTreePolyIndices(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    const uint32_t* data = reinterpret_cast<const uint32_t*>(chunk->data.data());
    size_t count = chunk->data.size() / sizeof(uint32_t);

    for (size_t i = 0; i < count; ++i) {
        fields.push_back({ "Polygon Index[" + std::to_string(i) + "]", "int32", std::to_string(data[i]) });
    }

    return fields;
}


inline std::vector<ChunkField> InterpretAABTreeNodes(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty AABTreeNodes chunk");
        return fields;
    }

    const auto& buf = chunk->data;
    constexpr size_t REC = sizeof(W3dMeshAABTreeNode);
    if (buf.size() % REC != 0) {
        fields.emplace_back(
            "error", "string",
            "Malformed AABTreeNodes chunk; size=" + std::to_string(buf.size())
        );
        return fields;
    }

    auto nodes = reinterpret_cast<const W3dMeshAABTreeNode*>(buf.data());
    size_t count = buf.size() / REC;
    for (size_t i = 0; i < count; ++i) {
        const auto& n = nodes[i];
        std::string pfx = "Node[" + std::to_string(i) + "]";

        // Box corners
        fields.emplace_back(pfx + ".Min", "vector", FormatVec3(n.Min));
        fields.emplace_back(pfx + ".Max", "vector", FormatVec3(n.Max));

        // Leaf vs interior
        bool isLeaf = (n.FrontOrPoly0 & 0x80000000u) != 0;
        if (isLeaf) {
            uint32_t poly0 = n.FrontOrPoly0 & 0x7FFFFFFFu;
            fields.emplace_back(pfx + ".Poly0", "uint32", std::to_string(poly0));
            fields.emplace_back(pfx + ".PolyCount", "uint32", std::to_string(n.BackOrPolyCount));
        }
        else {
            fields.emplace_back(
                pfx + ".Front", "int32",
                std::to_string(static_cast<int32_t>(n.FrontOrPoly0))
            );
            fields.emplace_back(
                pfx + ".Back", "int32",
                std::to_string(static_cast<int32_t>(n.BackOrPolyCount))
            );
        }
    }

    return fields;
}

