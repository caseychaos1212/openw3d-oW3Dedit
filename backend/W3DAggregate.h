#pragma once
#include "W3DStructs.h"
#include <vector>

inline std::vector<ChunkField> InterpretAggregateHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    
    if (!chunk || chunk->data.size() < sizeof(W3dAggregateHeaderStruct)) {
        fields.emplace_back("Error", "string", "AggregateHeader chunk too small");
        return fields;
    }

   
    auto const* hdr = reinterpret_cast<const W3dAggregateHeaderStruct*>(chunk->data.data());

    
    uint16_t major = static_cast<uint16_t>((hdr->Version >> 16) & 0xFFFF);
    uint16_t minor = static_cast<uint16_t>(hdr->Version & 0xFFFF);
    std::ostringstream ver;
    ver << major << "." << minor;

    
    std::string name(hdr->Name, strnlen(hdr->Name, W3D_NAME_LEN));

    fields.emplace_back("Version", "string", ver.str());
    fields.emplace_back("Name", "string", name);
    return fields;
}

inline std::vector<ChunkField> InterpretAggregateInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("Error", "string", "Empty AGGREGATE INFO chunk");
        return fields;
    }

    const auto& buf = chunk->data;
   
    if (buf.size() < sizeof(W3dAggregateInfoStruct)) {
        fields.emplace_back("Error", "string", "AGGREGATE INFO too small");
        return fields;
    }

    
    auto hdr = reinterpret_cast<const W3dAggregateInfoStruct*>(buf.data());
    size_t baseLen = strnlen(hdr->BaseModelName, W3D_NAME_LEN * 2);
    fields.emplace_back(
        "BaseModelName", "string",
        std::string(hdr->BaseModelName, baseLen)
    );
    uint32_t subCount = hdr->SubobjectCount;
    fields.emplace_back(
        "SubobjectCount", "uint32",
        std::to_string(subCount)
    );

    
    const size_t entrySize = sizeof(W3dAggregateSubobjectStruct);
    size_t maxEntries = (buf.size() - sizeof(*hdr)) / entrySize;
    size_t n = std::min<uint32_t>(subCount, maxEntries);
    auto subs = reinterpret_cast<const W3dAggregateSubobjectStruct*>(
        buf.data() + sizeof(*hdr)
        );

    for (size_t i = 0; i < n; ++i) {
       
        const char* s0 = subs[i].SubobjectName;
        const char* s1 = subs[i].BoneName;
        size_t l0 = strnlen(s0, W3D_NAME_LEN * 2);
        size_t l1 = strnlen(s1, W3D_NAME_LEN * 2);

        fields.emplace_back(
            "SubObject[" + std::to_string(i) + "].SubobjectName",
            "string",
            std::string(s0, l0)
        );
        fields.emplace_back(
            "SubObject[" + std::to_string(i) + "].BoneName",
            "string",
            std::string(s1, l1)
        );
    }

    return fields;
}

inline std::vector<ChunkField> InterpretTextureReplacerInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) {
        fields.emplace_back("Error", "string", "Empty TEXTURE REPLACER INFO chunk");
        return fields;
    }

    const auto& buf = chunk->data;
    
    if (buf.size() < sizeof(W3dTextureReplacerHeaderStruct)) {
        fields.emplace_back("Error", "string", "TEXTURE REPLACER INFO too small for header");
        return fields;
    }
    auto hdr = reinterpret_cast<const W3dTextureReplacerHeaderStruct*>(buf.data());
    uint32_t count = hdr->ReplacedTexturesCount;
    fields.emplace_back("ReplacedTexturesCount", "uint32", std::to_string(count));

    
    size_t offset = sizeof(*hdr);
    constexpr size_t ENTRY_SIZE = sizeof(W3dTextureReplacerStruct);
    for (uint32_t i = 0; i < count; ++i) {
        if (offset + ENTRY_SIZE > buf.size()) {
            fields.emplace_back("Error", "string", "Out of bounds reading replacer " + std::to_string(i));
            break;
        }
        auto rep = reinterpret_cast<const W3dTextureReplacerStruct*>(buf.data() + offset);

        
        for (int j = 0; j < 15; ++j) {
            const char* p = rep->MeshPath[j];
            size_t len = strnlen(p, sizeof rep->MeshPath[j]);
            fields.emplace_back(
                "Replacer[" + std::to_string(i) + "].MeshPath[" + std::to_string(j) + "]",
                "string",
                std::string(p, len)
            );
        }
       
        for (int j = 0; j < 15; ++j) {
            const char* p = rep->BonePath[j];
            size_t len = strnlen(p, sizeof rep->BonePath[j]);
            fields.emplace_back(
                "Replacer[" + std::to_string(i) + "].BonePath[" + std::to_string(j) + "]",
                "string",
                std::string(p, len)
            );
        }
        
        {
            size_t len0 = strnlen(rep->OldTextureName, sizeof rep->OldTextureName);
            fields.emplace_back(
                "Replacer[" + std::to_string(i) + "].OldTextureName",
                "string",
                std::string(rep->OldTextureName, len0)
            );
            size_t len1 = strnlen(rep->NewTextureName, sizeof rep->NewTextureName);
            fields.emplace_back(
                "Replacer[" + std::to_string(i) + "].NewTextureName",
                "string",
                std::string(rep->NewTextureName, len1)
            );
        }
        
        {
            auto fakeChunk = std::make_shared<ChunkItem>();
            fakeChunk->data.resize(sizeof rep->TextureParams);
            std::memcpy(fakeChunk->data.data(), &rep->TextureParams, sizeof rep->TextureParams);
            auto params = InterpretTextureInfo(fakeChunk);
            for (auto& f : params) {
                
                f.field = "Replacer[" + std::to_string(i) + "].TextureParams." + f.field;
                fields.push_back(f);
            }
        }

        offset += ENTRY_SIZE;
    }

    return fields;
}

inline std::vector<ChunkField> InterpretAggregateClassInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    
    if (!chunk || chunk->data.size() < sizeof(uint32_t) * 2) {
        fields.emplace_back("Error", "string",
            "AggregateClassInfo chunk too small (expected 8 bytes)");
        return fields;
    }

    
    auto info = reinterpret_cast<const W3dAggregateMiscInfo*>(chunk->data.data());

    
    fields.emplace_back(
        "OriginalClassID", "uint32",
        std::to_string(info->OriginalClassID)
    );
    fields.emplace_back(
        "Flags (raw)", "uint32",
        std::to_string(info->Flags)
    );

   
    if (info->Flags & W3D_AGGREGATE_FORCE_SUB_OBJ_LOD) {
        fields.emplace_back(
            "Flags", "flag",
            "W3D_AGGREGATE_FORCE_SUB_OBJ_LOD"
        );
    }

    return fields;
}
