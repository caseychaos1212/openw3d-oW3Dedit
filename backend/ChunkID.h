#pragma once
#include <cstdint>
enum class ChunkID : uint32_t {
	
	W3D_CHUNK_MESH	=0x00000000,	//Mesh definition
		W3D_CHUNK_VERTICES	=0x00000002,	//array of vertices(array of W3dVectorStruct's)
		W3D_CHUNK_VERTEX_NORMALS	=0x00000003,	//array of normals(array of W3dVectorStruct's)
		W3D_CHUNK_MESH_USER_TEXT	=0x0000000C,	//Text from the MAX comment field(Null terminated string)
		W3D_CHUNK_VERTEX_INFLUENCES	=0x0000000E,	//Mesh Deformation vertex connections(array of W3dVertInfStruct's)
		W3D_CHUNK_MESH_HEADER3	=0x0000001F,	//mesh header contains general info about the mesh. (W3dMeshHeader3Struct)
		W3D_CHUNK_TRIANGLES	=0x00000020,	//New improved triangles chunk(array of W3dTriangleStruct's)
		W3D_CHUNK_VERTEX_SHADE_INDICES	=0x00000022,	//shade indexes for each vertex(array of uint32's)
		W3D_CHUNK_PRELIT_UNLIT	=0x00000023,	//optional unlit material chunk wrapper
		W3D_CHUNK_PRELIT_VERTEX	=0x00000024,	//optional vertex - lit material chunk wrapper
		W3D_CHUNK_PRELIT_LIGHTMAP_MULTI_PASS	=0x00000025,	//optional lightmapped multi - pass material chunk wrapper
		W3D_CHUNK_PRELIT_LIGHTMAP_MULTI_TEXTURE	=0x00000026,	//optional lightmapped multi - texture material chunk wrapper
		W3D_CHUNK_MATERIAL_INFO	=0x00000028,	//materials information, pass count, etc(contains W3dMaterialInfoStruct)
		W3D_CHUNK_SHADERS	=0x00000029,	//shaders(array of W3dShaderStruct's)
		W3D_CHUNK_VERTEX_MATERIALS	=0x0000002A,	//wraps the vertex materials
			W3D_CHUNK_VERTEX_MATERIAL	=0x0000002B,	//sub wraps the vertex materials
				W3D_CHUNK_VERTEX_MATERIAL_NAME	=0x0000002C,	//vertex material name(NULL - terminated string)
				W3D_CHUNK_VERTEX_MATERIAL_INFO	=0x0000002D,	//W3dVertexMaterialStruct
				W3D_CHUNK_VERTEX_MAPPER_ARGS0	=0x0000002E,	//Null - terminated string
				W3D_CHUNK_VERTEX_MAPPER_ARGS1	=0x0000002F,	//Null - terminated string
		W3D_CHUNK_TEXTURES	=0x00000030,	//wraps all of the texture info
			W3D_CHUNK_TEXTURE	=0x00000031,	//wraps a texture definition
			W3D_CHUNK_TEXTURE_NAME	=0x00000032,	//texture filename(NULL - terminated string)
			W3D_CHUNK_TEXTURE_INFO	=0x00000033,	//optional W3dTextureInfoStruct
		W3D_CHUNK_MATERIAL_PASS	=0x00000038,	//wraps the information for a single material pass
			W3D_CHUNK_VERTEX_MATERIAL_IDS	=0x00000039,	//single or per - vertex array of uint32 vertex material indices(check chunk size)
			W3D_CHUNK_SHADER_IDS	=0x0000003A,	//single or per - tri array of uint32 shader indices(check chunk size)
			W3D_CHUNK_DCG	=0x0000003B,	//per - vertex diffuse color values(array of W3dRGBAStruct's)
			W3D_CHUNK_DIG	=0x0000003C,	//per - vertex diffuse illumination values(array of W3dRGBStruct's)
			W3D_CHUNK_SCG	=0x0000003E,	//per - vertex specular color values(array of W3dRGBStruct's)
			W3D_CHUNK_TEXTURE_STAGE	=0x00000048,	//wrapper around a texture stage.
				W3D_CHUNK_TEXTURE_IDS	=0x00000049,	//single or per - tri array of uint32 texture indices(check chunk size)
				W3D_CHUNK_STAGE_TEXCOORDS	=0x0000004A,//per - vertex texture coordinates(array of W3dTexCoordStruct's)
				W3D_CHUNK_PER_FACE_TEXCOORD_IDS	=0x0000004B,	//indices to W3D_CHUNK_STAGE_TEXCOORDS, (array of Vector3i)
		W3D_CHUNK_DEFORM	=0x00000058,	//mesh deform or 'damage' information.
			W3D_CHUNK_DEFORM_SET	=0x00000059,	//set of deform information
			W3D_CHUNK_DEFORM_KEYFRAME	=0x0000005A,	//a keyframe of deform information in the set
			W3D_CHUNK_DEFORM_DATA	=0x0000005B,	//deform information about a single vertex
		W3D_CHUNK_PS2_SHADERS	=0x00000080,	//Shader info specific to the Playstation 2.
		W3D_CHUNK_AABTREE	=0x00000090,	//Axis - Aligned Box Tree for hierarchical polygon culling
			W3D_CHUNK_AABTREE_HEADER	=0x00000091, //catalog of the contents of the AABTree
			W3D_CHUNK_AABTREE_POLYINDICES	=0x00000092,	//array of uint32 polygon indices with count is mesh.PolyCount
			W3D_CHUNK_AABTREE_NODES	=0x00000093,	//array of W3dMeshAABTreeNode's with count is aabheader.NodeCount
	
