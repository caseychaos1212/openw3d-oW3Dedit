#pragma once
#include "W3DStructs.h"
#include <vector>

inline std::vector<ChunkField> InterpretEmitterHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk || chunk->data.size() < sizeof(W3dEmitterHeaderStruct)) {
        fields.emplace_back("Error", "string", "Invalid EMITTER_HEADER chunk");
        return fields;
    }
    auto const* hdr = reinterpret_cast<const W3dEmitterHeaderStruct*>(chunk->data.data());

    // Version  highword.major, low word.minor
    uint16_t major = static_cast<uint16_t>(hdr->Version >> 16);
    uint16_t minor = static_cast<uint16_t>(hdr->Version & 0xFFFF);
    fields.emplace_back("Version", "string",
        std::to_string(major) + "." + std::to_string(minor));

    // Name is null terminated or padded
    std::string name(hdr->Name, strnlen(hdr->Name, W3D_NAME_LEN));
    fields.emplace_back("Name", "string", name);

    return fields;
}

inline std::vector<ChunkField> InterpretEmitterUserData(
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
      "User Data",
      "string",
      nts.value
        });
    return fields;
}

inline std::vector<ChunkField> InterpretEmitterInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk || chunk->data.size() < sizeof(W3dEmitterInfoStruct)) {
        fields.emplace_back("Error", "string", "Invalid EMITTER_INFO chunk");
        return fields;
    }

    
    auto const* info = reinterpret_cast<const W3dEmitterInfoStruct*>(chunk->data.data());

    
    std::string texName(info->TextureFilename,
        strnlen(info->TextureFilename, sizeof(info->TextureFilename)));
    fields.emplace_back("TextureFilename", "string", texName);

    
    fields.emplace_back("StartSize", "float", std::to_string(info->StartSize));
    fields.emplace_back("EndSize", "float", std::to_string(info->EndSize));
    fields.emplace_back("Lifetime", "float", std::to_string(info->Lifetime));
    fields.emplace_back("EmissionRate", "float", std::to_string(info->EmissionRate));
    fields.emplace_back("MaxEmissions", "float", std::to_string(info->MaxEmissions));
    fields.emplace_back("VelocityRandom", "float", std::to_string(info->VelocityRandom));
    fields.emplace_back("PositionRandom", "float", std::to_string(info->PositionRandom));
    fields.emplace_back("FadeTime", "float", std::to_string(info->FadeTime));
    fields.emplace_back("Gravity", "float", std::to_string(info->Gravity));
    fields.emplace_back("Elasticity", "float", std::to_string(info->Elasticity));

    
    auto vec3str = [&](const W3dVectorStruct& v) {
        std::ostringstream o;
        o << std::fixed << std::setprecision(6)
            << v.X << " " << v.Y << " " << v.Z;
        return o.str();
        };
    fields.emplace_back("Velocity", "vector3", vec3str(info->Velocity));
    fields.emplace_back("Acceleration", "vector3", vec3str(info->Acceleration));

    
    auto rgbaStr = [&](const W3dRGBAStruct& c) {
        std::ostringstream o;
        o << int(c.R) << " " << int(c.G) << " " << int(c.B) << " " << int(c.A);
        return o.str();
        };
    fields.emplace_back("StartColor", "RGBA", rgbaStr(info->StartColor));
    fields.emplace_back("EndColor", "RGBA", rgbaStr(info->EndColor));

    return fields;
}

// small helper to unroll each VolumeRandomizer
static void EmitVolumeRandomizer(std::vector<ChunkField>& fields,
    std::string const& prefix,
    W3dVolumeRandomizerStruct const& vol)
{
    fields.emplace_back(prefix + ".ClassID", "uint32",
        std::to_string(vol.ClassID));
    fields.emplace_back(prefix + ".Value1", "float",
        std::to_string(vol.Value1));
    fields.emplace_back(prefix + ".Value2", "float",
        std::to_string(vol.Value2));
    fields.emplace_back(prefix + ".Value3", "float",
        std::to_string(vol.Value3));
}


