//#pragma once
#include "W3DStructs.h"
#include <vector>


inline std::vector<ChunkField> InterpretLightInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dLightStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed LIGHT_INFO: " + *err);
        return fields;
    }
    const auto& L = std::get<W3dLightStruct>(v);

    ChunkFieldBuilder B(fields);

    // Raw attributes (full 32 bits, if that is the wire type)
    B.UInt32("Attributes", L.Attributes);

    // Decode type from low byte (1..3 -> POINT/DIRECTIONAL/SPOT)
    const uint32_t type = (L.Attributes & 0xFFu);
    switch (type) {
    case 1: B.Push("Attributes", "flag", "W3D_LIGHT_ATTRIBUTE_POINT");       break;
    case 2: B.Push("Attributes", "flag", "W3D_LIGHT_ATTRIBUTE_DIRECTIONAL"); break;
    case 3: B.Push("Attributes", "flag", "W3D_LIGHT_ATTRIBUTE_SPOT");        break;
    default: B.Push("Attributes", "string", "UNKNOWN_TYPE_" + std::to_string(type)); break;
    }

    // Cast shadows bit (0x00000100)
    if (L.Attributes & 0x00000100u) {
        B.Push("Attributes", "flag", "W3D_LIGHT_ATTRIBUTE_CAST_SHADOWS");
    }

    // Colors
    B.RGB("Ambient", L.Ambient.R, L.Ambient.G, L.Ambient.B);
    B.RGB("Diffuse", L.Diffuse.R, L.Diffuse.G, L.Diffuse.B);
    B.RGB("Specular", L.Specular.R, L.Specular.G, L.Specular.B);

    // Intensity
    B.Float("Intensity", L.Intensity);

    return fields;
}


inline std::vector<ChunkField> InterpretSpotLightInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dSpotLightStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SPOT_LIGHT_INFO: " + *err);
        return fields;
    }
    const auto& S = std::get<W3dSpotLightStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Vec3("SpotDirection", S.SpotDirection);
    B.Float("SpotAngle", S.SpotAngle);
    B.Float("SpotExponent", S.SpotExponent);
    return fields;
}

//TODO: Either this is never used or I'm unable to parse it.
inline std::vector<ChunkField> InterpretNearAtten(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dLightAttenuationStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed NEAR_ATTENUATION: " + *err);
        return fields;
    }
    const auto& A = std::get<W3dLightAttenuationStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Float("NearAtten.Start", A.Start);
    B.Float("NearAtten.End", A.End);
    return fields;
}


inline std::vector<ChunkField> InterpretFarAtten(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dLightAttenuationStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed FAR_ATTENUATION: " + *err);
        return fields;
    }
    const auto& A = std::get<W3dLightAttenuationStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Float("FarAtten.Start", A.Start);
    B.Float("FarAtten.End", A.End);
    return fields;
}

//TODO: Check w3dhub assets for examples
inline std::vector<ChunkField> InterpretSpotLightInfoTT(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dSpotLightTTStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed SPOT_LIGHT_INFO_5_0: " + *err);
        return fields;
    }
    const auto& S = std::get<W3dSpotLightTTStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Float("SpotOuterAngle", S.SpotOuterAngle);
    B.Float("SpotInnerAngle", S.SpotInnerAngle);
    return fields;
}

//TODO: Check w3dhub assets for examples
inline std::vector<ChunkField> InterpretLightPulse(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dLightPulseTTStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed PULSE: " + *err);
        return fields;
    }
    const auto& P = std::get<W3dLightPulseTTStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Float("MinIntensity", P.MinIntensity);
    B.Float("MaxIntensity", P.MaxIntensity);
    B.Float("IntensityTime", P.IntensityTime);
    B.Float("IntensityTimeRandom", P.IntensityTimeRandom);
    B.Float("IntensityAdjust", P.IntensityAdjust);
    B.UInt8("IntensityStopsAtMax", static_cast<uint8_t>(P.IntensityStopsAtMax));
    B.UInt8("IntensityStopsAtMin", static_cast<uint8_t>(P.IntensityStopsAtMin));
    return fields;
}