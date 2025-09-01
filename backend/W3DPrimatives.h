#pragma once
#include "W3DStructs.h"
#include <vector>
#include <optional>


// ---------- Shader flatten (use builder enum helpers instead of string tables)
inline void InterpretSphereShaderStruct(std::vector<ChunkField>& fields,
    const W3dShaderStruct& s)
{
    ChunkFieldBuilder B(fields);
    // These call your DepthCompareField / ... helpers:
    B.DepthCompareField("Shader.DepthCompare", s.DepthCompare);
    B.DepthMaskField("Shader.DepthMask", s.DepthMask);
    B.DestBlendField("Shader.DestBlend", s.DestBlend);
    B.PriGradientField("Shader.PriGradient", s.PriGradient);
    B.SecGradientField("Shader.SecGradient", s.SecGradient);
    B.SrcBlendField("Shader.SrcBlend", s.SrcBlend);
    B.TexturingField("Shader.Texturing", s.Texturing);
    B.DetailColorFuncField("Shader.DetailColor", s.DetailColorFunc);
    B.DetailAlphaFuncField("Shader.DetailAlpha", s.DetailAlphaFunc);
    B.AlphaTestField("Shader.AlphaTest", s.AlphaTest);
    // (skip pads/obsolete slots by design)
}

// ---------- Sphere header ----------
inline std::vector<ChunkField> InterpretSphereHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    if (!chunk) return F;

    auto parsed = ParseChunkStruct<W3dSphereStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        F.emplace_back("error", "string", "Chunk too small for W3dSphereStruct: " + *err);
        return F;
    }
    const auto& sph = std::get<W3dSphereStruct>(parsed);

    ChunkFieldBuilder B(F);
    B.Version("Version", sph.Version);

    // Sphere flags
    B.UInt32("Flags", sph.Attributes);
    B.Flag(sph.Attributes, 0x1, "SPHERE_ALPHA_VECTOR");
    B.Flag(sph.Attributes, 0x2, "SPHERE_CAMERA_ALIGNED");
    B.Flag(sph.Attributes, 0x4, "SPHERE_INVERT_EFFECT");
    B.Flag(sph.Attributes, 0x8, "SPHERE_LOOPING");

    if (sph.Attributes & ~0xFu) {
        B.Push("Flags.Unknown", "string",
            "Unknown bits 0x" + ToHex(sph.Attributes & ~0xFu));
    }

    B.Name("Name", sph.Name, 2 * W3D_NAME_LEN);
    B.Vec3("Center", sph.Center);
    B.Vec3("Extent", sph.Extent);
    B.Float("AnimationDuration", sph.AnimDuration);

    B.Vec3("Color", sph.DefaultColor);
    B.Float("Alpha", sph.DefaultAlpha);
    B.Vec3("Scale", sph.DefaultScale);

    // DefaultVector (quat + magnitude)
    {
        std::ostringstream q;
        q << std::fixed << std::setprecision(6)
            << sph.DefaultVector.angle.x << ' '
            << sph.DefaultVector.angle.y << ' '
            << sph.DefaultVector.angle.z << ' '
            << sph.DefaultVector.angle.w;
        B.Push("Angle", "quaternion", q.str());
        B.Float("Intensity", sph.DefaultVector.intensity);
    }

    B.Name("TextureName", sph.TextureName, 2 * W3D_NAME_LEN);

    // Shader
    InterpretSphereShaderStruct(F, sph.Shader);

    return F;
}


/*
// ----------Open the inner VARIABLES wrapper (id = 0x03150809) ----------

struct VariablesSpan { const uint8_t* base{}; size_t size{}; };

inline std::optional<VariablesSpan>
OpenVariablesWrapper(const std::shared_ptr<ChunkItem>& chunk) {
    if (!chunk) return std::nullopt;
    static constexpr uint32_t CHUNKID_VARIABLES = 0x03150809u;
    if (chunk->id == CHUNKID_VARIABLES) {
        return VariablesSpan{ chunk->data.data(), chunk->data.size() };
    }
    // Some wrappers embed [id,size]+payload in their data
    const auto& b = chunk->data;
    if (b.size() < 8) return std::nullopt;
    uint32_t id, size;
    std::memcpy(&id, b.data() + 0, 4);
    std::memcpy(&size, b.data() + 4, 4);
    if (id != CHUNKID_VARIABLES) return std::nullopt;
    if (8ull + size > b.size())  return std::nullopt;
    return VariablesSpan{ b.data() + 8, size };
}

inline bool ReadU32(const uint8_t*& p, size_t& left, uint32_t& out) {
    if (left < 4) return false; std::memcpy(&out, p, 4); p += 4; left -= 4; return true;
}
inline bool ReadF32(const uint8_t*& p, size_t& left, float& out) {
    if (left < 4) return false; std::memcpy(&out, p, 4); p += 4; left -= 4; return true;
}
*/
// ----micro - chunk core----
struct MicroSpan { const uint8_t* p; size_t left; };

