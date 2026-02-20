#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

#include "ChunkItem.h"
#include "ChunkJson.h"

class ChunkData {
public:
    ChunkData() = default;
    ~ChunkData() = default;

    // Load chunks from file (implementation in .cpp)
    bool loadFromFile(const std::string& filename);
    bool saveToFile(const std::string& filename);
    nlohmann::ordered_json toJson(
        JsonSerializationMode mode = JsonSerializationMode::StructuredPreferred) const;
    bool fromJson(
        const nlohmann::ordered_json& doc,
        std::vector<std::string>* warnings = nullptr);

    // Top-level chunks in the file
    const std::vector<std::shared_ptr<ChunkItem>>& getChunks() const {
        return chunks;
    }

    std::vector<std::shared_ptr<ChunkItem>>& getChunksMutable() {
        return chunks;
    }

    // Clears all loaded chunks
    void clear();

private:
    std::vector<std::shared_ptr<ChunkItem>> chunks;
    std::string sourceFilename;

    // Internal recursive parser used during load
    bool parseChunk(std::istream& stream, std::shared_ptr<ChunkItem>& parent);

};
