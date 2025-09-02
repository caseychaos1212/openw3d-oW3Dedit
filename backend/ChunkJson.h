// Helper functions for converting ChunkItem structures to and from JSON
#pragma once

#include <memory>
#include <QJsonObject>

class ChunkItem;

class ChunkJson {
public:
    static QJsonObject toJson(const ChunkItem& item);
    static std::shared_ptr<ChunkItem> fromJson(const QJsonObject& obj, ChunkItem* parent = nullptr);
};