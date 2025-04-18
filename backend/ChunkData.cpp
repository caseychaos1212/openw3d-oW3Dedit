#include <sstream>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include "ChunkData.h"

static uint32_t readUint32(std::istream& stream) {
    uint32_t value = 0;
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}

bool ChunkData::loadFromFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return false;
    }


    

    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    while (file.tellg() < fileSize) {
        std::shared_ptr<ChunkItem> chunk = std::make_shared<ChunkItem>();

        std::streampos startPos = file.tellg();
        chunk->id = readUint32(file);
        chunk->length = readUint32(file) & 0x7FFFFFFF;
        std::streampos endPos = file.tellg() + static_cast<std::streamoff>(chunk->length);

        // Sanity check
        if (chunk->length > fileSize || endPos > fileSize || chunk->length > 100000000) {
            std::cerr << "Suspicious chunk size at " << startPos << " ID: " << chunk->id << ", size: " << chunk->length << "\n";
            break;
        }

        std::streampos dataStart = file.tellg();
        file.seekg(chunk->length, std::ios::cur);
        std::streampos dataEnd = file.tellg();

        // Save raw data
        file.seekg(dataStart);
        chunk->data.resize(chunk->length);
        file.read(reinterpret_cast<char*>(chunk->data.data()), chunk->length);

        // Only parse child chunks if this is a known container
        switch (chunk->id) {
        case 0x0100: // HIERARCHY
        case 0x0102: // PIVOTS
        //case 0x0103: // PIVOT_FIXUPS
        case 0x0410: // GEOMETRY
        case 0x0810: // MESH
        {
            std::string subData(reinterpret_cast<char*>(chunk->data.data()), chunk->length);
            std::istringstream subStream(subData);
            parseChunk(subStream, chunk);
        }
        break;
        default:
            // treat as leaf chunk — don't parse further
            break;
        }

        file.seekg(dataEnd);  // Move back to outer stream position
        chunks.push_back(chunk);
    }

    return true;
}

bool ChunkData::parseChunk(std::istream& stream, std::shared_ptr<ChunkItem>& parent) {
    while (stream && stream.peek() != EOF) {
        std::streampos pos = stream.tellg();
        if (stream.rdbuf()->in_avail() < 8) break;

        auto child = std::make_shared<ChunkItem>();
        child->id = readUint32(stream);
        child->length = readUint32(stream) & 0x7FFFFFFF;

        if (stream.rdbuf()->in_avail() < static_cast<std::streamsize>(child->length)) {
            std::cerr << "Incomplete child chunk at " << pos << ", ID: " << child->id << ", expected size: " << child->length << "\n";
            break;
        }

        child->data.resize(child->length);
        stream.read(reinterpret_cast<char*>(child->data.data()), child->length);

        parent->children.push_back(child);

        switch (child->id) {
        case 0x0100: // HIERARCHY
        case 0x0102: // PIVOTS
        case 0x0410: // GEOMETRY
        case 0x0810: // MESH
        {
            std::string subData(reinterpret_cast<char*>(child->data.data()), child->length);
            std::istringstream subStream(subData);
            parseChunk(subStream, child);
        }
        break;
        default:
            break;  // don't recurse into flat chunks like PIVOT_FIXUPS
        }
    }
    return true;
}
void ChunkData::clear() {
    chunks.clear();
}