// Helper functions for converting ChunkItem structures to and from JSON
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using ordered_json = nlohmann::ordered_json;
class ChunkItem;

enum class JsonSerializationMode {
    StructuredPreferred,
    HexOnly
};

class ChunkJson {
public:
    static ordered_json toJson(const ChunkItem& item, JsonSerializationMode mode);
    static std::shared_ptr<ChunkItem> fromJson(
        const ordered_json& obj,
        ChunkItem* parent = nullptr,
        JsonSerializationMode declaredMode = JsonSerializationMode::StructuredPreferred,
        std::vector<std::string>* warnings = nullptr,
        const std::string& jsonPath = {});
};
