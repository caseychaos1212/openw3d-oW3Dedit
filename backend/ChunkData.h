#pragma once

#include <string>
#include <vector>
#include <memory>

#include "ChunkItem.h"

class ChunkData {
public:
    ChunkData() = default;
    ~ChunkData() = default;

    // Load chunks from file (implementation in .cpp)
    bool loadFromFile(const std::string& filename);

    // Top-level chunks in the file
    const std::vector<std::shared_ptr<ChunkItem>>& getChunks() const {
        return chunks;
    }

    // Clears all loaded chunks
    void clear();

private:
    std::vector<std::shared_ptr<ChunkItem>> chunks;

    // Internal recursive parser used during load
    bool parseChunk(std::ifstream& file, std::shared_ptr<ChunkItem>& outItem, std::streampos endPos);
};