inline std::vector<ChunkField> InterpretEmitterInfoV2(
    const std::shared_ptr<ChunkItem>& chunk)
{
    std::vector<ChunkField> fields;
    if (!chunk || chunk->data.size() < sizeof(W3dEmitterInfoStructV2)) {
        fields.emplace_back("Error", "string",
            "EmitterInfoV2 chunk too small");
        return fields;
    }

    
    auto const* info =
        reinterpret_cast<const W3dEmitterInfoStructV2*>(chunk->data.data());

    fields.emplace_back("BurstSize", "uint32",
        std::to_string(info->BurstSize));

    EmitVolumeRandomizer(fields, "CreationVolume", info->CreationVolume);
    EmitVolumeRandomizer(fields, "VelRandom", info->VelRandom);

    fields.emplace_back("OutwardVel", "float",
        std::to_string(info->OutwardVel));
    fields.emplace_back("VelInherit", "float",
        std::to_string(info->VelInherit));

    // inline unpack your shader struct
    fields.emplace_back("Shader.DepthCompare", "uint8",
        std::to_string(info->Shader.DepthCompare));
    fields.emplace_back("Shader.DepthMask", "uint8",
        std::to_string(info->Shader.DepthMask));
   

    fields.emplace_back("RenderMode", "uint32",
        std::to_string(info->RenderMode));
    fields.emplace_back("FrameMode", "uint32",
        std::to_string(info->FrameMode));
    // reserved you can ignore

    return fields;
}

inline std::vector<ChunkField> InterpretEmitterProps(
    const std::shared_ptr<ChunkItem>& chunk)
{
    std::vector<ChunkField> fields;
    if (!chunk || chunk->data.size() < sizeof(W3dEmitterPropertyStruct)) {
        fields.emplace_back("Error", "string",
            "EmitterProps chunk too small");
        return fields;
    }

    // 1) pull out the fixed size header in one cast
    auto const* hdr = reinterpret_cast<
        const W3dEmitterPropertyStruct*>(
            chunk->data.data());

    fields.emplace_back("ColorKeyframes", "uint32",
        std::to_string(hdr->ColorKeyframes));
    fields.emplace_back("OpacityKeyframes", "uint32",
        std::to_string(hdr->OpacityKeyframes));
    fields.emplace_back("SizeKeyframes", "uint32",
        std::to_string(hdr->SizeKeyframes));

    // ColorRandom is stored as RGBA:
    {
        auto const& c = hdr->ColorRandom;
        fields.emplace_back("ColorRandom", "rgba",
            std::to_string(c.R) + " "
            + std::to_string(c.G) + " "
            + std::to_string(c.B) + " "
            + std::to_string(c.A)  // if pad holds alpha
        );
    }

    fields.emplace_back("OpacityRandom", "float",
        std::to_string(hdr->OpacityRandom));
    fields.emplace_back("SizeRandom", "float",
        std::to_string(hdr->SizeRandom));

    // 2) now advance past that header into the keyframe data
    size_t offset = sizeof(W3dEmitterPropertyStruct);
    auto const* ptr = chunk->data.data();
    auto remain = chunk->data.size();

    // --- color keyframes (time + RGBA) ---
    for (uint32_t i = 0; i < hdr->ColorKeyframes; ++i) {
        if (offset + 4 + sizeof(W3dRGBAStruct) > remain) break;
        float t = *reinterpret_cast<const float*>(ptr + offset);
        offset += 4;
        auto const* col = reinterpret_cast<
            const W3dRGBAStruct*>(ptr + offset);
        offset += sizeof(W3dRGBAStruct);

        fields.emplace_back("ColorTime[" + std::to_string(i) + "]",
            "float", std::to_string(t));
        fields.emplace_back("Color[" + std::to_string(i) + "]",
            "rgba",
            std::to_string(col->R) + " "
            + std::to_string(col->G) + " "
            + std::to_string(col->B) + " "
            + std::to_string(col->A));
    }

    // --- opacity keyframes (time + value) ---
    for (uint32_t i = 0; i < hdr->OpacityKeyframes; ++i) {
        if (offset + 8 > remain) break;
        float t = *reinterpret_cast<const float*>(ptr + offset);
        float v = *reinterpret_cast<const float*>(ptr + offset + 4);
        offset += 8;

        fields.emplace_back("OpacityTime[" + std::to_string(i) + "]",
            "float", std::to_string(t));
        fields.emplace_back("Opacity[" + std::to_string(i) + "]",
            "float", std::to_string(v));
    }

    // --- size keyframes (time + value) ---
    for (uint32_t i = 0; i < hdr->SizeKeyframes; ++i) {
        if (offset + 8 > remain) break;
        float t = *reinterpret_cast<const float*>(ptr + offset);
        float v = *reinterpret_cast<const float*>(ptr + offset + 4);
        offset += 8;

        fields.emplace_back("SizeTime[" + std::to_string(i) + "]",
            "float", std::to_string(t));
        fields.emplace_back("Size[" + std::to_string(i) + "]",
            "float", std::to_string(v));
    }

    return fields;
}

