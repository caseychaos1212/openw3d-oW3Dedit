#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <iomanip>
constexpr std::size_t W3D_NAME_LEN = 16;
/// Extract the major version (high 16 bits) from a packed 32-bit version.
constexpr uint16_t GetMajorVersion(uint32_t ver) {
	return static_cast<uint16_t>(ver >> 16);
}

/// Extract the minor version (low 16 bits) from a packed 32-bit version.
constexpr uint16_t GetMinorVersion(uint32_t ver) {
	return static_cast<uint16_t>(ver & 0xFFFF);
}


inline std::string FormatVersion(uint32_t ver) {
	uint16_t major = uint16_t(ver >> 16);
	uint16_t minor = uint16_t(ver & 0xFFFF);
	return std::to_string(major) + "." + std::to_string(minor);
}

// a tiny fluent helper to build up a vector<ChunkField>
struct ChunkFieldBuilder {
	std::vector<ChunkField>& F;

	explicit ChunkFieldBuilder(std::vector<ChunkField>& out) : F(out) {}

	// push any field
	void Push(std::string name, std::string type, std::string val) {
		F.emplace_back(std::move(name), std::move(type), std::move(val));
	}

	// push a Version field, formatted a.b
	void Version(std::string name, uint32_t raw) {
		Push(std::move(name), "string", FormatVersion(raw));
	}

	// push fixed-length W3D_NAME_LEN names
	void Name(const char* fieldName, const char* rawName, size_t maxLen = W3D_NAME_LEN) {
		F.emplace_back(
			fieldName,
			"string",
			FormatName(rawName, maxLen)
	}
	//push null-terminated string
	void NullTerm(std::string fieldName,
		const char* dataPtr,
		size_t chunkBytes,
		size_t maxLen = 256)
	{
		// Only scan at most chunkBytes, and also cap at maxLen
		size_t scanLen = std::min(chunkBytes, maxLen);
		size_t realLen = std::strnlen(dataPtr, scanLen);
		Push(std::move(fieldName), "string", std::string(dataPtr, realLen));
	}

	// Push RGB
	void RGB(std::string name, const W3dRGBStruct& c) {
		std::ostringstream o;
		o << int(c.R) << " " << int(c.G) << " " << int(c.B);
		Push(std::move(name), "RGB", o.str());
	}

	void RGBA(std::string name, const W3dRGBAStruct& c) {
		Push(std::move(name), "RGBA",
			std::to_string(c.R) + " " + std::to_string(c.G) + " " + std::to_string(c.B) + " " + std::to_string(c.A));
	}
	
	// push a uint16 in decimal
	void UInt16(std::string name, uint16_t v) {
		Push(std::move(name), "uint16", std::to_string(v));
	}

	// push a uint32 in decimal
	void UInt32(std::string name, uint32_t v) {
		Push(std::move(name), "uint32", std::to_string(v));
	}

	// push a singed int32 in decimal
	void Int32(std::string name, int32_t v) {
		Push(std::move(name), "int32", std::to_string(v));
	}

	// push an array of int32: name, concatenated values, type "int32[n]"
	void UInt32Array(const std::string& name, const int32_t* arr, size_t count) {
		std::ostringstream oss;
		for (size_t i = 0; i < count; ++i) {
			if (i) oss << ' ';
			oss << arr[i];
		}
		Push(name, "int32[" + std::to_string(count) + "]", oss.str());
	}

	// push a flag if bit is set
	void Flag(uint32_t bits, uint32_t mask, const char* flagName) {
		if (bits & mask) Push("Attributes", "flag", flagName);

		// New helper for PrelitVersion style 
		// if the mask bit is not set             => "N/A"
		// else if version==0                     => "UNKNOWN"
		// else                                     => FormatVersion(version)
	void Versioned(std::string name,
		uint32_t bits,
		uint32_t mask,
		uint32_t version) {
		if ((bits & mask) == 0) {
			Push(std::move(name), "string", "N/A");
		}
		else if (version == 0) {
			Push(std::move(name), "string", "UNKNOWN");
		}
		else {
			Push(std::move(name), "string", FormatVersion(version));
		}

	// push a 3-component vector
	void Vec3(std::string name, const W3dVectorStruct & v) {
	Push(std::move(name), "vector3", FormatVec3(v));
		}

	void Vec3i(std::string name, const Vector3i& v) {
		Push(std::move(name), "vector3i", FormatVec3(v));
	}

	void Float(std::string name, float v) {
		Push(std::move(name), "float", FormatFloat(v));
	}

	// Shaders

	void DepthCompare(const char* name, uint8_t raw) {
		Push(name, "DepthCompare",
			ToString(static_cast<::DepthCompare>(raw)));
	}
	void DepthMask(const char* name, uint8_t raw) {
		Push(name, "DepthMask",
			ToString(static_cast<DepthMask>(raw)));
	}
	void DestBlend(const char* name, uint8_t raw) {
		Push(name, "DestBlend",
			ToString(static_cast<DestBlend>(raw)));
	}
	void PriGradient(const char* name, uint8_t raw) {
		Push(name, "PriGradient",
			ToString(static_cast<PriGradient>(raw)));
	}
	void SecGradient(const char* name, uint8_t raw) {
		Push(name, "SecGradient",
			ToString(static_cast<SecGradient>(raw)));
	}
	void SrcBlend(const char* name, uint8_t raw) {
		Push(name, "SrcBlend",
			ToString(static_cast<SrcBlend>(raw)));
	}
	void Texturing(const char* name, uint8_t raw) {
		Push(name, "Texturing",
			ToString(static_cast<Texturing>(raw)));
	}
	void DetailColorFunc(const char* name, uint8_t raw) {
		Push(name, "DetailColorFunc",
			ToString(static_cast<DetailColorFunc>(raw)));
	}
	void DetailAlphaFunc(const char* name, uint8_t raw) {
		Push(name, "DetailAlphaFunc",
			ToString(static_cast<DetailAlphaFunc>(raw)));
	}
	void AlphaTest(const char* name, uint8_t raw) {
		Push(name, "AlphaTest",
			ToString(static_cast<AlphaTest>(raw)));
	}
	//ps2 shaders
	void Ps2DepthCompare(const char* name, uint8_t raw) {
		Push(name, "DepthCompare",
			ToString(static_cast<DepthCompare>(raw)));
	}
	void Ps2DepthMask(const char* name, uint8_t raw) {
		Push(name, "DepthMask",
			ToString(static_cast<DepthMask>(raw)));
	}
	void Ps2PriGradient(const char* name, uint8_t raw) {
		Push(name, "PriGradient",
			ToString(static_cast<DepthCompare>(raw)));
	}
	void Ps2Texturing(const char* name, uint8_t raw) {
		Push(name, "Texturing",
			ToString(static_cast<Texturing>(raw)));
	}
	void Ps2AlphaTest(const char* name, uint8_t raw) {
		Push(name, "AlphaTest",
			ToString(static_cast<AlphaTest>(raw)));
	}
	void AParam(const char* name, uint8_t raw) {
		Push(name, "AParam",
			ToString(static_cast<AParam>(raw)));
	}
	void BParamCompare(const char* name, uint8_t raw) {
		Push(name, "BParam",
			ToString(static_cast<BParam>(raw)));
	}
	void CParam(const char* name, uint8_t raw) {
		Push(name, "CParam",
			ToString(static_cast<CParam>(raw)));
	}
	void DParam(const char* name, uint8_t raw) {
		Push(name, "DParam",
			ToString(static_cast<DParam>(raw)));
	}


	// Single UV (as vector2)
	void TexCoord(std::string name, const W3dTexCoordStruct& tc) {
		Push(std::move(name), "vector2", FormatTexCoord(tc));
	}

	// UV as separate components (optional, if you ever want it)
	void TexCoordUV(std::string base, const W3dTexCoordStruct& tc) {
		Push(base + ".U", "float", std::to_string(tc.U));
		Push(base + ".V", "float", std::to_string(tc.V));
	}

	// Array of UVs from a pointer + count
	void TexCoordArray(const char* base,
		const W3dTexCoordStruct* ptr,
		size_t count) {
		for (size_t i = 0; i < count; ++i) {
			TexCoord(std::string(base) + "[" + std::to_string(i) + "]", ptr[i]);
		}
	}
	};



inline std::string FormatTexCoord(const W3dTexCoordStruct& t) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6) << t.U << ' ' << t.V;
	return oss.str();
}

inline std::string FormatFloat(float f) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6) << f;
	return oss.str();
}

