#pragma once
#include "W3DStructs.h"
#include <vector>

inline std::vector<ChunkField> InterpretSoundRObjHeader(
    const std::shared_ptr<ChunkItem>& chunk
) 

{
    std::vector<ChunkField> fields;
    if (!chunk || chunk->data.size() < sizeof(W3dSoundRObjHeaderStruct)) {
        fields.push_back({ "Error", "string", "Invalid SOUNDROBJ_HEADER size" });
        return fields;
    }

    auto const* hdr = reinterpret_cast<const W3dSoundRObjHeaderStruct*>(chunk->data.data());

    // Version
    {
        uint16_t major = static_cast<uint16_t>(hdr->Version >> 16);
        uint16_t minor = static_cast<uint16_t>(hdr->Version & 0xFFFF);
        fields.push_back({
            "Version", "version",
            std::to_string(major) + "." + std::to_string(minor)
            });
    }

    // Name (null terminated within fixed buffer)
    {
        size_t len = strnlen(hdr->Name, sizeof(hdr->Name));
        fields.push_back({
            "Name", "string",
            std::string(hdr->Name, len)
            });
    }

    // Flags
    fields.push_back({
        "Flags", "uint32",
        std::to_string(hdr->Flags)
        });

    return fields;
}

inline std::vector<ChunkField> InterpretSoundRObjDefinition(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.push_back({ "error","string","Empty SOUNDROBJ_DEFINITION" });
        return fields;
    }


    constexpr uint32_t SOUND_RENDER_DEF_EXT = 0x0200;
    if (chunk->parent && chunk->parent->id == SOUND_RENDER_DEF_EXT) {
        for (auto& child : chunk->children) {
            switch (child->id) {
            case 0x01: {
                // m_ID is stored as a 4-byte uint
                uint32_t v = *reinterpret_cast<const uint32_t*>(child->data.data());
                fields.push_back({ "m_ID", "int32", std::to_string(v) });
                break;
            }
            case 0x03: {
                // m_Name is a null-terminated string
                std::string s(
                    reinterpret_cast<const char*>(child->data.data()),
                    child->data.size()
                );
                if (!s.empty() && s.back() == '\0') s.pop_back();
                fields.push_back({ "m_Name", "string", s });
                break;
            }
            default:
                // you can ignore or log other micro-IDs here
                break;
            }
        }
        return fields;
    }

    // For each saved variable sub-chunk, dispatch by its ID:
    for (auto& child : chunk->children) {
        uint32_t id = child->id;
        switch (id) {

        case 0x03: {
            float v = *reinterpret_cast<const float*>(child->data.data());
            fields.push_back({ "m_Priority", "float", std::to_string(v) });
            break;
        }
        case 0x04: {
            float v = *reinterpret_cast<const float*>(child->data.data());
            fields.push_back({ "m_Volume", "float", std::to_string(v) });
            break;
        }
        case 0x05: {
            float v = *reinterpret_cast<const float*>(child->data.data());
            fields.push_back({ "m_Pan", "float", std::to_string(v) });
            break;
        }
        case 0x06: {
            uint32_t v = *reinterpret_cast<const uint32_t*>(child->data.data());
            fields.push_back({ "m_LoopCount", "int32", std::to_string(v) });
            break;
        }
        case 0x07: {
            float v = *reinterpret_cast<const float*>(child->data.data());
            fields.push_back({ "m_DropoffRadius", "float", std::to_string(v) });
            break;
        }
        case 0x08: {
            float v = *reinterpret_cast<const float*>(child->data.data());
            fields.push_back({ "m_MaxVolRadius", "float", std::to_string(v) });
            break;
        }
        case 0x09: {
            uint32_t v = *reinterpret_cast<const uint32_t*>(child->data.data());
            fields.push_back({ "m_Type", "int32", std::to_string(v) });
            break;
        }
        case 0x0A: {
            uint8_t v = *reinterpret_cast<const uint8_t*>(child->data.data());
            fields.push_back({ "m_is3DSound", "int8", std::to_string(v) });
            break;
        }
        case 0x0B: {
            std::string s(reinterpret_cast<const char*>(child->data.data()), child->data.size());
            if (!s.empty() && s.back() == '\0') s.pop_back();
            fields.push_back({ "m_Filename", "string", s });
            break;
        }
        case 0x0C: {
            std::string s(reinterpret_cast<const char*>(child->data.data()), child->data.size());
            if (!s.empty() && s.back() == '\0') s.pop_back();
            fields.push_back({ "m_DisplayText", "string", s });
            break;
        }
        case 0x12: {
            float v = *reinterpret_cast<const float*>(child->data.data());
            fields.push_back({ "m_StartOffset", "float", std::to_string(v) });
            break;
        }
        case 0x13: {
            float v = *reinterpret_cast<const float*>(child->data.data());
            fields.push_back({ "m_PitchFactor", "float", std::to_string(v) });
            break;
        }
        case 0x14: {
            float v = *reinterpret_cast<const float*>(child->data.data());
            fields.push_back({ "m_PitchFactorRandomizer", "float", std::to_string(v) });
            break;
        }
        case 0x15: {
            float v = *reinterpret_cast<const float*>(child->data.data());
            fields.push_back({ "m_VolumeRandomizer", "float", std::to_string(v) });
            break;
        }
        case 0x16: {
            float v = *reinterpret_cast<const float*>(child->data.data());
            fields.push_back({ "m_VirtualChannel", "float", std::to_string(v) });
            break;
        }
        case 0x0D: {
            uint32_t v = *reinterpret_cast<const uint32_t*>(child->data.data());
            fields.push_back({ "m_LogicalType", "int32", std::to_string(v) });
            break;
        }
        case 0x0E: {
            float v = *reinterpret_cast<const float*>(child->data.data());
            fields.push_back({ "m_LogicalNotifDelay", "float", std::to_string(v) });
            break;
        }
        case 0x0F: {
            uint8_t v = *reinterpret_cast<const uint8_t*>(child->data.data());
            fields.push_back({ "m_CreateLogicalSound", "int8", std::to_string(v) });
            break;
        }
        case 0x10: {
            float v = *reinterpret_cast<const float*>(child->data.data());
            fields.push_back({ "m_LogicalDropoffRadius", "float", std::to_string(v) });
            break;
        }
        case 0x11: {
            auto ptr = reinterpret_cast<const float*>(child->data.data());
            std::string val = "("
                + std::to_string(ptr[0]) + " "
                + std::to_string(ptr[1]) + " "
                + std::to_string(ptr[2]) + ")";
            fields.push_back({ "m_SphereColor", "Vector", val });
            break;
        }

        default:
            // unknown or not-yet-supported
            fields.push_back({
              "UnknownVar(0x" + ToHex(id) + ")",
              "bytes[" + std::to_string(child->data.size()) + "]",
              std::to_string(child->data.size()) + " bytes"
                });
            break;
        }
    }

    return fields;
}