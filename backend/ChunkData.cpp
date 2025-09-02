#include <sstream>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <filesystem>
#include <QJsonDocument>
#include <QJsonArray>
#include "ChunkData.h"
#include "ChunkNames.h"
#include "FormatUtils.h"
#include "W3DStructs.h"
#include "ChunkJson.h"



static bool readUint32(std::istream& stream, uint32_t& value) {
    value = 0;
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return static_cast<bool>(stream);
}

inline bool IsForcedWrapper(uint32_t id, uint32_t parent = 0)
{
    switch (id) {
    case 0x0000: // W3D_CHUNK_MESH
    case 0x0015: // W3D_CHUNK_MATERIALS3
	case 0x0016: // W3D_CHUNK_MATERIAL3
    case 0x0023: // W3D_CHUNK_PRELIT_UNLIT
    case 0x0024: // W3D_CHUNK_PRELIT_VERTEX
    case 0x0025: // W3D_CHUNK_LIGHTMAP_MULTI_PASS
    case 0x0026: // W3D_CHUNK_LIGHTMAP_MULTI_TEXTURE
    case 0x002A: // W3D_CHUNK_VERTEX_MATERIALS
    case 0x002B: // W3D_CHUNK_VERTEX_MATERIAL
    case 0x0030: //W3D_CHUNK_TEXTURES
    case 0x0031: // W3D_CHUNK_TEXTURE
    case 0x0038: // W3D_CHUNK_MATERIAL_PASS
    case 0x0048: // W3D_CHUNK_TEXTURE_STAGE
    case 0x0058: // W3D_CHUNK_DEFORM
    case 0x0059: // W3D_CHUNK_DEFORM_SET
    case 0x005A: // W3D_CHUNK_DEFORM_KEYFRAME
    case 0x005B: // W3D_CHUNK_DEFORM_DATA
    case 0x0090: // W3D_CHUNK_AABTREE
    case 0x0100: // W3D_CHUNK_HIERARCHY
    case 0x0200: // W3D_CHUNK_ANIMATION
	case 0x0280: // W3D_CHUNK_COMPRESSED_ANIMATION
    case 0x02C0: // W3D_CHUNK_MORPH_ANIMATION
    case 0x02C2: // W3D_CHUNK_MORPH_CHANNEL
    case 0x0300: // W3D_CHUNK_HMODEL
    case 0x0400: // W3D_CHUNK_LODMODEL
    case 0x0420: // W3D_CHUNK_COLLECTION
    case 0x0460: // W3D_CHUNK_LIGHT
    case 0x0500: // W3D_CHUNK_EMITTER
    case 0x0600: // W3D_CHUNK_AGGREGATE
    case 0x0700: // W3D_CHUNK_HLOD
    case 0x0702: // W3D_CHUNK_HLOD_LOD_ARRAY
    case 0x0706:  // W3D_CHUNK_HLOD_PROXY_ARRAY
	case 0x0707: // W3D_CHUNK_HLOD_LIGHT_ARRAY
    case 0x0800: // W3D_CHUNK_LIGHTSCAPE
	case 0x0801: // W3D_CHUNK_LIGHTSCAPE_LIGHT
    case 0x0900: // W3D_CHUNK_DAZZLE
	case 0x0A00: // W3D_CHUNK_SOUNDROBJ
    case 0x0B01: // W3D_CHUNK_SHDMESH
    case 0x0B20: // W3D_CHUNK_SHDSUBMESH
    case 0x0B40: // W3D_CHUNK_SHDSUBMESH_SHADER


        return true;



    default:
        (void)parent; // parent available if you later need parent-sensitive overrides
        return false;
    }
}