inline std::string FormatName(const char* name, size_t len) {

	return std::string(name, strnlen(name, len));
}

inline std::string FormatUInt32(uint32_t data) {

	return std::to_string(data);
}

inline std::string FormatUInt16(uint16_t data) {

	return std::to_string(data);
}


inline std::string FormatRGBA(const W3dRGBAStruct& c) {
	return "("
		+ std::to_string(c.R) + " "
		+ std::to_string(c.G) + " "
		+ std::to_string(c.B) + " "
		+ std::to_string(c.A) + ")";
}

struct ChunkField {
	std::string field;
	std::string type;
	std::string value;
};

struct W3dVectorStruct
{
	float		X;							// X,Y,Z coordinates
	float		Y;
	float		Z;
};

inline std::string FormatVec3(const W3dVectorStruct& v) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6)
		<< v.X << " " << v.Y << " " << v.Z;
	return oss.str();
}

inline std::string ToHex(uint32_t value, int width = 0) {
	std::ostringstream oss;
	oss << std::hex << std::uppercase;
	if (width > 0) {
		oss << std::setw(width) << std::setfill('0');
	}
	oss << value;
	return oss.str();
}

struct W3dVertInfStruct
{
	uint16_t BoneIdx[2];
	uint16_t Weight[2];
};


/////////////////////////////////////////////////////////////////////////////////////////////
// Flags for the Mesh Attributes member
/////////////////////////////////////////////////////////////////////////////////////////////
enum class MeshAttr : uint32_t {
	W3D_MESH_FLAG_NONE = 0x00000000,		// plain ole normal mesh
	W3D_MESH_FLAG_COLLISION_BOX = 0x00000001,		// (obsolete as of 4.1) mesh is a collision box (should be 8 verts, should be hidden, etc)
	W3D_MESH_FLAG_SKIN = 0x00000002,		// (obsolete as of 4.1) skin mesh 
	W3D_MESH_FLAG_SHADOW = 0x00000004,		// (obsolete as of 4.1) intended to be projected as a shadow
	W3D_MESH_FLAG_ALIGNED = 0x00000008,		// (obsolete as of 4.1) always aligns with camera

	W3D_MESH_FLAG_COLLISION_TYPE_MASK = 0x00000FF0,		// mask for the collision type bits
	W3D_MESH_FLAG_COLLISION_TYPE_SHIFT = 4,		// shifting to get to the collision type bits
	W3D_MESH_FLAG_COLLISION_TYPE_PHYSICAL = 0x00000010,		// physical collisions
	W3D_MESH_FLAG_COLLISION_TYPE_PROJECTILE = 0x00000020,		// projectiles (rays) collide with this
	W3D_MESH_FLAG_COLLISION_TYPE_VIS = 0x00000040,		// vis rays collide with this mesh
	W3D_MESH_FLAG_COLLISION_TYPE_CAMERA = 0x00000080,		// camera rays/boxes collide with this mesh
	W3D_MESH_FLAG_COLLISION_TYPE_VEHICLE = 0x00000100,		// vehicles collide with this mesh (and with physical collision meshes)

	W3D_MESH_FLAG_HIDDEN = 0x00001000,		// this mesh is hidden by default
	W3D_MESH_FLAG_TWO_SIDED = 0x00002000,		// render both sides of this mesh
	OBSOLETE_W3D_MESH_FLAG_LIGHTMAPPED = 0x00004000,		// obsolete lightmapped mesh
	// NOTE: retained for backwards compatibility - use W3D_MESH_FLAG_PRELIT_* instead.
	W3D_MESH_FLAG_CAST_SHADOW = 0x00008000,		// this mesh casts shadows

	W3D_MESH_FLAG_GEOMETRY_TYPE_MASK = 0x00FF0000,		// (introduced with 4.1)
	W3D_MESH_FLAG_GEOMETRY_TYPE_NORMAL = 0x00000000,		// (4.1+) normal mesh geometry
	W3D_MESH_FLAG_GEOMETRY_TYPE_CAMERA_ALIGNED = 0x00010000,		// (4.1+) camera aligned mesh
	W3D_MESH_FLAG_GEOMETRY_TYPE_SKIN = 0x00020000,		// (4.1+) skin mesh
	OBSOLETE_W3D_MESH_FLAG_GEOMETRY_TYPE_SHADOW = 0x00030000,		// (4.1+) shadow mesh OBSOLETE!
	W3D_MESH_FLAG_GEOMETRY_TYPE_AABOX = 0x00040000,		// (4.1+) aabox OBSOLETE!
	W3D_MESH_FLAG_GEOMETRY_TYPE_OBBOX = 0x00050000,		// (4.1+) obbox OBSOLETE!
	W3D_MESH_FLAG_GEOMETRY_TYPE_CAMERA_ORIENTED = 0x00060000,		// (4.1+) camera oriented mesh (points _towards_ camera)

	W3D_MESH_FLAG_PRELIT_MASK = 0x0F000000,		// (4.2+) 
	W3D_MESH_FLAG_PRELIT_UNLIT = 0x01000000,		// mesh contains an unlit material chunk wrapper
	W3D_MESH_FLAG_PRELIT_VERTEX = 0x02000000,		// mesh contains a precalculated vertex-lit material chunk wrapper 
	W3D_MESH_FLAG_PRELIT_LIGHTMAP_MULTI_PASS = 0x04000000,		// mesh contains a precalculated multi-pass lightmapped material chunk wrapper
	W3D_MESH_FLAG_PRELIT_LIGHTMAP_MULTI_TEXTURE = 0x08000000,		// mesh contains a precalculated multi-texture lightmapped material chunk wrapper

