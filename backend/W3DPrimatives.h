#pragma once
#include "W3DStructs.h"
#include <vector>

// Ring and Sphere's use microchunks
inline void InterpretSphereShaderStruct(std::vector<ChunkField>& fields, const uint8_t* data, size_t baseOffset) {
    const char* DepthCompareValues[] = {
        "Pass Never", "Pass Less", "Pass Equal", "Pass Less or Equal",
        "Pass Greater", "Pass Not Equal", "Pass Greater or Equal", "Pass Always"
    };
    const char* DepthMaskValues[] = { "Disable", "Enable" };
    const char* DestBlendValues[] = {
        "Zero", "One", "Src Color", "One Minus Src Color", "Src Alpha", "One Minus Src Alpha",
        "Src Color Prefog", "Disable", "Enable", "Scale Fragment", "Replace Fragment"
    };
    const char* PriGradientValues[] = {
        "Disable", "Modulate", "Add", "Bump-Environment", "Bump-Environment Luminance", "Modulate 2x"
    };
    const char* SecGradientValues[] = { "Disable", "Enable" };
    const char* SrcBlendValues[] = { "Zero", "One", "Src Alpha", "One Minus Src Alpha" };
    const char* TexturingValues[] = { "Disable", "Enable" };
    const char* DetailColorValues[] = {
        "Disable", "Detail", "Scale", "InvScale", "Add", "Sub", "SubR", "Blend", "DetailBlend",
        "Add Signed", "Add Signed 2x", "Scale 2x", "Mod Alpha Add Color"
    };
    const char* DetailAlphaValues[] = {
        "Disable", "Detail", "Scale", "InvScale", "Disable", "Enable", "Smooth", "Flat"
    };
    const char* AlphaTestValues[] = { "Alpha Test Disable", "Alpha Test Enable" };

    auto readEnum = [&](const char* label, uint8_t value, const char* const* values, size_t count) {
        std::string val = (value < count) ? values[value] : ("Unknown (" + std::to_string(value) + ")");
        fields.push_back({ std::string("Shader.") + label, "enum", val });
        };

    readEnum("DepthCompare", data[baseOffset + 0], DepthCompareValues, std::size(DepthCompareValues));
    readEnum("DepthMask", data[baseOffset + 1], DepthMaskValues, std::size(DepthMaskValues));
    readEnum("DestBlend", data[baseOffset + 3], DestBlendValues, std::size(DestBlendValues)); // skip ColorMask
    readEnum("PriGradient", data[baseOffset + 5], PriGradientValues, std::size(PriGradientValues)); // skip FogFunc
    readEnum("SecGradient", data[baseOffset + 6], SecGradientValues, std::size(SecGradientValues));
    readEnum("SrcBlend", data[baseOffset + 7], SrcBlendValues, std::size(SrcBlendValues));
    readEnum("Texturing", data[baseOffset + 8], TexturingValues, std::size(TexturingValues));
    readEnum("DetailColor", data[baseOffset + 9], DetailColorValues, std::size(DetailColorValues));
    readEnum("DetailAlpha", data[baseOffset + 10], DetailAlphaValues, std::size(DetailAlphaValues));
    readEnum("AlphaTest", data[baseOffset + 12], AlphaTestValues, std::size(AlphaTestValues)); // skip ShaderPreset
}


