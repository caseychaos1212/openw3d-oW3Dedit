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
static bool isWrapperChunk(uint32_t id) {
    switch (id) {
    case 0x0000: // Root wrapper
    case 0x0029: // W3D_CHUNK_SHADERS
    case 0x002A: // W3D_CHUNK_VERTEX_MATERIALS
    case 0x002B: // W3D_CHUNK_VERTEX_MATERIAL
    case 0x0030: // W3D_CHUNK_TEXTURES
    case 0x0031: // W3D_CHUNK_TEXTURE
    case 0x0048: // W3D_CHUNK_TEXTURE_STAGE
    case 0x0038: // W3D_CHUNK_MATERIAL_PASS
    case 0x0090: // W3D_CHUNK_AABTREE
    case 0x0100: // W3D_CHUNK_HIERARCHY
    case 0x0102: // W3D_CHUNK_PIVOTS
    case 0x0300: // W3D_CHUNK_SHADER
    case 0x0410: // W3D_CHUNK_GEOMETRY
    case 0x0810: // W3D_CHUNK_MESH
    case 0x0A10: // W3D_CHUNK_TEXTURES (legacy style)
    case 0x0B10: // W3D_CHUNK_MATERIAL_PASS (legacy)
    case 0x0C10: // W3D_CHUNK_AABTREE
    case 0x0700: // W3D_CHUNK_HLOD
    case 0x0702: // W3D_CHUNK_HLOD_LOD_ARRAY
    case 0x0705: // W3D_CHUNK_HLOD_AGGREGATE_ARRAY
        return true;
    default:
        return false;
    }
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

        if (isWrapperChunk(chunk->id)) {
            std::string subData(reinterpret_cast<char*>(chunk->data.data()), chunk->length);
            std::istringstream subStream(subData);
            parseChunk(subStream, chunk);
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

        if (isWrapperChunk(child->id)) {
            std::string subData(reinterpret_cast<char*>(child->data.data()), child->length);
            std::istringstream subStream(subData);
            parseChunk(subStream, child);
        }

    }
    return true;
}
void ChunkData::clear() {
    chunks.clear();
}