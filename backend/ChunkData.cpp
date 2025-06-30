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
    case 0x0002: // W3D_CHUNK_VERTICES
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
	case 0x0741: // W3D_CHUNK_SPHERE
    case 0x0742: // W3D_CHUNK_RING
    case 0x0800: // W3D_CHUNK_LIGHTSCAPE
	case 0x0801: // W3D_CHUNK_LIGHTSCAPE_LIGHT
	case 0x0900: // W3D_CHUNK_DAZZLE
	case 0x0A00: // W3D_CHUNK_SOUNDROBJ
	case 0x0A02: // W3D_CHUNK_SOUNDROBJ_Definition
    case 0x03150809: //W3D_CHUNK_CHUNKID_DATA

        return true;
    default:
        return false;
    }
}
//static bool isMicroChunk(uint32_t id) {
//    switch (id) {
//    case 0x0001: // MicroChunk Frame for Primitives
//	case 0x0100: // MicroChunk for SoundRobjD
//
//
//        return true;
//    default:
//       return false;
//    }
//}

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

bool ChunkData::parseChunk(std::istream& stream, std::shared_ptr<ChunkItem>& parent)
    {
        static constexpr uint32_t
            DATA_WRAPPER = 0x03150809,
            SOUNDROBJ_DEFINITION = 0x0A02,
            SOUND_RENDER_DEF = 0x0100,
            SOUND_RENDER_DEF_EXT = 0x0200;

        while (stream && stream.peek() != EOF) {
            std::streampos pos = stream.tellg();

            // 1) Do we go into micromode?
            //    If the current chunk (parent) is a DATA_WRAPPER
            //    Or if it's one of the two SOUND_RENDER_DEF* chunks under SOUNDROBJ_DEFINITION
            bool isDataWrapper = (parent && parent->id == DATA_WRAPPER);
           // bool isSoundDef = (parent && parent->id == SOUND_RENDER_DEF);
            bool isSoundDefChild =
                parent                                    // 1) have a parent?
                && parent->id == SOUND_RENDER_DEF          // 2) that parent is *exactly* 0x0100
                && parent->parent                           // 3) have a grandparent?
                && (parent->parent->id == SOUNDROBJ_DEFINITION
                    || parent->parent->id == SOUND_RENDER_DEF_EXT

                    );     // 4) grandparent is 0x0A02 *or* 0x0200

            // DEBUG: print context for this iteration
            std::cout << "DBG parseChunk @ pos=" << pos;

            if (parent) {
                std::cout << "  parent=0x" << std::hex << parent->id << std::dec;
            }
            else {
                std::cout << "  parent=null";
            }

            if (parent && parent->parent) {
                std::cout << "  grandparent=0x"
                    << std::hex << parent->parent->id << std::dec;
            }
            else {
                std::cout << "  grandparent=null";
            }

            std::cout << "  isDataWrapper=" << isDataWrapper
                << "  isSoundDefChild=" << isSoundDefChild
                << "\n";

            if (isDataWrapper || isSoundDefChild) {
                std::cout << "DBG >> entering micro-mode at pos=" << pos
                    << " parent=0x" << std::hex << parent->id
                    << std::dec << "\n";

            // make sure we have at least 2 bytes for microId+microSize
            if (stream.rdbuf()->in_avail() < 2) break;

            uint8_t microId = 0;
            uint8_t microSize = 0;
            stream.read(reinterpret_cast<char*>(&microId), 1);
            stream.read(reinterpret_cast<char*>(&microSize), 1);

            std::cout << "DBG >> microId=0x" << std::hex << int(microId)
                << " size=" << std::dec << int(microSize) << "\n";

            // sanity
            if (stream.rdbuf()->in_avail() < microSize) {
                std::cerr
                    << "Incomplete microchunk at " << pos
                    << ", ID: " << int(microId)
                    << ", size: " << int(microSize) << "\n";
                break;
            }

            // build child
            auto micro = std::make_shared<ChunkItem>();
            micro->id = microId;
            micro->length = microSize;
            micro->data.resize(microSize);
            stream.read(reinterpret_cast<char*>(micro->data.data()),
                microSize);
            micro->parent = parent.get();
            parent->children.push_back(micro);
            continue;
        }
                
        /*
        // Handle microchunks inside CHUNKID_DATA
        if (parent && parent->id == 0x03150809) {
            if (stream.rdbuf()->in_avail() < 2) break;

            auto micro = std::make_shared<ChunkItem>();
            uint8_t microId = 0;
            uint8_t microSize = 0;
            stream.read(reinterpret_cast<char*>(&microId), 1);
            stream.read(reinterpret_cast<char*>(&microSize), 1);

            if (stream.rdbuf()->in_avail() < microSize) {
                std::cerr << "Incomplete microchunk at " << pos << ", ID: " << (int)microId << ", size: " << (int)microSize << "\n";
                break;
            }

            micro->id = static_cast<uint32_t>(microId);
            micro->length = static_cast<uint32_t>(microSize);
            micro->data.resize(microSize);
            stream.read(reinterpret_cast<char*>(micro->data.data()), microSize);
            micro->parent = parent.get();
            parent->children.push_back(micro);
            continue;
        }
        */

        // Normal chunk
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
        child->parent = parent.get();
        parent->children.push_back(child);

        bool treatAsWrapper = isWrapperChunk(child->id);

        static constexpr uint32_t
            SOUNDROBJ_DEFINITION = 0x00A2,
            SOUND_RENDER_DEF = 0x0100,
            SOUND_RENDER_DEF_EXT = 0x0200;

        // 1) recurse into the definition so you get its two children:
        //    - 0x0100 (render def) and
        //    - 0x0200 (render ext)
        if (!treatAsWrapper
            && child->id == SOUNDROBJ_DEFINITION)
        {
            treatAsWrapper = true;
        }

        // 2) recurse into the *first* level render def so that
        //    its micro mode logic can split the 1 byte entries
        else if (!treatAsWrapper
            && child->id == SOUND_RENDER_DEF
            && parent
            && parent->id == SOUNDROBJ_DEFINITION)
        {
            treatAsWrapper = true;
        }

        // 3) recurse into the extension wrapper so you get its nested 0x0100
        else if (!treatAsWrapper
            && child->id == SOUND_RENDER_DEF_EXT
            && parent
            && parent->id == SOUNDROBJ_DEFINITION)
        {
            treatAsWrapper = true;
        }


        // Special wrapper handling for sphere channel chunks
        if (!treatAsWrapper
            && parent
            && (parent->id == 0x0741 || parent->id == 0x0742)
            && (child->id == 0x0002 || child->id == 0x0003
                || child->id == 0x0004 || child->id == 0x0005))
        {
            treatAsWrapper = true;
        }
        std::cout << "DBG wrapper-check: child=0x" << std::hex << child->id
            << " parent=0x" << parent->id
            << " wasWrapper=" << treatAsWrapper << std::dec << "\n";

        if (treatAsWrapper) {
            std::string subData(reinterpret_cast<char*>(child->data.data()), child->length);
            std::istringstream subStream(subData);
            std::cout << "Parsing subchunks of wrapper: 0x" << std::hex << child->id
                << " (" << GetChunkName(child->id, parent ? parent->id : 0) << ")\n";
            parseChunk(subStream, child);
        }
    }
    return true;
}



void ChunkData::clear() {
    chunks.clear();
}