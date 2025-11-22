#pragma once
#include <unordered_map>
#include <string>
#include <sstream>
#include <algorithm>
#include "ChunkItem.h"

constexpr uint32_t COMMON_VARIABLE_CHUNK_ID = 0x00000100;
constexpr uint32_t SIMPLEFACTORY_CHUNKID_OBJPOINTER = 0x00100100;
constexpr uint32_t SIMPLEFACTORY_CHUNKID_OBJDATA = 0x00100101;
constexpr uint32_t CHUNKID_SAVELOAD_DEFMGR = 0x00000101;
constexpr uint32_t CHUNKID_COMMANDO_EDITOR_BEGIN = 0x00050000;

inline std::unordered_map<uint32_t, std::string>& ExternalChunkNameMap() {
    static std::unordered_map<uint32_t, std::string> map;
    return map;
}

inline void RegisterExternalChunkName(uint32_t id, const std::string& name) {
    ExternalChunkNameMap()[id] = name;
}

inline const std::string& LookupExternalChunkName(uint32_t id) {
    auto& map = ExternalChunkNameMap();
    auto it = map.find(id);
    if (it != map.end()) {
        return it->second;
    }
    static const std::string empty;
    return empty;
}

inline std::string GetChunkName(uint32_t id, uint32_t parentId = 0) {
    if (parentId == 0x0741 && id == 0x0001) {
        return "CHUNKID_SPHERE_DEF ";
    }
    if (parentId == 0x0742 && id == 0x0001) {
        return "CHUNKID_RING_DEF ";
    }
    if (parentId == 0x0741 && id == 0x0002) {
        return "CHUNKID_COLOR_CHANNEL";
    }
    if (parentId == 0x0742 && id == 0x0002) {
        return "CHUNKID_COLOR_CHANNEL";
    }
    if (parentId == 0x0741 && id == 0x0003) {
        return "CHUNKID_ALPHA_CHANNEL";
    }  
    if (parentId == 0x0742 && id == 0x0003) {
        return "CHUNKID_ALPHA_CHANNEL";
    }
    if (parentId == 0x0741 && id == 0x0004) {
        return "CHUNKID_SCALE_CHANNEL";
    }
    if (parentId == 0x0742 && id == 0x0004) {
        return "CHUNKID_INNER_SCALE_CHANNEL";
    }
    if (parentId == 0x0741 && id == 0x0005) {
        return "CHUNKID_VECTOR_CHANNEL";
    }
    if (parentId == 0x0742 && id == 0x0005) {
        return "CHUNKID_OUTER_SCALE_CHANNEL";
    }
    if (parentId == 0x31550809 && id == 0x1) {
        return "ChunkID_FRAME";
    }
    if (parentId == 0x0A02 && id == COMMON_VARIABLE_CHUNK_ID) {
        return "CHUNKID_VARIABLES";
    }
    if (parentId == 0x0A02 && id == 0x00000200) {
        return "CHUNKID_BASE_CLASS";
    }
    if (parentId == 0x00000200 && id == COMMON_VARIABLE_CHUNK_ID) {
        return "CHUNKID_VARIABLES";
    }
    static const std::unordered_map<uint32_t, std::string> chunkNames = {
        { 0x0000, "W3D_CHUNK_MESH" }, // Mesh definition
        { 0x0001, "W3D_CHUNK_MESH_HEADER" }, // Header for mesh (Legacy)
        { 0x0002, "W3D_CHUNK_VERTICES" }, // array of vertices (array of W3dVectorStruct's)
        { 0x0003, "W3D_CHUNK_VERTEX_NORMALS" }, // array of normals (array of W3dVectorStruct's)
		{ 0x0004, "W3D_CHUNK_SURRENDER_NORMALS" }, // array of vertex colors (array of W3dRGBAStruct's)
		{ 0x0005, "W3D_CHUNK_TEXCOORDS" }, // array of texture coordinates (array of W3dTexCoordStruct's)
        { 0x0006, "O_W3D_CHUNK_MATERIALS" }, //OBSOLETE: array of materials
        { 0x0007, "O_W3D_CHUNK_TRIANGLES" }, //OBSOLETE: array of triangles
        { 0x0008, "O_W3D_CHUNK_QUADRANGLES" }, // OBSOLETE: array of quads
        { 0x0009, "O_W3D_CHUNK_SURRENDER_TRIANGLES" }, // OBSOLETE: array of surrender format tris
        { 0x000A, "O_W3D_CHUNK_POV_TRIANGLES" }, // OBSOLETE: POV format triangles 
		{ 0x000B, "O_W3D_CHUNK_POV_QUADRANGLES" }, // OBSOLETE: POV format quads
        { 0x000C, "W3D_CHUNK_MESH_USER_TEXT" }, // Text from the MAX comment field (Null terminated string)
        { 0x000D, "W3D_CHUNK_VERTEX_COLORS" }, // OBSOLETE: Pre-set vertex coloring
        { 0x000E, "W3D_CHUNK_VERTEX_INFLUENCES" }, // Mesh Deformation vertex connections (array of W3dVertInfStruct's)
        { 0x000F, "W3D_CHUNK_DAMAGE" },             // OBSOLETE: Mesh damage (materials, verts, colors)
        { 0x0010, "W3D_CHUNK_DAMAGE_HEADER" },      // OBSOLETE: Header for damage data
        { 0x0011, "W3D_CHUNK_DAMAGE_VERTICES" },    // OBSOLETE: Modified vertices (W3dMeshDamageVertexStruct[])
        { 0x0012, "W3D_CHUNK_DAMAGE_COLORS" },      // OBSOLETE: Modified vertex colors (W3dMeshDamageColorStruct[])
        { 0x0013, "W3D_CHUNK_DAMAGE_MATERIALS" },   // OBSOLETE
        { 0x0014, "O_W3D_CHUNK_MATERIALS2" },       // OBSOLETE
        { 0x0015, "W3D_CHUNK_MATERIALS3" },         // OBSOLETE
        { 0x0016, "W3D_CHUNK_MATERIAL3" },          // OBSOLETE: wrapper for v3 material
        { 0x0017, "W3D_CHUNK_MATERIAL3_NAME" },     // OBSOLETE: material name (null term)
        { 0x0018, "W3D_CHUNK_MATERIAL3_INFO" },     // OBSOLETE: W3dMaterial3Struct
        { 0x0019, "W3D_CHUNK_MATERIAL3_DC_MAP" },   // OBSOLETE: diffuse color texture map
        { 0x001A, "W3D_CHUNK_MAP3_FILENAME" },      // OBSOLETE: filename of map texture
        { 0x001B, "W3D_CHUNK_MAP3_INFO" },          // OBSOLETE: W3dMap3Struct
        { 0x001C, "W3D_CHUNK_MATERIAL3_DI_MAP" },   // OBSOLETE: diffuse illumination map
        { 0x001D, "W3D_CHUNK_MATERIAL3_SC_MAP" },   // OBSOLETE: specular color map
        { 0x001E, "W3D_CHUNK_MATERIAL3_SI_MAP" },   // OBSOLETE: specular illumination map
        { 0x001F, "W3D_CHUNK_MESH_HEADER3" }, // mesh header contains general info about the mesh. (W3dMeshHeader3Struct)
        { 0x0020, "W3D_CHUNK_TRIANGLES" }, // New improved triangles chunk (array of W3dTriangleStruct's)
        { 0x0021, "W3D_CHUNK_PER_TRI_MATERIALS" }, // OBSOLETE: Multi-Mtl meshes - An array of uint16 material id's
        { 0x0022, "W3D_CHUNK_VERTEX_SHADE_INDICES" }, // shade indexes for each vertex (array of uint32's)
        { 0x0023, "W3D_CHUNK_PRELIT_UNLIT" }, // optional unlit material chunk wrapper
        { 0x0024, "W3D_CHUNK_PRELIT_VERTEX" }, // optional vertex-lit material chunk wrapper
        { 0x0025, "W3D_CHUNK_LIGHTMAP_MULTI_PASS" }, // optional lightmapped multi-pass material chunk wrapper
        { 0x0026, "W3D_CHUNK_LIGHTMAP_MULTI_TEXTURE" }, // optional lightmapped multi-texture material chunk wrapper
        { 0x0028, "W3D_CHUNK_MATERIAL_INFO" }, // materials information, pass count, etc (contains W3dMaterialInfoStruct)
        { 0x0029, "W3D_CHUNK_SHADERS" }, // shaders
        { 0x002A, "W3D_CHUNK_VERTEX_MATERIALS" }, // wraps the vertex materials
        { 0x002B, "W3D_CHUNK_VERTEX_MATERIAL" }, 
        { 0x002C, "W3D_CHUNK_VERTEX_MATERIAL_NAME" }, // vertex material name (NULL-terminated string)
        { 0x002D, "W3D_CHUNK_VERTEX_MATERIAL_INFO" }, // W3dVertexMaterialStruct
        { 0x002E, "W3D_CHUNK_VERTEX_MAPPER_ARGSO" }, // Null-terminated string
        { 0x002F, "W3D_CHUNK_VERTEX_MAPPER_ARGS1 "}, // Null-terminated string
        { 0x0030, "W3D_CHUNK_TEXTURES" }, // wraps all of the texture info
        { 0x0031, "W3D_CHUNK_TEXTURE" }, // wraps a texture definition
        { 0x0032, "W3D_CHUNK_TEXTURE_NAME" }, // texture filename (NULL-terminated string)
        { 0x0033, "W3D_CHUNK_TEXTURE_INFO" }, // optional W3dTextureInfoStruct
        { 0x0038, "W3D_CHUNK_MATERIAL_PASS" }, // wraps the information for a single material pass
        { 0x0039, "W3D_CHUNK_VERTEX_MATERIAL_IDS" }, // single or per-vertex array of uint32 vertex material indices (check chunk size)
        { 0x003A, "W3D_CHUNK_SHADER_IDS" }, // single or per-tri array of uint32 shader indices (check chunk size)
        { 0x003B, "W3D_CHUNK_DCG" }, // per-vertex diffuse color values (array of W3dRGBAStruct's)
        { 0x003C, "W3D_CHUNK_DIG" }, // per-vertex diffuse illumination values (array of W3dRGBStruct's)
        { 0x003E, "W3D_CHUNK_SCG" }, // per-vertex specular color values (array of W3dRGBStruct's)
        { 0x003F, "W3D_CHUNK_SHADER_MATERIAL_ID" },   // BFMEII: single or per-tri array of uint32 fx shader indices
        { 0x0048, "W3D_CHUNK_TEXTURE_STAGE" }, // wrapper around a texture stage.
        { 0x0049, "W3D_CHUNK_TEXTURE_IDS" }, // single or per-tri array of uint32 texture indices (check chunk size)
        { 0x004A, "W3D_CHUNK_STAGE_TEXCOORDS" }, // per-vertex texture coordinates (array of W3dTexCoordStruct's)
        { 0x004B, "W3D_CHUNK_PER_FACE_TEXCOORD_IDS" }, // indices to W3D_CHUNK_STAGE_TEXCOORDS, (array of Vector3i)
        { 0x0050, "W3D_CHUNK_SHADER_MATERIALS" },     // BFMEII: W3D_CHUNK_FX_SHADERS appears first
        { 0x0051, "W3D_CHUNK_SHADER_MATERIAL" },      // BFMEII
        { 0x0052, "W3D_CHUNK_SHADER_MATERIAL_HEADER" },// BFMEII
        { 0x0053, "W3D_CHUNK_SHADER_MATERIAL_PROPERTY" },// BFMEII
        { 0x0058, "W3D_CHUNK_DEFORM" }, // mesh deform or 'damage' information.
        { 0x0059, "W3D_CHUNK_DEFORM_SET" }, // set of deform information
        { 0x005A, "W3D_CHUNK_DEFORM_KEYFRAME" }, // a keyframe of deform information in the set
        { 0x005B, "W3D_CHUNK_DEFORM_DATA" }, // deform information about a single vertex
        { 0x0060, "W3D_CHUNK_TANGENTS" },             // BFMEII
        { 0x0061, "W3D_CHUNK_BINORMALS" },            // BFMEII (aka BITANGENTS)
        { 0x0080, "W3D_CHUNK_PS2_SHADERS" }, // Shader info specific to the Playstation 2.
        { 0x0090, "W3D_CHUNK_AABTREE" }, // Axis-Aligned Box Tree for hierarchical polygon culling
        { 0x0091, "W3D_CHUNK_AABTREE_HEADER" }, // catalog of the contents of the AABTree
        { 0x0092, "W3D_CHUNK_AABTREE_POLYINDICES" }, // array of uint32 polygon indices with count=mesh.PolyCount
        { 0x0093, "W3D_CHUNK_AABTREE_NODES" }, // array of W3dMeshAABTreeNode's with count=aabheader.NodeCount
        { 0x0100, "W3D_CHUNK_HIERARCHY" }, // hierarchy tree definition
        { 0x0101, "W3D_CHUNK_HIERARCHY_HEADER" }, // hierarchy tree Header
        { 0x0102, "W3D_CHUNK_PIVOTS" },       // pivots
        { 0x0103, "W3D_CHUNK_PIVOT_FIXUPS" }, // only needed by the exporter...
        { 0x0104, "W3D_CHUNK_PIVOT_UNKNOWN1" },       // ENB
        { 0x0200, "W3D_CHUNK_ANIMATION" }, // hierarchy animation data
        { 0x0201, "W3D_CHUNK_ANIMATION_HEADER" }, // Animation Header
        { 0x0202, "W3D_CHUNK_ANIMATION_CHANNEL" }, // channel of vectors
        { 0x0203, "W3D_CHUNK_BIT_CHANNEL" }, // channel of boolean values (e.g. visibility)
        { 0x0280, "W3D_CHUNK_COMPRESSED_ANIMATION"}, // compressed hierarchy animation data
        { 0x0281, "W3D_CHUNK_COMPRESSED_ANIMATION_HEADER"}, // describes playback rate, number of frames, and type of compression
        { 0x0282, "W3D_CHUNK_COMPRESSED_ANIMATION_CHANNEL"}, // compressed channel, format dependent on type of compression
        { 0x0283, "W3D_CHUNK_COMPRESSED_BIT_CHANNEL"}, // compressed bit stream channel, format dependent on type of compression
        { 0x0284, "W3D_CHUNK_COMPRESSED_ANIMATION_MOTION_CHANNEL" }, // BFMEII
        { 0x02C0, "W3D_CHUNK_MORPH_ANIMATION" }, // hierarchy morphing animation data (morphs between poses, for facial animation)
		{ 0x02C1, "W3D_CHUNK_MORPHANIM_HEADER" }, // W3dMorphAnimHeaderStruct describes playback rate, number of frames, and type of compression
		{ 0x02C2, "W3D_CHUNK_MORPHANIM_CHANNEL" }, // wrapper for a channel
		{ 0x02C3, "W3D_CHUNK_MORPHANIM_POSENAME" }, // name of the other anim which contains the poses for this morph channel
		{ 0x02C4, "W3D_CHUNK_MORPHANIM_KEYDATA" }, // morph key data for this channel
		{ 0x02C5, "W3D_CHUNK_MORPHANIM_PIVOTCHANNELDATA" }, // uin32 per pivot in the htree, indicating which channel controls the pivot
        { 0x0300, "W3D_CHUNK_HMODEL"}, // blueprint for a hierarchy model
		{ 0x0301, "W3D_CHUNK_HMODEL_HEADER" }, // Header for the hierarchy model
		{ 0x0302, "W3D_CHUNK_NODE" }, // render objects connected to the hierarchy
		{ 0x0303, "W3D_CHUNK_COLLISION_NODE" }, // collision meshes connected to the hierarchy
		{ 0x0304, "W3D_CHUNK_SKIN_NODE" }, // skins connected to the hierarchy
		{ 0x0305, "OBSOLETE_W3D_CHUNK_HMODEL_AUX_DATA" }, // extension of the hierarchy model header
        { 0x0306, "OBSOLETE_W3D_CHUNK_SHADOW_NODE" },  // shadow object connected to the hierarchy
        { 0x03150809, "CHUNKID_VARIABLES" },
        { 0x0400, "W3D_CHUNK_LODMODEL" }, // blueprint for an LOD model.  This is simply a collection of 'n' render objects, ordered in terms of their expected rendering costs. (highest is first)
		{ 0x0401, "W3D_CHUNK_LODMODEL_HEADER" }, // Header
		{ 0x0402, "W3D_CHUNK_LOD" },          //LOD
        { 0x0420, "W3D_CHUNK_COLLECTION" }, // collection of render object names
		{ 0x0421, "W3D_CHUNK_COLLECTION_HEADER" }, // general info regarding the collection
		{ 0x0422, "W3D_CHUNK_COLLECTION_OBJ_NAME" }, // contains a string which is the name of a render object
		{ 0x0423, "W3D_CHUNK_PLACEHOLDER" }, // contains information about a 'dummy' object that will be instanced later
		{ 0x0424, "W3D_CHUNK_TRANSFORM_NODE" }, // contains the filename of another w3d file that should be transformed by this node
        { 0x0440, "W3D_CHUNK_POINTS" }, // array of W3dVectorStruct's.  May appear in meshes, hmodels, lodmodels, or collections.
        { 0x0460, "W3D_CHUNK_LIGHT" }, // description of a light
        { 0x0461, "W3D_CHUNK_LIGHT_INFO" }, // generic light parameters
        { 0x0462, "W3D_CHUNK_SPOT_LIGHT_INFO" }, // extra spot light parameters
        { 0x0463, "W3D_CHUNK_NEAR_ATTENUATION" }, // optional near attenuation parameters
        { 0x0464, "W3D_CHUNK_FAR_ATTENUATION" }, // optional far attenuation parameters
        { 0x0465, "W3D_CHUNK_SPOT_LIGHT_INFO_5_0" },  // TT: extra spot light params (5.0)
        { 0x0466, "W3D_CHUNK_PULSE" },                // TT: pulse data (5.0)
		{ 0x0500, "W3D_CHUNK_EMITTER" }, // description of a particle emitter
		{ 0x0501, "W3D_CHUNK_EMITTER_HEADER" }, // general information such as name and version
		{ 0x0502, "W3D_CHUNK_EMITTER_USER_DATA" }, // user-defined data that specific loaders can switch on
		{ 0x0503, "W3D_CHUNK_EMITTER_INFO" }, // generic particle emitter definition
		{ 0x0504, "W3D_CHUNK_EMITTER_INFOV2" }, // generic particle emitter definition (version 2.0)
		{ 0x0505, "W3D_CHUNK_EMITTER_PROPS" }, // Key-frameable properties
		{ 0x0506, "OBSOLETE_W3D_CHUNK_EMMITER_COLOR_KEYFRAME" }, // structure defining a single color keyframe
		{ 0x0507, "OBSOLETE_W3D_CHUNK_EMITTER_OPACITY_KEYFRAME" }, // structure defining a single opacity keyframe
		{ 0x0508, "OBSOLETE_W3D_CHUNK_EMITTER_SIZE_KEYFRAME" }, // structure defining a single size keyframe
		{ 0x0509, "W3D_CHUNK_EMITTER_LINE_PROPERTIES" }, // line properties, used by line rendering mode
		{ 0x050A, "W3D_CHUNK_EMITTER_ROTATION_KEYFRAMES" }, // rotation keys for the particles
		{ 0x050B, "W3D_CHUNK_EMITTER_FRAME_KEYFRAMES" }, // frame keys (u-v based frame animation)
		{ 0x050C, "W3D_CHUNK_EMITTER_BLUR_TIME_KEYFRAMES" }, // length of tail for line groups
        { 0x050D, "W3D_CHUNK_EMITTER_EXTRA_INFO" },   // Generals / ENB
        { 0x0600, "W3D_CHUNK_AGGREGATE" }, // description of an aggregate object
		{ 0x0601, "W3D_CHUNK_AGGREGATE_HEADER" }, // general information such as name and version
		{ 0x0602, "W3D_CHUNK_AGGREGATE_INFO" }, // references to 'contained' models
		{ 0x0603, "W3D_CHUNK_TEXTURE_REPLACER_INFO" }, // information about which meshes need textures replaced
		{ 0x0604, "W3D_CHUNK_AGGREGATE_CLASS_INFO" }, // information about the original class that created this aggregate
        { 0x0700, "W3D_CHUNK_HLOD" }, // description of an HLod object (see HLodClass)
        { 0x0701, "W3D_CHUNk_HLOD_HEADER"}, // general information such as name and version
        { 0x0702, "W3D_CHUNK_HLOD_LOD_ARRAY" }, // wrapper around the array of objects for each level of detail
        { 0x0703, "W3D_CHUNK_HLOD_SUB_OBJECT_ARRAY_HEADER" }, // info on the objects in this level of detail array
        { 0x0704, "W3D_CHUNK_HLOD_SUB_OBJECT" }, // an object in this level of detail array
        { 0x0705, "W3D_CHUNK_HLOD_AGGREGATE_ARRAY" }, // array of aggregates, contains W3D_CHUNK_SUB_OBJECT_ARRAY_HEADER and W3D_CHUNK_SUB_OBJECT_ARRAY
        { 0x0706, "W3D_CHUNK_HLOD_PROXY_ARRAY" }, // array of proxies, used for application-defined purposes, provides a name and a bone.
        { 0x0707, "W3D_CHUNK_HLOD_LIGHT_ARRAY" },     // TT: array of lights (5.0)
        { 0x0740, "W3D_CHUNK_BOX" }, // defines an collision box render object (W3dBoxStruct)
        { 0x0741, "W3D_CHUNK_SPHERE"}, //???
        { 0x0742, "W3D_CHUNK_RING" }, //???
        { 0x0750, "W3D_CHUNK_NULL_OBJECT" }, // defines a NULL object (W3dNullObjectStruct)
        { 0x0800, "W3D_CHUNK_LIGHTSCAPE" },  // wrapper for lights created with Lightscape.
        { 0x0801, "W3D_CHUNK_LIGHTSCAPE_LIGHT" }, // definition of a light created with Lightscape.
        { 0x0802, "W3D_CHUNK_LIGHT_TRANSFORM" }, // position and orientation (defined as right-handed 4x3 matrix transform W3dLightTransformStruct).
        { 0x0900, "W3D_CHUNK_DAZZLE" }, // wrapper for a glare object.Creates halos and flare lines seen around a bright light source
        { 0x0901, "W3D_CHUNK_DAZZLE_NAME" }, // null-terminated string, name of the dazzle (typical w3d object naming: "container.object")
        { 0x0902, "W3D_CHUNK_DAZZLE_TYPENAME" }, // null-terminated string, type of dazzle (from dazzle.ini)
        { 0x0A00, "W3D_CHUNK_SOUNDROBJ" }, // description of a sound render object
        { 0x0A01, "W3D_CHUNK_SOUNDROBJ_HEADER" }, // general information such as name and version
        { 0x0A02, "W3D_CHUNK_SOUNDROBJ_DEFINITION" }, // chunk containing the definition of the sound that is to play
        { 0x0B00, "W3D_CHUNK_SHDMESH" },              // Generals: Shader mesh (multiple submeshes, scalable shaders)
        { 0x0B01, "W3D_CHUNK_SHDMESH_NAME" },         // Generals
        { 0x0B02, "W3D_CHUNK_SHDMESH_HEADER" },       // Generals
        { 0x0B03, "W3D_CHUNK_SHDMESH_USER_TEXT" },    // Generals: MAX comment field
        { 0x0B20, "W3D_CHUNK_SHDSUBMESH" },           // Generals: wrapper for submesh
        { 0x0B21, "W3D_CHUNK_SHDSUBMESH_HEADER" },    // Generals
        { 0x0B40, "W3D_CHUNK_SHDSUBMESH_SHADER" },    // Generals: shader wrapper
        { 0x0B41, "W3D_CHUNK_SHDSUBMESH_SHADER_CLASSID" }, // Generals
        { 0x0B42, "W3D_CHUNK_SHDSUBMESH_SHADER_DEF" },    // Generals
        { 0x0B43, "W3D_CHUNK_SHDSUBMESH_VERTICES" },  // Generals: array of vertices
        { 0x0B44, "W3D_CHUNK_SHDSUBMESH_VERTEX_NORMALS" },// Generals: normals
        { 0x0B45, "W3D_CHUNK_SHDSUBMESH_TRIANGLES" }, // Generals: 16-bit tri indices
        { 0x0B46, "W3D_CHUNK_SHDSUBMESH_VERTEX_SHADE_INDICES" }, // Generals: per-vertex shade indices
        { 0x0B47, "W3D_CHUNK_SHDSUBMESH_UV0" },       // Generals: UV0 coords
        { 0x0B48, "W3D_CHUNK_SHDSUBMESH_UV1" },       // Generals: UV1 coords
        { 0x0B49, "W3D_CHUNK_SHDSUBMESH_TANGENT_BASIS_S" }, // Generals: tangent basis S
        { 0x0B4A, "W3D_CHUNK_SHDSUBMESH_TANGENT_BASIS_T" }, // Generals: tangent basis T
        { 0x0B4B, "W3D_CHUNK_SHDSUBMESH_TANGENT_BASIS_SXT" },// Generals: tangent basis SxT
        { 0x0B4C, "W3D_CHUNK_SHDSUBMESH_COLOR" },     // Generals: per-vertex color
        { 0x0B4D, "W3D_CHUNK_SHDSUBMESH_VERTEX_INFLUENCES" }, // Generals: skinning support
        { 0x0C00, "W3D_CHUNK_SECONDARY_VERTICES" },   // BFMEII: aka VERTICES_COPY
        { 0x0C01, "W3D_CHUNK_SECONDARY_VERTEX_NORMALS" }, // BFMEII: aka VERTEX_NORMALS_COPY
        { 0x0C02, "W3D_CHUNK_LIGHTMAP_UV" },          // BFMEII
        { 0x16490430, "W3D_CHUNK_SHDDEF_CLASS_VARS" },   // base vars: Name, SurfaceType
        { 0x16490450, "W3D_CHUNK_SHDDEF_PARAM_VARS" },   // per-shader params: TextureName, Colors, Bumpiness
    };

    auto it = chunkNames.find(id);
    if (it != chunkNames.end()) return it->second;
    return "UNKNOWN";
}