	W3D_CHUNK_HIERARCHY	=0x00000100,	//hierarchy tree definition
		W3D_CHUNK_HIERARCHY_HEADER	=0x00000101,	//hierarchy header contains general info about the hierarchy
		W3D_CHUNK_PIVOTS	=0x00000102,	//contains an W3dPivotStructs for each node in the tree
		W3D_CHUNK_PIVOT_FIXUPS	=0x00000103,	//matrix transforms  from its original Max orientation to the simplified "translation-only" base pose used during export.
	
	W3D_CHUNK_ANIMATION	=0x00000200,	//hierarchy animation data
		W3D_CHUNK_ANIMATION_HEADER	=0x00000201,	//animation header contains general info about the hierarchy
		W3D_CHUNK_ANIMATION_CHANNEL	=0x00000202,	//channel of vectors
		W3D_CHUNK_BIT_CHANNEL	=0x00000203,	//channel of boolean values(e.g.visibility)
	
	W3D_CHUNK_COMPRESSED_ANIMATION	=0x00000280,	//compressed hierarchy animation data
		W3D_CHUNK_COMPRESSED_ANIMATION_HEADER	=0x00000281,	//describes playback rate, number of frames, and type of compression
		W3D_CHUNK_COMPRESSED_ANIMATION_CHANNEL	=0x00000282,	//compressed channel, format dependent on type of compression
		W3D_CHUNK_COMPRESSED_BIT_CHANNEL	=0x00000283,	//compressed bit stream channel, format dependent on type of compression
		W3D_CHUNK_COMPRESSED_ANIMATION_MOTION_CHANNEL = 0x00000284,    //adaptive delta/motion channel (BFMEII)

	W3D_CHUNK_MORPH_ANIMATION	=0x000002C0,	//hierarchy morphing animation data(morphs between poses, for facial animation)
		W3D_CHUNK_MORPHANIM_HEADER	=0x000002C1,	//W3dMorphAnimHeaderStruct describes playback rate, number of frames, and type of compression
		W3D_CHUNK_MORPHANIM_CHANNEL	=0x000002C2,	//wrapper for a channel
			W3D_CHUNK_MORPHANIM_POSENAME	=0x000002C3,	//name of the other anim which contains the poses for this morph channel
			W3D_CHUNK_MORPHANIM_KEYDATA	=0x000002C4,	//morph key data for this channel
		W3D_CHUNK_MORPHANIM_PIVOTCHANNELDATA	=0x000002C5,	//uin32 per pivot in the htree, indicating which channel controls the pivot
	
