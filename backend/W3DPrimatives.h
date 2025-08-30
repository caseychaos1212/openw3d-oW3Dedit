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



// ----------Open the inner VARIABLES wrapper (id = 0x03150809) ----------
struct VariablesSpan { const uint8_t* base{}; size_t size{}; };

inline std::optional<VariablesSpan>
OpenVariablesWrapper(const std::shared_ptr<ChunkItem>& chunk) {
    if (!chunk) return std::nullopt;

    // Case A: we're already at the DATA node (id == 0x03150809)
    static constexpr uint32_t CHUNKID_VARIABLES = 0x03150809u;
    if (chunk->id == CHUNKID_VARIABLES) {
        return VariablesSpan{ chunk->data.data(), chunk->data.size() };
    }

    // Case B: we're at the channel wrapper and the buffer begins with the header
    const auto& b = chunk->data;
    if (b.size() < 8) return std::nullopt;

    const auto* p = b.data();
    const uint32_t id = *reinterpret_cast<const uint32_t*>(p + 0);
    const uint32_t size = *reinterpret_cast<const uint32_t*>(p + 4);
    if (id != CHUNKID_VARIABLES) return std::nullopt;
    if (8ull + size > b.size())  return std::nullopt;  // truncated

    return VariablesSpan{ p + 8, size };
}

inline std::vector<ChunkField>
InterpretSphereColorChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesWrapper(chunk);
    if (!span) return F;

    const uint8_t* cur = span->base;
    size_t left = span->size;
    constexpr size_t REC = sizeof(W3dSphereVec3Key);

    size_t i = 0;
    while (left >= REC) {
        const auto* k = reinterpret_cast<const W3dSphereVec3Key*>(cur);
        ChunkFieldBuilder B(F);
        const std::string base = "ColorChannel[" + std::to_string(i) + "]";
        B.Vec3(base + ".Value", k->Value);
        B.Float(base + ".Time", k->Time);
        cur += REC; left -= REC; ++i;
    }
    return F;
}

inline std::vector<ChunkField>
InterpretSphereScaleChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesWrapper(chunk);
    if (!span) return F;

    const uint8_t* cur = span->base;
    size_t left = span->size;
    constexpr size_t REC = sizeof(W3dSphereVec3Key);

    size_t i = 0;
    while (left >= REC) {
        const auto* k = reinterpret_cast<const W3dSphereVec3Key*>(cur);
        ChunkFieldBuilder B(F);
        const std::string base = "ScaleChannel[" + std::to_string(i) + "]";
        B.Vec3(base + ".Value", k->Value);
        B.Float(base + ".Time", k->Time);
        cur += REC; left -= REC; ++i;
    }
    return F;
}

inline std::vector<ChunkField>
InterpretSphereAlphaChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesWrapper(chunk);
    if (!span) return F;

    const uint8_t* cur = span->base;
    size_t left = span->size;
    constexpr size_t REC = sizeof(W3dSphereAlphaKey);

    size_t i = 0;
    while (left >= REC) {
        const auto* k = reinterpret_cast<const W3dSphereAlphaKey*>(cur);
        ChunkFieldBuilder B(F);
        const std::string base = "AlphaChannel[" + std::to_string(i) + "]";
        B.Float(base + ".Value", k->Value);
        B.Float(base + ".Time", k->Time);
        cur += REC; left -= REC; ++i;
    }
    return F;
}