inline const std::unordered_map<uint32_t, std::string>& GetEditorChunkNameMap() {
    static const std::unordered_map<uint32_t, std::string> editorNames = {
        { 0x00050000, "CHUNKID_TERRAIN_DEF" },
        { 0x00050001, "CHUNKID_TILE_DEF" },
        { 0x00050002, "CHUNKID_LIGHT_DEF" },
        { 0x00050003, "CHUNKID_WAYPATH_DEF" },
        { 0x00050004, "CHUNKID_EDITOR_ZONE_DEF" },
        { 0x00050005, "CHUNKID_TRANSITION_DEF" },
        { 0x00050006, "CHUNKID_EDITOR_PHYS" },
        { 0x00050007, "CHUNKID_PRESET" },
        { 0x00050008, "CHUNKID_PRESETMGR" },
        { 0x00050009, "CHUNKID_NODEMGR" },
        { 0x0005000A, "CHUNKID_NODE_TERRAIN" },
        { 0x0005000B, "CHUNKID_NODE_TILE" },
        { 0x0005000C, "CHUNKID_NODE_OBJECTS" },
        { 0x0005000D, "CHUNKID_NODE_LIGHT" },
        { 0x0005000E, "CHUNKID_NODE_OLD_SOUND" },
        { 0x0005000F, "CHUNKID_NODE_WAYPATH" },
        { 0x00050010, "CHUNKID_NODE_ZONE" },
        { 0x00050011, "CHUNKID_NODE_TRANSITION" },
        { 0x00050012, "CHUNKID_EDITOR_SAVELOAD" },
        { 0x00050013, "CHUNKID_EDITOR_MISC" },
        { 0x00050014, "CHUNKID_NODE_TERRAIN_SECTION" },
        { 0x00050015, "CHUNKID_NODE_VIS_POINT" },
        { 0x00050016, "CHUNKID_VIS_POINT_DEF" },
        { 0x00050017, "CHUNKID_NODE_PATHFIND_START" },
        { 0x00050018, "CHUNKID_PATHFIND_START_DEF" },
        { 0x00050019, "CHUNKID_DUMMY_OBJECT_DEF" },
        { 0x0005001A, "CHUNKID_DUMMY_OBJECT" },
        { 0x0005001B, "CHUNKID_NODE_WAYPOINT" },
        { 0x0005001C, "CHUNKID_COVERSPOT_DEF" },
        { 0x0005001D, "CHUNKID_NODE_COVER_ATTACK_POINT" },
        { 0x0005001E, "CHUNKID_NODE_COVER_SPOT" },
        { 0x0005001F, "CHUNKID_NODE_DAMAGE_ZONE" },
        { 0x00050020, "CHUNKID_NODE_BUILDING_LEGACY" },
        { 0x00050021, "CHUNKID_NODE_SPAWN_POINT" },
        { 0x00050022, "CHUNKID_NODE_SPAWNER" },
        { 0x00050023, "CHUNKID_NODE_BUILDING" },
        { 0x00050024, "CHUNKID_NODE_BUILDING_CHILD" },
        { 0x00050025, "CHUNKID_NODE_NEW_SOUND" },
        { 0x00050026, "CHUNKID_EDITOR_ONLY_OBJECTS_DEF" },
        { 0x00050027, "CHUNKID_EDITOR_ONLY_OBJECTS" },
        { 0x00050028, "CHUNKID_EDITOR_PATHFIND_IMPORT_EXPORT" },
        { 0x00050029, "CHUNKID_EDITOR_LIGHT_SOLVE_SAVELOAD" },
        { 0x0005002A, "CHUNKID_HEIGHTFIELD_MGR" },
        { 0x08040528, "CHUNKID_OPTION_VARIABLES" },
        { 0x08040529, "CHUNKID_DIALOGUE_VARIABLES" },
        { 0x0804052A, "CHUNKID_DIALOGUE_OPTION" }
    };
    return editorNames;
}