inline std::vector<ChunkField> InterpretSphereHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk || chunk->data.size() < sizeof(W3dSphereStruct)) {
        fields.push_back({ "Error", "string", "Chunk too small for W3dSphereStruct" });
        return fields;
    }

    // Cast the head of the buffer straight into your struct:
    auto const* sph = reinterpret_cast<const W3dSphereStruct*>(chunk->data.data());

    // -- Version is packed major<<16 | minor
    uint32_t rawVer = sph->Version;
    uint16_t major = static_cast<uint16_t>(rawVer >> 16);
    uint16_t minor = static_cast<uint16_t>(rawVer & 0xFFFF);
    fields.push_back({
        "Version", "version",
        std::to_string(major) + "." + std::to_string(minor)
        });

    // -- Attributes (as hex & decimal)
    {
        std::ostringstream o;
        o << "0x" << std::hex << sph->Attributes
            << std::dec << " (" << sph->Attributes << ")";
        fields.push_back({ "Attributes", "hex", o.str() });
    }

    // -- Name
    fields.push_back({
        "Name", "string",
        std::string(sph->Name, strnlen(sph->Name, sizeof(sph->Name)))
        });

    // -- Center & Extent
    auto pushVec3 = [&](const char* label, const W3dVectorStruct& v) {
        std::ostringstream o;
        o << std::fixed << std::setprecision(6)
            << v.X << " " << v.Y << " " << v.Z;
        fields.push_back({ label, "vector3", o.str() });
        };
    pushVec3("Center", sph->Center);
    pushVec3("Extent", sph->Extent);

    // -- Animation duration
    fields.push_back({
        "AnimationDuration", "float",
        std::to_string(sph->AnimDuration)
        });

    // -- Default color, alpha, scale
    pushVec3("DefaultColor", sph->DefaultColor);
    fields.push_back({
        "DefaultAlpha", "float",
        std::to_string(sph->DefaultAlpha)
        });
    pushVec3("DefaultScale", sph->DefaultScale);

    // -- Default vector (quaternion + intensity)
    {
        std::ostringstream o;
        o << std::fixed << std::setprecision(6)
            << sph->DefaultVector.angle.x << " "
            << sph->DefaultVector.angle.y << " "
            << sph->DefaultVector.angle.z << " "
            << sph->DefaultVector.angle.w;
        fields.push_back({ "DefaultVector.Angle", "quat", o.str() });
        fields.push_back({
            "DefaultVector.Intensity", "float",
            std::to_string(sph->DefaultVector.intensity)
            });
    }

    // -- TextureName
    fields.push_back({
        "TextureName", "string",
        std::string(sph->TextureName,
                    strnlen(sph->TextureName, sizeof(sph->TextureName)))
        });

    // -- Shader (flattened already by your helper)
    InterpretSphereShaderStruct(fields, /* raw data ptr = */
        reinterpret_cast<const uint8_t*>(&sph->Shader),
        /* offset = */ 0
    );

    return fields;
}


inline std::vector<ChunkField> InterpretSphereChannel(
    const std::shared_ptr<ChunkItem>& chunk
) {
    enum {
        CHUNKID_SPHERE_DEF = 1, // (you already read & skipped the outer wrapper)
        CHUNKID_COLOR_CHANNEL,
        CHUNKID_ALPHA_CHANNEL,
        CHUNKID_SCALE_CHANNEL,
        CHUNKID_VECTOR_CHANNEL
    };

    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto const* ptr = chunk->data.data();
    size_t len = chunk->data.size();

    // skip outer wrapper (4 bytes ID + 4 bytes size)
    size_t offset = 8;
    int    idx = 0;

    while (offset + 2 <= len) {
        uint8_t id = ptr[offset++];
        uint8_t size = ptr[offset++];

        // make sure we have enough bytes left
        if (offset + size > len) break;

        switch (id) {
        case CHUNKID_COLOR_CHANNEL:
        case CHUNKID_SCALE_CHANNEL: {
            // both color & scale use the same Vec3+time layout
            if (size == sizeof(W3dSphereVec3Key)) {
                auto* key = reinterpret_cast<const W3dSphereVec3Key*>(ptr + offset);
                std::ostringstream oss;
                oss << key->Value.X << " " << key->Value.Y << " " << key->Value.Z;
                std::string base = (id == CHUNKID_COLOR_CHANNEL
                    ? "ColorChannel["
                    : "ScaleChannel[")
                    + std::to_string(idx) + "]";
                fields.push_back({ base + ".Value", "vector3", oss.str() });
                fields.push_back({ base + ".Time",  "float",   std::to_string(key->Time) });
            }
            break;
        }

        case CHUNKID_ALPHA_CHANNEL: {
            if (size == sizeof(W3dSphereAlphaKey)) {
                auto* key = reinterpret_cast<const W3dSphereAlphaKey*>(ptr + offset);
                std::string base = "AlphaChannel[" + std::to_string(idx) + "]";
                fields.push_back({ base + ".Value", "float", std::to_string(key->Value) });
                fields.push_back({ base + ".Time",  "float", std::to_string(key->Time) });
            }
            break;
        }

        case CHUNKID_VECTOR_CHANNEL: {
            if (size == sizeof(W3dSphereVectorKey)) {
                auto* key = reinterpret_cast<const W3dSphereVectorKey*>(ptr + offset);
                std::ostringstream q, m;
                q << key->Quat.x << " " << key->Quat.y << " "
                    << key->Quat.z << " " << key->Quat.w;
                std::string base = "VectorChannel[" + std::to_string(idx) + "]";
                fields.push_back({ base + ".Quat",      "quaternion", q.str() });
                fields.push_back({ base + ".Magnitude","float",      std::to_string(key->Magnitude) });
                fields.push_back({ base + ".Time",     "float",      std::to_string(key->Time) });
            }
            break;
        }

        default:
            // unknown micro chunk, skip it
            break;
        }

        offset += size;
        ++idx;
    }

    return fields;
}