inline std::vector<ChunkField>
InterpretSphereVectorChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesWrapper(chunk);
    if (!span) return F;

    const uint8_t* cur = span->base;
    size_t left = span->size;
    constexpr size_t REC = sizeof(W3dSphereVectorKey);

    size_t i = 0;
    while (left >= REC) {
        const auto* k = reinterpret_cast<const W3dSphereVectorKey*>(cur);
        ChunkFieldBuilder B(F);
        const std::string base = "VectorChannel[" + std::to_string(i) + "]";
        B.Push(base + ".Quat", "quaternion",
            FormatUtils::FormatQuat(k->Quat.x, k->Quat.y, k->Quat.z, k->Quat.w));
        B.Float(base + ".Magnitude", k->Magnitude);
        B.Float(base + ".Time", k->Time);
        cur += REC; left -= REC; ++i;
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
    B.Float("DefaultInnerScale.U", s.DefaultInnerScale.x);
    B.Float("DefaultInnerScale.V", s.DefaultInnerScale.y);
    B.Float("DefaultOuterScale.U", s.DefaultOuterScale.x);
    B.Float("DefaultOuterScale.V", s.DefaultOuterScale.y);

    B.Float("InnerExtent.U", s.InnerExtent.x);
    B.Float("InnerExtent.V", s.InnerExtent.y);
    B.Float("OuterExtent.U", s.OuterExtent.x);
    B.Float("OuterExtent.V", s.OuterExtent.y);

    B.Name("TextureName", s.TextureName, 2 * W3D_NAME_LEN);

    InterpretSphereShaderStruct(F, s.Shader);

    B.Int32("TextureTileCount", s.TextureTileCount);
    return F;
}

//TODO: Broken
// ---------- Ring channels ----------
inline std::vector<ChunkField>
InterpretRingColorChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesWrapper(chunk);
    if (!span) return F;

    struct Rec { W3dVectorStruct Value; float Time; };
    const uint8_t* cur = span->base;
    size_t left = span->size;

    size_t i = 0;
    while (left >= sizeof(Rec)) {
        const auto* r = reinterpret_cast<const Rec*>(cur);
        ChunkFieldBuilder B(F);
        const std::string base = "ColorChannel[" + std::to_string(i) + "]";
        B.Vec3(base + ".Value", r->Value);
        B.Float(base + ".Time", r->Time);
        cur += sizeof(Rec);
        left -= sizeof(Rec);
        ++i;
    }
    return F;
}

inline std::vector<ChunkField>
InterpretRingAlphaChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesWrapper(chunk);
    if (!span) return F;

    struct Rec { float Value; float Time; };
    const uint8_t* cur = span->base;
    size_t left = span->size;

    size_t i = 0;
    while (left >= sizeof(Rec)) {
        const auto* r = reinterpret_cast<const Rec*>(cur);
        ChunkFieldBuilder B(F);
        const std::string base = "AlphaChannel[" + std::to_string(i) + "]";
        B.Float(base + ".Value", r->Value);
        B.Float(base + ".Time", r->Time);
        cur += sizeof(Rec);
        left -= sizeof(Rec);
        ++i;
    }
    return F;
}

inline std::vector<ChunkField>
InterpretRingInnerScaleChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesWrapper(chunk);
    if (!span) return F;

    struct Rec { float U, V; float Time; };
    const uint8_t* cur = span->base;
    size_t left = span->size;

    size_t i = 0;
    while (left >= sizeof(Rec)) {
        const auto* r = reinterpret_cast<const Rec*>(cur);
        ChunkFieldBuilder B(F);
        const std::string base = "InnerScaleChannel[" + std::to_string(i) + "]";
        B.Float(base + ".Value.U", r->U);
        B.Float(base + ".Value.V", r->V);
        B.Float(base + ".Time", r->Time);
        cur += sizeof(Rec);
        left -= sizeof(Rec);
        ++i;
    }
    return F;
}

inline std::vector<ChunkField>
InterpretRingOuterScaleChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesWrapper(chunk);
    if (!span) return F;

    struct Rec { float U, V; float Time; };
    const uint8_t* cur = span->base;
    size_t left = span->size;

    size_t i = 0;
    while (left >= sizeof(Rec)) {
        const auto* r = reinterpret_cast<const Rec*>(cur);
        ChunkFieldBuilder B(F);
        const std::string base = "OuterScaleChannel[" + std::to_string(i) + "]";
        B.Float(base + ".Value.U", r->U);
        B.Float(base + ".Value.V", r->V);
        B.Float(base + ".Time", r->Time);
        cur += sizeof(Rec);
        left -= sizeof(Rec);
        ++i;
    }
    return F;
}