inline std::vector<ChunkField> InterpretEmitterColorKeyframe(
    const std::shared_ptr<ChunkItem>& chunk)
{
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty EMITTER_COLOR_KEYFRAME chunk");
        return fields;
    }

    constexpr size_t REC_SZ = sizeof(W3dEmitterColorKeyframeStruct);
    auto& data = chunk->data;
    if (data.size() % REC_SZ != 0) {
        fields.emplace_back(
            "error", "string",
            "Unexpected EMITTER_COLOR_KEYFRAME size: " + std::to_string(data.size()));
        return fields;
    }

    auto ptr = reinterpret_cast<const W3dEmitterColorKeyframeStruct*>(data.data());
    size_t count = data.size() / REC_SZ;
    for (size_t i = 0; i < count; ++i) {
        auto const& r = ptr[i];
        std::string pfx = "Keyframe[" + std::to_string(i) + "]";
        fields.emplace_back(pfx + ".Time", "float", std::to_string(r.Time));
        fields.emplace_back(pfx + ".Color.R", "uint8_t", std::to_string(r.Color.R));
        fields.emplace_back(pfx + ".Color.G", "uint8_t", std::to_string(r.Color.G));
        fields.emplace_back(pfx + ".Color.B", "uint8_t", std::to_string(r.Color.B));
        fields.emplace_back(pfx + ".Color.A", "uint8_t", std::to_string(r.Color.A));
    }
    return fields;
}

inline std::vector<ChunkField> InterpretEmitterOpacityKeyframe(
    const std::shared_ptr<ChunkItem>& chunk)
{
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty EMITTER_OPACITY_KEYFRAME chunk");
        return fields;
    }

    constexpr size_t REC_SZ = sizeof(W3dEmitterOpacityKeyframeStruct);
    auto& data = chunk->data;
    if (data.size() % REC_SZ != 0) {
        fields.emplace_back(
            "error", "string",
            "Unexpected EMITTER_OPACITY_KEYFRAME size: " + std::to_string(data.size()));
        return fields;
    }

    auto ptr = reinterpret_cast<const W3dEmitterOpacityKeyframeStruct*>(data.data());
    size_t count = data.size() / REC_SZ;
    for (size_t i = 0; i < count; ++i) {
        auto const& r = ptr[i];
        std::string pfx = "Keyframe[" + std::to_string(i) + "]";
        fields.emplace_back(pfx + ".Time", "float", std::to_string(r.Time));
        fields.emplace_back(pfx + ".Opacity", "float", std::to_string(r.Opacity));
    }
    return fields;
}

