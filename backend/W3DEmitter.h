#pragma once
#include "W3DStructs.h"
#include <vector>


inline std::vector<ChunkField> InterpretEmitterHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dEmitterHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed EMITTER_HEADER: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dEmitterHeaderStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Version("Version", h.Version);
    B.Name("Name", h.Name);
    return fields;
}


inline std::vector<ChunkField> InterpretEmitterUserData(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;
    ChunkFieldBuilder B(fields);
    B.NullTerm("User Data",
        reinterpret_cast<const char*>(chunk->data.data()),
        chunk->data.size());
    return fields;
}


inline std::vector<ChunkField> InterpretEmitterInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dEmitterInfoStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed EMITTER_INFO: " + *err);
        return fields;
    }
    const auto& info = std::get<W3dEmitterInfoStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Name("TextureFilename", info.TextureFilename, sizeof info.TextureFilename);
    B.Float("StartSize", info.StartSize);
    B.Float("EndSize", info.EndSize);
    B.Float("Lifetime", info.Lifetime);
    B.Float("EmissionRate", info.EmissionRate);
    B.Float("MaxEmissions", info.MaxEmissions);
    B.Float("VelocityRandom", info.VelocityRandom);
    B.Float("PositionRandom", info.PositionRandom);
    B.Float("FadeTime", info.FadeTime);
    B.Float("Gravity", info.Gravity);
    B.Float("Elasticity", info.Elasticity);
    B.Vec3("Velocity", info.Velocity);
    B.Vec3("Acceleration", info.Acceleration);
    B.RGBA("StartColor", info.StartColor.R, info.StartColor.G, info.StartColor.B, info.StartColor.A);
    B.RGBA("EndColor", info.EndColor.R, info.EndColor.G, info.EndColor.B, info.EndColor.A);
    return fields;
}

static inline void EmitVolumeRandomizer(ChunkFieldBuilder& B,
    const std::string& pfx,
    const W3dVolumeRandomizerStruct& v) {
    B.UInt32(pfx + ".ClassID", v.ClassID);
    B.Float(pfx + ".Value1", v.Value1);
    B.Float(pfx + ".Value2", v.Value2);
    B.Float(pfx + ".Value3", v.Value3);
}


inline std::vector<ChunkField> InterpretEmitterInfoV2(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dEmitterInfoStructV2>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed EMITTER_INFO_V2: " + *err);
        return fields;
    }
    const auto& info = std::get<W3dEmitterInfoStructV2>(v);

    ChunkFieldBuilder B(fields);
    B.UInt32("BurstSize", info.BurstSize);
    EmitVolumeRandomizer(B, "CreationVolume", info.CreationVolume);
    EmitVolumeRandomizer(B, "VelRandom", info.VelRandom);
    B.Float("OutwardVel", info.OutwardVel);
    B.Float("VelInherit", info.VelInherit);

    // Shader sub-struct
    B.DepthCompareField("Shader.DepthCompare", info.Shader.DepthCompare);
    B.DepthMaskField("Shader.DepthMask", info.Shader.DepthMask);
	B.DestBlendField("Shader.DestBlend", info.Shader.DestBlend);
	B.PriGradientField("Shader.PriGradient", info.Shader.PriGradient);
	B.SecGradientField("Shader.SecGradient", info.Shader.SecGradient);
	B.SrcBlendField("Shader.SrcBlend", info.Shader.SrcBlend);
	B.TexturingField("Shader.Texturing", info.Shader.Texturing);
	B.DetailColorFuncField("Shader.DetailColor", info.Shader.DetailColorFunc);
	B.DetailAlphaFuncField("Shader.DetailAlpha", info.Shader.DetailAlphaFunc);
	B.AlphaTestField("Shader.AlphaTest", info.Shader.AlphaTest);
    

    B.UInt32("RenderMode", info.RenderMode);
    B.UInt32("FrameMode", info.FrameMode);
    return fields;
}