inline std::optional<MicroSpan> OpenVariablesMicro(const std::shared_ptr<ChunkItem>& chunk) {
    if (!chunk) return std::nullopt;
    static constexpr uint32_t VARS = 0x03150809u;

    if (chunk->id == VARS) return MicroSpan{ chunk->data.data(), chunk->data.size() };

    if (chunk->data.size() >= 8) {
        uint32_t id, sz;
        std::memcpy(&id, chunk->data.data() + 0, 4);
        std::memcpy(&sz, chunk->data.data() + 4, 4);
        if (id == VARS && 8ull + sz <= chunk->data.size())
            return MicroSpan{ chunk->data.data() + 8, sz };
    }
    return std::nullopt;
}

inline bool NextMicro(MicroSpan& s, uint8_t& mtype, const uint8_t*& payload, uint8_t& msize) {
    if (s.left < 2) return false;
    mtype = s.p[0];
    msize = s.p[1];
    s.p += 2;
    s.left -= 2;
    if (s.left < msize) return false; // truncated
    payload = s.p;
    s.p += msize;
    s.left -= msize;
    return true;
}

inline std::vector<ChunkField>
InterpretSphereColorChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesMicro(chunk); if (!span) return F;
    MicroSpan s = *span;

    uint8_t t = 0;
    uint8_t sz = 0;
    const uint8_t* pay = nullptr;

    for (uint32_t i = 0; NextMicro(s, t, pay, sz); ++i) {
        if (sz != 16) { F.emplace_back("error", "string", "Color mc size != 16"); break; }
        W3dVectorStruct v; float tm;
        std::memcpy(&v, pay + 0, 12);
        std::memcpy(&tm, pay + 12, 4);
        ChunkFieldBuilder B(F);
        const std::string base = "ColorChannel[" + std::to_string(i) + "]";
        B.Float(base + ".Value.X", v.X);
        B.Float(base + ".Value.Y", v.Y);
        B.Float(base + ".Value.Z", v.Z);
        B.Float(base + ".Time", tm);
    }
    return F;
}

inline std::vector<ChunkField>
InterpretSphereScaleChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesMicro(chunk); if (!span) return F;
    MicroSpan s = *span;

    uint8_t t = 0;
    uint8_t sz = 0;
    const uint8_t* pay = nullptr;

    for (uint32_t i = 0; NextMicro(s, t, pay, sz); ++i) {
        if (sz != 16) { F.emplace_back("error", "string", "Scale mc size != 16"); break; }
        W3dVectorStruct v; float tm;
        std::memcpy(&v, pay + 0, 12);
        std::memcpy(&tm, pay + 12, 4);
        ChunkFieldBuilder B(F);
        const std::string base = "ScaleChannel[" + std::to_string(i) + "]";
        B.Float(base + ".Value.X", v.X);
        B.Float(base + ".Value.Y", v.Y);
        B.Float(base + ".Value.Z", v.Z);
        B.Float(base + ".Time", tm);
    }
    return F;
}

inline std::vector<ChunkField>
InterpretSphereAlphaChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesMicro(chunk); if (!span) return F;
    MicroSpan s = *span;

    uint8_t t = 0;
    uint8_t sz = 0;
    const uint8_t* pay = nullptr;

    for (uint32_t i = 0; NextMicro(s, t, pay, sz); ++i) {
        if (sz != 8) { F.emplace_back("error", "string", "Alpha mc size != 8"); break; }
        float v, tm;
        std::memcpy(&v, pay + 0, 4);
        std::memcpy(&tm, pay + 4, 4);
        ChunkFieldBuilder B(F);
        const std::string base = "AlphaChannel[" + std::to_string(i) + "]";
        B.Float(base + ".Value", v);
        B.Float(base + ".Time", tm);
    }
    return F;
}

inline std::vector<ChunkField>
InterpretSphereVectorChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesMicro(chunk); if (!span) return F;
    MicroSpan s = *span;

    struct QuatF { float x, y, z, w; };

    uint8_t t = 0;
    uint8_t sz = 0;
    const uint8_t* pay = nullptr;

    for (uint32_t i = 0; NextMicro(s, t, pay, sz); ++i) {
        if (sz != 24) { F.emplace_back("error", "string", "Vector mc size != 24"); break; }
        QuatF q; float mag, tm;
        std::memcpy(&q, pay + 0, 16);
        std::memcpy(&mag, pay + 16, 4);
        std::memcpy(&tm, pay + 20, 4);
        ChunkFieldBuilder B(F);
        const std::string base = "VectorChannel[" + std::to_string(i) + "]";
        B.Push(base + ".Quat", "quaternion", FormatUtils::FormatQuat(q.x, q.y, q.z, q.w));
        B.Float(base + ".Magnitude", mag);
        B.Float(base + ".Time", tm);
    }
    return F;
}