inline std::vector<ChunkField> InterpretEmitterSizeKeyframe(
    const std::shared_ptr<ChunkItem>& chunk)
{
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("error", "string", "Empty EMITTER_SIZE_KEYFRAME chunk");
        return fields;
    }

    constexpr size_t REC_SZ = sizeof(W3dEmitterSizeKeyframeStruct);
    auto& data = chunk->data;
    if (data.size() % REC_SZ != 0) {
        fields.emplace_back(
            "error", "string",
            "Unexpected EMITTER_SIZE_KEYFRAME size: " + std::to_string(data.size()));
        return fields;
    }

    auto ptr = reinterpret_cast<const W3dEmitterSizeKeyframeStruct*>(data.data());
    size_t count = data.size() / REC_SZ;
    for (size_t i = 0; i < count; ++i) {
        auto const& r = ptr[i];
        std::string pfx = "Keyframe[" + std::to_string(i) + "]";
        fields.emplace_back(pfx + ".Time", "float", std::to_string(r.Time));
        fields.emplace_back(pfx + ".Size", "float", std::to_string(r.Size));
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

inline std::vector<ChunkField> InterpretEmitterLineProperties(
    const std::shared_ptr<ChunkItem>& chunk)
{
    std::vector<ChunkField> fields;
    // must have at least one full struct
    if (!chunk || chunk->data.size() < sizeof(W3dEmitterLinePropertiesStruct)) {
        fields.emplace_back("error", "string", "Invalid EMITTER_LINE_PROPERTIES chunk");
        return fields;
    }

    auto const* p = reinterpret_cast<const W3dEmitterLinePropertiesStruct*>(chunk->data.data());

    // 1 Flags
    uint32_t flags = p->Flags;
    fields.emplace_back("Flags (raw)", "uint32", std::to_string(flags));

    // decode bits
    std::vector<std::string> parts;
    if (flags & W3D_ELINE_MERGE_INTERSECTIONS)  parts.emplace_back("MergeIntersections");
    if (flags & W3D_ELINE_FREEZE_RANDOM)        parts.emplace_back("FreezeRandom");
    if (flags & W3D_ELINE_DISABLE_SORTING)      parts.emplace_back("DisableSorting");
    if (flags & W3D_ELINE_END_CAPS)             parts.emplace_back("EndCaps");
    // texture map mode in high byte
    switch ((flags & W3D_ELINE_TEXTURE_MAP_MODE_MASK) >> W3D_ELINE_TEXTURE_MAP_MODE_OFFSET) {
    case W3D_ELINE_UNIFORM_WIDTH_TEXTURE_MAP:  parts.emplace_back("UniformWidthTextureMap"); break;
    case W3D_ELINE_UNIFORM_LENGTH_TEXTURE_MAP: parts.emplace_back("UniformLengthTextureMap"); break;
    case W3D_ELINE_TILED_TEXTURE_MAP:          parts.emplace_back("TiledTextureMap"); break;
    default:                                   parts.emplace_back("UnknownTextureMapMode"); break;
    }
    // join
    std::string decoded;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) decoded += " | ";
        decoded += parts[i];
    }
    fields.emplace_back("Flags (decoded)", "flags", decoded);

    // now the rest of the struct
    fields.emplace_back("SubdivisionLevel", "uint32", std::to_string(p->SubdivisionLevel));
    fields.emplace_back("NoiseAmplitude", "float", std::to_string(p->NoiseAmplitude));
    fields.emplace_back("MergeAbortFactor", "float", std::to_string(p->MergeAbortFactor));
    fields.emplace_back("TextureTileFactor", "float", std::to_string(p->TextureTileFactor));
    fields.emplace_back("UPerSec", "float", std::to_string(p->UPerSec));
    fields.emplace_back("VPerSec", "float", std::to_string(p->VPerSec));

    return fields;
}
inline std::vector<ChunkField> InterpretEmitterRotationKeys(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk || chunk->data.size() < sizeof(W3dEmitterRotationHeaderStruct)) {
        fields.emplace_back("error", "string", "Invalid EMITTER_ROTATION_KEYFRAMES chunk");
        return fields;
    }

    // 1) header
    auto const* hdr = reinterpret_cast<const W3dEmitterRotationHeaderStruct*>(chunk->data.data());
    fields.emplace_back("KeyframeCount", "uint32", std::to_string(hdr->KeyframeCount));
    fields.emplace_back("RandomVelocity", "float", std::to_string(hdr->Random));
    fields.emplace_back("OrientationRandom", "float", std::to_string(hdr->OrientationRandom));

    // 2) keyframes (just an array of float32's)
    size_t n = hdr->KeyframeCount;
    auto const* keys = reinterpret_cast<const float*>(
        chunk->data.data() + sizeof(W3dEmitterRotationHeaderStruct)
        );
    // make sure we have enough bytes
    size_t available = (chunk->data.size() - sizeof(W3dEmitterRotationHeaderStruct)) / sizeof(float);
    n = std::min(n, available);
    for (size_t i = 0; i < n; ++i) {
        fields.emplace_back(
            "Keyframe[" + std::to_string(i) + "]",
            "float",
            std::to_string(keys[i])
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretEmitterFrameKeys(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    // 1) Make sure we have at least the header
    if (!chunk || chunk->data.size() < sizeof(W3dEmitterFrameHeaderStruct)) {
        fields.emplace_back("error", "string", "Invalid EMITTER_FRAME_KEYFRAMES chunk");
        return fields;
    }

    // 2) Interpret the header
    auto const* hdr = reinterpret_cast<const W3dEmitterFrameHeaderStruct*>(chunk->data.data());
    fields.emplace_back("KeyframeCount", "uint32", std::to_string(hdr->KeyframeCount));
    fields.emplace_back("Random", "float", std::to_string(hdr->Random));

    // 3) Now point at the keyframe data
    size_t headerBytes = sizeof(W3dEmitterFrameHeaderStruct);
    auto const* keys = reinterpret_cast<const W3dEmitterFrameKeyframeStruct*>(
        chunk->data.data() + headerBytes
        );

    // 4) How many full keyframes actually fit?
    size_t available = (chunk->data.size() - headerBytes)
        / sizeof(W3dEmitterFrameKeyframeStruct);
    size_t count = std::min<size_t>(hdr->KeyframeCount, available);

    // 5) Emit each (time,frame) pair
    for (size_t i = 0; i < count; ++i) {
        fields.emplace_back(
            "Time[" + std::to_string(i) + "]",
            "float",
            std::to_string(keys[i].Time)
        );
        fields.emplace_back(
            "Frame[" + std::to_string(i) + "]",
            "float",
            std::to_string(keys[i].Frame)
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretEmitterBlurTimeKeyframes(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;

    // 1) Check we have at least the header
    if (!chunk || chunk->data.size() < sizeof(W3dEmitterBlurTimeHeaderStruct)) {
        fields.emplace_back("error", "string", "Invalid EMITTER_BLUR_TIME_KEYFRAMES chunk");
        return fields;
    }

    // 2) Cast and emit header fields
    auto const* hdr = reinterpret_cast<const W3dEmitterBlurTimeHeaderStruct*>(chunk->data.data());
    fields.emplace_back("KeyframeCount", "uint32", std::to_string(hdr->KeyframeCount));
    fields.emplace_back("Random", "float", std::to_string(hdr->Random));

    // 3) Compute how many keyframes actually fit
    size_t headerBytes = sizeof(W3dEmitterBlurTimeHeaderStruct);
    size_t entrySize = sizeof(W3dEmitterBlurTimeKeyframeStruct);
    size_t available = chunk->data.size() - headerBytes;
    size_t maxEntries = available / entrySize;
    size_t count = std::min<size_t>(hdr->KeyframeCount, maxEntries);

    // 4) Walk the array of {Time, BlurTime}
    auto const* entries = reinterpret_cast<const W3dEmitterBlurTimeKeyframeStruct*>(
        chunk->data.data() + headerBytes
        );
    for (size_t i = 0; i < count; ++i) {
        fields.emplace_back(
            "Keyframe[" + std::to_string(i) + "].Time",
            "float",
            std::to_string(entries[i].Time)
        );
        fields.emplace_back(
            "Keyframe[" + std::to_string(i) + "].BlurTime",
            "float",
            std::to_string(entries[i].BlurTime)
        );
    }

    return fields;
}