bool ChunkData::loadFromFile(const std::string& filename) {
    // Ensure previous data does not persist between loads
    clear();
    sourceFilename = std::filesystem::path(filename).filename().string();
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

    while (file && file.tellg() < fileSize) {
        auto chunk = std::make_shared<ChunkItem>();
        std::streampos startPos = file.tellg();

        // 1 read ID
        if (!readUint32(file, chunk->id)) break;

        // 2 read raw length word  split out hasSubChunks bit
        uint32_t rawLen = 0;
        if (!readUint32(file, rawLen)) break;
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
        if (!file) break;
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
        DATA_WRAPPER = 0x03150809, // legacy data wrapper
        SOUNDROBJ_DEF = 0x0A02,
        SOUND_RENDER_DEF = 0x0100,
        SOUND_RENDER_DEF_EXT = 0x0200,
        SPHERE_DEF = 0x0741,
        RING_DEF = 0x0742;

    while (stream && stream.peek() != EOF) {
        auto pos = stream.tellg();

        // 1) decide micro mode (1b ID + 1b len).  This has *nothing* to do with the MSB of the length word.
        bool microMode = false;
        if (parent) {
            // data wrapper always micro mode
            if (parent->id == DATA_WRAPPER) {
                microMode = true;
            }
            // under SOUNDROBJ_DEF  SOUND_RENDER_DEF or EXT
            else if (parent->id == SOUND_RENDER_DEF &&
                parent->parent &&
                (parent->parent->id == SOUNDROBJ_DEF ||
                    parent->parent->id == SOUND_RENDER_DEF_EXT))
            {
                microMode = true;
            }
          
        }

        if (microMode) {
            // need at least 2 bytes for micro ID + micro length
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

        // 2) otherwise it's a normal 4 byte ID + 4 byte length + payload
        if (stream.rdbuf()->in_avail() < 8) break;
        auto child = std::make_shared<ChunkItem>();
        if (!readUint32(stream, child->id)) break;

        uint32_t rawLen = 0;
        if (!readUint32(stream, rawLen)) break;
        // MSB here *only* means this chunk *may* contain subchunks
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
        if (!stream) break;
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
    sourceFilename.clear();
}

static void writeChunkStream(std::ostream& stream, const std::shared_ptr<ChunkItem>& chunk, ChunkItem* parent = nullptr) {
    bool microMode = false;
    if (parent) {
        constexpr uint32_t DATA_WRAPPER = 0x03150809;
        constexpr uint32_t SOUNDROBJ_DEF = 0x0A02;
        constexpr uint32_t SOUND_RENDER_DEF = 0x0100;
        constexpr uint32_t SOUND_RENDER_DEF_EXT = 0x0200;
        if (parent->id == DATA_WRAPPER)
            microMode = true;
        else if (parent->id == SOUND_RENDER_DEF && parent->parent &&
            (parent->parent->id == SOUNDROBJ_DEF || parent->parent->id == SOUND_RENDER_DEF_EXT))
            microMode = true;
    }

    if (microMode) {
        uint8_t mid = static_cast<uint8_t>(chunk->id);
        uint8_t mlen = static_cast<uint8_t>(chunk->data.size());
        stream.write(reinterpret_cast<char*>(&mid), 1);
        stream.write(reinterpret_cast<char*>(&mlen), 1);
        if (!chunk->data.empty())
            stream.write(reinterpret_cast<const char*>(chunk->data.data()), chunk->data.size());
        return;
    }

    std::vector<uint8_t> payload;
    if (!chunk->children.empty()) {
        std::ostringstream buffer;
        for (const auto& c : chunk->children)
            writeChunkStream(buffer, c, chunk.get());
        std::string buf = buffer.str();
        payload.assign(buf.begin(), buf.end());
    }
    else {
        payload = chunk->data;
    }

    uint32_t rawLen = static_cast<uint32_t>(payload.size()) |
        (chunk->hasSubChunks ? 0x80000000u : 0);
    stream.write(reinterpret_cast<const char*>(&chunk->id), sizeof(chunk->id));
    stream.write(reinterpret_cast<const char*>(&rawLen), sizeof(rawLen));
    if (!payload.empty())
        stream.write(reinterpret_cast<const char*>(payload.data()), payload.size());
}

QJsonDocument ChunkData::toJson() const {
    QJsonObject root;
    root["SCHEMA_VERSION"] = 1;
    QJsonArray arr;
    for (const auto& c : chunks)
        arr.append(ChunkJson::toJson(*c));
    QString key = sourceFilename.empty() ? QStringLiteral("CHUNKS")
        : QString::fromStdString(sourceFilename);
    root[key] = arr;
    return QJsonDocument(root);
}

bool ChunkData::fromJson(const QJsonDocument& doc) {
    if (!doc.isObject()) return false;
    clear();
    QJsonObject root = doc.object();
    QJsonArray arr;
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (it.key() == QLatin1String("SCHEMA_VERSION")) continue;
        if (it.value().isArray()) {
            sourceFilename = it.key().toStdString();
            arr = it.value().toArray();
            break;
        }
    }
    if (arr.isEmpty()) return false;
    for (const auto& v : arr) {
        if (v.isObject()) {
            chunks.push_back(ChunkJson::fromJson(v.toObject()));
        }
    }
    return true;
}

bool ChunkData::saveToFile(const std::string& filename) const {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;
    for (const auto& c : chunks)
        writeChunkStream(out, c);
    return true;
}