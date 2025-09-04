#include "ChunkJson.h"

#include <nlohmann/json.hpp>

#include "ChunkItem.h"
#include "ChunkNames.h"
#include "ChunkSerializers.h"
#include "ChunkSerializer.h"




ordered_json ChunkJson::toJson(const ChunkItem& item) {
    ordered_json obj;
    obj["CHUNK_NAME"] = LabelForChunk(item.id, const_cast<ChunkItem*>(&item));
    obj["SUBCHUNKS"] = item.hasSubChunks;
    obj["CHUNK_ID"] = item.id;
    obj["LENGTH"] = item.length;

    if (!item.children.empty()) {
        ordered_json arr = ordered_json::array();
        for (const auto& c : item.children) {
            arr.push_back(ChunkJson::toJson(*c));
        }
        obj["CHILDREN"] = arr;

    }
    else if (!item.data.empty()) {
        const auto& registry = chunkSerializerRegistry();
        auto it = registry.find(item.id);
        if (it != registry.end()) {
            obj["DATA"] = it->second->toJson(item);
        }
        
    }
    return obj;
}

std::shared_ptr<ChunkItem> ChunkJson::fromJson(const ordered_json& obj, ChunkItem* parent) {
    auto item = std::make_shared<ChunkItem>();
    item->id = obj.at("CHUNK_ID").get<uint32_t>();
    item->length = obj.at("LENGTH").get<uint32_t>();
    item->hasSubChunks = obj.at("SUBCHUNKS").get<bool>();
    if (obj.contains("CHUNK_NAME"))
        item->typeName = obj.at("CHUNK_NAME").get<std::string>();
    item->parent = parent;

    if (obj.contains("CHILDREN")) {
        for (const auto& v : obj.at("CHILDREN")) {
            if (v.is_object()) {
                auto child = ChunkJson::fromJson(v, item.get());
                item->children.push_back(child);
            }
        }
    }
    else if (obj.contains("DATA")) {
        const auto& registry = chunkSerializerRegistry();
        auto it = registry.find(item->id);
        if (it != registry.end()) {
            it->second->fromJson(obj.at("DATA"), *item);
        }
    }

    return item;
}