// ---------- Ring header ----------
inline std::vector<ChunkField> InterpretRingHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    if (!chunk) return F;

    auto parsed = ParseChunkStruct<W3dRingStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        F.emplace_back("error", "string", "Invalid RING chunk size: " + *err);
        return F;
    }
    const auto& s = std::get<W3dRingStruct>(parsed);

    ChunkFieldBuilder B(F);
    B.Version("Version", s.Version);
    
    // Ring flags
    B.UInt32("Flags", s.Attributes);
    B.Flag(s.Attributes, 0x1, "RING_CAMERA_ALIGNED");
    B.Flag(s.Attributes, 0x2, "RING_LOOPING");

    if (s.Attributes & ~0xFu) {
        B.Push("Flags.Unknown", "string",
            "Unknown bits 0x" + ToHex(s.Attributes & ~0xFu));
    }
    B.Name("Name", s.Name, 2 * W3D_NAME_LEN);

    B.Vec3("Center", s.Center);
    B.Vec3("Extent", s.Extent);
    B.Float("AnimDuration", s.AnimDuration);

    B.Vec3("DefaultColor", s.DefaultColor);
    B.Float("DefaultAlpha", s.DefaultAlpha);

    // Vector2 helpers aren't in the builder; output as 2 fields for clarity
    B.Float("DefaultInnerScale.X", s.DefaultInnerScale.x);
    B.Float("DefaultInnerScale.Y", s.DefaultInnerScale.y);
    B.Float("DefaultOuterScale.X", s.DefaultOuterScale.x);
    B.Float("DefaultOuterScale.Y", s.DefaultOuterScale.y);

    B.Float("InnerExtent.X", s.InnerExtent.x);
    B.Float("InnerExtent.Y", s.InnerExtent.y);
    B.Float("OuterExtent.X", s.OuterExtent.x);
    B.Float("OuterExtent.Y", s.OuterExtent.y);

    B.Name("TextureName", s.TextureName, 2 * W3D_NAME_LEN);

    InterpretSphereShaderStruct(F, s.Shader);

    B.Int32("TextureTileCount", s.TextureTileCount);
    return F;
}

inline std::vector<ChunkField>
InterpretRingColorChannel(const std::shared_ptr<ChunkItem>& chunk) {
    // same as Sphere color (16 bytes)
    return InterpretSphereColorChannel(chunk);
}

inline std::vector<ChunkField>
InterpretRingAlphaChannel(const std::shared_ptr<ChunkItem>& chunk) {
    // same as Sphere alpha (8 bytes)
    return InterpretSphereAlphaChannel(chunk);
}

struct Vec2F { float u, v; };

inline std::vector<ChunkField>
InterpretRingInnerScaleChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesMicro(chunk); if (!span) return F;
    MicroSpan s = *span;

    uint8_t t = 0;
    uint8_t sz = 0;
    const uint8_t* pay = nullptr;

    for (uint32_t i = 0; NextMicro(s, t, pay, sz); ++i) {
        if (sz != 12) { F.emplace_back("error", "string", "InnerScale mc size != 12"); break; }
        Vec2F v; float tm;
        std::memcpy(&v, pay + 0, 8);
        std::memcpy(&tm, pay + 8, 4);
        ChunkFieldBuilder B(F);
        const std::string base = "InnerScaleChannel[" + std::to_string(i) + "]";
        B.Float(base + ".Value.U", v.u);
        B.Float(base + ".Value.V", v.v);
        B.Float(base + ".Time", tm);
    }
    return F;
}

inline std::vector<ChunkField>
InterpretRingOuterScaleChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesMicro(chunk); if (!span) return F;
    MicroSpan s = *span;

    uint8_t t = 0;
    uint8_t sz = 0;
    const uint8_t* pay = nullptr;

    for (uint32_t i = 0; NextMicro(s, t, pay, sz); ++i) {
        if (sz != 12) { F.emplace_back("error", "string", "OuterScale mc size != 12"); break; }
        Vec2F v; float tm;
        std::memcpy(&v, pay + 0, 8);
        std::memcpy(&tm, pay + 8, 4);
        ChunkFieldBuilder B(F);
        const std::string base = "OuterScaleChannel[" + std::to_string(i) + "]";
        B.Float(base + ".Value.U", v.u);
        B.Float(base + ".Value.V", v.v);
        B.Float(base + ".Time", tm);
    }
    return F;
}