	W3D_MESH_FLAG_SHATTERABLE = 0x10000000,		// this mesh is shatterable.
	W3D_MESH_FLAG_NPATCHABLE = 0x20000000,		// it is ok to NPatch this mesh
};

enum class VertexChannelAttr : uint32_t {
	W3D_VERTEX_CHANNEL_LOCATION = 0x00000001,	// object-space location of the vertex
	W3D_VERTEX_CHANNEL_NORMAL = 0x00000002,	// object-space normal for the vertex
	W3D_VERTEX_CHANNEL_TEXCOORD = 0x00000004,	// texture coordinate
	W3D_VERTEX_CHANNEL_COLOR = 0x00000008,	// vertex color
	W3D_VERTEX_CHANNEL_BONEID = 0x00000010,	// per-vertex bone id for skins
};

constexpr uint32_t W3D_FACE_CHANNEL_FACE = 0x00000001;	// basic face info, W3dTriStruct...

enum class SortLevel : int32_t {
	SORT_LEVEL_NONE = 0,
	MAX_SORT_LEVEL = 32,
	SORT_LEVEL_BIN1 = 20,
	SORT_LEVEL_BIN2 = 15,
	SORT_LEVEL_BIN3 = 10,
};

struct W3dMeshHeader3Struct
{
	uint32_t					Version;
	uint32_t					Attributes;

	char						MeshName[W3D_NAME_LEN];
	char						ContainerName[W3D_NAME_LEN];

	//
	// Counts, these can be regarded as an inventory of what is to come in the file.
	//
	uint32_t					NumTris;				// number of triangles
	uint32_t					NumVertices;		// number of unique vertices
	uint32_t					NumMaterials;		// number of unique materials
	uint32_t					NumDamageStages;	// number of damage offset chunks
	int32_t				SortLevel;			// static sorting level of this mesh
	uint32_t					PrelitVersion;		// mesh generated by this version of Lightmap Tool
	uint32_t					FutureCounts[1];	// future counts

	uint32_t					VertexChannels;	// bits for presence of types of per-vertex info
	uint32_t					FaceChannels;		// bits for presence of types of per-face info

	//
	// Bounding volumes
	//
	W3dVectorStruct		Min;					// Min corner of the bounding box
	W3dVectorStruct		Max;					// Max corner of the bounding box
	W3dVectorStruct		SphCenter;			// Center of bounding sphere
	float					SphRadius;			// Bounding sphere radius

};

inline std::string FormatString(const char* raw, size_t maxLen) {
	size_t len = ::strnlen(raw, maxLen);
	return std::string(raw, len);
}

struct W3dNullTermString {
	std::string value;

	W3dNullTermString() = default;

	
	W3dNullTermString(const uint8_t* data, size_t size) {
		parse(data, size);
	}

	void parse(const uint8_t* data, size_t size) {
		std::string cur;
		for (size_t i = 0; i < size; ++i) {
			auto c = data[i];
			if (std::isprint(c) || c == '\t') {
				
				cur.push_back(char(c));
			}
			else {
				
				if (!cur.empty()) {
					if (!value.empty()) value.push_back(' ');
					value += cur;
					cur.clear();
				}
			}
		}
		
		if (!cur.empty()) {
			if (!value.empty()) value.push_back(' ');
			value += cur;
		}
	}
};

struct W3dTriStruct
{
	uint32_t					Vindex[3];			// vertex,vnormal,texcoord,color indices
	uint32_t					Attributes;			// attributes bits
	W3dVectorStruct		Normal;				// plane normal
	uint32_t				Dist;					// plane distance
};

struct W3dMaterialInfoStruct
{
	uint32_t		PassCount;				// how many material passes this render object uses
	uint32_t		VertexMaterialCount;	// how many vertex materials are used
	uint32_t		ShaderCount;			// how many shaders are used
	uint32_t		TextureCount;			// how many textures are used
};

// W3dShaderStruct bits.  These control every setting in the shader.  Use the helper functions
// to set them and test them more easily.

enum class DepthCompare : uint8_t {
	PASS_NEVER, PASS_LESS, PASS_EQUAL, PASS_LEQUAL,
	PASS_GREATER, PASS_NOTEQUAL, PASS_GEQUAL, PASS_ALWAYS,
	MAX
};

inline const char* ToString(DepthCompare v) {
	static constexpr const char* names[] = {
		"Pass Never","Pass Less","Pass Equal","Pass Less or Equal",
		"Pass Greater","Pass Not Equal","Pass Greater or Equal","Pass Always"
	};
	return names[std::min<int>(int(v), int(DepthCompare::MAX) - 1)];
}

enum class DepthMask : uint8_t { WRITE_DISABLE, WRITE_ENABLE, MAX 
};

inline const char* ToString(DepthMask v) {
	static constexpr const char* names[] = { "Write Disable", "Write Enable" };
	return names[std::min<int>(int(v), int(DepthMask::MAX) - 1)];
}

enum class DestBlend : uint8_t {
	ZERO, ONE, SRC_COLOR, ONE_MINUS_SRC_COLOR,
	SRC_ALPHA, ONE_MINUS_SRC_ALPHA, SRC_COLOR_PREFOG, MAX
};

inline const char* ToString(DestBlend v) {
	static constexpr const char* names[] = {
	  "Zero","One","Src Color","One Minus Src Color",
	  "Src Alpha","One Minus Src Alpha","Src Color Prefog"
	};
	return names[std::min<int>(int(v), int(DestBlend::MAX) - 1)];
}
enum class PriGradient : uint8_t {
	DISABLE, MODULATE, ADD, BUMPENVMAP, MAX
};

inline const char* ToString(PriGradient v) {
	static constexpr const char* names[] = {
	  "Disable","Modulate","Add","Bump-Environment"
	};
	return names[std::min<int>(int(v), int(PriGradient::MAX) - 1)];
}

enum class SecGradient : uint8_t {
	DISABLE, ENABLE, MAX
};

inline const char* ToString(SecGradient v) {
	static constexpr const char* names[] = {
	  "Disable","Enable"
	};
	return names[std::min<int>(int(v), int(SecGradient::MAX) - 1)];
}

enum class SrcBlend : uint8_t {
	ZERO, ONE, SRC_ALPHA, ONE_MINUS_SRC_ALPHA, MAX
};

inline const char* ToString(SrcBlend v) {
	static constexpr const char* names[] = {
	  "Zero","One","Src Alpha","One Minus Src Alpha"
	};
	return names[std::min<int>(int(v), int(SrcBlend::MAX) - 1)];
}

enum class Texturing : uint8_t {
	DISABLE, ENABLE, MAX
};

inline const char* ToString(Texturing v) {
	static constexpr const char* names[] = {
	  "Disable","Enable"
	};
	return names[std::min<int>(int(v), int(Texturing::MAX) - 1)];
}

enum class DetailColorFunc : uint8_t {
	DISABLE, DETAIL, SCALE, INVSCALE, ADD, SUB, SUBR, BLEND, DETAILBLEND, MAX
};

