#pragma once
#include <unordered_map>
#include <string>


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
    if (parentId == 0xA02 && id == 0x100) {
        return "CHUNKID_VARIABLES";
    }
    if (parentId == 0xA02 && id == 0x200) {
        return "CHUNKID_BASE_CLASS";
    }
    if (parentId == 0x200 && id == 0x100) {
        return "CHUNKID_VARIABLES";
    }
 
    static const std::unordered_map<uint32_t, std::string> chunkNames = {
        { 0x0000, "W3D_CHUNK_MESH" }, // Mesh definition
        { 0x0001, "CHUNKID_SPHERE_DEF" }, // Header for mesh (Legacy)
        { 0x0002, "W3D_CHUNK_VERTICES" }, // array of vertices (array of W3dVectorStruct's)
        { 0x0003, "W3D_CHUNK_VERTEX_NORMALS" }, // array of normals (array of W3dVectorStruct's)
		{ 0x0004, "W3D_CHUNK_SPHERE_CHUNKID_SCALE_CHANNEL" }, // array of vertex colors (array of W3dRGBAStruct's)
		{ 0x0005, "W3D_CHUNK_SPHERE_CHUNKID_VECTOR_CHANNEL" }, // array of texture coordinates (array of W3dTexCoordStruct's)        
        { 0x000C, "W3D_CHUNK_MESH_USER_TEXT"}, // Text from the MAX comment field (Null terminated string)
        { 0x000E, "W3D_CHUNK_VERTEX_INFLUENCES" }, // Mesh Deformation vertex connections (array of W3dVertInfStruct's)
        { 0x001F, "W3D_CHUNK_MESH_HEADER3" }, // mesh header contains general info about the mesh. (W3dMeshHeader3Struct)
        { 0x0020, "W3D_CHUNK_TRIANGLES" }, // New improved triangles chunk (array of W3dTriangleStruct's)
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
        { 0x0048, "W3D_CHUNK_TEXTURE_STAGE" }, // wrapper around a texture stage.
        { 0x0049, "W3D_CHUNK_TEXTURE_IDS" }, // single or per-tri array of uint32 texture indices (check chunk size)
        { 0x004A, "W3D_CHUNK_STAGE_TEXCOORDS" }, // per-vertex texture coordinates (array of W3dTexCoordStruct's)
        { 0x004B, "W3D_CHUNK_PER_FACE_TEXCOORD_IDS" }, // indices to W3D_CHUNK_STAGE_TEXCOORDS, (array of Vector3i)
        { 0x0058, "W3D_CHUNK_DEFORM" }, // mesh deform or 'damage' information.
        { 0x0059, "W3D_CHUNK_DEFORM_SET" }, // set of deform information
        { 0x005A, "W3D_CHUNK_DEFORM_KEYFRAME" }, // a keyframe of deform information in the set
        { 0x005B, "W3D_CHUNK_DEFORM_DATA" }, // deform information about a single vertex
        { 0x0080, "W3D_CHUNK_PS2_SHADERS" }, // Shader info specific to the Playstation 2.
        { 0x0090, "W3D_CHUNK_AABTREE" }, // Axis-Aligned Box Tree for hierarchical polygon culling
        { 0x0091, "W3D_CHUNK_AABTREE_HEADER" }, // catalog of the contents of the AABTree
        { 0x0092, "W3D_CHUNK_AABTREE_POLYINDICES" }, // array of uint32 polygon indices with count=mesh.PolyCount
        { 0x0093, "W3D_CHUNK_AABTREE_NODES" }, // array of W3dMeshAABTreeNode's with count=aabheader.NodeCount
        { 0x0100, "W3D_CHUNK_HIERARCHY" }, // hierarchy tree definition
        { 0x0101, "W3D_CHUNK_HIERARCHY_HEADER" }, // hierarchy tree Header
        { 0x0102, "W3D_CHUNK_PIVOTS" },       // pivots
        { 0x0103, "W3D_CHUNK_PIVOT_FIXUPS" }, // only needed by the exporter...
        { 0x0200, "W3D_CHUNK_ANIMATION" }, // hierarchy animation data
        { 0x0201, "W3D_CHUNK_ANIMATION_HEADER" }, // Animation Header
        { 0x0202, "W3D_CHUNK_ANIMATION_CHANNEL" }, // channel of vectors
        { 0x0203, "W3D_CHUNK_BIT_CHANNEL" }, // channel of boolean values (e.g. visibility)
        { 0x0280, "W3D_CHUNK_COMPRESSED_ANIMATION"}, // compressed hierarchy animation data
        { 0x0281, "W3D_CHUNK_COMPRESSED_ANIMATION_HEADER"}, // describes playback rate, number of frames, and type of compression
        { 0x0282, "W3D_CHUNK_COMPRESSED_ANIMATION_CHANNEL"}, // compressed channel, format dependent on type of compression
        { 0x0283, "W3D_CHUNK_COMPRESSED_BIT_CHANNEL"}, // compressed bit stream channel, format dependent on type of compression
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
    };

    auto it = chunkNames.find(id);
    if (it != chunkNames.end()) return it->second;
    return "UNKNOWN";
}
inline std::string LabelForChunk(uint32_t id, ChunkItem* item) {
    uint32_t parentId = item && item->parent ? item->parent->id : 0;

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