	W3D_CHUNK_HMODEL	=0x00000300,	//blueprint for a hierarchy model
		W3D_CHUNK_HMODEL_HEADER	=0x00000301,	//Header for the hierarchy model
		W3D_CHUNK_NODE	=0x00000302,	//render objects connected to the hierarchy
		W3D_CHUNK_COLLISION_NODE	=0x00000303,	//collision meshes connected to the hierarchy
		W3D_CHUNK_SKIN_NODE	=0x00000304,	//skins connected to the hierarchy
		OBSOLETE_W3D_CHUNK_HMODEL_AUX_DATA	=0x00000305,	//extension of the hierarchy model header
		OBSOLETE_W3D_CHUNK_SHADOW_NODE	=0x00000306,	//shadow object connected to the hierarchy
	
	W3D_CHUNK_LODMODEL	=0x00000400,	//blueprint for an LOD model.This is simply a collection of 'n' render objects, ordered in terms of their expected rendering costs. (highest is first)
		W3D_CHUNK_LODMODEL_HEADER	=0x00000401,	//Header for the LOD model
		W3D_CHUNK_LOD	=0x00000402,	//LOD Data
	
	W3D_CHUNK_COLLECTION	=0x00000420,	//collection of render object names
		W3D_CHUNK_COLLECTION_HEADER	=0x00000421,	//general info regarding the collection
		W3D_CHUNK_COLLECTION_OBJ_NAME	=0x00000422,	//contains a string which is the name of a render object
		W3D_CHUNK_PLACEHOLDER	=0x00000423,	//contains information about a 'dummy' object that will be instanced later
		W3D_CHUNK_TRANSFORM_NODE	=0x00000424,	//contains the filename of another w3d file that should be transformed by this node
	
	W3D_CHUNK_POINTS	=0x00000440,	//array of W3dVectorStruct's.  May appear in meshes, hmodels, lodmodels, or collections.
	
	W3D_CHUNK_LIGHT	=0x00000460,	//description of a light
		W3D_CHUNK_LIGHT_INFO	=0x00000461, //generic light parameters
		W3D_CHUNK_SPOT_LIGHT_INFO	=0x00000462,	//extra spot light parameters
		W3D_CHUNK_NEAR_ATTENUATION	=0x00000463,	//optional near attenuation parameters
		W3D_CHUNK_FAR_ATTENUATION	=0x00000464,	//optional far attenuation parameters
		W3D_CHUNK_SPOT_LIGHT_INFO_5_0 = 0x00000465,    //TT: extra spot light params (5.0)
		W3D_CHUNK_PULSE = 0x00000466,    //TT: pulse data (5.0)
	
	W3D_CHUNK_EMITTER	=0x00000500,	//description of a particle emitter
		W3D_CHUNK_EMITTER_HEADER	=0x00000501,	//general information such as name and version
		W3D_CHUNK_EMITTER_USER_DATA	=0x00000502,	//user - defined data that specific loaders can switch on
		W3D_CHUNK_EMITTER_INFO	=0x00000503,	//generic particle emitter definition
		W3D_CHUNK_EMITTER_INFOV2	=0x00000504,	//generic particle emitter definition(version 2.0)
		W3D_CHUNK_EMITTER_PROPS	=0x00000505,	//Key - frameable properties
		OBSOLETE_W3D_CHUNK_EMITTER_COLOR_KEYFRAME	=0x00000506,	//structure defining a single color keyframe
		OBSOLETE_W3D_CHUNK_EMITTER_OPACITY_KEYFRAME	=0x00000507,	//structure defining a single opacity keyframe
		OBSOLETE_W3D_CHUNK_EMITTER_SIZE_KEYFRAME	=0x00000508, //structure defining a single size keyframe
		W3D_CHUNK_EMITTER_LINE_PROPERTIES	=0x00000509,	//line properties, used by line rendering mode
		W3D_CHUNK_EMITTER_ROTATION_KEYFRAMES	=0x0000050A,	//rotation keys for the particles
		W3D_CHUNK_EMITTER_FRAME_KEYFRAMES	=0x0000050B,	//frame keys(u - v based frame animation)
		W3D_CHUNK_EMITTER_BLUR_TIME_KEYFRAMES	=0x0000050C,	//length of tail for line groups
		W3D_CHUNK_EMITTER_EXTRA_INFO = 0x0000050D,	//extra info for emitters
	
