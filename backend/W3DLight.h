#pragma once
#include "W3DStructs.h"
#include <vector>

inline std::vector<ChunkField> InterpretLightInfo(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;

    // 1 Bounds check
    if (!chunk || chunk->data.size() < sizeof(W3dLightStruct)) {
        fields.emplace_back("error", "string", "Invalid LIGHT_INFO chunk size");
        return fields;
    }

    // 2 Reinterpret the buffer directly as our struct
    auto const* light = reinterpret_cast<const W3dLightStruct*>(chunk->data.data());

    // 3 Decode type low byte of Attributes
    uint8_t type = static_cast<uint8_t>(light->Attributes & 0xFF);
    static const std::array<const char*, 3> typeNames = {
        "POINT", "DIRECTIONAL", "SPOT"
    };
    std::string typeStr = (type >= 1 && type <= 3)
        ? std::string("W3D_LIGHT_ATTRIBUTE_") + typeNames[type - 1]
        : "UNKNOWN(0x" + std::to_string(type) + ")";
    fields.emplace_back("Attributes.Type", "string", typeStr);

    // 4 Cast shadows bit
    if (light->Attributes & 0x00000100) {
        fields.emplace_back(
            "Attributes.CastShadows",
            "flag",
            "W3D_LIGHT_ATTRIBUTE_CAST_SHADOWS"
        );
    }

    // 5 Helper to emit an RGB triple
    auto emitRGB = [&](const char* name, const W3dRGBStruct& c) {
        std::ostringstream oss;
        oss << "("
            << int(c.R) << " "
            << int(c.G) << " "
            << int(c.B) << ")";
        fields.emplace_back(name, "RGB", oss.str());
        };

    emitRGB("Ambient", light->Ambient);
    emitRGB("Diffuse", light->Diffuse);
    emitRGB("Specular", light->Specular);

    // 6 Intensity
    fields.emplace_back(
        "Intensity",
        "float",
        std::to_string(light->Intensity)
    );

    return fields;
}

inline std::vector<ChunkField> InterpretSpotLightInfo(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;

    // 1 Size check
    if (!chunk || chunk->data.size() < sizeof(W3dSpotLightStruct)) {
        fields.emplace_back("error", "string", "Invalid SPOT_LIGHT_INFO chunk size");
        return fields;
    }

    // 2 Cast once
    auto const* spot = reinterpret_cast<const W3dSpotLightStruct*>(chunk->data.data());

    // 3 SpotDirection
    {
        std::ostringstream oss;
        oss << "("
            << std::fixed << std::setprecision(6)
            << spot->SpotDirection.X << " "
            << spot->SpotDirection.Y << " "
            << spot->SpotDirection.Z
            << ")";
        fields.emplace_back("SpotDirection", "vector3", oss.str());
    }

    // 4 SpotAngle & SpotExponent
    fields.emplace_back(
        "SpotAngle",
        "float",
        std::to_string(spot->SpotAngle)
    );
    fields.emplace_back(
        "SpotExponent",
        "float",
        std::to_string(spot->SpotExponent)
    );

    return fields;
}

inline std::vector<ChunkField> InterpretNearAtten(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> f;
    if (!chunk || chunk->data.size() < sizeof(W3dLightAttenuationStruct)) {
        f.emplace_back("error", "string", "Invalid NEAR_ATTENUATION chunk size");
        return f;
    }
    auto const* att = reinterpret_cast<const W3dLightAttenuationStruct*>(chunk->data.data());
    f.emplace_back("Near Atten Start", "float", std::to_string(att->Start));
    f.emplace_back("Near Atten End", "float", std::to_string(att->End));
    return f;
}

inline std::vector<ChunkField> InterpretFarAtten(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> f;
    if (!chunk || chunk->data.size() < sizeof(W3dLightAttenuationStruct)) {
        f.emplace_back("error", "string", "Invalid FAR_ATTENUATION chunk size");
        return f;
    }
    auto const* att = reinterpret_cast<const W3dLightAttenuationStruct*>(chunk->data.data());
    f.emplace_back("Far Atten Start", "float", std::to_string(att->Start));
    f.emplace_back("Far Atten End", "float", std::to_string(att->End));
    return f;
}