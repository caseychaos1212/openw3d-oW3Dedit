#pragma once

#include "JsonCompat.h"

class ChunkItem;

struct ChunkSerializer {
    virtual ~ChunkSerializer() = default;
    virtual QJsonObject toJson(const ChunkItem& item) const = 0;
    virtual void fromJson(const QJsonObject& obj, ChunkItem& item) const = 0;
};