inline const char* ToString(DetailColorFunc v) {
	static constexpr const char* names[] = {
	  "Disable","Detail","Scale","InvScale","Add","Sub","SubR","Blend","DetailBlend"
	};
	return names[std::min<int>(int(v), int(DetailColorFunc::MAX) - 1)];
}

enum class DetailAlphaFunc : uint8_t {
	DISABLE, DETAIL, SCALE, INVSCALE, MAX
};

inline const char* ToString(DetailAlphaFunc v) {
	static constexpr const char* names[] = {
	  "Disable","Detail","Scale","InvScale"
	};
	return names[std::min<int>(int(v), int(DetailAlphaFunc::MAX) - 1)];
}

enum class AlphaTest : uint8_t {
	DISABLE, ENABLE, MAX
};

inline const char* ToString(AlphaTest v) {
	static constexpr const char* names[] = {
	  "Disable","Enable"
	};
	return names[std::min<int>(int(v), int(AlphaTest::MAX) - 1)];
}



struct W3dShaderStruct
{
	W3dShaderStruct(void) {}
	uint8_t						DepthCompare;
	uint8_t						DepthMask;
	uint8_t						ColorMask;		// now obsolete and ignored
	uint8_t						DestBlend;
	uint8_t						FogFunc;			// now obsolete and ignored
	uint8_t						PriGradient;
	uint8_t						SecGradient;
	uint8_t						SrcBlend;
	uint8_t						Texturing;
	uint8_t						DetailColorFunc;
	uint8_t						DetailAlphaFunc;
	uint8_t						ShaderPreset;	// now obsolete and ignored
	uint8_t						AlphaTest;
	uint8_t						PostDetailColorFunc;
	uint8_t						PostDetailAlphaFunc;
	uint8_t						pad[1];
};

// each of these masks lives in the low bits of Attributes
constexpr std::array<std::pair<uint32_t, std::string_view>, 4> VERTMAT_BASIC_FLAGS = { {
	{0x00000001, "W3DVERTMAT_USE_DEPTH_CUE"},
	{0x00000002, "W3DVERTMAT_ARGB_EMISSIVE_ONLY"},
	{0x00000004, "W3DVERTMAT_COPY_SPECULAR_TO_DIFFUSE"},
	{0x00000008, "W3DVERTMAT_DEPTH_CUE_TO_ALPHA"},
} };

// Stage‑mapping comes in nibbles at 0xF00 and 0xF000
constexpr std::array<std::pair<uint8_t, std::string_view>, 6> VERTMAT_STAGE_MAPPING = { {
	{0, "W3DVERTMAT_STAGE?_MAPPING_UV"},
	{1, "W3DVERTMAT_STAGE?_MAPPING_ENVIRONMENT"},
	{2, "W3DVERTMAT_STAGE?_MAPPING_CHEAP_ENVIRONMENT"},
	{3, "W3DVERTMAT_STAGE?_MAPPING_SCREEN"},
	{4, "W3DVERTMAT_STAGE?_MAPPING_LINEAR_OFFSET"},
	{5, "W3DVERTMAT_STAGE?_MAPPING_SILHOUETTE"},
} };