inline const std::unordered_map<uint32_t, std::string>& GetSaveLoadChunkNameMap() {
    static const std::unordered_map<uint32_t, std::string> saveLoadNames = {
        { 0x00000100, "CHUNKID_SAVELOAD_BEGIN" },
        { 0x00000101, "CHUNKID_SAVELOAD_DEFMGR" },
        { 0x00000102, "CHUNKID_TWIDDLER" },
        { 0x00030000, "CHUNKID_WWAUDIO_BEGIN" }
    };
    return saveLoadNames;
}

inline const std::unordered_map<uint32_t, std::string>& GetFactoryChunkNameMap() {
    static const std::unordered_map<uint32_t, std::string> factoryNames = {
        { SIMPLEFACTORY_CHUNKID_OBJPOINTER, "SIMPLEFACTORY_CHUNKID_OBJPOINTER" },
        { SIMPLEFACTORY_CHUNKID_OBJDATA, "SIMPLEFACTORY_CHUNKID_OBJDATA" }
    };
    return factoryNames;
}

inline const std::unordered_map<uint32_t, std::string>& GetPhysicsChunkNameMap() {
    static const std::unordered_map<uint32_t, std::string> physicsNames = {
        { 0x01070002, "STATICPHYSDEF_CHUNK_PHYSDEF" },
        { 0x01070003, "STATICPHYSDEF_CHUNK_VARIABLES" }
    };
    return physicsNames;
}

