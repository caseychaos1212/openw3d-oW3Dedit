#include "ChunkData.h"

#include <fstream>
#include <iostream>

// Helper to read a 4-byte little-endian integer from file
static uint32_t readUint32(std::ifstream& stream) {
    uint32_t value = 0;
    stream.read(reinterpret_cast<char*>(&value), sizeof(uint32_t));
    return value;
}

bool ChunkData::loadFromFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return false;
    }

    clear();

    while (file.peek() != EOF) {
        auto chunk = std::make_shared<ChunkItem>();

        std::streampos chunkStart = file.tellg();
        chunk->id = readUint32(file);
        chunk->length = readUint32(file);
        std::streampos chunkEnd = file.tellg() + static_cast<std::streamoff>(chunk->length);

        // Read raw data
        chunk->data.resize(chunk->length);
        file.read(reinterpret_cast<char*>(chunk->data.data()), chunk->length);

        // Recursively check for nested chunks (if any)
        parseChunk(file, chunk, chunkEnd);

        chunks.push_back(chunk);

        // Move to next chunk
        file.seekg(chunkEnd);
    }

    return true;
}

bool ChunkData::parseChunk(std::ifstream& file, std::shared_ptr<ChunkItem>& parent, std::streampos endPos) {
    while (file.tellg() < endPos) {
        auto child = std::make_shared<ChunkItem>();
        child->id = readUint32(file);
        child->length = readUint32(file);
        std::streampos childEnd = file.tellg() + static_cast<std::streamoff>(child->length);

        // Read child data
        child->data.resize(child->length);
        file.read(reinterpret_cast<char*>(child->data.data()), child->length);

        // Recurse if possible
        parseChunk(file, child, childEnd);

        parent->children.push_back(child);

        file.seekg(childEnd);
    }
    return true;
}

void ChunkData::clear() {
    chunks.clear();
}