inline std::vector<ChunkField> InterpretEmitterProps(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dEmitterPropertyStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed EMITTER_PROPS: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dEmitterPropertyStruct>(v);

    const size_t headerBytes = sizeof(W3dEmitterPropertyStruct);
    if (chunk->data.size() < headerBytes) {
        fields.emplace_back("error", "string", "EMITTER_PROPS truncated before keyframes");
        return fields;
    }

    ChunkFieldBuilder B(fields);
    B.UInt32("ColorKeyframes", h.ColorKeyframes);
    B.UInt32("OpacityKeyframes", h.OpacityKeyframes);
    B.UInt32("SizeKeyframes", h.SizeKeyframes);
    B.RGBA("ColorRandom", h.ColorRandom.R, h.ColorRandom.G, h.ColorRandom.B, h.ColorRandom.A);
    B.Float("OpacityRandom", h.OpacityRandom);
    B.Float("SizeRandom", h.SizeRandom);

    // Walk payload after fixed header.
    const uint8_t* ptr = chunk->data.data() + headerBytes;
    const size_t   remain = chunk->data.size() - headerBytes;
    size_t off = 0;

    // Color keyframes: [float time][RGBA]
    for (uint32_t i = 0; i < h.ColorKeyframes; ++i) {
        if (off + sizeof(float) + sizeof(W3dRGBAStruct) > remain) break;
        float t; std::memcpy(&t, ptr + off, sizeof(float)); off += sizeof(float);
        W3dRGBAStruct c{}; std::memcpy(&c, ptr + off, sizeof(W3dRGBAStruct)); off += sizeof(W3dRGBAStruct);

        B.Float("Time[" + std::to_string(i) + "]", t);
        B.RGBA("Color[" + std::to_string(i) + "]", c.R, c.G, c.B, c.A);
    }

    // Opacity keyframes: [float time][float value]
    for (uint32_t i = 0; i < h.OpacityKeyframes; ++i) {
        if (off + sizeof(float) * 2 > remain) break;
        float t, v;
        std::memcpy(&t, ptr + off, sizeof(float)); off += sizeof(float);
        std::memcpy(&v, ptr + off, sizeof(float)); off += sizeof(float);

        B.Float("Time[" + std::to_string(i) + "]", t);
        B.Float("Opacity[" + std::to_string(i) + "]", v);
    }

    // Size keyframes: [float time][float value]
    for (uint32_t i = 0; i < h.SizeKeyframes; ++i) {
        if (off + sizeof(float) * 2 > remain) break;
        float t, v;
        std::memcpy(&t, ptr + off, sizeof(float)); off += sizeof(float);
        std::memcpy(&v, ptr + off, sizeof(float)); off += sizeof(float);

        B.Float("Time[" + std::to_string(i) + "]", t);
        B.Float("Size[" + std::to_string(i) + "]", v);
    }

    return fields;
}

//TODO: OBSOLETE
inline std::vector<ChunkField> InterpretEmitterColorKeyframe(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dEmitterColorKeyframeStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed EMITTER_COLOR_KEYFRAME: " + *err);
        return fields;
    }
    const auto& arr = std::get<std::vector<W3dEmitterColorKeyframeStruct>>(parsed);

    ChunkFieldBuilder B(fields);
    B.UInt32("Count", static_cast<uint32_t>(arr.size()));
    for (size_t i = 0; i < arr.size(); ++i) {
        const auto& r = arr[i];
        const std::string pfx = "Keyframe[" + std::to_string(i) + "]";
        B.Float(pfx + ".Time", r.Time);
        B.RGBA(pfx + ".Color", r.Color.R, r.Color.G, r.Color.B, r.Color.A);
    }
    return fields;
}

//TODO: OBSOLETE
inline std::vector<ChunkField> InterpretEmitterOpacityKeyframe(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dEmitterOpacityKeyframeStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed EMITTER_OPACITY_KEYFRAME: " + *err);
        return fields;
    }
    const auto& arr = std::get<std::vector<W3dEmitterOpacityKeyframeStruct>>(parsed);

    ChunkFieldBuilder B(fields);
    B.UInt32("Count", static_cast<uint32_t>(arr.size()));
    for (size_t i = 0; i < arr.size(); ++i) {
        const auto& r = arr[i];
        const std::string pfx = "Keyframe[" + std::to_string(i) + "]";
        B.Float(pfx + ".Time", r.Time);
        B.Float(pfx + ".Opacity", r.Opacity);
    }
    return fields;
}

