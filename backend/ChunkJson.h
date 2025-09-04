// Helper functions for converting ChunkItem structures to and from JSON
#pragma once

#include <memory>
#include <nlohmann/json.hpp>

using ordered_json = nlohmann::ordered_json;
class ChunkItem;

class ChunkJson {
public:
    static ordered_json toJson(const ChunkItem& item);
    static std::shared_ptr<ChunkItem> fromJson(const ordered_json& obj, ChunkItem* parent = nullptr);
};