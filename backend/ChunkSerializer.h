#pragma once

#include <nlohmann/json.hpp>

using ordered_json = nlohmann::ordered_json;

class ChunkItem;

struct ChunkSerializer {
    virtual ~ChunkSerializer() = default;
    virtual ordered_json toJson(const ChunkItem& item) const = 0;
    virtual void fromJson(const ordered_json& obj, ChunkItem& item) const = 0;
};