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


// ---- Sound RObj: CHUNKID_VARIABLES micro stream parser -----------------
static inline void ParseSROVariablesMicro(
    const std::shared_ptr<ChunkItem>& chunk,
    ChunkFieldBuilder& B,
    bool isBaseClassBlock /*true when parent->id == 0x0200*/
) {
    if (!chunk) return;
    const uint8_t* cur = chunk->data.data();
    const uint8_t* end = cur + chunk->data.size();

    auto need = [&](size_t n) { return static_cast<size_t>(end - cur) >= n; };

    while (need(2)) {               // micro header: [id:u8][size:u8]
        uint8_t id = cur[0];
        uint8_t size = cur[1];
        cur += 2;
        if (!need(size)) break;     // truncated; bail quietly

        const uint8_t* payload = cur;

        // BASE_CLASS block holds only m_ID(0x01) and m_Name(0x03)
        if (isBaseClassBlock) {
            switch (id) {
            case 0x01:                         // m_ID : uint32
                if (size == 4) {
                    uint32_t v;
                    std::memcpy(&v, payload, 4);
                    B.UInt32("m_ID", v);
                }
                break;
            case 0x03: {                        // m_Name : null-terminated bytes
                const char* s = reinterpret_cast<const char*>(payload);
                const size_t n = std::min<size_t>(size, strnlen(s, size));
                B.Push("m_Name", "string", std::string(s, n));
                break;
            }
            default: break;
            }
            cur += size;
            continue;
        }

        // MAIN variables block
        switch (id) {
        case 0x03: if (size == 4) { float f; std::memcpy(&f, payload, 4); B.Float("m_Priority", f); } break;
        case 0x04: if (size == 4) { float f; std::memcpy(&f, payload, 4); B.Float("m_Volume", f); } break;
        case 0x05: if (size == 4) { float f; std::memcpy(&f, payload, 4); B.Float("m_Pan", f); } break;
        case 0x06: if (size == 4) { uint32_t v; std::memcpy(&v, payload, 4); B.UInt32("m_LoopCount", v); } break;
        case 0x07: if (size == 4) { float f; std::memcpy(&f, payload, 4); B.Float("m_DropoffRadius", f); } break;
        case 0x08: if (size == 4) { float f; std::memcpy(&f, payload, 4); B.Float("m_MaxVolRadius", f); } break;
        case 0x09: if (size == 4) { uint32_t v; std::memcpy(&v, payload, 4); B.UInt32("m_Type", v); } break;
        case 0x0A: if (size >= 1) { B.UInt8("m_is3DSound", payload[0]); } break;

        case 0x0B: { // m_Filename (bytes include NUL)
            const char* s = reinterpret_cast<const char*>(payload);
            const size_t n = std::min<size_t>(size, strnlen(s, size));
            B.Push("m_Filename", "string", std::string(s, n));
            break;
        }
        case 0x0C: { // m_DisplayText
            const char* s = reinterpret_cast<const char*>(payload);
            const size_t n = std::min<size_t>(size, strnlen(s, size));
            B.Push("m_DisplayText", "string", std::string(s, n));
            break;
        }

        case 0x12: if (size == 4) { float f; std::memcpy(&f, payload, 4); B.Float("m_StartOffset", f); } break;
        case 0x13: if (size == 4) { float f; std::memcpy(&f, payload, 4); B.Float("m_PitchFactor", f); } break;
        case 0x14: if (size == 4) { float f; std::memcpy(&f, payload, 4); B.Float("m_PitchFactorRandomizer", f); } break;
        case 0x15: if (size == 4) { float f; std::memcpy(&f, payload, 4); B.Float("m_VolumeRandomizer", f); } break;
        case 0x16: if (size == 4) { uint32_t v; std::memcpy(&v, payload, 4); B.UInt32("m_VirtualChannel", v); } break;

        case 0x0D: if (size == 4) { uint32_t v; std::memcpy(&v, payload, 4); B.UInt32("m_LogicalType", v); } break;
        case 0x0E: if (size == 4) { float f; std::memcpy(&f, payload, 4); B.Float("m_LogicalNotifDelay", f); } break;
        case 0x0F: if (size >= 1) { B.UInt8("m_CreateLogicalSound", payload[0]); } break;
        case 0x10: if (size == 4) { float f; std::memcpy(&f, payload, 4); B.Float("m_LogicalDropoffRadius", f); } break;

        case 0x11: // m_SphereColor (3 floats)
            if (size == 12) {
                const float* f = reinterpret_cast<const float*>(payload);
                B.Push("m_SphereColor", "vector3",
                    FormatUtils::FormatVec3(f[0], f[1], f[2]));
            }
            break;

        default: {
            std::ostringstream tag; tag << "UnknownVar(0x" << std::hex << std::uppercase << int(id) << ")";
            std::ostringstream ty;  ty << "bytes[" << std::dec << int(size) << "]";
            B.Push(tag.str(), ty.str(), std::to_string(int(size)) + " bytes");
            break;
        }
        }

        cur += size;
    }
}

inline std::vector<ChunkField> InterpretSoundRObjDefinition(const std::shared_ptr<ChunkItem>& chunk)
{
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    ChunkFieldBuilder B(fields);
    const bool isBaseClassBlock =
        (chunk->parent && chunk->parent->id == 0x0200); // CHUNKID_BASE_CLASS

    ParseSROVariablesMicro(chunk, B, isBaseClassBlock);
    return fields;
}
