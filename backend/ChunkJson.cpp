#include "ChunkJson.h"

#include <nlohmann/json.hpp>
#include "JsonCompat.h"

#include "ChunkItem.h"
#include "ChunkNames.h"
#include "ChunkSerializers.h"
#include "ChunkSerializer.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

std::string encodeHex(const std::vector<uint8_t>& bytes) {
    static constexpr char kHexDigits[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        out.push_back(kHexDigits[(b >> 4) & 0x0F]);
        out.push_back(kHexDigits[b & 0x0F]);
    }
    return out;
}

int hexNibble(char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    if (uc >= '0' && uc <= '9') return uc - '0';
    if (uc >= 'a' && uc <= 'f') return 10 + (uc - 'a');
    if (uc >= 'A' && uc <= 'F') return 10 + (uc - 'A');
    return -1;
}

bool decodeHex(const std::string& hex, std::vector<uint8_t>& bytes) {
    if ((hex.size() % 2) != 0) {
        return false;
    }
    bytes.clear();
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        const int hi = hexNibble(hex[i]);
        const int lo = hexNibble(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

bool readUInt32Field(const ordered_json& obj, const char* key, uint32_t& out) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        return false;
    }
    if (it->is_number_unsigned()) {
        const auto v = it->get<uint64_t>();
        if (v > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
        out = static_cast<uint32_t>(v);
        return true;
    }
    if (it->is_number_integer()) {
        const auto v = it->get<int64_t>();
        if (v < 0 || static_cast<uint64_t>(v) > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
        out = static_cast<uint32_t>(v);
        return true;
    }
    return false;
}

bool readBoolField(const ordered_json& obj, const char* key, bool& out) {
    auto it = obj.find(key);
    if (it == obj.end() || !it->is_boolean()) {
        return false;
    }
    out = it->get<bool>();
    return true;
}

} // namespace

ordered_json ChunkJson::toJson(const ChunkItem& item) {
    ordered_json obj;
    obj["CHUNK_NAME"] = LabelForChunk(item.id, const_cast<ChunkItem*>(&item));
    obj["SUBCHUNKS"] = item.hasSubChunks;
    obj["CHUNK_ID"] = item.id;
    obj["LENGTH"] = item.length;

    if (!item.children.empty()) {
        ordered_json arr = ordered_json::array();
        for (const auto& c : item.children) {
            arr.push_back(ChunkJson::toJson(*c));
        }
        obj["CHILDREN"] = arr;

    }
    else if (!item.data.empty()) {
        const auto& registry = chunkSerializerRegistry();
        auto it = registry.find(item.id);
        if (it != registry.end()) {
            obj["DATA"] = toOrdered(it->second->toJson(item));
        }
        else {
            obj["RAW_DATA_HEX"] = encodeHex(item.data);
        }
    }
    return obj;
}

std::shared_ptr<ChunkItem> ChunkJson::fromJson(const ordered_json& obj, ChunkItem* parent) {
    try {
        if (!obj.is_object()) {
            return nullptr;
        }

        auto item = std::make_shared<ChunkItem>();
        if (!readUInt32Field(obj, "CHUNK_ID", item->id)) {
            return nullptr;
        }
        if (!readUInt32Field(obj, "LENGTH", item->length)) {
            return nullptr;
        }
        if (!readBoolField(obj, "SUBCHUNKS", item->hasSubChunks)) {
            return nullptr;
        }
        if (obj.contains("CHUNK_NAME")) {
            if (!obj.at("CHUNK_NAME").is_string()) {
                return nullptr;
            }
            item->typeName = obj.at("CHUNK_NAME").get<std::string>();
        }
        item->parent = parent;

        if (obj.contains("CHILDREN")) {
            if (!obj.at("CHILDREN").is_array()) {
                return nullptr;
            }
            for (const auto& v : obj.at("CHILDREN")) {
                if (!v.is_object()) {
                    return nullptr;
                }
                auto child = ChunkJson::fromJson(v, item.get());
                if (!child) {
                    return nullptr;
                }
                item->children.push_back(child);
            }
            return item;
        }

        const auto& registry = chunkSerializerRegistry();
        auto it = registry.find(item->id);
        bool restoredData = false;

        if (obj.contains("DATA")) {
            if (it == registry.end()) {
                return nullptr;
            }
            it->second->fromJson(toQJsonObject(obj.at("DATA")), *item);
            restoredData = true;
        }

        if (!restoredData && obj.contains("RAW_DATA_HEX")) {
            if (!obj.at("RAW_DATA_HEX").is_string()) {
                return nullptr;
            }
            if (!decodeHex(obj.at("RAW_DATA_HEX").get<std::string>(), item->data)) {
                return nullptr;
            }
            item->length = static_cast<uint32_t>(item->data.size());
            restoredData = true;
        }

        if (!restoredData && item->length > 0) {
            return nullptr;
        }

        return item;
    }
    catch (const nlohmann::json::exception&) {
        return nullptr;
    }
}
