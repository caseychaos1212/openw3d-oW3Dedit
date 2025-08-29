#pragma once
#include "W3DStructs.h"
#include <vector>


inline std::vector<ChunkField> InterpretAggregateHeader(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dAggregateHeaderStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed AGGREGATE_HEADER: " + *err);
        return fields;
    }
    const auto& H = std::get<W3dAggregateHeaderStruct>(v);

    ChunkFieldBuilder B(fields);
    B.Version("Version", H.Version);
    B.Name("Name", H.Name);
    return fields;
}


inline std::vector<ChunkField> InterpretAggregateInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    if (buf.size() < sizeof(W3dAggregateInfoStruct)) {
        fields.emplace_back("error", "string", "AGGREGATE_INFO too small");
        return fields;
    }

    const auto* hdr = reinterpret_cast<const W3dAggregateInfoStruct*>(buf.data());

    ChunkFieldBuilder B(fields);
    // Base model name (spec allows up to 2*W3D_NAME_LEN here)
    {
        const size_t maxLen = W3D_NAME_LEN * 2;
        B.Push("BaseModelName", "string",
            FormatUtils::FormatString(hdr->BaseModelName, maxLen));
    }
    B.UInt32("SubobjectCount", hdr->SubobjectCount);

    // Subobjects follow immediately
    const size_t entrySize = sizeof(W3dAggregateSubobjectStruct);
    const size_t availableBytes = buf.size() - sizeof(W3dAggregateInfoStruct);
    const size_t maxEntries = availableBytes / entrySize;
    const size_t n = std::min<size_t>(hdr->SubobjectCount, maxEntries);
    const auto* subs = reinterpret_cast<const W3dAggregateSubobjectStruct*>(
        buf.data() + sizeof(W3dAggregateInfoStruct)
        );

    for (size_t i = 0; i < n; ++i) {
        const std::string pfx = "SubObject[" + std::to_string(i) + "]";
        B.Push(pfx + ".SubobjectName", "string",
            FormatUtils::FormatString(subs[i].SubobjectName, W3D_NAME_LEN * 2));
        B.Push(pfx + ".BoneName", "string",
            FormatUtils::FormatString(subs[i].BoneName, W3D_NAME_LEN * 2));
    }

    return fields;
}


inline std::vector<ChunkField> InterpretTextureReplacerInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    const auto& buf = chunk->data;
    if (buf.size() < sizeof(W3dTextureReplacerHeaderStruct)) {
        fields.emplace_back("error", "string", "TEXTURE_REPLACER_INFO header too small");
        return fields;
    }

    const auto* hdr = reinterpret_cast<const W3dTextureReplacerHeaderStruct*>(buf.data());
    ChunkFieldBuilder B(fields);
    B.UInt32("ReplacedTexturesCount", hdr->ReplacedTexturesCount);

    size_t offset = sizeof(W3dTextureReplacerHeaderStruct);
    const size_t ENTRY_SIZE = sizeof(W3dTextureReplacerStruct);

    for (uint32_t i = 0; i < hdr->ReplacedTexturesCount; ++i) {
        if (offset + ENTRY_SIZE > buf.size()) {
            B.Push("error", "string", "Out of bounds reading replacer[" + std::to_string(i) + "]");
            break;
        }
        const auto* rep = reinterpret_cast<const W3dTextureReplacerStruct*>(buf.data() + offset);
        const std::string pfx = "Replacer[" + std::to_string(i) + "]";

        // MeshPath/BonePath arrays (15 entries each in this format)
        for (int j = 0; j < 15; ++j) {
            B.Push(pfx + ".MeshPath[" + std::to_string(j) + "]", "string",
                FormatUtils::FormatString(rep->MeshPath[j], sizeof rep->MeshPath[j]));
        }
        for (int j = 0; j < 15; ++j) {
            B.Push(pfx + ".BonePath[" + std::to_string(j) + "]", "string",
                FormatUtils::FormatString(rep->BonePath[j], sizeof rep->BonePath[j]));
        }

        B.Push(pfx + ".OldTextureName", "string",
            FormatUtils::FormatString(rep->OldTextureName, sizeof rep->OldTextureName));
        B.Push(pfx + ".NewTextureName", "string",
            FormatUtils::FormatString(rep->NewTextureName, sizeof rep->NewTextureName));

        // Inline the TextureParams (same format as TEXTURE_INFO). Reuse interpreter.
        {
            auto fake = std::make_shared<ChunkItem>();
            fake->data.resize(sizeof rep->TextureParams);
            std::memcpy(fake->data.data(), &rep->TextureParams, sizeof rep->TextureParams);

            auto sub = InterpretTextureInfo(fake);
            // Re-prefix fields to live under TextureParams
            for (auto& f : sub) {
                f.field = pfx + ".TextureParams." + f.field;
                fields.push_back(std::move(f));
            }
        }

        offset += ENTRY_SIZE;
    }

    return fields;
}


inline std::vector<ChunkField> InterpretAggregateClassInfo(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    auto v = ParseChunkStruct<W3dAggregateMiscInfo>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed AGGREGATE_CLASS_INFO: " + *err);
        return fields;
    }
    const auto& A = std::get<W3dAggregateMiscInfo>(v);

    ChunkFieldBuilder B(fields);
    B.UInt32("OriginalClassID", A.OriginalClassID);
    B.UInt32("Flags", A.Flags);

    // Known flags
    B.Flag(A.Flags, uint32_t(W3D_AGGREGATE_FORCE_SUB_OBJ_LOD), "W3D_AGGREGATE_FORCE_SUB_OBJ_LOD");

    return fields;
}