	W3D_CHUNK_AGGREGATE	=0x00000600,	//description of an aggregate object
		W3D_CHUNK_AGGREGATE_HEADER	=0x00000601,	//general information such as name and version
			W3D_CHUNK_AGGREGATE_INFO	=0x00000602,	//references to 'contained' models
		W3D_CHUNK_TEXTURE_REPLACER_INFO	=0x00000603,	//information about which meshes need textures replaced
		W3D_CHUNK_AGGREGATE_CLASS_INFO	=0x00000604,	//information about the original class that created this aggregate
	
	W3D_CHUNK_HLOD	=0x00000700,	//description of an HLod object(see HLodClass)
		W3D_CHUNK_HLOD_HEADER	=0x00000701,	//general information such as name and version
		W3D_CHUNK_HLOD_LOD_ARRAY	=0x00000702,	//wrapper around the array of objects for each level of detail
			W3D_CHUNK_HLOD_SUB_OBJECT_ARRAY_HEADER	=0x00000703,	//info on the objects in this level of detail array
			W3D_CHUNK_HLOD_SUB_OBJECT	=0x00000704,	//an object in this level of detail array
		W3D_CHUNK_HLOD_AGGREGATE_ARRAY	=0x00000705,	//array of aggregates, contains W3D_CHUNK_SUB_OBJECT_ARRAY_HEADER and W3D_CHUNK_SUB_OBJECT_ARRAY
		W3D_CHUNK_HLOD_PROXY_ARRAY	=0x00000706,	//array of proxies, used for application - defined purposes, provides a name and a bone.
	
	W3D_CHUNK_BOX	=0x00000740,	//defines an collision box render object(W3dBoxStruct)
	
	W3D_CHUNK_SPHERE	=0x00000741,	//Primative Sphere(uses microchunks)
	
	W3D_CHUNK_RING	=0x00000742,	//Primative Ring(uses microchunks)
	
	W3D_CHUNK_NULL_OBJECT	=0x00000750,	//defines a NULL object(W3dNullObjectStruct)
	
	W3D_CHUNK_LIGHTSCAPE	=0x00000800,	//wrapper for lights created with Lightscape.
		W3D_CHUNK_LIGHTSCAPE_LIGHT	=0x00000801,	//definition of a light created with Lightscape.
		W3D_CHUNK_LIGHT_TRANSFORM	=0x00000802,	//position and orientation(defined as right - handed 4x3 matrix transform W3dLightTransformStruct).
	
	W3D_CHUNK_DAZZLE	=0x00000900,	//wrapper for a glare object.Creates halos and flare lines seen around a bright light source
		W3D_CHUNK_DAZZLE_NAME	=0x00000901,	//null - terminated string, name of the dazzle(typical w3d object naming : "container.object")
		W3D_CHUNK_DAZZLE_TYPENAME	=0x00000902,	//null - terminated string, type of dazzle(from dazzle.ini)
	
	W3D_CHUNK_SOUNDROBJ	=0x00000A00,	//description of a sound render object
		W3D_CHUNK_SOUNDROBJ_HEADER	=0x00000A01,	//general information such as name and version
		W3D_CHUNK_SOUNDROBJ_DEFINITION	=0x00000A02,	//chunk containing the definition of the sound that is to play
		};