//TODO: OBSOLETE
inline std::vector<ChunkField> InterpretEmitterSizeKeyframe(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;

    auto parsed = ParseChunkArray<W3dEmitterSizeKeyframeStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.emplace_back("error", "string", "Malformed EMITTER_SIZE_KEYFRAME: " + *err);
        return fields;
    }
    const auto& arr = std::get<std::vector<W3dEmitterSizeKeyframeStruct>>(parsed);

    ChunkFieldBuilder B(fields);
    B.UInt32("Count", static_cast<uint32_t>(arr.size()));
    for (size_t i = 0; i < arr.size(); ++i) {
        const auto& r = arr[i];
        const std::string pfx = "Keyframe[" + std::to_string(i) + "]";
        B.Float(pfx + ".Time", r.Time);
        B.Float(pfx + ".Size", r.Size);
    }
    return fields;
}


enum : uint32_t {
    W3D_ELINE_MERGE_INTERSECTIONS = 0x00000001,        // merge crossings
    W3D_ELINE_FREEZE_RANDOM = 0x00000002,        // freeze random ordering
    W3D_ELINE_DISABLE_SORTING = 0x00000004,        // disable z sorting
    W3D_ELINE_END_CAPS = 0x00000008,        // draw end cap geometry

    // texture map mode is in the high byte (bits 24 31)
    W3D_ELINE_TEXTURE_MAP_MODE_OFFSET = 24,
    W3D_ELINE_TEXTURE_MAP_MODE_MASK = 0xFFu << W3D_ELINE_TEXTURE_MAP_MODE_OFFSET,

    W3D_ELINE_UNIFORM_WIDTH_TEXTURE_MAP = 0x00u << W3D_ELINE_TEXTURE_MAP_MODE_OFFSET,
    W3D_ELINE_UNIFORM_LENGTH_TEXTURE_MAP = 0x01u << W3D_ELINE_TEXTURE_MAP_MODE_OFFSET,
    W3D_ELINE_TILED_TEXTURE_MAP = 0x02u << W3D_ELINE_TEXTURE_MAP_MODE_OFFSET,
};

inline std::vector<ChunkField> InterpretEmitterLineProperties(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dEmitterLinePropertiesStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed EMITTER_LINE_PROPERTIES: " + *err);
        return fields;
    }
    const auto& p = std::get<W3dEmitterLinePropertiesStruct>(v);

    ChunkFieldBuilder B(fields);
    B.UInt32("Flags", p.Flags);

    // decoded flags
    if (p.Flags & W3D_ELINE_MERGE_INTERSECTIONS) B.Push("Flags", "flag", "MergeIntersections");
    if (p.Flags & W3D_ELINE_FREEZE_RANDOM)       B.Push("Flags", "flag", "FreezeRandom");
    if (p.Flags & W3D_ELINE_DISABLE_SORTING)     B.Push("Flags", "flag", "DisableSorting");
    if (p.Flags & W3D_ELINE_END_CAPS)            B.Push("Flags", "flag", "EndCaps");

    const uint32_t tmode = (p.Flags & W3D_ELINE_TEXTURE_MAP_MODE_MASK) >> W3D_ELINE_TEXTURE_MAP_MODE_OFFSET;
    switch (tmode) {
    case (W3D_ELINE_UNIFORM_WIDTH_TEXTURE_MAP >> W3D_ELINE_TEXTURE_MAP_MODE_OFFSET):  B.Push("Flags", "flag", "UniformWidthTextureMap");  break;
    case (W3D_ELINE_UNIFORM_LENGTH_TEXTURE_MAP >> W3D_ELINE_TEXTURE_MAP_MODE_OFFSET):  B.Push("Flags", "flag", "UniformLengthTextureMap"); break;
    case (W3D_ELINE_TILED_TEXTURE_MAP >> W3D_ELINE_TEXTURE_MAP_MODE_OFFSET):  B.Push("Flags", "flag", "TiledTextureMap");         break;
    default: B.Push("Flags", "flag", "UnknownTextureMapMode"); break;
    }

    B.UInt32("SubdivisionLevel", p.SubdivisionLevel);
    B.Float("NoiseAmplitude", p.NoiseAmplitude);
    B.Float("MergeAbortFactor", p.MergeAbortFactor);
    B.Float("TextureTileFactor", p.TextureTileFactor);
    B.Float("UPerSec", p.UPerSec);
    B.Float("VPerSec", p.VPerSec);

    return fields;
}

