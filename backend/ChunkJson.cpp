#include "ChunkJson.h"

#include <nlohmann/json.hpp>
#include "JsonCompat.h"

#include "ChunkItem.h"
#include "ChunkNames.h"
#include "ChunkSerializers.h"
#include "ChunkSerializer.h"

#include <cstdint>
#include <exception>
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

bool shouldUseSerializerForItem(const ChunkItem& item) {
    // Chunk IDs are not globally unique across all parent contexts.
    // Under SPHERE/RING wrappers, child 0x0001 is sphere/ring header data,
    // not mesh header1; force raw fallback there to keep exact bytes.
    if (item.parent != nullptr) {
        const uint32_t parentId = item.parent->id;
        if ((parentId == 0x0741 || parentId == 0x0742) && item.id == 0x0001) {
            return false;
        }
    }
    return true;
}

std::string appendObjectPath(const std::string& base, const char* key) {
    return base.empty() ? std::string(key) : (base + "." + key);
}

std::string appendArrayPath(const std::string& base, const char* key, std::size_t index) {
    return appendObjectPath(base, key) + "[" + std::to_string(index) + "]";
}

void appendWarning(
    std::vector<std::string>* warnings,
    const std::string& path,
    const std::string& message)
{
    if (!warnings) {
        return;
    }
    warnings->push_back(path + ": " + message);
}

} // namespace

ordered_json ChunkJson::toJson(const ChunkItem& item, JsonSerializationMode mode) {
    ordered_json obj;
    obj["CHUNK_NAME"] = LabelForChunk(item.id, const_cast<ChunkItem*>(&item));
    obj["SUBCHUNKS"] = item.hasSubChunks;
    obj["CHUNK_ID"] = item.id;
    obj["LENGTH"] = item.length;

    if (!item.children.empty()) {
        ordered_json arr = ordered_json::array();
        for (const auto& c : item.children) {
            arr.push_back(ChunkJson::toJson(*c, mode));
        }
        obj["CHILDREN"] = arr;

    }
    else if (!item.data.empty()) {
        if (mode == JsonSerializationMode::HexOnly) {
            obj["RAW_DATA_HEX"] = encodeHex(item.data);
        }
        else {
            const auto& registry = chunkSerializerRegistry();
            auto it = registry.find(item.id);
            if (it != registry.end() && shouldUseSerializerForItem(item)) {
                obj["DATA"] = toOrdered(it->second->toJson(item));
            }
            else {
                obj["RAW_DATA_HEX"] = encodeHex(item.data);
            }
        }
    }
    return obj;
}

std::shared_ptr<ChunkItem> ChunkJson::fromJson(
    const ordered_json& obj,
    ChunkItem* parent,
    JsonSerializationMode declaredMode,
    std::vector<std::string>* warnings,
    const std::string& jsonPath)
{
    try {
        if (!obj.is_object()) {
            return nullptr;
        }

        const std::string currentPath = jsonPath.empty() ? std::string("$") : jsonPath;
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
            std::size_t childIndex = 0;
            for (const auto& v : obj.at("CHILDREN")) {
                if (!v.is_object()) {
                    return nullptr;
                }
                auto child = ChunkJson::fromJson(
                    v,
                    item.get(),
                    declaredMode,
                    warnings,
                    appendArrayPath(currentPath, "CHILDREN", childIndex));
                if (!child) {
                    return nullptr;
                }
                item->children.push_back(child);
                ++childIndex;
            }
            return item;
        }

        const auto& registry = chunkSerializerRegistry();
        auto it = registry.find(item->id);
        const bool canUseSerializer = shouldUseSerializerForItem(*item);
        const bool hasData = obj.contains("DATA");
        const bool hasRawHex = obj.contains("RAW_DATA_HEX");

        auto tryRestoreFromSerializer = [&]() -> bool {
            if (!hasData) {
                return false;
            }
            if (it == registry.end()) {
                appendWarning(
                    warnings,
                    appendObjectPath(currentPath, "DATA"),
                    "No serializer registered for this chunk ID.");
                return false;
            }
            if (!canUseSerializer) {
                appendWarning(
                    warnings,
                    appendObjectPath(currentPath, "DATA"),
                    "Serializer is disabled for this chunk in the current parent context.");
                return false;
            }
            if (!obj.at("DATA").is_object()) {
                appendWarning(
                    warnings,
                    appendObjectPath(currentPath, "DATA"),
                    "DATA exists but is not an object.");
                return false;
            }
            try {
                it->second->fromJson(toQJsonObject(obj.at("DATA")), *item);
            }
            catch (const std::exception& e) {
                appendWarning(
                    warnings,
                    appendObjectPath(currentPath, "DATA"),
                    std::string("Serializer import failed: ") + e.what());
                return false;
            }
            catch (...) {
                appendWarning(
                    warnings,
                    appendObjectPath(currentPath, "DATA"),
                    "Serializer import failed with unknown exception.");
                return false;
            }
            item->length = static_cast<uint32_t>(item->data.size());
            return true;
        };

        auto tryRestoreFromRawHex = [&]() -> bool {
            if (!hasRawHex) {
                return false;
            }
            if (!obj.at("RAW_DATA_HEX").is_string()) {
                appendWarning(
                    warnings,
                    appendObjectPath(currentPath, "RAW_DATA_HEX"),
                    "RAW_DATA_HEX exists but is not a string.");
                return false;
            }
            std::vector<uint8_t> decoded;
            if (!decodeHex(obj.at("RAW_DATA_HEX").get<std::string>(), decoded)) {
                appendWarning(
                    warnings,
                    appendObjectPath(currentPath, "RAW_DATA_HEX"),
                    "Failed to decode RAW_DATA_HEX.");
                return false;
            }
            item->data = std::move(decoded);
            item->length = static_cast<uint32_t>(item->data.size());
            return true;
        };

        bool restoredData = false;
        if (declaredMode == JsonSerializationMode::HexOnly) {
            restoredData = tryRestoreFromRawHex();
            if (!restoredData && hasData) {
                if (hasRawHex) {
                    appendWarning(
                        warnings,
                        currentPath,
                        "RAW_DATA_HEX could not be imported; falling back to DATA.");
                }
                else {
                    appendWarning(
                        warnings,
                        currentPath,
                        "RAW_DATA_HEX missing for HEX_ONLY payload; using DATA.");
                }
                restoredData = tryRestoreFromSerializer();
            }
        }
        else {
            restoredData = tryRestoreFromSerializer();
            if (!restoredData && hasRawHex) {
                if (hasData) {
                    appendWarning(
                        warnings,
                        currentPath,
                        "DATA could not be imported; falling back to RAW_DATA_HEX.");
                }
                else {
                    appendWarning(
                        warnings,
                        currentPath,
                        "DATA missing for STRUCTURED_PREFERRED payload; using RAW_DATA_HEX.");
                }
                restoredData = tryRestoreFromRawHex();
            }
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