// PSX transparency bits at 0x00F00000
constexpr std::array<std::pair<uint32_t, std::string_view>, 6> VERTMAT_PSX_FLAGS = { {
	{0x0010'0000, "W3DVERTMAT_PSX_NO_RT_LIGHTING"},
	{0x0020'0000, "W3DVERTMAT_PSX_TRANS_NONE"},
	{0x0030'0000, "W3DVERTMAT_PSX_TRANS_100"},
	{0x0040'0000, "W3DVERTMAT_PSX_TRANS_50"},
	{0x0050'0000, "W3DVERTMAT_PSX_TRANS_25"},
	{0x0060'0000, "W3DVERTMAT_PSX_TRANS_MINUS_100"},
} };



/////////////////////////////////////////////////////////////////////////////////////////////
// rgb color, one byte per channel, padded to an even 4 bytes
/////////////////////////////////////////////////////////////////////////////////////////////
struct W3dRGBStruct {
	uint8_t R = 0;
	uint8_t G = 0;
	uint8_t B = 0;
	// pad byte is only needed if you're overlaying on packed data:
	uint8_t pad = 0;

	constexpr W3dRGBStruct() noexcept = default;
	constexpr W3dRGBStruct(uint8_t r, uint8_t g, uint8_t b) noexcept
		: R(r), G(g), B(b) {
	}

	// integer setter
	void Set(uint8_t r, uint8_t g, uint8_t b) noexcept {
		R = r; G = g; B = b;
	}

	// normalized‑float setter (expects [0,1])
	void Set(float r, float g, float b) noexcept {
		// clamp to [0,1], then scale to [0,255]
		R = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
		G = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
		B = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
	}

	// comparison
	constexpr bool operator==(const W3dRGBStruct& o) const noexcept = default;
	constexpr bool operator!=(const W3dRGBStruct& o) const noexcept = default;

	// addition with saturation
	W3dRGBStruct& operator+=(const W3dRGBStruct& o) noexcept {
		R = static_cast<uint8_t>(std::min<unsigned>(R + o.R, 255u));
		G = static_cast<uint8_t>(std::min<unsigned>(G + o.G, 255u));
		B = static_cast<uint8_t>(std::min<unsigned>(B + o.B, 255u));
		return *this;
	}

	// multiplication (modulate)
	W3dRGBStruct& operator*=(const W3dRGBStruct& o) noexcept {
		R = static_cast<uint8_t>((static_cast<unsigned>(R) * o.R) / 255u);
		G = static_cast<uint8_t>((static_cast<unsigned>(G) * o.G) / 255u);
		B = static_cast<uint8_t>((static_cast<unsigned>(B) * o.B) / 255u);
		return *this;
	}

	// pack into 0xRRGGBB00
	uint32_t GetColor() const noexcept {
		return  (static_cast<uint32_t>(R) << 24)
			| (static_cast<uint32_t>(G) << 16)
			| (static_cast<uint32_t>(B) << 8);
	}
};

// free‐function aliases if you liked `+` and `*` returning new:
inline W3dRGBStruct operator+(W3dRGBStruct a, const W3dRGBStruct& b) noexcept { return a += b; }
inline W3dRGBStruct operator*(W3dRGBStruct a, const W3dRGBStruct& b) noexcept { return a *= b; }

struct W3dVertexMaterialStruct
{
	W3dVertexMaterialStruct(void) {}

	bool operator == (W3dVertexMaterialStruct vm)
	{
		return (Attributes == vm.Attributes
			&& Ambient == vm.Ambient
			&& Diffuse == vm.Diffuse
			&& Specular == vm.Specular
			&& Emissive == vm.Emissive
			&& Shininess == vm.Shininess
			&& Opacity == vm.Opacity
			&& Translucency == vm.Translucency);
	}

	bool operator != (W3dVertexMaterialStruct vm)
	{
		return (!(*this == vm));
	}

	uint32_t					Attributes;					// bitfield for the flags defined above
	W3dRGBStruct			Ambient;
	W3dRGBStruct			Diffuse;
	W3dRGBStruct			Specular;
	W3dRGBStruct			Emissive;
	float					Shininess;					// how tight the specular highlight will be, 1 - 1000 (default = 1)
	float				    Opacity;						// how opaque the material is, 0.0 = invisible, 1.0 = fully opaque (default = 1)
	float				    Translucency;				// how much light passes through the material. (default = 0)
};



/////////////////////////////////////////////////////////////////////////////////////////////
// Texture Animation parameters
// May occur inside a texture chunk if its needed
/////////////////////////////////////////////////////////////////////////////////////////////
/*
enum class TextureAttr : uint16_t {
	W3DTEXTURE_PUBLISH = 0x0001,		// this texture should be "published" (indirected so its changeable in code)
	W3DTEXTURE_RESIZE_OBSOLETE = 0x0002,		// this texture should be resizeable (OBSOLETE!!!)
	W3DTEXTURE_NO_LOD = 0x0004,		// this texture should not have any LOD (mipmapping or resizing)
	W3DTEXTURE_CLAMP_U = 0x0008,		// this texture should be clamped on U
	W3DTEXTURE_CLAMP_V = 0x0010,		// this texture should be clamped on V
	W3DTEXTURE_ALPHA_BITMAP = 0x0020,		// this texture's alpha channel should be collapsed to one bit

	// Specify desired no. of mip-levels to be generated.
	W3DTEXTURE_MIP_LEVELS_MASK = 0x00c0,
	W3DTEXTURE_MIP_LEVELS_ALL = 0x0000,		// generate all mip-levels
	W3DTEXTURE_MIP_LEVELS_2 = 0x0040,		// generate up to 2 mip-levels (NOTE: use W3DTEXTURE_NO_LOD to generate just 1 mip-level)
	W3DTEXTURE_MIP_LEVELS_3 = 0x0080,		// generate up to 3 mip-levels
	W3DTEXTURE_MIP_LEVELS_4 = 0x00c0,		// generate up to 4 mip-levels

	// Hints to describe the intended use of the various passes / stages
	// This will go into the high byte of Attributes.
	W3DTEXTURE_HINT_SHIFT = 8,				// number of bits to shift up
	W3DTEXTURE_HINT_MASK = 0x0000ff00,	// mask for shifted hint value

	W3DTEXTURE_HINT_BASE = 0x0000,		// base texture
	W3DTEXTURE_HINT_EMISSIVE = 0x0100,		// emissive map
	W3DTEXTURE_HINT_ENVIRONMENT = 0x0200,		// environment/reflection map
	W3DTEXTURE_HINT_SHINY_MASK = 0x0300,		// shinyness mask map

	W3DTEXTURE_TYPE_MASK = 0x1000,
	W3DTEXTURE_TYPE_COLORMAP = 0x0000,		// Color map.
	W3DTEXTURE_TYPE_BUMPMAP = 0x1000,		// Grayscale heightmap (to be converted to bumpmap).

	// Animation types
	W3DTEXTURE_ANIM_LOOP = 0x0000,
	W3DTEXTURE_ANIM_PINGPONG = 0x0001,
	W3DTEXTURE_ANIM_ONCE = 0x0002,
	W3DTEXTURE_ANIM_MANUAL = 0x0003
};
*/

enum class TextureAttr : uint16_t {
	PUBLISH = 0x0001,
	RESIZE_OBSOLETE = 0x0002,
	NO_LOD = 0x0004,
	CLAMP_U = 0x0008,
	CLAMP_V = 0x0010,
	ALPHA_BITMAP = 0x0020,
	
};

static constexpr std::array<std::pair<TextureAttr, std::string_view>, 6> TextureAttrNames{ {
	{ TextureAttr::PUBLISH,      "W3DTEXTURE_PUBLISH"       },
	{ TextureAttr::RESIZE_OBSOLETE, "W3DTEXTURE_RESIZE_OBSOLETE" },
	{ TextureAttr::NO_LOD,       "W3DTEXTURE_NO_LOD"        },
	{ TextureAttr::CLAMP_U,      "W3DTEXTURE_CLAMP_U"       },
	{ TextureAttr::CLAMP_V,      "W3DTEXTURE_CLAMP_V"       },
	{ TextureAttr::ALPHA_BITMAP, "W3DTEXTURE_ALPHA_BITMAP"  },
} };

enum class TextureAnim : uint16_t {
	LOOP = 0,
	PINGPONG = 1,
	ONCE = 2,
	MANUAL = 3,
};

inline std::string_view ToString(TextureAnim a) {
	switch (a) {
	case TextureAnim::LOOP:     return "W3DTEXTURE_ANIM_LOOP";
	case TextureAnim::PINGPONG: return "W3DTEXTURE_ANIM_PINGPONG";
	case TextureAnim::ONCE:     return "W3DTEXTURE_ANIM_ONCE";
	case TextureAnim::MANUAL:   return "W3DTEXTURE_ANIM_MANUAL";
	default:                    return "UNKNOWN_ANIM";
	}
}


struct W3dTextureInfoStruct
{
	W3dTextureInfoStruct(void) {}
	uint16_t					Attributes;					// flags for this texture
	uint16_t					AnimType;					// animation logic
	uint32_t					FrameCount;					// Number of frames (1 if not animated)
	float					    FrameRate;					// Frame rate, frames per second in floating point
};

struct W3dRGBAStruct
{
	uint8_t			R;
	uint8_t			G;
	uint8_t			B;
	uint8_t			A;
};

struct W3dTexCoordStruct
{
	bool operator == (W3dTexCoordStruct t)
	{
		return ((U == t.U) && (V == t.V));
	}

	float		U;					  	// U,V coordinates
	float		V;
};

struct Vector3i {
	int32_t I, J, K;
};

//
// Deform information.  Each mesh can have sets of keyframes of
//	deform info associated with it.
// 
struct W3dMeshDeform
{
	uint32_t					SetCount;
	uint32_t					AlphaPasses;
 // uint32_t					reserved[3];
	// any trailing bytes (usually 3×4 reserved) are ignored
};
//
// Deform set information.  Each set is made up of a series
// of keyframes.
// 
struct W3dDeformSetInfo
{
	uint32_t				    KeyframeCount;
	uint32_t					flags;
	uint32_t					reserved[1];
};

//
// Deform keyframe information.  Each keyframe is made up of
// a set of per-vert deform data.
// 
struct W3dDeformKeyframeInfo
{
	float					DeformPercent;
	uint32_t					DataCount;
	uint32_t					reserved[2];
};

//
// Deform data.  Contains deform information about a vertex
// in the mesh.
// 
struct W3dDeformData
{
	uint32_t					VertexIndex;
	W3dVectorStruct		Position;
	W3dRGBAStruct			Color;
	uint32_t					reserved[2];
};

struct W3dPS2ShaderStruct
{
	uint8_t						DepthCompare;
	uint8_t						DepthMask;
	uint8_t						PriGradient;
	uint8_t						Texturing;
	uint8_t						AlphaTest;
	uint8_t						AParam;
	uint8_t						BParam;
	uint8_t						CParam;
	uint8_t						DParam;
	uint8_t						pad[3];
};

// 
// AABTree header.  Each mesh can have an associated Axis-Aligned-Bounding-Box tree
// which is used for collision detection and certain rendering algorithms (like 
// texture projection.
//
struct W3dMeshAABTreeHeader
{
	uint32_t					NodeCount;
	uint32_t					PolyCount;
	uint32_t					Padding[6];
};

// 
// AABTree Node.  This is a node in the AABTree.
// If the MSB of FrontOrPoly0 is 1, then the node is a leaf and contains Poly0 and PolyCount
// else, the node is not a leaf and contains indices to its front and back children.  This matches
// the format used by AABTreeClass in WW3D.
//
struct W3dMeshAABTreeNode
{
	W3dVectorStruct		Min;						// min corner of the box 
	W3dVectorStruct		Max;						// max corner of the box
	uint32_t					FrontOrPoly0;			// index of the front child or poly0 (if MSB is set, then leaf and poly0 is valid)
	uint32_t					BackOrPolyCount;		// index of the back child or polycount
};

struct W3dHierarchyStruct
{
	uint32_t					Version;
	char						Name[W3D_NAME_LEN];	// Name of the hierarchy
	uint32_t					NumPivots;
	W3dVectorStruct		Center;
};

struct W3dQuaternionStruct
{
	float		Q[4];
};

struct W3dPivotStruct
{
	char						Name[W3D_NAME_LEN];	// Name of the node (UR_ARM, LR_LEG, TORSO, etc)
	uint32_t					ParentIdx;					// 0xffffffff = root pivot; no parent
	W3dVectorStruct		Translation;			// translation to pivot point
	W3dVectorStruct		EulerAngles;			// orientation of the pivot point
	W3dQuaternionStruct	Rotation;				// orientation of the pivot point
};

struct W3dPivotFixupStruct
{
	float					TM[4][3];				// this is a direct dump of a MAX 3x4 matrix
};

struct W3dAnimHeaderStruct
{
	uint32_t					Version;
	char						Name[W3D_NAME_LEN];
	char						HierarchyName[W3D_NAME_LEN];
	uint32_t					NumFrames;
	uint32_t					FrameRate;

};

struct W3dAnimChannelStruct
{
	uint16_t					FirstFrame;
	uint16_t					LastFrame;
	uint16_t					VectorLen;			// length of each vector in this channel
	uint16_t					Flags;					// channel type.
	uint16_t					Pivot;					// pivot affected by this channel
	uint16_t					pad;
	float					Data[1];				// will be (LastFrame - FirstFrame + 1) * VectorLen long
};

struct W3dBitChannelStruct
{
	uint16_t					FirstFrame;			// all frames outside "First" and "Last" are assumed = DefaultVal
	uint16_t					LastFrame;
	uint16_t					Flags;					// channel type.
	uint16_t					Pivot;					// pivot affected by this channel
	uint8_t						DefaultVal;			// default state when outside valid range.
	uint8_t						Data[1];				// will be (LastFrame - FirstFrame + 1) / 8 long
};

struct W3dCompressedAnimHeaderStruct
{
	uint32_t					Version;
	char						Name[W3D_NAME_LEN];
	char						HierarchyName[W3D_NAME_LEN];
	uint32_t					NumFrames;
	uint16_t					FrameRate;
	uint16_t					Flavor;
};

// A time code is a uint32 that prefixes each vector
// the MSB is used to indicate a binary (non interpolated) movement

constexpr auto W3D_TIMECODED_BINARY_MOVEMENT_FLAG = 0x80000000;

struct W3dTimeCodedAnimChannelStruct
{
	uint32_t				NumTimeCodes;		// number of time coded entries
	uint16_t					Pivot;				// pivot affected by this channel
	uint8_t						VectorLen;			// length of each vector in this channel
	uint8_t						Flags;				// channel type.
	uint32_t					Data[1];				// will be (NumTimeCodes * ((VectorLen * sizeof(uint32)) + sizeof(uint32)))
};

// The bit channel is encoded right into the MSB of each time code
constexpr auto W3D_TIMECODED_BIT_MASK = 0x80000000;

struct W3dTimeCodedBitChannelStruct
{
	uint32_t					NumTimeCodes;  		// number of time coded entries 
	uint16_t					Pivot;					// pivot affected by this channel
	uint8_t						Flags;					// channel type.
	uint8_t						DefaultVal;				// default state when outside valid range.
	uint32_t					Data[1];					// will be (NumTimeCodes * sizeof(uint32))
};

// End Time Coded Structures

// Begin AdaptiveDelta Structures
struct W3dAdaptiveDeltaAnimChannelStruct
{
	uint32_t					NumFrames;			// number of frames of animation
	uint16_t					Pivot;				// pivot effected by this channel
	uint8_t						VectorLen;			// num Channels
	uint8_t						Flags;				// channel type
	float						Scale;				// Filter Table Scale

	uint32_t					Data[1];				// OpCode Data Stream

};
// End AdaptiveDelta Structures

struct W3dMorphAnimHeaderStruct
{
	uint32_t					Version;
	char						Name[W3D_NAME_LEN];
	char						HierarchyName[W3D_NAME_LEN];
	uint32_t					FrameCount;
	float					FrameRate;
	uint32_t					ChannelCount;
};

struct W3dMorphAnimKeyStruct
{
	uint32_t					MorphFrame;
	uint32_t					PoseFrame;
};

struct W3dHModelHeaderStruct
{
	uint32_t					Version;
	char						Name[W3D_NAME_LEN];				// Name of this model
	char						HierarchyName[W3D_NAME_LEN];	// Name of the hierarchy tree this model uses
	uint16_t					NumConnections;
};

struct W3dHModelNodeStruct
{
	// Note: the full name of the Render object is expected to be: <HModelName>.<RenderObjName>
	char						RenderObjName[W3D_NAME_LEN];
	uint16_t					PivotIdx;
};

//
// W3dHModelAuxDataStruct.  HModels had this extra chunk defining the counts of individual
// types of objects to be found in the model and some obsolete distance-based LOD settings.
// As changes were made to the ww3d library, all of this became useles so the chunk was
// "retired".
//
struct W3dHModelAuxDataStruct
{
	uint32_t			Attributes;
	uint32_t			MeshCount;
	uint32_t			CollisionCount;
	uint32_t			SkinCount;
	uint32_t			ShadowCount;
	uint32_t			NullCount;
	uint32_t			FutureCounts[6];

	float				LODMin;
	float				LODMax;
	uint32_t			FutureUse[32];
};

struct W3dLODModelHeaderStruct
{
	uint32_t					Version;
	char						Name[W3D_NAME_LEN];				// Name of this LOD Model
	uint16_t					NumLODs;
};

struct W3dLODStruct
{
	char						RenderObjName[2 * W3D_NAME_LEN];
	float					LODMin;								// "artist" inspired switching distances
	float					LODMax;
};

struct W3dCollectionHeaderStruct
{
	uint32_t		Version;
	char			Name[W3D_NAME_LEN];
	uint32_t		RenderObjectCount;
	uint32_t		pad[2];
};

//
//	Note:  This structure is follwed directly by an array of char's 'name_len' in length
// which specify the name of the placeholder object in our Commando-level editor.
//
struct W3dPlaceholderStruct
{
	uint32_t		version;
	float			transform[4][3];				// this is a direct dump of a MAX 3x4 matrix
	uint32_t		name_len;
};

//
//	Note:  This structure is followed directly by an array of char's 'name_len' in length
// which specifies the name of the file to apply the transform to.
//
struct W3dTransformNodeStruct
{
	uint32_t		version;
	float	transform[4][3];				// this is a direct dump of a MAX 3x4 matrix
	uint32_t		name_len;
};

struct W3dLightStruct
{
	uint32_t				Attributes;
	uint32_t				Unused; // Old exclusion bit deprecated
	W3dRGBStruct		Ambient;
	W3dRGBStruct		Diffuse;
	W3dRGBStruct		Specular;
	float				Intensity;
};

struct W3dSpotLightStruct
{
	W3dVectorStruct	SpotDirection;
	float				SpotAngle;
	float				SpotExponent;
};

struct W3dLightAttenuationStruct
{
	float				Start;
	float				End;
};

struct W3dEmitterHeaderStruct
{
	uint32_t				Version;
	char					Name[W3D_NAME_LEN];
};

struct W3dEmitterInfoStruct
{
	char			TextureFilename[260];
	float			StartSize;
	float			EndSize;
	float			Lifetime;
	float			EmissionRate;
	float			MaxEmissions;
	float			VelocityRandom;
	float			PositionRandom;
	float			FadeTime;
	float			Gravity;
	float			Elasticity;
	W3dVectorStruct	Velocity;
	W3dVectorStruct	Acceleration;
	W3dRGBAStruct	StartColor;
	W3dRGBAStruct	EndColor;
};

struct W3dVolumeRandomizerStruct
{
	uint32_t			ClassID;
	float				Value1;
	float				Value2;
	float				Value3;
	uint32_t			reserved[4];
};

struct W3dEmitterInfoStructV2
{
	uint32_t							BurstSize;
	W3dVolumeRandomizerStruct			CreationVolume;
	W3dVolumeRandomizerStruct			VelRandom;
	float							OutwardVel;
	float							VelInherit;
	W3dShaderStruct						Shader;
	uint32_t							RenderMode;		// render as particles or lines?
	uint32_t							FrameMode;		// chop the texture into a grid of smaller squares?
	uint32_t							reserved[6];
};


// W3D_CHUNK_EMITTER_PROPS
// Contains a W3dEmitterPropertyStruct followed by a number of color keyframes, 
// opacity keyframes, and size keyframes

struct W3dEmitterPropertyStruct
{
	uint32_t			ColorKeyframes;
	uint32_t			OpacityKeyframes;
	uint32_t			SizeKeyframes;
	W3dRGBAStruct		ColorRandom;
	float				OpacityRandom;
	float				SizeRandom;
	uint32_t			reserved[4];
};

struct W3dEmitterColorKeyframeStruct
{
	float				Time;
	W3dRGBAStruct		Color;
};

struct W3dEmitterOpacityKeyframeStruct
{
	float			Time;
	float			Opacity;
};

struct W3dEmitterSizeKeyframeStruct
{
	float			Time;
	float			Size;
};


struct W3dEmitterLinePropertiesStruct
{
	uint32_t					Flags;
	uint32_t					SubdivisionLevel;
	float						NoiseAmplitude;
	float						MergeAbortFactor;
	float						TextureTileFactor;
	float						UPerSec;
	float						VPerSec;
	uint32_t					Reserved[9];
};

// W3D_CHUNK_EMITTER_ROTATION_KEYFRAMES 
// Contains a W3dEmitterRotationHeaderStruct followed by a number of
// rotational velocity keyframes.  
struct W3dEmitterRotationHeaderStruct
{
	uint32_t			KeyframeCount;
	float				Random;					// random initial rotational velocity (rotations/sec)
	float				OrientationRandom;	// random initial orientation (rotations, 1.0=360deg)
	uint32_t			Reserved[1];
};

// W3D_CHUNK_EMITTER_FRAME_KEYFRAMES
// Contains a W3dEmitterFrameHeaderStruct followed by a number of
// frame keyframes (sub-texture indexing)
struct W3dEmitterFrameHeaderStruct
{
	uint32_t			KeyframeCount;
	float				Random;
	uint32_t			Reserved[2];
};

struct W3dEmitterFrameKeyframeStruct
{
	float				Time;
	float				Frame;
};

// W3D_CHUNK_EMITTER_BLUR_TIME_KEYFRAMES
// Contains a W3dEmitterFrameHeaderStruct followed by a number of
// frame keyframes (sub-texture indexing)
struct W3dEmitterBlurTimeHeaderStruct
{
	uint32_t			KeyframeCount;
	float				Random;
	uint32_t			Reserved[1];
};

struct W3dEmitterBlurTimeKeyframeStruct
{
	float			Time;
	float			BlurTime;
};

struct W3dAggregateHeaderStruct
{
	uint32_t				Version;
	char					Name[W3D_NAME_LEN];
};

struct W3dAggregateInfoStruct
{
	char					BaseModelName[W3D_NAME_LEN * 2];
	uint32_t				SubobjectCount;
};

struct W3dAggregateSubobjectStruct
{
	char					SubobjectName[W3D_NAME_LEN * 2];
	char					BoneName[W3D_NAME_LEN * 2];
};

struct W3dTextureReplacerHeaderStruct
{
	uint32_t				ReplacedTexturesCount;
};

struct W3dTextureReplacerStruct
{
	char						MeshPath[15][32];
	char						BonePath[15][32];
	char						OldTextureName[260];
	char						NewTextureName[260];
	W3dTextureInfoStruct	TextureParams;
};

//
// Flags used in the W3dAggregateMiscInfo structure
//
const int W3D_AGGREGATE_FORCE_SUB_OBJ_LOD = 0x00000001;

//
// Structures for version 1.2 and newer
//
struct W3dAggregateMiscInfo
{
	uint32_t				OriginalClassID;
	uint32_t				Flags;
	uint32_t				reserved[3];
};

struct W3dHLodHeaderStruct
{
	uint32_t					Version;
	uint32_t					LodCount;
	char						Name[W3D_NAME_LEN];
	char						HierarchyName[W3D_NAME_LEN];		// name of the hierarchy tree to use (\0 if none)
};

struct W3dHLodArrayHeaderStruct
{
	uint32_t				ModelCount;
	float					MaxScreenSize;		// if model is bigger than this, switch to higher lod.
};

struct W3dHLodSubObjectStruct
{
	uint32_t					BoneIndex;
	char						Name[W3D_NAME_LEN * 2];
};
enum class BoxAtrr : uint32_t {
 W3D_BOX_ATTRIBUTE_ORIENTED	= 0x00000001,
 W3D_BOX_ATTRIBUTE_ALIGNED	= 0x00000002,
 W3D_BOX_ATTRIBUTE_COLLISION_TYPE_MASK = 0x00000FF0,		// mask for the collision type bits
 W3D_BOX_ATTRIBUTE_COLLISION_TYPE_SHIFT = 4,		// shifting to get to the collision type bits
 W3D_BOX_ATTRIBTUE_COLLISION_TYPE_PHYSICAL = 0x00000010,		// physical collisions
 W3D_BOX_ATTRIBTUE_COLLISION_TYPE_PROJECTILE = 0x00000020,		// projectiles (rays) collide with this
 W3D_BOX_ATTRIBTUE_COLLISION_TYPE_VIS =	0x00000040,		// vis rays collide with this mesh
 W3D_BOX_ATTRIBTUE_COLLISION_TYPE_CAMERA = 0x00000080,		// cameras collide with this mesh
 W3D_BOX_ATTRIBTUE_COLLISION_TYPE_VEHICLE =	0x00000100		// vehicles collide with this mesh

};


struct W3dBoxStruct
{
	uint32_t			Version;						// file format version
	uint32_t			Attributes;					// box attributes (above #define's)
	char				Name[2 * W3D_NAME_LEN];	// name is in the form <containername>.<boxname>
	W3dRGBStruct		Color;						// color to use when drawing the box
	W3dVectorStruct		Center;						// center of the box
	W3dVectorStruct		Extent;						// extent of the box
};

struct W3dNullObjectStruct
{
	uint32_t				Version;						// file format version
	uint32_t				Attributes;					// object attributes (currently un-used)
	uint32_t				pad[2];						// pad space
	char					Name[2 * W3D_NAME_LEN];	// name is in the form <containername>.<boxname>
};

struct W3dLightTransformStruct
{
	float Transform[3][4];
};

//-- a lean quaternion (identity by default)
struct SimpleQuat {
	float x{ 0 }, y{ 0 }, z{ 0 }, w{ 1 };
};

//-- the alpha‐vector per‐sphere
struct AlphaVectorStruct {
	SimpleQuat angle;     // default (0,0,0,1)
	float      intensity{ 1.0f };
};

struct W3dSphereStruct
{
	uint32_t				Version;							// file format version
	uint32_t				Attributes;						// sphere attributes (above #define's)
	char					Name[2 * W3D_NAME_LEN];		// name is in the form <containername>.<spherename>

	W3dVectorStruct	Center;							// center of the sphere
	W3dVectorStruct	Extent;							// extent of the sphere

	float					AnimDuration;					// Animation time (in seconds)

	W3dVectorStruct	DefaultColor;
	float					DefaultAlpha;
	W3dVectorStruct	DefaultScale;
	AlphaVectorStruct DefaultVector;

	char					TextureName[2 * W3D_NAME_LEN];
	W3dShaderStruct	Shader;

	//
	//	Note this structure is followed by a series of
	// W3dSphereKeyArrayStruct structures which define the
	// variable set of keyframes for each channel.
	//
};

//–– keyframe for color and scale channels (Vec3 + time)
struct W3dSphereVec3Key {
	W3dVectorStruct  Value;  // X, Y, Z
	float            Time;   // normalized [0..1]
};

//–– keyframe for alpha channel (scalar + time)
struct W3dSphereAlphaKey {
	float Value;  // alpha
	float Time;   // normalized [0..1]
};

//–– keyframe for the vector channel (quat + magnitude + time)
struct W3dSphereVectorKey {
	SimpleQuat Quat;      // rotation part
	float      Magnitude; // vector intensity
	float      Time;      // normalized [0..1]
};

struct Vector2 {
	float x{ 0 }, y{ 0 };

	constexpr Vector2() noexcept = default;
	constexpr Vector2(float _x, float _y) noexcept : x(_x), y(_y) {}

	// array‑style access
	float& operator[](size_t i)       noexcept { return (i == 0 ? x : y); }
	const float& operator[](size_t i) const noexcept { return (i == 0 ? x : y); }

	// equality
	constexpr bool operator==(const Vector2& o) const noexcept = default;
	constexpr bool operator!=(const Vector2& o) const noexcept = default;

	// arithmetic
	Vector2& operator+=(const Vector2& o) noexcept { x += o.x; y += o.y; return *this; }
	Vector2& operator-=(const Vector2& o) noexcept { x -= o.x; y -= o.y; return *this; }
	Vector2& operator*=(float s)        noexcept { x *= s;    y *= s;    return *this; }
	Vector2& operator/=(float s)        noexcept { x /= s;    y /= s;    return *this; }

	friend Vector2 operator+(Vector2 a, const Vector2& b) noexcept { return a += b; }
	friend Vector2 operator-(Vector2 a, const Vector2& b) noexcept { return a -= b; }
	friend Vector2 operator*(Vector2 v, float s)        noexcept { return v *= s; }
	friend Vector2 operator*(float s, Vector2 v)        noexcept { return v *= s; }
	friend Vector2 operator/(Vector2 v, float s)        noexcept { return v /= s; }

	// dot product, length, normalize
	static float Dot(const Vector2& a, const Vector2& b) noexcept { return a.x * b.x + a.y * b.y; }
	float        Length2()   const noexcept { return x * x + y * y; }
	float        Length()    const noexcept { return std::sqrt(Length2()); }
	Vector2& Normalize() noexcept {
		float l = Length();
		if (l > 0) { x /= l; y /= l; }
		return *this;
	}
};

struct W3dRingStruct
{
	uint32_t				Version;						// file format version
	uint32_t				Attributes;					// box attributes (above #define's)
	char					Name[2 * W3D_NAME_LEN];	// name is in the form <containername>.<boxname>

	W3dVectorStruct	Center;						// center of the box
	W3dVectorStruct	Extent;						// extent of the box

	float					AnimDuration;				// Animation time (in seconds)

	W3dVectorStruct	DefaultColor;
	float					DefaultAlpha;
	Vector2				DefaultInnerScale;
	Vector2				DefaultOuterScale;

	Vector2				InnerExtent;
	Vector2				OuterExtent;

	char					TextureName[2 * W3D_NAME_LEN];
	W3dShaderStruct	Shader;
	int					TextureTileCount;

	//
	// Note this structure is followed by a series of
	// W3dRingKeyArrayStruct structures which define the
	// variable set of keyframes for each channel
};

//–– keyframe for color and scale channels (Vec3 + time)
struct W3dRingVec3Key {
	W3dVectorStruct  Value;  // X, Y, Z
	float            Time;   // normalized [0..1]
};

//–– keyframe for alpha channel (scalar + time)
struct W3dRingAlphaKey {
	float Value;  // alpha
	float Time;   // normalized [0..1]
};


//
//	Note:  This structure is follwed directly by a chunk (W3D_CHUNK_SOUNDROBJ_DEFINITION)
// that contains an embedded AudibleSoundDefinitionClass's storage.  See audibledound.h
// for details.
//
struct W3dSoundRObjHeaderStruct
{
	uint32_t				Version;
	char					Name[W3D_NAME_LEN];
	uint32_t				Flags;
	uint32_t				Padding[8];
};