inline std::vector<ChunkField> InterpretEmitterRotationKeys(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dEmitterRotationHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed EMITTER_ROTATION_KEYFRAMES: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dEmitterRotationHeaderStruct>(v);

    constexpr size_t HDR = sizeof(W3dEmitterRotationHeaderStruct);
    if (chunk->data.size() < HDR) {
        fields.emplace_back("error", "string", "Rotation header truncated");
        return fields;
    }

    // Total float pairs expected after the header
    const size_t expected_pairs = static_cast<size_t>(h.KeyframeCount) + 1; // wdump behavior
    const size_t avail_floats = (chunk->data.size() - HDR) / sizeof(float);
    const size_t avail_pairs = avail_floats / 2; // each pair = 2 floats
    const size_t pairs = std::min(expected_pairs, avail_pairs);

    ChunkFieldBuilder B(fields);
    B.UInt32("KeyframeCount", h.KeyframeCount);
    B.Float("Random", h.Random);
    B.Float("OrientationRandom", h.OrientationRandom);

    const float* f = reinterpret_cast<const float*>(chunk->data.data() + HDR);
    for (size_t i = 0; i < pairs; ++i) {
        const float t = f[i * 2 + 0];
        const float r = f[i * 2 + 1];
        B.Float("Time[" + std::to_string(i) + "]", t);
        B.Float("Rotation[" + std::to_string(i) + "]", r);
    }

    return fields;
}

inline std::vector<ChunkField> InterpretEmitterFrameKeys(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dEmitterFrameHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed EMITTER_FRAME_KEYFRAMES: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dEmitterFrameHeaderStruct>(v);

    constexpr size_t HDR = sizeof(W3dEmitterFrameHeaderStruct);
    if (chunk->data.size() < HDR) {
        fields.emplace_back("error", "string", "Frame header truncated");
        return fields;
    }

    const size_t expected_pairs = static_cast<size_t>(h.KeyframeCount) + 1; 
    const size_t avail_pairs = (chunk->data.size() - HDR) / sizeof(W3dEmitterFrameKeyframeStruct);
    const size_t pairs = std::min(expected_pairs, avail_pairs);

    ChunkFieldBuilder B(fields);
    B.UInt32("KeyframeCount", h.KeyframeCount);
    B.Float("Random", h.Random);

    const auto* keys = reinterpret_cast<const W3dEmitterFrameKeyframeStruct*>(chunk->data.data() + HDR);
    for (size_t i = 0; i < pairs; ++i) {
        B.Float("Time[" + std::to_string(i) + "]", keys[i].Time);
        B.Float("Frame[" + std::to_string(i) + "]", keys[i].Frame);
    }

    if (pairs < expected_pairs) {
        B.Push("warning", "string",
            "Only " + std::to_string(pairs) + " pairs available; expected " +
            std::to_string(expected_pairs));
    }

    return fields;
}


inline std::vector<ChunkField> InterpretEmitterBlurTimeKeyframes(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dEmitterBlurTimeHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string",
            "Malformed EMITTER_BLUR_TIME_KEYFRAMES: " + *err);
        return fields;
    }
    const auto& h = std::get<W3dEmitterBlurTimeHeaderStruct>(v);

    constexpr size_t HDR = sizeof(W3dEmitterBlurTimeHeaderStruct);
    if (chunk->data.size() < HDR) {
        fields.emplace_back("error", "string", "BlurTime header truncated");
        return fields;
    }

    // available entries in buffer
    const size_t avail = (chunk->data.size() - HDR) / sizeof(W3dEmitterBlurTimeKeyframeStruct);
    // rule: actual entries = KeyframeCount + 1
    const size_t want = static_cast<size_t>(h.KeyframeCount) + 1;
    const size_t count = std::min(want, avail);

    const auto* keys =
        reinterpret_cast<const W3dEmitterBlurTimeKeyframeStruct*>(chunk->data.data() + HDR);

    ChunkFieldBuilder B(fields);
    B.UInt32("KeyframeCount", h.KeyframeCount);
    B.Float("Random", h.Random);

    for (size_t i = 0; i < count; ++i) {
        B.Float("Time[" + std::to_string(i) + "]", keys[i].Time);
        B.Float("BlurTime[" + std::to_string(i) + "]", keys[i].BlurTime);
    }
    return fields;
}


inline std::vector<ChunkField> InterpretEmitterExtraInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields; if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dEmitterExtraInfoStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed EXTRA_INFO: " + *err);
        return fields;
    }
    const auto& info = std::get<W3dEmitterExtraInfoStruct>(v);

    ChunkFieldBuilder B(fields);
    
    B.Float("FutureStartTime", info.FutureStartTime);

    return fields;
}
