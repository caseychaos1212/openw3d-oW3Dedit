#pragma once
#include "W3DStructs.h"
#include <vector>

// SOUND RENDER OBJECTS
//TODO: Broken
inline std::vector<ChunkField> InterpretSoundRObjHeader(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto parsed = ParseChunkStruct<W3dSoundRObjHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        fields.push_back({ "Error", "string", "Invalid SOUNDROBJ_HEADER: " + *err });
        return fields;
    }
    const auto& hdr = std::get<W3dSoundRObjHeaderStruct>(parsed);

    ChunkFieldBuilder B(fields);
    B.Version("Version", hdr.Version);
    B.Name("Name", hdr.Name);                 // 16-byte fixed buffer; B.Name trims NUL
    B.UInt32("Flags", hdr.Flags);

    return fields;
}


// Helpers for the definition/microchunks
inline bool ReadFloat(const std::shared_ptr<ChunkItem>& c, float& out) {
    if (!c || c->data.size() < sizeof(float)) return false;
    std::memcpy(&out, c->data.data(), sizeof(float));
    return true;
}
inline bool ReadU32(const std::shared_ptr<ChunkItem>& c, uint32_t& out) {
    if (!c || c->data.size() < sizeof(uint32_t)) return false;
    std::memcpy(&out, c->data.data(), sizeof(uint32_t));
    return true;
}
inline bool ReadU8(const std::shared_ptr<ChunkItem>& c, uint8_t& out) {
    if (!c || c->data.size() < sizeof(uint8_t)) return false;
    out = *reinterpret_cast<const uint8_t*>(c->data.data());
    return true;
}
inline void PushNullTerm(ChunkFieldBuilder& B, const std::string& field, const std::shared_ptr<ChunkItem>& c) {
    B.NullTerm(field, reinterpret_cast<const char*>(c->data.data()), c->data.size());
}
//TODO: Broken
inline std::vector<ChunkField> InterpretSoundRObjDefinition(
    const std::shared_ptr<ChunkItem>& chunk
) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;
    ChunkFieldBuilder B(fields);

    // CHUNKID_BASE_CLASS wrapper path (0x0200) -> inner CHUNKID_VARIABLES (contains m_ID, m_Name)
    constexpr uint32_t CHUNKID_BASE_CLASS = 0x0200;
    if (chunk->parent && chunk->parent->id == CHUNKID_BASE_CLASS) {
        for (const auto& child : chunk->children) {
            switch (child->id) {
            case 0x01: { // m_ID (uint32)
                uint32_t v{};
                if (ReadU32(child, v)) B.UInt32("m_ID", v);
                break;
            }
            case 0x03: { // m_Name (null-terminated)
                PushNullTerm(B, "m_Name", child);
                break;
            }
            default: break; // ignore other base-class fields
            }
        }
        return fields;
    }

    // Main CHUNKID_VARIABLES block (0x0100) — individual micro-IDs
    for (const auto& child : chunk->children) {
        switch (child->id) {
        case 0x03: { float v{}; if (ReadFloat(child, v)) B.Float("m_Priority", v); break; }
        case 0x04: { float v{}; if (ReadFloat(child, v)) B.Float("m_Volume", v); break; }
        case 0x05: { float v{}; if (ReadFloat(child, v)) B.Float("m_Pan", v); break; }
        case 0x06: { uint32_t v{}; if (ReadU32(child, v)) B.UInt32("m_LoopCount", v); break; }
        case 0x07: { float v{}; if (ReadFloat(child, v)) B.Float("m_DropoffRadius", v); break; }
        case 0x08: { float v{}; if (ReadFloat(child, v)) B.Float("m_MaxVolRadius", v); break; }
        case 0x09: { uint32_t v{}; if (ReadU32(child, v)) B.UInt32("m_Type", v); break; }
        case 0x0A: { uint8_t v{}; if (ReadU8(child, v))  B.UInt8("m_is3DSound", v); break; }
        case 0x0B: { PushNullTerm(B, "m_Filename", child); break; }
        case 0x0C: { PushNullTerm(B, "m_DisplayText", child); break; }
        case 0x12: { float v{}; if (ReadFloat(child, v)) B.Float("m_StartOffset", v); break; }
        case 0x13: { float v{}; if (ReadFloat(child, v)) B.Float("m_PitchFactor", v); break; }
        case 0x14: { float v{}; if (ReadFloat(child, v)) B.Float("m_PitchFactorRandomizer", v); break; }
        case 0x15: { float v{}; if (ReadFloat(child, v)) B.Float("m_VolumeRandomizer", v); break; }
        case 0x16: { uint32_t v{}; if (ReadU32(child, v)) B.UInt32("m_VirtualChannel", v); break; }
        case 0x0D: { uint32_t v{}; if (ReadU32(child, v)) B.UInt32("m_LogicalType", v); break; }
        case 0x0E: { float v{}; if (ReadFloat(child, v)) B.Float("m_LogicalNotifDelay", v); break; }
        case 0x0F: { uint8_t v{}; if (ReadU8(child, v))  B.UInt8("m_CreateLogicalSound", v); break; }
        case 0x10: { float v{}; if (ReadFloat(child, v)) B.Float("m_LogicalDropoffRadius", v); break; }
        case 0x11: { // m_SphereColor as 3 floats
            if (child->data.size() >= sizeof(float) * 3) {
                const float* f = reinterpret_cast<const float*>(child->data.data());
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(6) << f[0] << ' ' << f[1] << ' ' << f[2];
                B.Push("m_SphereColor", "vector3", oss.str());
            }
            break;
        }

        default: {
            // Unknown/unsupported var: record ID and byte count for debugging
            std::ostringstream tag; tag << "UnknownVar(0x" << std::hex << std::uppercase << child->id << ")";
            std::ostringstream ty;  ty << "bytes[" << std::dec << child->data.size() << "]";
            B.Push(tag.str(), ty.str(), std::to_string(child->data.size()) + " bytes");
            break;
        }
        }
    }

    return fields;
}