inline std::vector<ChunkField> InterpretSphereColorChannel(const std::shared_ptr<ChunkItem>& c) {
    auto fields = InterpretSphereChannel(c);
    for (auto& f : fields) f.field = "ColorChannel." + f.field;
    return fields;
}
inline std::vector<ChunkField> InterpretSphereAlphaChannel(const std::shared_ptr<ChunkItem>& c) {
    auto fields = InterpretSphereChannel(c);
    for (auto& f : fields) f.field = "AlphaChannel." + f.field;
    return fields;
}
inline std::vector<ChunkField> InterpretSphereScaleChannel(const std::shared_ptr<ChunkItem>& c) {
    auto fields = InterpretSphereChannel(c);
    for (auto& f : fields) f.field = "ScaleChannel." + f.field;
    return fields;
}
inline std::vector<ChunkField> InterpretSphereVectorChannel(const std::shared_ptr<ChunkItem>& c) {
    auto fields = InterpretSphereChannel(c);
    for (auto& f : fields) f.field = "VectorChannel." + f.field;
    return fields;
}


inline std::vector<ChunkField> InterpretRingHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk || chunk->data.size() < sizeof(W3dRingStruct)) {
        fields.push_back({ "Error","string","Invalid RING chunk size" });
        return fields;
    }

    auto const* s = reinterpret_cast<const W3dRingStruct*>(chunk->data.data());

    // Version
    {
        uint16_t maj = uint16_t((s->Version >> 16) & 0xFFFF);
        uint16_t min = uint16_t(s->Version & 0xFFFF);
        fields.push_back({ "Version","string", std::to_string(maj) + "." + std::to_string(min) });
    }

    // Attributes
    {
        std::ostringstream o;
        o << "0x" << std::hex << s->Attributes
            << std::dec << " (" << s->Attributes << ")";
        fields.push_back({ "Attributes", "hex", o.str() });
    }

    // Name
    fields.push_back({ "Name","string", std::string(s->Name, strnlen(s->Name,sizeof(s->Name))) });

    // Center / Extent
    auto fmt3 = [&](const W3dVectorStruct& v) {
        std::ostringstream o; o << std::fixed << std::setprecision(6)
            << v.X << " " << v.Y << " " << v.Z;
        return o.str();
        };
    fields.push_back({ "Center","vector3", fmt3(s->Center) });
    fields.push_back({ "Extent","vector3", fmt3(s->Extent) });

    // AnimDuration
    fields.push_back({ "AnimDuration","float", std::to_string(s->AnimDuration) });

    // DefaultColor / Alpha
    fields.push_back({ "DefaultColor","vector3", fmt3(W3dVectorStruct{ s->DefaultColor.X, s->DefaultColor.Y, s->DefaultColor.Z }) });
    fields.push_back({ "DefaultAlpha","float", std::to_string(s->DefaultAlpha) });

    // Default scales
    auto fmt2 = [&](const Vector2& v) {
        std::ostringstream o; o << std::fixed << std::setprecision(6)
            << v.x << " " << v.y;
        return o.str();
        };
    fields.push_back({ "DefaultInnerScale","vector2", fmt2(s->DefaultInnerScale) });
    fields.push_back({ "DefaultOuterScale","vector2", fmt2(s->DefaultOuterScale) });

    // Ring extents
    fields.push_back({ "InnerExtent","vector2", fmt2(s->InnerExtent) });
    fields.push_back({ "OuterExtent","vector2", fmt2(s->OuterExtent) });

    // Texture name
    fields.push_back({ "TextureName","string", std::string(s->TextureName, strnlen(s->TextureName,sizeof(s->TextureName))) });

    // Shader (reuse your shader helper, passing pointer to s->Shader)
    InterpretSphereShaderStruct(fields, reinterpret_cast<const uint8_t*>(&s->Shader), /*offset=*/0);

    // Tile count
    fields.push_back({ "TextureTileCount","int32", std::to_string(s->TextureTileCount) });

    return fields;
}

