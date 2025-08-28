#pragma once
#include "W3DStructs.h"
#include <vector>

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
    B.DetailColorFuncField("Shader.DetailColorFunc", s.DetailColorFunc);
    B.DetailAlphaFuncField("Shader.DetailAlphaFunc", s.DetailAlphaFunc);
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

    B.UInt32("Attributes", sph.Attributes);
    B.Name("Name", sph.Name);
    B.Vec3("Center", sph.Center);
    B.Vec3("Extent", sph.Extent);
    B.Float("AnimationDuration", sph.AnimDuration);

    B.Vec3("DefaultColor", sph.DefaultColor);
    B.Float("DefaultAlpha", sph.DefaultAlpha);
    B.Vec3("DefaultScale", sph.DefaultScale);

    // DefaultVector (quat + magnitude)
    {
        std::ostringstream q;
        q << std::fixed << std::setprecision(6)
            << sph.DefaultVector.angle.x << ' '
            << sph.DefaultVector.angle.y << ' '
            << sph.DefaultVector.angle.z << ' '
            << sph.DefaultVector.angle.w;
        B.Push("DefaultVector.Angle", "quaternion", q.str());
        B.Float("DefaultVector.Intensity", sph.DefaultVector.intensity);
    }

    B.Name("TextureName", sph.TextureName);

    // Shader
    InterpretSphereShaderStruct(F, sph.Shader);

    return F;
}

// ---------- Common micro-chunk reader for Sphere/Ring channels ----------
struct MicroChunkSpan {
    const uint8_t* base = nullptr;
    size_t         size = 0; // payload size after wrapper
};

// Expects: [uint32 id][uint32 size][payload...], id == 0x03150809
inline std::optional<MicroChunkSpan>
OpenVariablesWrapper(const std::shared_ptr<ChunkItem>& chunk, const char* label_for_errors) {
    if (!chunk || chunk->data.size() < 8) return std::nullopt;
    const uint8_t* p = chunk->data.data();
    const size_t   n = chunk->data.size();

    uint32_t id = *reinterpret_cast<const uint32_t*>(p + 0);
    uint32_t size = *reinterpret_cast<const uint32_t*>(p + 4);
    if (id != 0x03150809u || size > n - 8) return std::nullopt;

    MicroChunkSpan span;
    span.base = p + 8;
    span.size = size;
    return span;
}

// ---------- Sphere channels ----------
inline std::vector<ChunkField> InterpretSphereChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesWrapper(chunk, "SPHERE channel");
    if (!span) return F;

    const uint8_t* cur = span->base;
    const uint8_t* end = span->base + span->size;

    size_t idx = 0;
    while (cur + 2 <= end) {
        uint8_t id = cur[0];
        uint8_t size = cur[1];
        cur += 2;
        if (cur + size > end) break;

        switch (id) {
        case 0x02: // CHUNKID_COLOR_CHANNEL -> W3dSphereVec3Key
        case 0x04: // CHUNKID_SCALE_CHANNEL -> W3dSphereVec3Key
        {
            if (size == sizeof(W3dSphereVec3Key)) {
                auto const& key = *reinterpret_cast<const W3dSphereVec3Key*>(cur);
                std::string base = (id == 0x02 ? "ColorChannel[" : "ScaleChannel[")
                    + std::to_string(idx) + "]";
                ChunkFieldBuilder B(F);
                B.Vec3(base + ".Value", key.Value);
                B.Float(base + ".Time", key.Time);
            }
            break;
        }
        case 0x03: // CHUNKID_ALPHA_CHANNEL -> W3dSphereAlphaKey
        {
            if (size == sizeof(W3dSphereAlphaKey)) {
                auto const& key = *reinterpret_cast<const W3dSphereAlphaKey*>(cur);
                ChunkFieldBuilder B(F);
                std::string base = "AlphaChannel[" + std::to_string(idx) + "]";
                B.Float(base + ".Value", key.Value);
                B.Float(base + ".Time", key.Time);
            }
            break;
        }
        case 0x05: // CHUNKID_VECTOR_CHANNEL -> W3dSphereVectorKey
        {
            if (size == sizeof(W3dSphereVectorKey)) {
                auto const& key = *reinterpret_cast<const W3dSphereVectorKey*>(cur);
                std::ostringstream q;
                q << std::fixed << std::setprecision(6)
                    << key.Quat.x << ' ' << key.Quat.y << ' '
                    << key.Quat.z << ' ' << key.Quat.w;
                std::string base = "VectorChannel[" + std::to_string(idx) + "]";
                ChunkFieldBuilder B(F);
                B.Push(base + ".Quat", "quaternion", q.str());
                B.Float(base + ".Magnitude", key.Magnitude);
                B.Float(base + ".Time", key.Time);
            }
            break;
        }
        default:
            // Unknown micro-chunk; skip.
            break;
        }

        cur += size;
        ++idx;
    }

    return F;
}

