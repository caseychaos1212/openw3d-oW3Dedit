#include "ChunkJson.h"

#include <QJsonArray>

#include "ChunkItem.h"
#include "ChunkNames.h"
#include "ChunkSerializers.h"
#include "ChunkSerializer.h"




QJsonObject ChunkJson::toJson(const ChunkItem& item) {
    QJsonObject obj;
    obj["CHUNK_NAME"] = QString::fromStdString(LabelForChunk(item.id, const_cast<ChunkItem*>(&item)));
    obj["SUBCHUNKS"] = item.hasSubChunks;
    obj["CHUNK_ID"] = static_cast<int>(item.id);
    obj["LENGTH"] = static_cast<int>(item.length);

    if (!item.children.empty()) {
        QJsonArray arr;
        for (const auto& c : item.children) {
            arr.append(ChunkJson::toJson(*c));
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

std::shared_ptr<ChunkItem> ChunkJson::fromJson(const QJsonObject& obj, ChunkItem* parent) {
    auto item = std::make_shared<ChunkItem>();
    item->id = obj["CHUNK_ID"].toInt();
    item->length = obj["LENGTH"].toInt();
    item->hasSubChunks = obj["SUBCHUNKS"].toBool();
    item->typeName = obj.value("CHUNK_NAME").toString().toStdString();
    item->parent = parent;

    if (obj.contains("CHILDREN")) {
        QJsonArray arr = obj.value("CHILDREN").toArray();
        for (const auto& v : arr) {
            if (v.isObject()) {
                auto child = ChunkJson::fromJson(v.toObject(), item.get());
                item->children.push_back(child);
            }
        }
    }
    else if (obj.contains("DATA")) {
        QJsonObject dataObj = obj.value("DATA").toObject();
        const auto& registry = chunkSerializerRegistry();
        auto it = registry.find(item->id);
        if (it != registry.end()) {
            it->second->fromJson(dataObj, *item);
        }
    }

    return item;
}