inline std::string LookupDefinitionDBChunkName(uint32_t id, uint32_t parentId) {
    const auto& saveLoadNames = GetSaveLoadChunkNameMap();
    if (auto it = saveLoadNames.find(id); it != saveLoadNames.end()) {
        return it->second;
    }

    const auto& editorNames = GetEditorChunkNameMap();
    if (auto it = editorNames.find(id); it != editorNames.end()) {
        return it->second;
    }

    const auto& factoryNames = GetFactoryChunkNameMap();
    if (auto it = factoryNames.find(id); it != factoryNames.end()) {
        return it->second;
    }

    const auto& physicsNames = GetPhysicsChunkNameMap();
    if (auto it = physicsNames.find(id); it != physicsNames.end()) {
        return it->second;
    }

    if (id == COMMON_VARIABLE_CHUNK_ID) {
        if (parentId == 0 ||
            parentId == CHUNKID_SAVELOAD_DEFMGR ||
            parentId == SIMPLEFACTORY_CHUNKID_OBJDATA ||
            parentId == SIMPLEFACTORY_CHUNKID_OBJPOINTER ||
            parentId >= CHUNKID_COMMANDO_EDITOR_BEGIN ||
            parentId == 0x00000200)
        {
            return "CHUNKID_VARIABLES";
        }
    }

    if (id == 0x00000200 &&
        (parentId >= CHUNKID_COMMANDO_EDITOR_BEGIN ||
            parentId == SIMPLEFACTORY_CHUNKID_OBJDATA))
    {
        return "CHUNKID_BASE_CLASS";
    }

    return {};
}

