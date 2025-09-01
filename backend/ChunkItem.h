#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <QJsonObject>
#include <QJsonArray>

class ChunkItem {
public:
    uint32_t id = 0;
    uint32_t length = 0;
    std::string typeName; 
    bool hasSubChunks;  // high bit of the raw length
    std::vector<uint8_t> data;
    std::vector<std::shared_ptr<ChunkItem>> children;
    ChunkItem* parent = nullptr;

    ChunkItem() = default;

    QJsonObject toJson() const;
    static std::shared_ptr<ChunkItem> fromJson(const QJsonObject& obj, ChunkItem* parent = nullptr);
};
