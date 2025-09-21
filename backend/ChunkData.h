#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
using ordered_json = nlohmann::ordered_json;

#include "ChunkItem.h"

class ChunkData {
public:
    ChunkData() = default;
    ~ChunkData() = default;

    // Load chunks from file (implementation in .cpp)
    bool loadFromFile(const std::string& filename);
    bool saveToFile(const std::string& filename) const;
    ordered_json toJson() const;
    bool fromJson(const ordered_json& doc);

    // Top-level chunks in the file
    const std::vector<std::shared_ptr<ChunkItem>>& getChunks() const {
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