inline std::vector<ChunkField> InterpretRingChannel(
    const std::shared_ptr<ChunkItem>& chunk
) {
    enum {
        CHUNKID_RING_DEF = 1,          // wrapper, already skipped
        CHUNKID_COLOR_CHANNEL,
        CHUNKID_ALPHA_CHANNEL,
        CHUNKID_INNER_SCALE_CHANNEL,
        CHUNKID_OUTER_SCALE_CHANNEL
    };

    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto* data = chunk->data.data();
    size_t len = chunk->data.size();
    size_t offset = 8;  // skip the 4 byte ID + 4 byte size wrapper
    int idx = 0;

    while (offset + 2 <= len) {
        uint8_t id = data[offset++];
        uint8_t size = data[offset++];

        // bounds check
        if (offset + size > len) break;

        switch (id) {
            // all three use the same Vec3 time record
        case CHUNKID_COLOR_CHANNEL:
        case CHUNKID_INNER_SCALE_CHANNEL:
        case CHUNKID_OUTER_SCALE_CHANNEL: {
            if (size == sizeof(W3dRingVec3Key)) {
                auto key = reinterpret_cast<const W3dRingVec3Key*>(data + offset);
                std::ostringstream vos;
                vos << key->Value.X
                    << " " << key->Value.Y
                    << " " << key->Value.Z;

                // pick the right channel name
                const char* chan =
                    (id == CHUNKID_COLOR_CHANNEL ? "ColorChannel" :
                        id == CHUNKID_INNER_SCALE_CHANNEL ? "InnerScaleChannel" :
                        "OuterScaleChannel");
                std::string prefix = std::string(chan) + "[" + std::to_string(idx) + "]";

                fields.emplace_back(prefix + ".Value", "vector3", vos.str());
                fields.emplace_back(prefix + ".Time", "float", std::to_string(key->Time));
            }
            break;
        }

        case CHUNKID_ALPHA_CHANNEL: {
            if (size == sizeof(W3dRingAlphaKey)) {
                auto key = reinterpret_cast<const W3dRingAlphaKey*>(data + offset);
                std::string prefix = "AlphaChannel[" + std::to_string(idx) + "]";
                fields.emplace_back(prefix + ".Value", "float", std::to_string(key->Value));
                fields.emplace_back(prefix + ".Time", "float", std::to_string(key->Time));
            }
            break;
        }

        default:
            // unknown micro chunk, just skip it
            break;
        }

        offset += size;
        ++idx;
    }

    return fields;
}
