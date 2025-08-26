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

inline bool IsForcedWrapper(uint32_t id, uint32_t parent = 0)
{
    switch (id) {
    case 0x002A: // W3D_CHUNK_VERTEX_MATERIALS
        return true;

        // You can add more here if needed in the future:
        // case 0x0030: /* W3D_CHUNK_TEXTURES */ return true;
        // case 0x0029: /* W3D_CHUNK_SHADERS  */ return true;

    default:
        (void)parent; // parent available if you later need parent-sensitive overrides
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

    std::cout << "Opening file: " << filename << "\n"
        << "File size: " << fileSize << "\n";

    while (file.tellg() < fileSize) {
        auto chunk = std::make_shared<ChunkItem>();
        std::streampos startPos = file.tellg();

        // 1 read ID
        chunk->id = readUint32(file);

        // 2 read raw length word  split out hasSubChunks bit
        uint32_t rawLen = readUint32(file);
        chunk->hasSubChunks = (rawLen & 0x80000000u) != 0;
        chunk->length = rawLen & 0x7FFFFFFFu;

        // 3 compute end of data position and sanity check
        std::streampos dataEnd = file.tellg()
            + static_cast<std::streamoff>(chunk->length);
        if (chunk->length > fileSize
            || dataEnd > fileSize
            || chunk->length > 100000000)
        {
            std::cerr << "Suspicious chunk size at "
                << startPos
                << " ID: 0x" << std::hex << chunk->id
                << std::dec << ", size: " << chunk->length
                << "\n";
            break;
        }

        // 4 read the payload
        chunk->data.resize(chunk->length);
        file.read(reinterpret_cast<char*>(chunk->data.data()), chunk->length);

        std::cout << "Top level chunk: 0x"
            << std::hex << chunk->id
            << std::dec << "  size=" << chunk->length
            << "  wraps=" << chunk->hasSubChunks
            << "\n";

        // 5 if MSB said this has subchunks OR the ID is in our forced-wrapper list
        const bool wraps = chunk->hasSubChunks || IsForcedWrapper(chunk->id);
        if (wraps) {
            std::string buf(reinterpret_cast<char*>(chunk->data.data()), chunk->length);
            std::istringstream subStream(buf);
            parseChunk(subStream, chunk);
        }


        // 5 if MSB said this has subchunks recurse
        //if (chunk->hasSubChunks) {
        //    std::string buf(reinterpret_cast<char*>(chunk->data.data()),
        //        chunk->length);
        //    std::istringstream subStream(buf);
        //    parseChunk(subStream, chunk);
      //  }

        // 6 advance to next top level chunk
        file.seekg(dataEnd);
        chunks.push_back(std::move(chunk));
    }

    return true;
}

bool ChunkData::parseChunk(std::istream& stream, std::shared_ptr<ChunkItem>& parent) {
    static constexpr uint32_t
        DATA_WRAPPER = 0x03150809, // legacy “data wrapper”
        SOUNDROBJ_DEF = 0x0A02,
        SOUND_RENDER_DEF = 0x0100,
        SOUND_RENDER_DEF_EXT = 0x0200,
        SPHERE_DEF = 0x0741,
        RING_DEF = 0x0742;

    while (stream && stream.peek() != EOF) {
        auto pos = stream.tellg();

        // 1) decide micro‐mode (1b ID + 1b len).  This has *nothing* to do with the MSB of the length word.
        bool microMode = false;
        if (parent) {
            // data‐wrapper always micro‐mode
            if (parent->id == DATA_WRAPPER) {
                microMode = true;
            }
            // under SOUNDROBJ_DEF → SOUND_RENDER_DEF or EXT
            else if (parent->id == SOUND_RENDER_DEF &&
                parent->parent &&
                (parent->parent->id == SOUNDROBJ_DEF ||
                    parent->parent->id == SOUND_RENDER_DEF_EXT))
            {
                microMode = true;
            }
            // sphere/ring channels are also micro‐mode
            else if ((parent->id == SPHERE_DEF || parent->id == RING_DEF)) {
                // peek next byte (but don't consume it here)
                // we know valid micro‐IDs are 2..5 for sphere/ring
                int peek = stream.peek();
                if (peek >= 2 && peek <= 5) microMode = true;
            }
        }

        if (microMode) {
            // need at least 2 bytes for micro‐ID + micro‐length
            if (stream.rdbuf()->in_avail() < 2) break;
            uint8_t mid, mlen;
            stream.read(reinterpret_cast<char*>(&mid), 1);
            stream.read(reinterpret_cast<char*>(&mlen), 1);

            if (stream.rdbuf()->in_avail() < mlen) {
                std::cerr << "Truncated microchunk at " << pos
                    << " id=0x" << std::hex << int(mid) << std::dec
                    << " size=" << int(mlen) << "\n";
                break;
            }

            auto child = std::make_shared<ChunkItem>();
            child->id = mid;
            child->length = mlen;
            child->data.resize(mlen);
            stream.read(reinterpret_cast<char*>(child->data.data()), mlen);

            child->parent = parent.get();
            parent->children.push_back(child);
            continue;
        }

        // 2) otherwise it's a normal 4‑byte ID + 4‑byte length + payload
        if (stream.rdbuf()->in_avail() < 8) break;
        auto child = std::make_shared<ChunkItem>();
        child->id = readUint32(stream);

        uint32_t rawLen = readUint32(stream);
        // MSB here *only* means “this chunk *may* contain sub‑chunks”
        child->hasSubChunks = (rawLen & 0x80000000u) != 0;
        child->length = rawLen & 0x7FFFFFFFu;

        if (stream.rdbuf()->in_avail() < static_cast<std::streamsize>(child->length)) {
            std::cerr << "Truncated child chunk at " << pos
                << " ID=0x" << std::hex << child->id << std::dec
                << " expected=" << child->length << "\n";
            break;
        }

        // read the payload
        child->data.resize(child->length);
        stream.read(reinterpret_cast<char*>(child->data.data()), child->length);

        child->parent = parent.get();
        parent->children.push_back(child);

        // 3) recurse only if MSB was set
        if (child->hasSubChunks) {
            std::string buf(reinterpret_cast<char*>(child->data.data()), child->length);
            std::istringstream subStream(buf);
            parseChunk(subStream, child);
        }
    }

    return true;
}




void ChunkData::clear() {
    chunks.clear();
}