inline std::string LabelForChunk(uint32_t id, ChunkItem* item, ChunkFileKind kindHint = ChunkFileKind::Unknown) {
    ChunkFileKind context = kindHint;
    if (context == ChunkFileKind::Unknown && item != nullptr) {
        context = item->fileKind;
    }
    uint32_t parentId = item && item->parent ? item->parent->id : 0;

    if (const auto& ext = LookupExternalChunkName(id); !ext.empty()) {
        return ext;
    }

    if (context == ChunkFileKind::DefinitionDB) {
        std::string dbName = LookupDefinitionDBChunkName(id, parentId);
        if (!dbName.empty()) {
            return dbName;
        }
    }

    // Special case: label microchunks inside CHUNKID_DATA as frame array entries
    if (parentId == 0x03150809) {
        if (item && item->parent) {
            const auto& siblings = item->parent->children;
            auto it = std::find_if(siblings.begin(), siblings.end(),
                [&](const std::shared_ptr<ChunkItem>& sibling) { return sibling.get() == item; });

            if (it != siblings.end()) {
                int index = std::distance(siblings.begin(), it);
                return "ChunkID_FRAME[" + std::to_string(index) + "]";
            }
        }
        return "ChunkID_FRAME[?]";
    }

    // Fallback to regular GetChunkName logic
    std::string name = GetChunkName(id, parentId);
    if (name == "UNKNOWN") {
        std::ostringstream fallback;
        fallback << "0x" << std::hex << id;
        return fallback.str();
    }

    return name;
}



