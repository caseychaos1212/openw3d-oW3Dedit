#include <sstream>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include "ChunkData.h"
#include "ChunkNames.h"


static uint32_t readUint32(std::istream& stream) {
    uint32_t value = 0;
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}
static bool isWrapperChunk(uint32_t id) {
    switch (id) {
    case 0x0000: // W3D_CHUNK_MESH
	case 0x0024: // W3D_CHUNK_PRELIT_VERTEX
    case 0x0025: // W3D_CHUNK_LIGHTMAP_MULTI_PASS
    case 0x0026: // W3D_CHUNK_LIGHTMAP_MULTI_TEXTURE
    case 0x0029: // W3D_CHUNK_SHADERS
    case 0x002A: // W3D_CHUNK_VERTEX_MATERIALS
    case 0x002B: // W3D_CHUNK_VERTEX_MATERIAL
    case 0x0030: // W3D_CHUNK_TEXTURES
    case 0x0031: // W3D_CHUNK_TEXTURE
    case 0x0038: // W3D_CHUNK_MATERIAL_PASS
    case 0x0048: // W3D_CHUNK_TEXTURE_STAGE
	case 0x0058: // W3D_CHUNK_DEFORM
	case 0x0059: // W3D_CHUNK_DEFORM_SET
	case 0x005A: // W3D_CHUNK_DEFORM_KEYFRAME
    case 0x0090: // W3D_CHUNK_AABTREE
    case 0x0100: // W3D_CHUNK_HIERARCHY
    case 0x0102: // W3D_CHUNK_PIVOTS
    case 0x0200: // W3D_CHUNK_ANIMATION
    case 0x0280: // W3D_CHUNK_COMPRESSED_ANIMATION
	case 0x02C0: // W3D_CHUNK_MORPH_ANIMATION
	case 0x02C2: // W3D_CHUNK_MORPHANIM_CHANNEL
    case 0x0300: // W3D_CHUNK_HMODEL
    case 0x0400: // W3D_CHUNK_LODMODEL
	case 0x0420: // W3D_CHUNK_COLLECTION
    case 0x0460: // W3D_CHUNK_LIGHT
    case 0x0500: // W3D_CHUNK_EMITTER
	case 0x0600: // W3D_CHUNK_AGGREGATE
	case 0x0601: // W3D_CHUNK_AGGREGATE_HEADER
    case 0x0700: // W3D_CHUNK_HLOD
    case 0x0702: // W3D_CHUNK_HLOD_LOD_ARRAY
    case 0x0705: // W3D_CHUNK_HLOD_AGGREGATE_ARRAY
    case 0x0800: // W3D_CHUNK_LIGHTSCAPE
	case 0x0801: // W3D_CHUNK_LIGHTSCAPE_LIGHT
	case 0x0900: // W3D_CHUNK_DAZZLE
	case 0x0A00: // W3D_CHUNK_SOUNDROBJ


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
    
    std::cout << "Opening file: " << filename << "\n";
    std::cout << "File size: " << fileSize << "\n";


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

        std::cout << "Top-level chunk: 0x" << std::hex << chunk->id << " (" << GetChunkName(chunk->id).c_str() << ") size=" << std::dec << chunk->length << "\n";
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
            std::ostringstream oss;
            std::string chunkName = GetChunkName(child->id);
            std::cout << "Parsing subchunks of wrapper: 0x" << std::hex << child->id << " (" << chunkName << ")\n";


            parseChunk(subStream, child);
        }

    }
    return true;
}
void ChunkData::clear() {
    chunks.clear();
}