inline std::vector<ChunkField> InterpretSphereColorChannel(const std::shared_ptr<ChunkItem>& c) { auto v = InterpretSphereChannel(c); return v; }
inline std::vector<ChunkField> InterpretSphereAlphaChannel(const std::shared_ptr<ChunkItem>& c) { auto v = InterpretSphereChannel(c); return v; }
inline std::vector<ChunkField> InterpretSphereScaleChannel(const std::shared_ptr<ChunkItem>& c) { auto v = InterpretSphereChannel(c); return v; }
inline std::vector<ChunkField> InterpretSphereVectorChannel(const std::shared_ptr<ChunkItem>& c) { auto v = InterpretSphereChannel(c); return v; }

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
    B.UInt32("Attributes", s.Attributes);
    B.Name("Name", s.Name);

    B.Vec3("Center", s.Center);
    B.Vec3("Extent", s.Extent);
    B.Float("AnimDuration", s.AnimDuration);

    B.Vec3("DefaultColor", s.DefaultColor);
    B.Float("DefaultAlpha", s.DefaultAlpha);

    // Vector2 helpers aren’t in the builder; output as 2 fields for clarity
    B.Float("DefaultInnerScale.U", s.DefaultInnerScale.x);
    B.Float("DefaultInnerScale.V", s.DefaultInnerScale.y);
    B.Float("DefaultOuterScale.U", s.DefaultOuterScale.x);
    B.Float("DefaultOuterScale.V", s.DefaultOuterScale.y);

    B.Float("InnerExtent.U", s.InnerExtent.x);
    B.Float("InnerExtent.V", s.InnerExtent.y);
    B.Float("OuterExtent.U", s.OuterExtent.x);
    B.Float("OuterExtent.V", s.OuterExtent.y);

    B.Name("TextureName", s.TextureName);

    InterpretSphereShaderStruct(F, s.Shader);

    B.Int32("TextureTileCount", s.TextureTileCount);
    return F;
}

// ---------- Ring channels ----------
inline std::vector<ChunkField> InterpretRingChannel(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> F;
    auto span = OpenVariablesWrapper(chunk, "RING channel");
    if (!span) return F;

    const uint8_t* cur = span->base;
    const uint8_t* end = span->base + span->size;

    size_t idx = 0;
    while (cur + 2 <= end) {
        uint8_t id = cur[0];
        uint8_t size = cur[1];
        cur += 2;
        if (cur + size > end) break;

        switch (id) {
        case 0x02: // COLOR -> W3dRingVec3Key {Vec3, Time}
        {
            if (size == sizeof(W3dRingVec3Key)) {
                auto const& key = *reinterpret_cast<const W3dRingVec3Key*>(cur);
                ChunkFieldBuilder B(F);
                std::string base = "ColorChannel[" + std::to_string(idx) + "]";
                B.Vec3(base + ".Value", key.Value);
                B.Float(base + ".Time", key.Time);
            }
            break;
        }
        case 0x03: // ALPHA -> W3dRingAlphaKey {float, Time}
        {
            if (size == sizeof(W3dRingAlphaKey)) {
                auto const& key = *reinterpret_cast<const W3dRingAlphaKey*>(cur);
                ChunkFieldBuilder B(F);
                std::string base = "AlphaChannel[" + std::to_string(idx) + "]";
                B.Float(base + ".Value", key.Value);
                B.Float(base + ".Time", key.Time);
            }
            break;
        }
        case 0x04: // INNER_SCALE -> W3dRingVec2Key {Vec2, Time}
        case 0x05: // OUTER_SCALE -> W3dRingVec2Key
        {
            if (size == sizeof(W3dRingVec2Key)) {
                auto const& key = *reinterpret_cast<const W3dRingVec2Key*>(cur);
                std::string base = (id == 0x04 ? "InnerScaleChannel[" : "OuterScaleChannel[")
                    + std::to_string(idx) + "]";
                ChunkFieldBuilder B(F);
                B.Float(base + ".Value.U", key.Value.x);
                B.Float(base + ".Value.V", key.Value.y);
                B.Float(base + ".Time", key.Time);
            }
            break;
        }
        default:
            // unknown micro-chunk; skip
            break;
        }

        cur += size;
        ++idx;
    }

    return F;
}

