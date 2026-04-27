#include "SceneBuilder.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

#include "../W3DStructs.h"
#include "../parseUtils.h"

namespace OW3D::Render {
namespace {

constexpr uint32_t kChunkMesh = 0x0000;
constexpr uint32_t kChunkVertices = 0x0002;
constexpr uint32_t kChunkVertexNormals = 0x0003;
constexpr uint32_t kChunkTexCoords = 0x0005;
constexpr uint32_t kChunkVertexInfluences = 0x000E;
constexpr uint32_t kChunkMeshHeader3 = 0x001F;
constexpr uint32_t kChunkTriangles = 0x0020;
constexpr uint32_t kChunkShaders = 0x0029;
constexpr uint32_t kChunkTextureWrapper = 0x0031;
constexpr uint32_t kChunkTextureName = 0x0032;
constexpr uint32_t kChunkTextureInfo = 0x0033;
constexpr uint32_t kChunkMaterialPass = 0x0038;
constexpr uint32_t kChunkVertexMaterialIds = 0x0039;
constexpr uint32_t kChunkShaderIds = 0x003A;
constexpr uint32_t kChunkTextureStage = 0x0048;
constexpr uint32_t kChunkTextureIds = 0x0049;
constexpr uint32_t kChunkStageTexCoords = 0x004A;
constexpr uint32_t kChunkVertexMaterial = 0x002B;
constexpr uint32_t kChunkVertexMaterialInfo = 0x002D;
constexpr uint32_t kChunkVertexMapperArgs0 = 0x002E;
constexpr uint32_t kChunkPrelitUnlit = 0x0023;
constexpr uint32_t kChunkPrelitVertex = 0x0024;
constexpr uint32_t kChunkPrelitLightmapMultiPass = 0x0025;
constexpr uint32_t kChunkPrelitLightmapMultiTexture = 0x0026;
constexpr uint32_t kChunkHierarchy = 0x0100;
constexpr uint32_t kChunkHierarchyHeader = 0x0101;
constexpr uint32_t kChunkPivots = 0x0102;
constexpr uint32_t kChunkAnimation = 0x0200;
constexpr uint32_t kChunkAnimationHeader = 0x0201;
constexpr uint32_t kChunkAnimationChannel = 0x0202;
constexpr uint32_t kChunkAnimationBitChannel = 0x0203;
constexpr uint32_t kChunkCompressedAnimation = 0x0280;
constexpr uint32_t kChunkCompressedAnimationHeader = 0x0281;
constexpr uint32_t kChunkCompressedAnimationChannel = 0x0282;
constexpr uint32_t kChunkCompressedAnimationBitChannel = 0x0283;
constexpr uint32_t kChunkCompressedAnimationAdaptiveDeltaChannel = 0x0284;
constexpr uint32_t kChunkAggregate = 0x0600;
constexpr uint32_t kChunkAggregateHeader = 0x0601;
constexpr uint32_t kChunkAggregateInfo = 0x0602;
constexpr uint32_t kChunkAggregateClassInfo = 0x0604;
constexpr uint32_t kChunkHModel = 0x0300;
constexpr uint32_t kChunkHModelHeader = 0x0301;
constexpr uint32_t kChunkHModelNode = 0x0302;
constexpr uint32_t kChunkHModelSkinNode = 0x0304;
constexpr uint32_t kChunkLodModel = 0x0400;
constexpr uint32_t kChunkLodModelHeader = 0x0401;
constexpr uint32_t kChunkLodModelLod = 0x0402;
constexpr uint32_t kChunkHLod = 0x0700;
constexpr uint32_t kChunkHLodHeader = 0x0701;
constexpr uint32_t kChunkHLodLodArray = 0x0702;
constexpr uint32_t kChunkHLodSubObjectArrayHeader = 0x0703;
constexpr uint32_t kChunkHLodSubObject = 0x0704;
constexpr uint32_t kChunkSecondaryVertices = 0x0C00;
constexpr uint32_t kChunkSecondaryVertexNormals = 0x0C01;
constexpr uint32_t kChunkVertexInfluencesExtended = 0x0C03;
constexpr uint8_t kStageMappingScreen = 0x03;
constexpr uint8_t kStageMappingLinearOffset = 0x04;
constexpr uint8_t kStageMappingRotate = 0x08;

std::string ReadFixedString(const char* data, std::size_t maxLen) {
    if (!data || maxLen == 0) {
        return {};
    }
    std::size_t len = 0;
    while (len < maxLen && data[len] != '\0') {
        ++len;
    }
    return std::string(data, len);
}

std::string ReadNullTerminatedChunkString(const std::shared_ptr<ChunkItem>& chunk) {
    if (!chunk || chunk->data.empty()) {
        return {};
    }
    const char* text = reinterpret_cast<const char*>(chunk->data.data());
    std::size_t len = 0;
    while (len < chunk->data.size() && text[len] != '\0') {
        ++len;
    }
    return std::string(text, len);
}

std::string NormalizeName(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (const char c : input) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

std::string BuildChunkPath(const ChunkItem* node) {
    if (!node) {
        return {};
    }
    std::vector<uint32_t> ids;
    ids.reserve(16);
    const ChunkItem* cursor = node;
    while (cursor) {
        ids.push_back(cursor->id);
        cursor = cursor->parent;
    }

    std::string out;
    out.reserve(ids.size() * 11);
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        if (!out.empty()) {
            out += '/';
        }
        char buffer[16] = {};
        std::snprintf(buffer, sizeof(buffer), "0x%08X", *it);
        out += buffer;
    }
    return out;
}

std::vector<std::shared_ptr<ChunkItem>> FindChildrenById(
    const std::shared_ptr<ChunkItem>& parent,
    uint32_t childId)
{
    std::vector<std::shared_ptr<ChunkItem>> out;
    if (!parent) {
        return out;
    }
    for (const auto& child : parent->children) {
        if (child && child->id == childId) {
            out.push_back(child);
        }
    }
    return out;
}

std::shared_ptr<ChunkItem> FindFirstChildById(
    const std::shared_ptr<ChunkItem>& parent,
    uint32_t childId)
{
    if (!parent) {
        return nullptr;
    }
    for (const auto& child : parent->children) {
        if (child && child->id == childId) {
            return child;
        }
    }
    return nullptr;
}

void CollectChunksByIdRecursive(
    const std::shared_ptr<ChunkItem>& node,
    uint32_t id,
    std::vector<std::shared_ptr<ChunkItem>>& out)
{
    if (!node) {
        return;
    }
    if (node->id == id) {
        out.push_back(node);
    }
    for (const auto& child : node->children) {
        CollectChunksByIdRecursive(child, id, out);
    }
}

bool IsPrelitChunkId(uint32_t id) {
    return id == kChunkPrelitUnlit
        || id == kChunkPrelitVertex
        || id == kChunkPrelitLightmapMultiPass
        || id == kChunkPrelitLightmapMultiTexture;
}

std::shared_ptr<ChunkItem> SelectMaterialSourceRoot(const std::shared_ptr<ChunkItem>& meshChunk) {
    if (!meshChunk) {
        return nullptr;
    }

    for (const auto& child : meshChunk->children) {
        if (child && IsPrelitChunkId(child->id)) {
            return child;
        }
    }
    return meshChunk;
}

template <typename T>
std::optional<T> ParseStructWithWarning(
    const std::shared_ptr<ChunkItem>& chunk,
    std::vector<SceneBuildWarning>& warnings)
{
    const auto parsed = ParseChunkStruct<T>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        warnings.push_back({
            SceneBuildWarningCode::MissingPayload,
            BuildChunkPath(chunk.get()),
            "Failed to parse struct payload: " + *err });
        return std::nullopt;
    }
    return std::get<T>(parsed);
}

template <typename T>
std::optional<std::vector<T>> ParseArrayWithWarning(
    const std::shared_ptr<ChunkItem>& chunk,
    std::vector<SceneBuildWarning>& warnings)
{
    const auto parsed = ParseChunkArray<T>(chunk);
    if (auto err = std::get_if<std::string>(&parsed)) {
        warnings.push_back({
            SceneBuildWarningCode::MissingPayload,
            BuildChunkPath(chunk.get()),
            "Failed to parse array payload: " + *err });
        return std::nullopt;
    }
    return std::get<std::vector<T>>(parsed);
}

template <typename T>
bool ReadChunkValueAt(
    const std::shared_ptr<ChunkItem>& chunk,
    std::size_t offset,
    T& outValue)
{
    if (!chunk) {
        return false;
    }
    if ((offset + sizeof(T)) > chunk->data.size()) {
        return false;
    }
    std::memcpy(&outValue, chunk->data.data() + offset, sizeof(T));
    return true;
}

template <typename T>
bool ReadChunkArrayAt(
    const std::shared_ptr<ChunkItem>& chunk,
    std::size_t offset,
    std::size_t count,
    std::vector<T>& outValues)
{
    outValues.clear();
    if (!chunk) {
        return false;
    }
    const std::size_t bytes = count * sizeof(T);
    if ((offset + bytes) > chunk->data.size()) {
        return false;
    }
    outValues.resize(count);
    if (bytes > 0) {
        std::memcpy(outValues.data(), chunk->data.data() + offset, bytes);
    }
    return true;
}

struct ParsedRawAnimChannel {
    uint16_t firstFrame = 0;
    uint16_t lastFrame = 0;
    uint16_t vectorLen = 0;
    uint16_t flags = 0;
    uint16_t pivot = 0;
    std::vector<float> data;
};

std::optional<ParsedRawAnimChannel> ParseRawAnimChannel(
    const std::shared_ptr<ChunkItem>& chunk,
    std::vector<SceneBuildWarning>& warnings)
{
    constexpr std::size_t kHeaderSize = 12;
    if (!chunk || chunk->data.size() < kHeaderSize) {
        warnings.push_back({
            SceneBuildWarningCode::MissingPayload,
            BuildChunkPath(chunk.get()),
            "Animation channel payload is truncated."
            });
        return std::nullopt;
    }

    ParsedRawAnimChannel parsed{};
    if (!ReadChunkValueAt(chunk, 0, parsed.firstFrame)
        || !ReadChunkValueAt(chunk, 2, parsed.lastFrame)
        || !ReadChunkValueAt(chunk, 4, parsed.vectorLen)
        || !ReadChunkValueAt(chunk, 6, parsed.flags)
        || !ReadChunkValueAt(chunk, 8, parsed.pivot))
    {
        warnings.push_back({
            SceneBuildWarningCode::MissingPayload,
            BuildChunkPath(chunk.get()),
            "Animation channel header could not be read."
            });
        return std::nullopt;
    }

    const uint32_t frameCount =
        (parsed.lastFrame >= parsed.firstFrame)
        ? (static_cast<uint32_t>(parsed.lastFrame) - static_cast<uint32_t>(parsed.firstFrame) + 1u)
        : 0u;
    const std::size_t sampleCount =
        static_cast<std::size_t>(frameCount) * static_cast<std::size_t>(parsed.vectorLen);
    if (!ReadChunkArrayAt(chunk, kHeaderSize, sampleCount, parsed.data)) {
        warnings.push_back({
            SceneBuildWarningCode::MissingPayload,
            BuildChunkPath(chunk.get()),
            "Animation channel sample payload is truncated."
            });
        return std::nullopt;
    }

    return parsed;
}

struct ParsedTimeCodedAnimChannel {
    uint32_t numTimeCodes = 0;
    uint16_t pivot = 0;
    uint8_t vectorLen = 0;
    uint8_t flags = 0;
    std::vector<uint32_t> words;
};

std::optional<ParsedTimeCodedAnimChannel> ParseTimeCodedAnimChannel(
    const std::shared_ptr<ChunkItem>& chunk,
    std::vector<SceneBuildWarning>& warnings)
{
    constexpr std::size_t kHeaderSize = 8;
    if (!chunk || chunk->data.size() < kHeaderSize) {
        warnings.push_back({
            SceneBuildWarningCode::MissingPayload,
            BuildChunkPath(chunk.get()),
            "Compressed animation channel payload is truncated."
            });
        return std::nullopt;
    }

    ParsedTimeCodedAnimChannel parsed{};
    if (!ReadChunkValueAt(chunk, 0, parsed.numTimeCodes)
        || !ReadChunkValueAt(chunk, 4, parsed.pivot)
        || !ReadChunkValueAt(chunk, 6, parsed.vectorLen)
        || !ReadChunkValueAt(chunk, 7, parsed.flags))
    {
        warnings.push_back({
            SceneBuildWarningCode::MissingPayload,
            BuildChunkPath(chunk.get()),
            "Compressed animation channel header could not be read."
            });
        return std::nullopt;
    }

    const std::size_t packetSize = static_cast<std::size_t>(parsed.vectorLen) + 1u;
    const std::size_t wordCount = static_cast<std::size_t>(parsed.numTimeCodes) * packetSize;
    if (!ReadChunkArrayAt(chunk, kHeaderSize, wordCount, parsed.words)) {
        warnings.push_back({
            SceneBuildWarningCode::MissingPayload,
            BuildChunkPath(chunk.get()),
            "Compressed animation channel sample payload is truncated."
            });
        return std::nullopt;
    }

    return parsed;
}

void AppendRoots(
    std::vector<std::shared_ptr<ChunkItem>>& out,
    const W3DChunk* roots)
{
    if (!roots || roots->empty()) {
        return;
    }
    out.insert(out.end(), roots->begin(), roots->end());
}

std::vector<std::shared_ptr<ChunkItem>> CollectAllRoots(
    const W3DChunk& primaryRoots,
    const W3DChunk* skeletonRoots,
    const W3DChunk* animationRoots)
{
    std::vector<std::shared_ptr<ChunkItem>> allRoots = primaryRoots;
    AppendRoots(allRoots, skeletonRoots);
    AppendRoots(allRoots, animationRoots);
    return allRoots;
}

bool FileExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

bool IsSupportedTextureExtension(std::string_view extension) {
    static constexpr std::array<std::string_view, 6> kTextureExtensions = {
        ".dds", ".tga", ".png", ".jpg", ".jpeg", ".bmp"
    };
    const std::string normalized = NormalizeName(std::string(extension));
    return std::find(
        kTextureExtensions.begin(),
        kTextureExtensions.end(),
        std::string_view(normalized)) != kTextureExtensions.end();
}

std::string NormalizePathKey(std::string input) {
    for (char& c : input) {
        if (c == '\\') {
            c = '/';
        }
        else {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return input;
}

std::string BaseNameKey(const std::string& input) {
    const std::string normalized = NormalizePathKey(input);
    const std::size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos) {
        return normalized;
    }
    return normalized.substr(slash + 1);
}

std::string TrimAscii(std::string value) {
    std::size_t begin = 0;
    while (begin < value.size()
        && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin
        && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return value.substr(begin, end - begin);
}

bool TryParseFloat(const std::string& text, float& outValue) {
    const std::string trimmed = TrimAscii(text);
    if (trimmed.empty()) {
        return false;
    }

    char* parseEnd = nullptr;
    const float parsed = std::strtof(trimmed.c_str(), &parseEnd);
    if (parseEnd == trimmed.c_str() || *parseEnd != '\0') {
        return false;
    }

    outValue = parsed;
    return true;
}

std::unordered_map<std::string, std::string> ParseMapperArgs(const std::string& mapperArgsText) {
    std::unordered_map<std::string, std::string> argsByKey;
    std::size_t lineStart = 0;
    while (lineStart < mapperArgsText.size()) {
        const std::size_t lineEnd = mapperArgsText.find('\n', lineStart);
        std::string line = mapperArgsText.substr(
            lineStart,
            (lineEnd == std::string::npos) ? std::string::npos : (lineEnd - lineStart));
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const std::size_t commentPos = line.find(';');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }

        const std::size_t equalsPos = line.find('=');
        if (equalsPos != std::string::npos) {
            std::string key = TrimAscii(line.substr(0, equalsPos));
            std::string value = TrimAscii(line.substr(equalsPos + 1));
            if (!key.empty()) {
                key = NormalizeName(key);
                argsByKey[key] = value;
            }
        }

        if (lineEnd == std::string::npos) {
            break;
        }
        lineStart = lineEnd + 1;
    }

    return argsByKey;
}

std::optional<float> GetMapperArgFloat(
    const std::unordered_map<std::string, std::string>& argsByKey,
    const char* key)
{
    if (!key) {
        return std::nullopt;
    }

    const auto it = argsByKey.find(NormalizeName(key));
    if (it == argsByKey.end()) {
        return std::nullopt;
    }

    float value = 0.0f;
    if (!TryParseFloat(it->second, value)) {
        return std::nullopt;
    }
    return value;
}

std::string FormatFloatKey(float value) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.6g", value);
    return std::string(buffer);
}

float NormalizeColorComponent(uint8_t value) {
    return static_cast<float>(value) / 255.0f;
}

Vec3 ToRenderColor(const W3dRGBStruct& color) {
    return {
        NormalizeColorComponent(color.R),
        NormalizeColorComponent(color.G),
        NormalizeColorComponent(color.B)
    };
}

bool IsOpaqueBlend(uint8_t srcBlend, uint8_t destBlend) {
    return srcBlend == 1 && destBlend == 0;
}

void FinalizeRenderMaterial(RenderMaterial& material) {
    material.shininess = std::max(1.0f, material.shininess);
    material.opacity = std::clamp(material.opacity, 0.0f, 1.0f);
    material.translucency = std::clamp(material.translucency, 0.0f, 1.0f);

    if ((material.opacity < 0.999f || material.translucency > 0.001f)
        && IsOpaqueBlend(material.srcBlend, material.destBlend))
    {
        material.srcBlend = 2;  // SRC_ALPHA
        material.destBlend = 5; // ONE_MINUS_SRC_ALPHA
    }

    material.translucent =
        !IsOpaqueBlend(material.srcBlend, material.destBlend)
        || material.opacity < 0.999f
        || material.translucency > 0.001f;
}

struct LocalTextureSlot {
    int textureIndex = -1;
    bool alphaBitmap = false;
    bool clampU = false;
    bool clampV = false;
};

struct UvAnimationParams {
    uint8_t mode = 0; // 0=none, 1=scroll, 2=rotate
    float offsetU = 0.0f;
    float offsetV = 0.0f;
    float scrollU = 0.0f;
    float scrollV = 0.0f;
    float scaleU = 1.0f;
    float scaleV = 1.0f;
    float centerU = 0.0f;
    float centerV = 0.0f;
    float rotateRadPerSec = 0.0f;
};

UvAnimationParams ParseUvAnimationParams(const std::shared_ptr<ChunkItem>& materialRoot) {
    UvAnimationParams params{};
    if (!materialRoot) {
        return params;
    }

    std::vector<std::shared_ptr<ChunkItem>> vertexMaterials;
    CollectChunksByIdRecursive(materialRoot, kChunkVertexMaterial, vertexMaterials);
    if (vertexMaterials.empty()) {
        return params;
    }

    int primaryVertexMaterialId = -1;
    std::vector<std::shared_ptr<ChunkItem>> materialPassChunks;
    CollectChunksByIdRecursive(materialRoot, kChunkMaterialPass, materialPassChunks);
    if (!materialPassChunks.empty()) {
        if (const auto vertexMaterialIdsChunk = FindFirstChildById(materialPassChunks.front(), kChunkVertexMaterialIds)) {
            const auto parsedIds = ParseChunkArray<uint32_t>(vertexMaterialIdsChunk);
            if (const auto ids = std::get_if<std::vector<uint32_t>>(&parsedIds)) {
                if (!ids->empty()) {
                    primaryVertexMaterialId = static_cast<int>((*ids)[0]);
                }
            }
        }
    }

    int selectedMaterialIndex = 0;
    if (primaryVertexMaterialId >= 0
        && primaryVertexMaterialId < static_cast<int>(vertexMaterials.size())) {
        selectedMaterialIndex = primaryVertexMaterialId;
    }

    const auto& vertexMaterialChunk = vertexMaterials[static_cast<std::size_t>(selectedMaterialIndex)];
    if (!vertexMaterialChunk) {
        return params;
    }

    uint8_t stage0MappingCode = 0;
    if (const auto infoChunk = FindFirstChildById(vertexMaterialChunk, kChunkVertexMaterialInfo)) {
        const auto parsedInfo = ParseChunkStruct<W3dVertexMaterialStruct>(infoChunk);
        if (const auto info = std::get_if<W3dVertexMaterialStruct>(&parsedInfo)) {
            stage0MappingCode = ExtractStageMapping(info->Attributes, 0);
        }
    }

    std::string mapperArgsText;
    if (const auto mapperArgsChunk = FindFirstChildById(vertexMaterialChunk, kChunkVertexMapperArgs0)) {
        mapperArgsText = ReadNullTerminatedChunkString(mapperArgsChunk);
    }
    const auto argsByKey = ParseMapperArgs(mapperArgsText);

    if (const auto value = GetMapperArgFloat(argsByKey, "uoffset")) {
        params.offsetU = *value;
    }
    if (const auto value = GetMapperArgFloat(argsByKey, "voffset")) {
        params.offsetV = *value;
    }
    if (const auto value = GetMapperArgFloat(argsByKey, "uscale")) {
        params.scaleU = *value;
    }
    if (const auto value = GetMapperArgFloat(argsByKey, "vscale")) {
        params.scaleV = *value;
    }
    if (const auto value = GetMapperArgFloat(argsByKey, "ucenter")) {
        params.centerU = *value;
    }
    if (const auto value = GetMapperArgFloat(argsByKey, "vcenter")) {
        params.centerV = *value;
    }

    const auto uPerSec = GetMapperArgFloat(argsByKey, "upersec");
    const auto vPerSec = GetMapperArgFloat(argsByKey, "vpersec");
    const auto speedHz = GetMapperArgFloat(argsByKey, "speed");

    if (stage0MappingCode == kStageMappingRotate) {
        params.mode = 2;
        params.rotateRadPerSec = speedHz.has_value() ? (*speedHz * 6.28318530717958647692f) : 0.0f;
    }
    else if (stage0MappingCode == kStageMappingScreen || stage0MappingCode == kStageMappingLinearOffset) {
        params.mode = 1;
        params.scrollU = uPerSec.value_or(0.0f);
        params.scrollV = vPerSec.value_or(0.0f);
    }
    else if (speedHz.has_value()) {
        params.mode = 2;
        params.rotateRadPerSec = *speedHz * 6.28318530717958647692f;
    }
    else if (uPerSec.has_value() || vPerSec.has_value()) {
        params.mode = 1;
        params.scrollU = uPerSec.value_or(0.0f);
        params.scrollV = vPerSec.value_or(0.0f);
    }

    return params;
}

std::string ToUpperAscii(std::string input) {
    for (char& c : input) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return input;
}

uint32_t ComputeCRC32(const std::string& value) {
    static uint32_t table[256];
    static bool initialized = false;
    if (!initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                if (crc & 1U) {
                    crc = (crc >> 1) ^ 0xEDB88320U;
                }
                else {
                    crc >>= 1;
                }
            }
            table[i] = crc;
        }
        initialized = true;
    }

    uint32_t crc = 0xFFFFFFFFU;
    for (const unsigned char c : value) {
        crc = (crc >> 8) ^ table[(crc ^ c) & 0xFFU];
    }
    return crc ^ 0xFFFFFFFFU;
}

float ComputeRadiusFromBounds(const Vec3& min, const Vec3& max) {
    const Vec3 ext = {
        std::max(0.0f, (max.x - min.x) * 0.5f),
        std::max(0.0f, (max.y - min.y) * 0.5f),
        std::max(0.0f, (max.z - min.z) * 0.5f)
    };
    return std::sqrt(ext.x * ext.x + ext.y * ext.y + ext.z * ext.z);
}

float ScreenSizeToDistance(float maxScreenSize, std::size_t lodIndex) {
    if (maxScreenSize <= 0.0f) {
        return 250.0f + static_cast<float>(lodIndex) * 500.0f;
    }
    const float distance = 2100.0f / std::max(0.01f, maxScreenSize);
    return std::max(50.0f, distance);
}

struct HlodLodArray {
    float maxScreenSize = 0.0f;
    struct SubObjectRef {
        W3dHLodSubObjectStruct payload{};
        const ChunkItem* sourceChunk = nullptr;
    };
    std::vector<SubObjectRef> subObjects;
};

struct HlodDefinition {
    std::string name;
    std::string hierarchyName;
    bool sourceFromReferenceOnly = false;
    const ChunkItem* sourceChunk = nullptr;
    std::vector<HlodLodArray> lodArrays;
};

struct LodModelDefinition {
    std::string name;
    bool sourceFromReferenceOnly = false;
    struct EntryRef {
        W3dLODStruct payload{};
        const ChunkItem* sourceChunk = nullptr;
    };
    std::vector<EntryRef> entries;
};

struct HModelDefinition {
    std::string name;
    std::string hierarchyName;
    bool sourceFromReferenceOnly = false;
    struct NodeRef {
        std::string renderObjName;
        int pivotIndex = -1;
        bool skinned = false;
        const ChunkItem* sourceChunk = nullptr;
    };
    std::vector<NodeRef> nodes;
    const ChunkItem* sourceChunk = nullptr;
};

struct ParsedAnimationDefinition {
    RenderAnimationClip clip{};
    const ChunkItem* sourceHeaderChunk = nullptr;
};

struct AggregateDefinition {
    std::string name;
    std::string baseModelName;
    bool forceSubObjectLod = false;
    bool sourceFromReferenceOnly = false;
    struct SubObjectRef {
        std::string renderObjName;
        std::string boneName;
        const ChunkItem* sourceChunk = nullptr;
    };
    std::vector<SubObjectRef> subObjects;
    const ChunkItem* sourceChunk = nullptr;
};

struct ModelDefinitionLookup {
    std::unordered_map<std::string, std::size_t> hmodelByName;
    std::unordered_map<std::string, std::size_t> hmodelByHierarchyName;
    std::unordered_map<std::string, std::size_t> hlodByName;
    std::unordered_map<std::string, std::size_t> hlodByHierarchyName;
    std::unordered_map<std::string, std::size_t> lodModelByName;
    std::unordered_map<std::string, std::size_t> aggregateByName;
};

struct LockedLodRangeSlot {
    float minDistance = 0.0f;
    float maxDistance = std::numeric_limits<float>::max();
    float maxScreenSize = 0.0f;
};

struct BuildContext {
    SceneBuildOptions options{};
    SceneBuildResult result{};

    std::unordered_map<std::string, int> textureByName;
    std::unordered_map<std::string, int> materialByKey;
    std::unordered_map<std::string, int> meshByName;
    std::unordered_map<std::string, int> hierarchyByName;
    std::unordered_set<std::string> warnedMissingHierarchies;
    std::unordered_set<int> referencedMeshes;
    std::unordered_set<int> supplementalMeshes;
    std::unordered_set<int> referenceOnlyMeshes;
    std::unordered_set<std::string> externalTextureByPath;
    std::unordered_set<std::string> externalTextureByBaseName;
    std::unordered_set<uint32_t> externalTextureHashes;
    std::vector<std::filesystem::path> textureSearchRoots;
    std::unordered_map<std::string, std::string> recursiveTextureByRelativePath;
    std::unordered_map<std::string, std::string> recursiveTextureByBaseName;
    bool textureSearchRootsInitialized = false;
    bool recursiveTextureLookupBuilt = false;

    void InitializeExternalTextures() {
        externalTextureByPath.clear();
        externalTextureByBaseName.clear();
        externalTextureHashes.clear();
        externalTextureByPath.reserve(options.externalTextureNames.size());
        externalTextureByBaseName.reserve(options.externalTextureNames.size());
        externalTextureHashes.reserve(options.externalTextureHashes.size());

        for (const auto& name : options.externalTextureNames) {
            if (name.empty()) {
                continue;
            }
            externalTextureByPath.insert(NormalizePathKey(name));
            externalTextureByBaseName.insert(BaseNameKey(name));
        }
        for (const uint32_t hash : options.externalTextureHashes) {
            externalTextureHashes.insert(hash);
        }
    }

    void InitializeTextureSearchRoots() {
        if (textureSearchRootsInitialized) {
            return;
        }

        textureSearchRootsInitialized = true;
        if (!options.textureSearchDirectory.empty()) {
            textureSearchRoots.emplace_back(options.textureSearchDirectory);
        }
        for (const auto& directory : options.additionalTextureSearchDirectories) {
            if (directory.empty()) {
                continue;
            }
            const std::filesystem::path candidate(directory);
            if (std::find(textureSearchRoots.begin(), textureSearchRoots.end(), candidate)
                == textureSearchRoots.end())
            {
                textureSearchRoots.push_back(candidate);
            }
        }
        if (textureSearchRoots.empty()) {
            textureSearchRoots.push_back(std::filesystem::current_path());
        }
    }

    void BuildRecursiveTextureLookup() {
        if (recursiveTextureLookupBuilt) {
            return;
        }

        recursiveTextureLookupBuilt = true;
        InitializeTextureSearchRoots();

        for (const auto& root : textureSearchRoots) {
            std::error_code rootEc;
            if (!std::filesystem::exists(root, rootEc)
                || rootEc
                || !std::filesystem::is_directory(root, rootEc)
                || rootEc)
            {
                continue;
            }

            std::error_code iterEc;
            std::filesystem::recursive_directory_iterator it(
                root,
                std::filesystem::directory_options::skip_permission_denied,
                iterEc);
            const std::filesystem::recursive_directory_iterator end;
            while (!iterEc && it != end) {
                const std::filesystem::directory_entry& entry = *it;
                std::error_code entryEc;
                if (entry.is_regular_file(entryEc) && !entryEc) {
                    const std::filesystem::path path = entry.path();
                    if (IsSupportedTextureExtension(path.extension().string())) {
                        const std::string fullPath = path.string();
                        std::error_code relativeEc;
                        const std::filesystem::path relativePath =
                            std::filesystem::relative(path, root, relativeEc);
                        if (!relativeEc && !relativePath.empty()) {
                            recursiveTextureByRelativePath.emplace(
                                NormalizePathKey(relativePath.generic_string()),
                                fullPath);
                        }
                        recursiveTextureByBaseName.emplace(
                            BaseNameKey(path.filename().string()),
                            fullPath);
                    }
                }
                it.increment(iterEc);
            }
        }
    }

    std::string ResolveTexturePath(const std::string& textureName) {
        if (textureName.empty()) {
            return {};
        }

        static const std::array<const char*, 6> kTextureExtensions = {
            ".dds", ".tga", ".png", ".jpg", ".jpeg", ".bmp"
        };

        std::filesystem::path input(textureName);

        InitializeTextureSearchRoots();

        auto tryCandidate = [](const std::filesystem::path& candidate) -> std::string {
            if (FileExists(candidate)) {
                return candidate.string();
            }
            return {};
        };

        auto tryRelativePath = [&](const std::filesystem::path& base, const std::filesystem::path& relative) -> std::string {
            if (relative.empty()) {
                return {};
            }

            if (const std::string resolved = tryCandidate(base / relative); !resolved.empty()) {
                return resolved;
            }

            const std::filesystem::path fileName = relative.filename();
            if (!fileName.empty() && fileName != relative) {
                if (const std::string resolved = tryCandidate(base / fileName); !resolved.empty()) {
                    return resolved;
                }
            }

            return {};
        };

        std::vector<std::filesystem::path> explicitCandidates;
        explicitCandidates.push_back(input);
        if (input.has_extension()
            && NormalizeName(input.extension().string()) != std::string(".dds"))
        {
            std::filesystem::path ddsFallback = input;
            ddsFallback.replace_extension(".dds");
            if (ddsFallback != input) {
                explicitCandidates.push_back(std::move(ddsFallback));
            }
        }

        if (input.is_absolute()) {
            for (const auto& candidate : explicitCandidates) {
                if (const std::string resolved = tryCandidate(candidate); !resolved.empty()) {
                    return resolved;
                }
            }
        }

        if (input.has_extension()) {
            for (const auto& base : textureSearchRoots) {
                for (const auto& candidate : explicitCandidates) {
                    if (const std::string resolved = tryRelativePath(base, candidate); !resolved.empty()) {
                        return resolved;
                    }
                }
            }
        }
        else {
            for (const auto& base : textureSearchRoots) {
                for (const char* ext : kTextureExtensions) {
                    auto relative = input;
                    relative += ext;
                    if (const std::string resolved = tryRelativePath(base, relative); !resolved.empty()) {
                        return resolved;
                    }
                }
            }
        }

        BuildRecursiveTextureLookup();

        auto tryRecursiveLookup = [&](const std::string& keyText) -> std::string {
            const std::string relativeKey = NormalizePathKey(keyText);
            if (const auto found = recursiveTextureByRelativePath.find(relativeKey);
                found != recursiveTextureByRelativePath.end())
            {
                return found->second;
            }

            const std::string baseKey = BaseNameKey(relativeKey);
            if (const auto found = recursiveTextureByBaseName.find(baseKey);
                found != recursiveTextureByBaseName.end())
            {
                return found->second;
            }

            return {};
        };

        if (input.has_extension()) {
            for (const auto& candidate : explicitCandidates) {
                if (const std::string resolved = tryRecursiveLookup(candidate.generic_string());
                    !resolved.empty())
                {
                    return resolved;
                }
            }
            return {};
        }

        for (const char* ext : kTextureExtensions) {
            if (const std::string resolved = tryRecursiveLookup(textureName + ext); !resolved.empty()) {
                return resolved;
            }
        }

        return {};
    }

    bool HasExternalTexture(const std::string& textureName) const {
        static const std::array<const char*, 6> kTextureExtensions = {
            ".dds", ".tga", ".png", ".jpg", ".jpeg", ".bmp"
        };

        const std::string pathKey = NormalizePathKey(textureName);
        if (externalTextureByPath.contains(pathKey)) {
            return true;
        }
        const std::string baseKey = BaseNameKey(pathKey);
        if (externalTextureByBaseName.contains(baseKey)) {
            return true;
        }

        const std::filesystem::path normalizedPath(pathKey);
        const std::filesystem::path basePath(baseKey);
        const std::string stem = basePath.stem().string();
        for (const char* ext : kTextureExtensions) {
            if (!stem.empty() && externalTextureByBaseName.contains(stem + ext)) {
                return true;
            }
            if (!normalizedPath.empty() && normalizedPath.has_parent_path()) {
                const std::filesystem::path sibling = normalizedPath.parent_path() / (normalizedPath.stem().string() + ext);
                if (externalTextureByPath.contains(NormalizePathKey(sibling.generic_string()))) {
                    return true;
                }
            }
        }

        if (!externalTextureHashes.empty()) {
            auto hashMatchesExact = [&](const std::string& candidate) -> bool {
                if (candidate.empty()) {
                    return false;
                }
                if (externalTextureHashes.contains(ComputeCRC32(candidate))) {
                    return true;
                }
                if (externalTextureHashes.contains(ComputeCRC32(NormalizeName(candidate)))) {
                    return true;
                }
                if (externalTextureHashes.contains(ComputeCRC32(ToUpperAscii(candidate)))) {
                    return true;
                }
                return false;
            };

            auto hashMatches = [&](const std::string& candidate) -> bool {
                if (candidate.empty()) {
                    return false;
                }
                if (hashMatchesExact(candidate)) {
                    return true;
                }
                std::string slashVariant = candidate;
                std::replace(slashVariant.begin(), slashVariant.end(), '\\', '/');
                if (slashVariant != candidate && hashMatchesExact(slashVariant)) {
                    return true;
                }
                std::replace(slashVariant.begin(), slashVariant.end(), '/', '\\');
                if (slashVariant != candidate && hashMatchesExact(slashVariant)) {
                    return true;
                }
                return false;
            };

            if (hashMatches(pathKey) || hashMatches(baseKey)) {
                return true;
            }

            const std::string pathStem = normalizedPath.stem().string();
            for (const char* ext : kTextureExtensions) {
                if (hashMatches(stem + ext)) {
                    return true;
                }
                if (!pathStem.empty() && normalizedPath.has_parent_path()) {
                    const std::filesystem::path sibling = normalizedPath.parent_path() / (pathStem + ext);
                    if (hashMatches(NormalizePathKey(sibling.generic_string()))) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    int EnsureTexture(const std::string& textureName, const ChunkItem* sourceNode) {
        const std::string key = NormalizeName(textureName);
        const auto existing = textureByName.find(key);
        if (existing != textureByName.end()) {
            return existing->second;
        }

        RenderTexture texture{};
        texture.name = textureName;
        texture.resolvedPath = ResolveTexturePath(textureName);
        if (texture.resolvedPath.empty() && HasExternalTexture(textureName)) {
            texture.resolvedPath = "archive:" + textureName;
        }
        texture.resolved = !texture.resolvedPath.empty();

        const int index = static_cast<int>(result.scene.textures.size());
        result.scene.textures.push_back(texture);
        textureByName.emplace(key, index);

        if (!texture.resolved) {
            result.warnings.push_back({
                SceneBuildWarningCode::MissingTexture,
                BuildChunkPath(sourceNode),
                "Texture could not be resolved from search path: " + textureName
                });
        }
        return index;
    }

    int EnsureMaterial(const RenderMaterial& materialTemplate) {
        std::string key;
        key.reserve(320);
        key += std::to_string(materialTemplate.textureIndex);
        key += ':';
        key += (materialTemplate.alphaTest ? '1' : '0');
        key += ':';
        key += (materialTemplate.translucent ? '1' : '0');
        key += ':';
        key += (materialTemplate.texturingEnabled ? '1' : '0');
        key += ':';
        key += (materialTemplate.twoSided ? '1' : '0');
        key += ':';
        key += (materialTemplate.unlit ? '1' : '0');
        key += ':';
        key += (materialTemplate.clampU ? '1' : '0');
        key += ':';
        key += (materialTemplate.clampV ? '1' : '0');
        key += ':';
        key += std::to_string(materialTemplate.depthCompare);
        key += ':';
        key += (materialTemplate.depthWrite ? '1' : '0');
        key += ':';
        key += std::to_string(materialTemplate.srcBlend);
        key += ':';
        key += std::to_string(materialTemplate.destBlend);
        key += ':';
        key += std::to_string(materialTemplate.colorWriteMask);
        key += ':';
        key += FormatFloatKey(materialTemplate.diffuseColor.x);
        key += ':';
        key += FormatFloatKey(materialTemplate.diffuseColor.y);
        key += ':';
        key += FormatFloatKey(materialTemplate.diffuseColor.z);
        key += ':';
        key += FormatFloatKey(materialTemplate.diffuseColor.w);
        key += ':';
        key += FormatFloatKey(materialTemplate.ambientColor.x);
        key += ':';
        key += FormatFloatKey(materialTemplate.ambientColor.y);
        key += ':';
        key += FormatFloatKey(materialTemplate.ambientColor.z);
        key += ':';
        key += FormatFloatKey(materialTemplate.specularColor.x);
        key += ':';
        key += FormatFloatKey(materialTemplate.specularColor.y);
        key += ':';
        key += FormatFloatKey(materialTemplate.specularColor.z);
        key += ':';
        key += FormatFloatKey(materialTemplate.emissiveColor.x);
        key += ':';
        key += FormatFloatKey(materialTemplate.emissiveColor.y);
        key += ':';
        key += FormatFloatKey(materialTemplate.emissiveColor.z);
        key += ':';
        key += FormatFloatKey(materialTemplate.shininess);
        key += ':';
        key += FormatFloatKey(materialTemplate.opacity);
        key += ':';
        key += FormatFloatKey(materialTemplate.translucency);
        key += ':';
        key += std::to_string(materialTemplate.uvAnimMode);
        key += ':';
        key += FormatFloatKey(materialTemplate.uvOffsetU);
        key += ':';
        key += FormatFloatKey(materialTemplate.uvOffsetV);
        key += ':';
        key += FormatFloatKey(materialTemplate.uvScrollU);
        key += ':';
        key += FormatFloatKey(materialTemplate.uvScrollV);
        key += ':';
        key += FormatFloatKey(materialTemplate.uvScaleU);
        key += ':';
        key += FormatFloatKey(materialTemplate.uvScaleV);
        key += ':';
        key += FormatFloatKey(materialTemplate.uvCenterU);
        key += ':';
        key += FormatFloatKey(materialTemplate.uvCenterV);
        key += ':';
        key += FormatFloatKey(materialTemplate.uvRotateRadPerSec);

        const auto existing = materialByKey.find(key);
        if (existing != materialByKey.end()) {
            return existing->second;
        }

        const int index = static_cast<int>(result.scene.materials.size());
        result.scene.materials.push_back(materialTemplate);
        materialByKey.emplace(key, index);
        return index;
    }

    std::string LookupRootSourceLabel(const std::shared_ptr<ChunkItem>& root) const {
        if (!root) {
            return {};
        }
        const auto it = options.rootSourceLabels.find(root.get());
        if (it == options.rootSourceLabels.end()) {
            return {};
        }
        return it->second;
    }

    void WarnMissingHierarchy(
        const ChunkItem* sourceNode,
        const std::string& hierarchyName,
        const std::string& context)
    {
        if (hierarchyName.empty()) {
            return;
        }
        const std::string key = NormalizeName(hierarchyName);
        if (!warnedMissingHierarchies.insert(key + "|" + context).second) {
            return;
        }

        result.warnings.push_back({
            SceneBuildWarningCode::MissingHierarchy,
            BuildChunkPath(sourceNode),
            context + ": " + hierarchyName
            });
    }
};

int ResolveHierarchyIndex(BuildContext& ctx, const std::string& hierarchyName, const std::string& fallbackName);

std::optional<RenderMesh> BuildRenderMeshFromChunk(
    BuildContext& ctx,
    const std::shared_ptr<ChunkItem>& meshChunk)
{
    const auto headerChunk = FindFirstChildById(meshChunk, kChunkMeshHeader3);
    const auto verticesChunk = FindFirstChildById(meshChunk, kChunkVertices);
    const auto trisChunk = FindFirstChildById(meshChunk, kChunkTriangles);

    if (!headerChunk || !verticesChunk || !trisChunk) {
        ctx.result.warnings.push_back({
            SceneBuildWarningCode::MissingPayload,
            BuildChunkPath(meshChunk.get()),
            "Mesh is missing one or more required chunks (header/vertices/triangles)."
            });
        return std::nullopt;
    }

    const auto header = ParseStructWithWarning<W3dMeshHeader3Struct>(headerChunk, ctx.result.warnings);
    const auto vertices = ParseArrayWithWarning<W3dVectorStruct>(verticesChunk, ctx.result.warnings);
    const auto tris = ParseArrayWithWarning<W3dTriStruct>(trisChunk, ctx.result.warnings);
    if (!header || !vertices || !tris) {
        return std::nullopt;
    }

    const std::shared_ptr<ChunkItem> materialRoot = SelectMaterialSourceRoot(meshChunk);
    std::vector<std::shared_ptr<ChunkItem>> materialPassChunks;
    std::shared_ptr<ChunkItem> primaryMaterialPassChunk;
    std::shared_ptr<ChunkItem> primaryTextureStageChunk;
    if (materialRoot) {
        CollectChunksByIdRecursive(materialRoot, kChunkMaterialPass, materialPassChunks);
        if (!materialPassChunks.empty()) {
            primaryMaterialPassChunk = materialPassChunks.front();
            const auto textureStages = FindChildrenById(primaryMaterialPassChunk, kChunkTextureStage);
            if (!textureStages.empty()) {
                primaryTextureStageChunk = textureStages.front();
            }
        }
        if (!primaryTextureStageChunk) {
            std::vector<std::shared_ptr<ChunkItem>> textureStages;
            CollectChunksByIdRecursive(materialRoot, kChunkTextureStage, textureStages);
            if (!textureStages.empty()) {
                primaryTextureStageChunk = textureStages.front();
            }
        }
    }

    std::vector<W3dVectorStruct> normals;
    if (const auto normalsChunk = FindFirstChildById(meshChunk, kChunkVertexNormals)) {
        if (const auto parsed = ParseArrayWithWarning<W3dVectorStruct>(normalsChunk, ctx.result.warnings)) {
            normals = *parsed;
        }
    }

    std::vector<W3dVectorStruct> secondaryVertices;
    if (const auto secondaryVerticesChunk = FindFirstChildById(meshChunk, kChunkSecondaryVertices)) {
        if (const auto parsed = ParseArrayWithWarning<W3dVectorStruct>(secondaryVerticesChunk, ctx.result.warnings)) {
            secondaryVertices = *parsed;
        }
    }

    std::vector<W3dVectorStruct> secondaryNormals;
    if (const auto secondaryNormalsChunk = FindFirstChildById(meshChunk, kChunkSecondaryVertexNormals)) {
        if (const auto parsed = ParseArrayWithWarning<W3dVectorStruct>(secondaryNormalsChunk, ctx.result.warnings)) {
            secondaryNormals = *parsed;
        }
    }

    std::vector<W3dTexCoordStruct> texCoords;
    if (const auto texChunk = FindFirstChildById(meshChunk, kChunkTexCoords)) {
        if (const auto parsed = ParseArrayWithWarning<W3dTexCoordStruct>(texChunk, ctx.result.warnings)) {
            texCoords = *parsed;
        }
    }
    if (texCoords.empty() && primaryTextureStageChunk) {
        if (const auto stageTexChunk = FindFirstChildById(primaryTextureStageChunk, kChunkStageTexCoords)) {
            if (const auto parsed = ParseArrayWithWarning<W3dTexCoordStruct>(stageTexChunk, ctx.result.warnings)) {
                texCoords = *parsed;
            }
        }
    }
    if (texCoords.empty()) {
        std::vector<std::shared_ptr<ChunkItem>> stageTexCoordChunks;
        CollectChunksByIdRecursive(materialRoot ? materialRoot : meshChunk, kChunkStageTexCoords, stageTexCoordChunks);
        if (!stageTexCoordChunks.empty()) {
            if (const auto parsed = ParseArrayWithWarning<W3dTexCoordStruct>(stageTexCoordChunks.front(), ctx.result.warnings)) {
                texCoords = *parsed;
            }
        }
    }

    std::vector<LocalTextureSlot> localTextures;
    std::vector<std::shared_ptr<ChunkItem>> textureWrappers;
    CollectChunksByIdRecursive(materialRoot ? materialRoot : meshChunk, kChunkTextureWrapper, textureWrappers);
    localTextures.reserve(textureWrappers.size());
    for (const auto& textureWrapper : textureWrappers) {
        const auto texNameChunk = FindFirstChildById(textureWrapper, kChunkTextureName);
        if (!texNameChunk) {
            continue;
        }

        LocalTextureSlot slot{};
        const std::string texName = ReadNullTerminatedChunkString(texNameChunk);
        slot.textureIndex = ctx.EnsureTexture(texName, texNameChunk.get());
        if (const auto textureInfoChunk = FindFirstChildById(textureWrapper, kChunkTextureInfo)) {
            if (const auto parsed =
                ParseStructWithWarning<W3dTextureInfoStruct>(textureInfoChunk, ctx.result.warnings))
            {
                const uint16_t attributes = parsed->Attributes;
                slot.alphaBitmap =
                    (attributes & static_cast<uint16_t>(TextureAttr::ALPHA_BITMAP)) != 0;
                slot.clampU =
                    (attributes & static_cast<uint16_t>(TextureAttr::CLAMP_U)) != 0;
                slot.clampV =
                    (attributes & static_cast<uint16_t>(TextureAttr::CLAMP_V)) != 0;
            }
        }
        localTextures.push_back(slot);
    }

    std::vector<uint32_t> triTextureAssignments;
    {
        std::shared_ptr<ChunkItem> textureIdsChunk;
        if (primaryTextureStageChunk) {
            textureIdsChunk = FindFirstChildById(primaryTextureStageChunk, kChunkTextureIds);
        }
        if (!textureIdsChunk) {
            std::vector<std::shared_ptr<ChunkItem>> textureIdChunks;
            CollectChunksByIdRecursive(materialRoot ? materialRoot : meshChunk, kChunkTextureIds, textureIdChunks);
            if (!textureIdChunks.empty()) {
                textureIdsChunk = textureIdChunks.front();
            }
        }
        if (textureIdsChunk) {
            if (const auto ids = ParseArrayWithWarning<uint32_t>(textureIdsChunk, ctx.result.warnings)) {
                triTextureAssignments = *ids;
                if (!(triTextureAssignments.size() == 1 || triTextureAssignments.size() == tris->size())) {
                    ctx.result.warnings.push_back({
                        SceneBuildWarningCode::InvalidIndex,
                        BuildChunkPath(textureIdsChunk.get()),
                        "Texture ID count is neither one entry nor per-triangle; using first value only."
                        });
                    if (!triTextureAssignments.empty()) {
                        triTextureAssignments.resize(1);
                    }
                }
            }
        }
    }

    std::vector<W3dShaderStruct> localShaders;
    {
        std::vector<std::shared_ptr<ChunkItem>> shaderChunks;
        CollectChunksByIdRecursive(materialRoot ? materialRoot : meshChunk, kChunkShaders, shaderChunks);
        if (!shaderChunks.empty()) {
            if (const auto parsed =
                ParseArrayWithWarning<W3dShaderStruct>(shaderChunks.front(), ctx.result.warnings))
            {
                localShaders = *parsed;
            }
        }
    }

    std::vector<uint32_t> triShaderAssignments;
    {
        std::shared_ptr<ChunkItem> shaderIdsChunk;
        if (primaryMaterialPassChunk) {
            shaderIdsChunk = FindFirstChildById(primaryMaterialPassChunk, kChunkShaderIds);
        }
        if (!shaderIdsChunk) {
            std::vector<std::shared_ptr<ChunkItem>> shaderIdChunks;
            CollectChunksByIdRecursive(materialRoot ? materialRoot : meshChunk, kChunkShaderIds, shaderIdChunks);
            if (!shaderIdChunks.empty()) {
                shaderIdsChunk = shaderIdChunks.front();
            }
        }
        if (shaderIdsChunk) {
            if (const auto ids = ParseArrayWithWarning<uint32_t>(shaderIdsChunk, ctx.result.warnings)) {
                triShaderAssignments = *ids;
                if (!(triShaderAssignments.size() == 1 || triShaderAssignments.size() == tris->size())) {
                    ctx.result.warnings.push_back({
                        SceneBuildWarningCode::InvalidIndex,
                        BuildChunkPath(shaderIdsChunk.get()),
                        "Shader ID count is neither one entry nor per-triangle; using first value only."
                        });
                    if (!triShaderAssignments.empty()) {
                        triShaderAssignments.resize(1);
                    }
                }
            }
        }
    }

    W3dVertexMaterialStruct selectedVertexMaterial{};
    bool hasSelectedVertexMaterial = false;
    {
        std::vector<std::shared_ptr<ChunkItem>> vertexMaterials;
        CollectChunksByIdRecursive(materialRoot ? materialRoot : meshChunk, kChunkVertexMaterial, vertexMaterials);
        int selectedVertexMaterialIndex = 0;

        std::shared_ptr<ChunkItem> vertexMaterialIdsChunk;
        if (primaryMaterialPassChunk) {
            vertexMaterialIdsChunk = FindFirstChildById(primaryMaterialPassChunk, kChunkVertexMaterialIds);
        }
        if (vertexMaterialIdsChunk) {
            if (const auto ids =
                ParseArrayWithWarning<uint32_t>(vertexMaterialIdsChunk, ctx.result.warnings))
            {
                if (!ids->empty()) {
                    selectedVertexMaterialIndex = static_cast<int>((*ids)[0]);
                    const uint32_t firstId = (*ids)[0];
                    const bool hasVaryingIds = std::any_of(
                        ids->begin(),
                        ids->end(),
                        [firstId](uint32_t id) { return id != firstId; });
                    if (hasVaryingIds) {
                        ctx.result.warnings.push_back({
                            SceneBuildWarningCode::UnsupportedChunk,
                            BuildChunkPath(vertexMaterialIdsChunk.get()),
                            "Per-vertex material IDs vary; renderer is using the first vertex material for the whole mesh."
                            });
                    }
                }
            }
        }

        if (selectedVertexMaterialIndex >= 0
            && selectedVertexMaterialIndex < static_cast<int>(vertexMaterials.size()))
        {
            if (const auto infoChunk = FindFirstChildById(
                vertexMaterials[static_cast<std::size_t>(selectedVertexMaterialIndex)],
                kChunkVertexMaterialInfo))
            {
                if (const auto parsed =
                    ParseStructWithWarning<W3dVertexMaterialStruct>(infoChunk, ctx.result.warnings))
                {
                    selectedVertexMaterial = *parsed;
                    hasSelectedVertexMaterial = true;
                }
            }
        }
    }

    const auto headerValue = *header;
    const std::string meshName = ReadFixedString(headerValue.MeshName, W3D_NAME_LEN);
    const std::string containerName = ReadFixedString(headerValue.ContainerName, W3D_NAME_LEN);
    const bool geometryIsSkin =
        (headerValue.Attributes & static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_MASK))
        == static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_GEOMETRY_TYPE_SKIN);

    std::vector<W3dVertInfStruct> legacyInfluences;
    std::vector<W3dVertInf3WStruct> extendedInfluences;
    if (const auto influencesChunk = FindFirstChildById(meshChunk, kChunkVertexInfluencesExtended)) {
        if (const auto parsed = ParseArrayWithWarning<W3dVertInf3WStruct>(influencesChunk, ctx.result.warnings)) {
            extendedInfluences = *parsed;
        }
    }
    if (extendedInfluences.empty()) {
        if (const auto influencesChunk = FindFirstChildById(meshChunk, kChunkVertexInfluences)) {
            if (const auto parsed = ParseArrayWithWarning<W3dVertInfStruct>(influencesChunk, ctx.result.warnings)) {
                legacyInfluences = *parsed;
            }
        }
    }

    const bool skinned = !extendedInfluences.empty() || !legacyInfluences.empty();
    const uint8_t bonesPerVertex = !extendedInfluences.empty()
        ? 4u
        : ((!legacyInfluences.empty() || geometryIsSkin) ? 2u : 0u);

    std::string fullName = meshName;
    if (!containerName.empty()) {
        fullName = containerName + "." + meshName;
    }
    if (fullName.empty()) {
        fullName = "mesh_" + std::to_string(ctx.result.scene.meshes.size());
    }

    std::unordered_map<int, std::vector<uint32_t>> groupedIndices;
    groupedIndices.reserve(4);

    const bool twoSided =
        (headerValue.Attributes & static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_TWO_SIDED)) != 0;
    const bool hidden =
        (headerValue.Attributes & static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_HIDDEN)) != 0;
    const UvAnimationParams uvAnim = ParseUvAnimationParams(materialRoot ? materialRoot : meshChunk);

    RenderMaterial baseMaterial{};
    baseMaterial.name = fullName + "_mat";
    baseMaterial.twoSided = twoSided;
    baseMaterial.unlit = materialRoot && materialRoot->id == kChunkPrelitUnlit;
    baseMaterial.uvAnimMode = uvAnim.mode;
    baseMaterial.uvOffsetU = uvAnim.offsetU;
    baseMaterial.uvOffsetV = uvAnim.offsetV;
    baseMaterial.uvScrollU = uvAnim.scrollU;
    baseMaterial.uvScrollV = uvAnim.scrollV;
    baseMaterial.uvScaleU = uvAnim.scaleU;
    baseMaterial.uvScaleV = uvAnim.scaleV;
    baseMaterial.uvCenterU = uvAnim.centerU;
    baseMaterial.uvCenterV = uvAnim.centerV;
    baseMaterial.uvRotateRadPerSec = uvAnim.rotateRadPerSec;
    if (hasSelectedVertexMaterial) {
        baseMaterial.ambientColor = ToRenderColor(selectedVertexMaterial.Ambient);
        baseMaterial.diffuseColor = {
            NormalizeColorComponent(selectedVertexMaterial.Diffuse.R),
            NormalizeColorComponent(selectedVertexMaterial.Diffuse.G),
            NormalizeColorComponent(selectedVertexMaterial.Diffuse.B),
            1.0f
        };
        baseMaterial.specularColor = ToRenderColor(selectedVertexMaterial.Specular);
        baseMaterial.emissiveColor = ToRenderColor(selectedVertexMaterial.Emissive);
        baseMaterial.shininess = selectedVertexMaterial.Shininess;
        baseMaterial.opacity = selectedVertexMaterial.Opacity;
        baseMaterial.translucency = selectedVertexMaterial.Translucency;
    }

    auto resolveTextureForTriangle = [&](std::size_t triIndex) -> std::optional<LocalTextureSlot> {
        if (triTextureAssignments.empty()) {
            return std::nullopt;
        }
        uint32_t texId = triTextureAssignments[0];
        if (triTextureAssignments.size() > 1 && triIndex < triTextureAssignments.size()) {
            texId = triTextureAssignments[triIndex];
        }
        if (texId == 0xFFFFFFFFu) {
            return std::nullopt;
        }
        if (texId >= localTextures.size()) {
            ctx.result.warnings.push_back({
                SceneBuildWarningCode::InvalidIndex,
                BuildChunkPath(trisChunk.get()),
                "Triangle texture index is out of range."
                });
            return std::nullopt;
        }
        return localTextures[texId];
    };

    auto resolveShaderForTriangle = [&](std::size_t triIndex) -> const W3dShaderStruct* {
        if (triShaderAssignments.empty()) {
            return nullptr;
        }
        uint32_t shaderId = triShaderAssignments[0];
        if (triShaderAssignments.size() > 1 && triIndex < triShaderAssignments.size()) {
            shaderId = triShaderAssignments[triIndex];
        }
        if (shaderId == 0xFFFFFFFFu || shaderId >= localShaders.size()) {
            if (shaderId != 0xFFFFFFFFu) {
                ctx.result.warnings.push_back({
                    SceneBuildWarningCode::InvalidIndex,
                    BuildChunkPath(trisChunk.get()),
                    "Triangle shader index is out of range."
                    });
            }
            return nullptr;
        }
        return &localShaders[shaderId];
    };

    auto buildMaterialIndexForTriangle = [&](std::size_t triIndex) -> int {
        RenderMaterial material = baseMaterial;
        material.name = fullName + "_mat";

        if (const auto textureSlot = resolveTextureForTriangle(triIndex)) {
            material.textureIndex = textureSlot->textureIndex;
            material.alphaTest = material.alphaTest || textureSlot->alphaBitmap;
            material.clampU = textureSlot->clampU;
            material.clampV = textureSlot->clampV;
        }

        if (const W3dShaderStruct* shader = resolveShaderForTriangle(triIndex)) {
            material.depthCompare = shader->DepthCompare;
            material.depthWrite = shader->DepthMask != 0;
            material.srcBlend = shader->SrcBlend;
            material.destBlend = shader->DestBlend;
            material.alphaTest = material.alphaTest || shader->AlphaTest != 0;
            material.texturingEnabled = shader->Texturing != 0;
            const uint8_t writeMask = static_cast<uint8_t>(shader->ColorMask & 0x0F);
            material.colorWriteMask = (writeMask == 0) ? static_cast<uint8_t>(0x0F) : writeMask;
        }

        FinalizeRenderMaterial(material);
        return ctx.EnsureMaterial(material);
    };

    std::vector<RenderVertex> baseVertices;
    baseVertices.resize(vertices->size());
    for (std::size_t i = 0; i < vertices->size(); ++i) {
        const auto& v = (*vertices)[i];
        auto& dst = baseVertices[i];
        dst.position = { v.X, v.Y, v.Z };
        dst.secondaryPosition = dst.position;

        if (i < normals.size()) {
            const auto& n = normals[i];
            dst.normal = { n.X, n.Y, n.Z };
        }
        dst.secondaryNormal = dst.normal;

        if (i < secondaryVertices.size()) {
            const auto& sv = secondaryVertices[i];
            dst.secondaryPosition = { sv.X, sv.Y, sv.Z };
        }
        if (i < secondaryNormals.size()) {
            const auto& sn = secondaryNormals[i];
            dst.secondaryNormal = { sn.X, sn.Y, sn.Z };
        }

        if (i < texCoords.size()) {
            const auto& uv = texCoords[i];
            dst.uv = { uv.U, 1.0f - uv.V };
        }

        if (i < extendedInfluences.size()) {
            const auto& influence = extendedInfluences[i];
            for (int j = 0; j < 4; ++j) {
                dst.boneIndices[static_cast<std::size_t>(j)] = influence.BoneIdx[j];
            }
            dst.boneWeights[0] = static_cast<float>(influence.Weight[0]) / 65535.0f;
            dst.boneWeights[1] = static_cast<float>(influence.Weight[1]) / 65535.0f;
            dst.boneWeights[2] = static_cast<float>(influence.Weight[2]) / 65535.0f;
            dst.boneWeights[3] = static_cast<float>(DeriveVertInf3WWeight3(influence)) / 65535.0f;

            float weightSum = 0.0f;
            for (float weight : dst.boneWeights) {
                weightSum += std::max(0.0f, weight);
            }
            if (weightSum > 0.0f) {
                const float invWeightSum = 1.0f / weightSum;
                for (float& weight : dst.boneWeights) {
                    weight = std::max(0.0f, weight) * invWeightSum;
                }
            }
            else if (dst.boneIndices[0] != 0xFFFFu) {
                dst.boneWeights[0] = 1.0f;
            }
        }
        else if (i < legacyInfluences.size()) {
            const auto& influence = legacyInfluences[i];
            dst.boneIndices[0] = influence.BoneIdx[0];
            dst.boneIndices[1] = influence.BoneIdx[1];

            float w0 = static_cast<float>(influence.Weight[0]) / 100.0f;
            float w1 = static_cast<float>(influence.Weight[1]) / 100.0f;
            if (dst.boneIndices[0] == 0xFFFFu) {
                w0 = 0.0f;
            }
            if (dst.boneIndices[1] == 0xFFFFu) {
                w1 = 0.0f;
            }
            const float weightSum = std::max(0.0f, w0) + std::max(0.0f, w1);
            if (weightSum > 0.0f) {
                dst.boneWeights[0] = std::max(0.0f, w0) / weightSum;
                dst.boneWeights[1] = std::max(0.0f, w1) / weightSum;
            }
            else if (dst.boneIndices[0] != 0xFFFFu) {
                dst.boneWeights[0] = 1.0f;
            }
        }
    }

    std::vector<RenderVertex> renderVertices = baseVertices;

    for (std::size_t triIndex = 0; triIndex < tris->size(); ++triIndex) {
        const auto& tri = (*tris)[triIndex];

        const uint32_t a = tri.Vindex[0];
        const uint32_t b = tri.Vindex[1];
        const uint32_t c = tri.Vindex[2];
        if (a >= baseVertices.size() || b >= baseVertices.size() || c >= baseVertices.size()) {
            ctx.result.warnings.push_back({
                SceneBuildWarningCode::InvalidIndex,
                BuildChunkPath(trisChunk.get()),
                "Triangle has out-of-range vertex indices and was skipped."
                });
            continue;
        }

        const int materialIndex = buildMaterialIndexForTriangle(triIndex);
        auto& indices = groupedIndices[materialIndex];
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }

    if (groupedIndices.empty()) {
        return std::nullopt;
    }

    const Vec3 boundsMin = { headerValue.Min.X, headerValue.Min.Y, headerValue.Min.Z };
    const Vec3 boundsMax = { headerValue.Max.X, headerValue.Max.Y, headerValue.Max.Z };
    const Vec3 boundsCenter = { headerValue.SphCenter.X, headerValue.SphCenter.Y, headerValue.SphCenter.Z };

    std::optional<RenderMesh> primaryMesh;
    int groupSuffix = 0;
    for (const auto& pair : groupedIndices) {
        const int materialIndex = pair.first;
        const auto& grouped = pair.second;
        if (grouped.empty()) {
            continue;
        }

        RenderMesh subMesh{};
        if (groupedIndices.size() == 1 || groupSuffix == 0) {
            subMesh.fullName = fullName;
        }
        else {
            subMesh.fullName = fullName + "#" + std::to_string(groupSuffix);
        }
        subMesh.vertices = renderVertices;
        subMesh.indices = grouped;
        subMesh.twoSided = twoSided;
        subMesh.hidden = hidden;
        subMesh.boundsMin = boundsMin;
        subMesh.boundsMax = boundsMax;
        subMesh.boundsCenter = boundsCenter;
        subMesh.boundsRadius =
            (headerValue.SphRadius > 0.0f) ? headerValue.SphRadius : ComputeRadiusFromBounds(boundsMin, boundsMax);
        subMesh.sourceMeshHeaderChunk = headerChunk.get();
        subMesh.skinned = skinned;
        subMesh.hasSecondaryVertexStream = !secondaryVertices.empty();
        subMesh.bonesPerVertex = bonesPerVertex;
        subMesh.materialIndex = materialIndex;

        if (!primaryMesh.has_value()) {
            primaryMesh = subMesh;
        }
        else {
            const int meshIndex = static_cast<int>(ctx.result.scene.meshes.size());
            ctx.meshByName.emplace(NormalizeName(subMesh.fullName), meshIndex);
            ctx.result.scene.meshes.push_back(std::move(subMesh));
        }

        ++groupSuffix;
    }

    return primaryMesh;
}

void ParseHierarchies(const W3DChunk& roots, BuildContext& ctx) {
    std::vector<std::shared_ptr<ChunkItem>> hierarchyChunks;
    for (const auto& root : roots) {
        CollectChunksByIdRecursive(root, kChunkHierarchy, hierarchyChunks);
    }

    for (const auto& hierarchyChunk : hierarchyChunks) {
        if (!hierarchyChunk) {
            continue;
        }

        const auto headerChunk = FindFirstChildById(hierarchyChunk, kChunkHierarchyHeader);
        const auto pivotsChunk = FindFirstChildById(hierarchyChunk, kChunkPivots);
        if (!headerChunk || !pivotsChunk) {
            ctx.result.warnings.push_back({
                SceneBuildWarningCode::MissingPayload,
                BuildChunkPath(hierarchyChunk.get()),
                "Hierarchy chunk is missing header or pivots data."
                });
            continue;
        }

        const auto header = ParseStructWithWarning<W3dHierarchyStruct>(headerChunk, ctx.result.warnings);
        const auto pivots = ParseArrayWithWarning<W3dPivotStruct>(pivotsChunk, ctx.result.warnings);
        if (!header || !pivots) {
            continue;
        }

        RenderHierarchy hierarchy{};
        hierarchy.name = ReadFixedString(header->Name, W3D_NAME_LEN);
        hierarchy.sourceHierarchyChunk = hierarchyChunk.get();
        hierarchy.sourcePivotsChunk = pivotsChunk.get();
        hierarchy.pivots.reserve(pivots->size());

        for (std::size_t i = 0; i < pivots->size(); ++i) {
            const auto& src = (*pivots)[i];
            RenderPivot pivot{};
            pivot.name = ReadFixedString(src.Name, W3D_NAME_LEN);
            pivot.baseTranslation = { src.Translation.X, src.Translation.Y, src.Translation.Z };
            pivot.baseRotation = {
                src.Rotation.Q[0],
                src.Rotation.Q[1],
                src.Rotation.Q[2],
                src.Rotation.Q[3]
            };
            if (i == 0) {
                // Match TT HTreeClass semantics: pivot 0 is the object root, not a serialized base transform.
                pivot.parentIndex = -1;
                pivot.baseTranslation = { 0.0f, 0.0f, 0.0f };
                pivot.baseRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
                pivot.localTransform = Mat4::Identity();
            }
            else {
                pivot.parentIndex = (src.ParentIdx == 0xFFFFFFFFu)
                    ? -1
                    : static_cast<int>(src.ParentIdx);
                pivot.localTransform = TransformFromTranslationRotation(
                    { src.Translation.X, src.Translation.Y, src.Translation.Z },
                    src.Rotation.Q[0],
                    src.Rotation.Q[1],
                    src.Rotation.Q[2],
                    src.Rotation.Q[3]);
            }

            if (pivot.parentIndex >= static_cast<int>(pivots->size())) {
                ctx.result.warnings.push_back({
                    SceneBuildWarningCode::InvalidIndex,
                    BuildChunkPath(pivotsChunk.get()),
                    "Pivot parent index is out of range; clamping to root."
                    });
                pivot.parentIndex = -1;
            }

            hierarchy.pivots.push_back(pivot);
        }

        std::vector<uint8_t> state(hierarchy.pivots.size(), 0);
        std::function<void(int)> visit = [&](int i) {
            if (i < 0 || i >= static_cast<int>(hierarchy.pivots.size())) {
                return;
            }
            if (state[i] == 2) {
                return;
            }
            if (state[i] == 1) {
                ctx.result.warnings.push_back({
                    SceneBuildWarningCode::CyclicHierarchy,
                    BuildChunkPath(pivotsChunk.get()),
                    "Cycle detected in pivot parent chain; cycle edge removed."
                    });
                hierarchy.pivots[i].parentIndex = -1;
                state[i] = 2;
                return;
            }

            state[i] = 1;
            const int parent = hierarchy.pivots[i].parentIndex;
            visit(parent);
            state[i] = 2;
        };

        for (int i = 0; i < static_cast<int>(hierarchy.pivots.size()); ++i) {
            visit(i);
        }

        const int hierarchyIndex = static_cast<int>(ctx.result.scene.hierarchies.size());
        if (!hierarchy.name.empty()) {
            const std::string hierarchyKey = NormalizeName(hierarchy.name);
            if (!ctx.hierarchyByName.contains(hierarchyKey)) {
                ctx.hierarchyByName.emplace(hierarchyKey, hierarchyIndex);
            }
        }
        ctx.result.scene.hierarchies.push_back(std::move(hierarchy));
    }
}

float ReadWordAsFloat(uint32_t word) {
    float out = 0.0f;
    std::memcpy(&out, &word, sizeof(out));
    return out;
}

void AddFloatKeyframes(
    std::vector<RenderFloatKeyframe>& track,
    uint16_t firstFrame,
    const std::vector<float>& samples)
{
    track.reserve(track.size() + samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        RenderFloatKeyframe key{};
        key.frame = static_cast<float>(static_cast<uint32_t>(firstFrame) + static_cast<uint32_t>(i));
        key.value = samples[i];
        key.hold = false;
        track.push_back(key);
    }
}

void AddQuatKeyframes(
    std::vector<RenderQuatKeyframe>& track,
    uint16_t firstFrame,
    const std::vector<float>& samples)
{
    const std::size_t keyCount = samples.size() / 4u;
    track.reserve(track.size() + keyCount);
    for (std::size_t i = 0; i < keyCount; ++i) {
        const std::size_t base = i * 4u;
        RenderQuatKeyframe key{};
        key.frame = static_cast<float>(static_cast<uint32_t>(firstFrame) + static_cast<uint32_t>(i));
        key.value = {
            samples[base + 0],
            samples[base + 1],
            samples[base + 2],
            samples[base + 3]
        };
        key.hold = false;
        track.push_back(key);
    }
}

void AddTimeCodedFloatKeyframes(
    std::vector<RenderFloatKeyframe>& track,
    const ParsedTimeCodedAnimChannel& channel)
{
    const std::size_t packetSize = static_cast<std::size_t>(channel.vectorLen) + 1u;
    track.reserve(track.size() + channel.numTimeCodes);
    for (std::size_t i = 0; i < channel.numTimeCodes; ++i) {
        const std::size_t offset = i * packetSize;
        RenderFloatKeyframe key{};
        key.frame = static_cast<float>(channel.words[offset] & 0x7FFFFFFFu);
        key.value = (packetSize > 1u) ? ReadWordAsFloat(channel.words[offset + 1u]) : 0.0f;
        key.hold =
            (i + 1u < channel.numTimeCodes)
            && ((channel.words[offset + packetSize] & W3D_TIMECODED_BINARY_MOVEMENT_FLAG) != 0u);
        track.push_back(key);
    }
}

void AddTimeCodedQuatKeyframes(
    std::vector<RenderQuatKeyframe>& track,
    const ParsedTimeCodedAnimChannel& channel)
{
    const std::size_t packetSize = static_cast<std::size_t>(channel.vectorLen) + 1u;
    track.reserve(track.size() + channel.numTimeCodes);
    for (std::size_t i = 0; i < channel.numTimeCodes; ++i) {
        const std::size_t offset = i * packetSize;
        RenderQuatKeyframe key{};
        key.frame = static_cast<float>(channel.words[offset] & 0x7FFFFFFFu);
        key.value = {
            (packetSize > 1u) ? ReadWordAsFloat(channel.words[offset + 1u]) : 0.0f,
            (packetSize > 2u) ? ReadWordAsFloat(channel.words[offset + 2u]) : 0.0f,
            (packetSize > 3u) ? ReadWordAsFloat(channel.words[offset + 3u]) : 0.0f,
            (packetSize > 4u) ? ReadWordAsFloat(channel.words[offset + 4u]) : 1.0f
        };
        key.hold =
            (i + 1u < channel.numTimeCodes)
            && ((channel.words[offset + packetSize] & W3D_TIMECODED_BINARY_MOVEMENT_FLAG) != 0u);
        track.push_back(key);
    }
}

void AddRawAnimationChannel(
    RenderAnimationClip& clip,
    const ParsedRawAnimChannel& channel,
    const ChunkItem* sourceChunk,
    BuildContext& ctx)
{
    if (channel.pivot >= clip.pivots.size()) {
        clip.pivots.resize(static_cast<std::size_t>(channel.pivot) + 1u);
    }
    auto& pivot = clip.pivots[static_cast<std::size_t>(channel.pivot)];

    switch (channel.flags) {
    case 0:
        if (channel.vectorLen >= 1u) {
            AddFloatKeyframes(pivot.translationX, channel.firstFrame, channel.data);
        }
        break;
    case 1:
        if (channel.vectorLen >= 1u) {
            AddFloatKeyframes(pivot.translationY, channel.firstFrame, channel.data);
        }
        break;
    case 2:
        if (channel.vectorLen >= 1u) {
            AddFloatKeyframes(pivot.translationZ, channel.firstFrame, channel.data);
        }
        break;
    case 6:
        if (channel.vectorLen >= 4u) {
            AddQuatKeyframes(pivot.rotation, channel.firstFrame, channel.data);
        }
        break;
    default:
        ctx.result.warnings.push_back({
            SceneBuildWarningCode::UnsupportedChunk,
            BuildChunkPath(sourceChunk),
            "Animation channel type is not supported for rendering: " + std::to_string(channel.flags)
            });
        break;
    }
}

void AddCompressedAnimationChannel(
    RenderAnimationClip& clip,
    const ParsedTimeCodedAnimChannel& channel,
    const ChunkItem* sourceChunk,
    BuildContext& ctx)
{
    if (channel.pivot >= clip.pivots.size()) {
        clip.pivots.resize(static_cast<std::size_t>(channel.pivot) + 1u);
    }
    auto& pivot = clip.pivots[static_cast<std::size_t>(channel.pivot)];

    switch (channel.flags) {
    case 0:
        if (channel.vectorLen >= 1u) {
            AddTimeCodedFloatKeyframes(pivot.translationX, channel);
        }
        break;
    case 1:
        if (channel.vectorLen >= 1u) {
            AddTimeCodedFloatKeyframes(pivot.translationY, channel);
        }
        break;
    case 2:
        if (channel.vectorLen >= 1u) {
            AddTimeCodedFloatKeyframes(pivot.translationZ, channel);
        }
        break;
    case 6:
        if (channel.vectorLen >= 4u) {
            AddTimeCodedQuatKeyframes(pivot.rotation, channel);
        }
        break;
    default:
        ctx.result.warnings.push_back({
            SceneBuildWarningCode::UnsupportedChunk,
            BuildChunkPath(sourceChunk),
            "Compressed animation channel type is not supported for rendering: " + std::to_string(channel.flags)
            });
        break;
    }
}

std::vector<ParsedAnimationDefinition> ParseAnimationDefinitions(
    const W3DChunk& roots,
    BuildContext& ctx,
    bool sourceFromAnimationLibrary)
{
    std::vector<std::shared_ptr<ChunkItem>> rawAnimationChunks;
    std::vector<std::shared_ptr<ChunkItem>> compressedAnimationChunks;

    std::vector<ParsedAnimationDefinition> out;
    for (const auto& root : roots) {
        if (!root) {
            continue;
        }

        rawAnimationChunks.clear();
        compressedAnimationChunks.clear();
        CollectChunksByIdRecursive(root, kChunkAnimation, rawAnimationChunks);
        CollectChunksByIdRecursive(root, kChunkCompressedAnimation, compressedAnimationChunks);

        const std::string sourceFileLabel = ctx.LookupRootSourceLabel(root);

        for (const auto& animationChunk : rawAnimationChunks) {
            const auto headerChunk = FindFirstChildById(animationChunk, kChunkAnimationHeader);
            if (!headerChunk) {
                continue;
            }
            const auto header = ParseStructWithWarning<W3dAnimHeaderStruct>(headerChunk, ctx.result.warnings);
            if (!header) {
                continue;
            }

            ParsedAnimationDefinition def{};
            def.clip.fullName = ReadFixedString(header->Name, W3D_NAME_LEN);
            def.clip.hierarchyName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
            if (!def.clip.hierarchyName.empty() && !def.clip.fullName.empty()) {
                def.clip.fullName = def.clip.hierarchyName + "." + def.clip.fullName;
            }
            def.clip.sourceFileLabel = sourceFileLabel;
            def.clip.numFrames = header->NumFrames;
            def.clip.frameRate = static_cast<float>(header->FrameRate);
            def.clip.compressed = false;
            def.clip.supportedForPlayback = true;
            def.clip.sourceFromAnimationLibrary = sourceFromAnimationLibrary;
            def.clip.sourceAnimationChunk = animationChunk.get();
            def.sourceHeaderChunk = headerChunk.get();

            const auto channels = FindChildrenById(animationChunk, kChunkAnimationChannel);
            for (const auto& channelChunk : channels) {
                const auto parsed = ParseRawAnimChannel(channelChunk, ctx.result.warnings);
                if (parsed) {
                    AddRawAnimationChannel(def.clip, *parsed, channelChunk.get(), ctx);
                }
            }

            out.push_back(std::move(def));
        }

        for (const auto& animationChunk : compressedAnimationChunks) {
            const auto headerChunk = FindFirstChildById(animationChunk, kChunkCompressedAnimationHeader);
            if (!headerChunk) {
                continue;
            }
            const auto header =
                ParseStructWithWarning<W3dCompressedAnimHeaderStruct>(headerChunk, ctx.result.warnings);
            if (!header) {
                continue;
            }

            ParsedAnimationDefinition def{};
            def.clip.fullName = ReadFixedString(header->Name, W3D_NAME_LEN);
            def.clip.hierarchyName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
            if (!def.clip.hierarchyName.empty() && !def.clip.fullName.empty()) {
                def.clip.fullName = def.clip.hierarchyName + "." + def.clip.fullName;
            }
            def.clip.sourceFileLabel = sourceFileLabel;
            def.clip.numFrames = header->NumFrames;
            def.clip.frameRate = static_cast<float>(header->FrameRate);
            def.clip.compressed = true;
            def.clip.supportedForPlayback = true;
            def.clip.sourceFromAnimationLibrary = sourceFromAnimationLibrary;
            def.clip.sourceAnimationChunk = animationChunk.get();
            def.sourceHeaderChunk = headerChunk.get();

            if (header->Flavor == 1u) {
                def.clip.supportedForPlayback = false;
                ctx.result.warnings.push_back({
                    SceneBuildWarningCode::UnsupportedChunk,
                    BuildChunkPath(animationChunk.get()),
                    "Adaptive-delta compressed animation is not supported for rendering yet."
                    });
                out.push_back(std::move(def));
                continue;
            }

            const auto channels = FindChildrenById(animationChunk, kChunkCompressedAnimationChannel);
            for (const auto& channelChunk : channels) {
                const auto parsed = ParseTimeCodedAnimChannel(channelChunk, ctx.result.warnings);
                if (parsed) {
                    AddCompressedAnimationChannel(def.clip, *parsed, channelChunk.get(), ctx);
                }
            }

            if (FindFirstChildById(animationChunk, kChunkCompressedAnimationAdaptiveDeltaChannel)) {
                def.clip.supportedForPlayback = false;
                ctx.result.warnings.push_back({
                    SceneBuildWarningCode::UnsupportedChunk,
                    BuildChunkPath(animationChunk.get()),
                    "Adaptive-delta animation channels are not supported for rendering yet."
                    });
            }

            out.push_back(std::move(def));
        }
    }

    return out;
}

std::vector<HModelDefinition> ParseHModelDefinitions(
    const W3DChunk& roots,
    BuildContext& ctx,
    bool sourceFromReferenceOnly)
{
    std::vector<std::shared_ptr<ChunkItem>> hmodelChunks;
    for (const auto& root : roots) {
        CollectChunksByIdRecursive(root, kChunkHModel, hmodelChunks);
    }

    std::vector<HModelDefinition> out;
    out.reserve(hmodelChunks.size());
    for (const auto& hmodelChunk : hmodelChunks) {
        if (!hmodelChunk) {
            continue;
        }

        const auto headerChunk = FindFirstChildById(hmodelChunk, kChunkHModelHeader);
        if (!headerChunk) {
            continue;
        }
        const auto header = ParseStructWithWarning<W3dHModelHeaderStruct>(headerChunk, ctx.result.warnings);
        if (!header) {
            continue;
        }

        HModelDefinition def{};
        def.name = ReadFixedString(header->Name, W3D_NAME_LEN);
        def.hierarchyName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
        def.sourceFromReferenceOnly = sourceFromReferenceOnly;
        def.sourceChunk = hmodelChunk.get();

        for (const auto& child : hmodelChunk->children) {
            if (!child) {
                continue;
            }
            if (child->id != kChunkHModelNode && child->id != kChunkHModelSkinNode) {
                continue;
            }
            const auto parsedNode = ParseStructWithWarning<W3dHModelNodeStruct>(child, ctx.result.warnings);
            if (!parsedNode) {
                continue;
            }

            HModelDefinition::NodeRef node{};
            node.renderObjName = ReadFixedString(parsedNode->RenderObjName, W3D_NAME_LEN);
            if (!def.name.empty() && node.renderObjName.find('.') == std::string::npos) {
                node.renderObjName = def.name + "." + node.renderObjName;
            }
            node.pivotIndex = static_cast<int>(parsedNode->PivotIdx);
            node.skinned = (child->id == kChunkHModelSkinNode);
            node.sourceChunk = child.get();
            def.nodes.push_back(std::move(node));
        }

        if (!def.nodes.empty()) {
            out.push_back(std::move(def));
        }
    }

    return out;
}

void AssignAnimationsToHierarchies(
    BuildContext& ctx,
    const std::vector<ParsedAnimationDefinition>& animations)
{
    for (const auto& animation : animations) {
        const int animationIndex = static_cast<int>(ctx.result.scene.animations.size());
        ctx.result.scene.animations.push_back(animation.clip);

        if (animation.clip.hierarchyName.empty()) {
            continue;
        }

        const auto hierarchyIt = ctx.hierarchyByName.find(NormalizeName(animation.clip.hierarchyName));
        if (hierarchyIt == ctx.hierarchyByName.end()) {
            ctx.WarnMissingHierarchy(
                animation.sourceHeaderChunk,
                animation.clip.hierarchyName,
                "Animation hierarchy was not found");
            continue;
        }

        RenderHierarchy& hierarchy =
            ctx.result.scene.hierarchies[static_cast<std::size_t>(hierarchyIt->second)];
        hierarchy.compatibleAnimationIndices.push_back(animationIndex);
    }
}

int ResolvePivotIndexByName(const RenderHierarchy& hierarchy, const std::string& pivotName)
{
    const std::string key = NormalizeName(pivotName);
    if (key.empty()) {
        return -1;
    }

    for (int pivotIndex = 0; pivotIndex < static_cast<int>(hierarchy.pivots.size()); ++pivotIndex) {
        if (NormalizeName(hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].name) == key) {
            return pivotIndex;
        }
    }
    return -1;
}

const HModelDefinition* FindHModelByName(
    const std::vector<HModelDefinition>& defs,
    const ModelDefinitionLookup& lookup,
    const std::string& name,
    bool allowHierarchyFallback)
{
    const std::string key = NormalizeName(name);
    if (key.empty()) {
        return nullptr;
    }

    if (const auto it = lookup.hmodelByName.find(key); it != lookup.hmodelByName.end()) {
        return &defs[it->second];
    }
    if (allowHierarchyFallback) {
        if (const auto it = lookup.hmodelByHierarchyName.find(key); it != lookup.hmodelByHierarchyName.end()) {
            return &defs[it->second];
        }
    }
    return nullptr;
}

const HlodDefinition* FindHLodByName(
    const std::vector<HlodDefinition>& defs,
    const ModelDefinitionLookup& lookup,
    const std::string& name,
    bool allowHierarchyFallback)
{
    const std::string key = NormalizeName(name);
    if (key.empty()) {
        return nullptr;
    }

    if (const auto it = lookup.hlodByName.find(key); it != lookup.hlodByName.end()) {
        return &defs[it->second];
    }
    if (allowHierarchyFallback) {
        if (const auto it = lookup.hlodByHierarchyName.find(key); it != lookup.hlodByHierarchyName.end()) {
            return &defs[it->second];
        }
    }
    return nullptr;
}

const LodModelDefinition* FindLodModelByName(
    const std::vector<LodModelDefinition>& defs,
    const ModelDefinitionLookup& lookup,
    const std::string& name)
{
    const std::string key = NormalizeName(name);
    if (key.empty()) {
        return nullptr;
    }

    if (const auto it = lookup.lodModelByName.find(key); it != lookup.lodModelByName.end()) {
        return &defs[it->second];
    }
    return nullptr;
}

const AggregateDefinition* FindAggregateByName(
    const std::vector<AggregateDefinition>& defs,
    const ModelDefinitionLookup& lookup,
    const std::string& name)
{
    const std::string key = NormalizeName(name);
    if (key.empty()) {
        return nullptr;
    }

    if (const auto it = lookup.aggregateByName.find(key); it != lookup.aggregateByName.end()) {
        return &defs[it->second];
    }
    return nullptr;
}

void AddMeshNodeInstance(
    BuildContext& ctx,
    const std::string& nodeName,
    int meshIndex,
    int hierarchyIndex,
    int pivotIndex,
    const ChunkItem* sourceChunk,
    const Mat4& localTransform)
{
    RenderNode node{};
    node.name = nodeName;
    node.meshIndex = meshIndex;
    node.hierarchyIndex = hierarchyIndex;
    node.pivotIndex = pivotIndex;
    node.localTransform = localTransform;
    node.sourceBindingChunk = sourceChunk;
    ctx.referencedMeshes.insert(meshIndex);
    ctx.result.scene.looseNodes.push_back(std::move(node));
}

void AppendNodesFromHModelDefinition(
    BuildContext& ctx,
    const HModelDefinition& def,
    int hierarchyIndex)
{
    for (const auto& source : def.nodes) {
        const auto meshIt = ctx.meshByName.find(NormalizeName(source.renderObjName));
        if (meshIt == ctx.meshByName.end()) {
            ctx.result.warnings.push_back({
                SceneBuildWarningCode::InvalidIndex,
                BuildChunkPath(source.sourceChunk),
                "Referenced HModel mesh not found: " + source.renderObjName
                });
            continue;
        }

        int pivotIndex = -1;
        const RenderMesh& mesh =
            ctx.result.scene.meshes[static_cast<std::size_t>(meshIt->second)];
        if (!mesh.skinned) {
            pivotIndex = source.pivotIndex;
            if (hierarchyIndex >= 0
                && hierarchyIndex < static_cast<int>(ctx.result.scene.hierarchies.size())) {
                const auto& pivots =
                    ctx.result.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)].pivots;
                if (pivotIndex < 0 || pivotIndex >= static_cast<int>(pivots.size())) {
                    ctx.result.warnings.push_back({
                        SceneBuildWarningCode::InvalidIndex,
                        BuildChunkPath(source.sourceChunk),
                        "HModel pivot index is out of range for hierarchy: " + std::to_string(pivotIndex)
                        });
                    pivotIndex = -1;
                }
            }
            else {
                pivotIndex = -1;
            }
        }

        AddMeshNodeInstance(
            ctx,
            source.renderObjName,
            meshIt->second,
            hierarchyIndex,
            pivotIndex,
            source.sourceChunk,
            Mat4::Identity());
    }
}

void BuildNodesFromHModelDefinitions(
    BuildContext& ctx,
    const std::vector<HModelDefinition>& defs)
{
    for (const auto& def : defs) {
        if (def.sourceFromReferenceOnly) {
            continue;
        }

        const int hierarchyIndex = ResolveHierarchyIndex(ctx, def.hierarchyName, def.name);
        if (hierarchyIndex < 0) {
            ctx.WarnMissingHierarchy(
                def.sourceChunk,
                !def.hierarchyName.empty() ? def.hierarchyName : def.name,
                "HModel hierarchy was not found");
        }

        AppendNodesFromHModelDefinition(ctx, def, hierarchyIndex);
    }
}

std::vector<HlodDefinition> ParseHLodDefinitions(
    const W3DChunk& roots,
    BuildContext& ctx,
    bool sourceFromReferenceOnly)
{
    std::vector<std::shared_ptr<ChunkItem>> hlodChunks;
    for (const auto& root : roots) {
        CollectChunksByIdRecursive(root, kChunkHLod, hlodChunks);
    }

    std::vector<HlodDefinition> out;
    out.reserve(hlodChunks.size());

    for (const auto& hlodChunk : hlodChunks) {
        if (!hlodChunk) {
            continue;
        }

        const auto headerChunk = FindFirstChildById(hlodChunk, kChunkHLodHeader);
        if (!headerChunk) {
            ctx.result.warnings.push_back({
                SceneBuildWarningCode::MissingPayload,
                BuildChunkPath(hlodChunk.get()),
                "HLOD chunk is missing HLOD_HEADER."
                });
            continue;
        }

        const auto header = ParseStructWithWarning<W3dHLodHeaderStruct>(headerChunk, ctx.result.warnings);
        if (!header) {
            continue;
        }

        HlodDefinition def{};
        def.name = ReadFixedString(header->Name, W3D_NAME_LEN);
        def.hierarchyName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
        def.sourceFromReferenceOnly = sourceFromReferenceOnly;
        def.sourceChunk = headerChunk.get();

        const auto lodArrays = FindChildrenById(hlodChunk, kChunkHLodLodArray);
        for (const auto& lodArrayChunk : lodArrays) {
            if (!lodArrayChunk) {
                continue;
            }

            HlodLodArray lodArray{};
            if (const auto arrayHeaderChunk = FindFirstChildById(lodArrayChunk, kChunkHLodSubObjectArrayHeader)) {
                if (const auto arrayHeader = ParseStructWithWarning<W3dHLodArrayHeaderStruct>(arrayHeaderChunk, ctx.result.warnings)) {
                    lodArray.maxScreenSize = arrayHeader->MaxScreenSize;
                }
            }

            const auto subObjects = FindChildrenById(lodArrayChunk, kChunkHLodSubObject);
            for (const auto& subObjectChunk : subObjects) {
                if (!subObjectChunk) {
                    continue;
                }
                const auto parsedSub = ParseStructWithWarning<W3dHLodSubObjectStruct>(subObjectChunk, ctx.result.warnings);
                if (!parsedSub) {
                    continue;
                }
                HlodLodArray::SubObjectRef subRef{};
                subRef.payload = *parsedSub;
                subRef.sourceChunk = subObjectChunk.get();
                lodArray.subObjects.push_back(std::move(subRef));
            }

            if (!lodArray.subObjects.empty()) {
                def.lodArrays.push_back(std::move(lodArray));
            }
        }

        if (!def.lodArrays.empty()) {
            out.push_back(std::move(def));
        }
    }

    return out;
}

std::vector<LodModelDefinition> ParseLodModelDefinitions(
    const W3DChunk& roots,
    BuildContext& ctx,
    bool sourceFromReferenceOnly)
{
    std::vector<std::shared_ptr<ChunkItem>> lodModelChunks;
    for (const auto& root : roots) {
        CollectChunksByIdRecursive(root, kChunkLodModel, lodModelChunks);
    }

    std::vector<LodModelDefinition> out;
    out.reserve(lodModelChunks.size());

    for (const auto& lodModelChunk : lodModelChunks) {
        const auto headerChunk = FindFirstChildById(lodModelChunk, kChunkLodModelHeader);
        if (!headerChunk) {
            continue;
        }

        const auto header = ParseStructWithWarning<W3dLODModelHeaderStruct>(headerChunk, ctx.result.warnings);
        if (!header) {
            continue;
        }

        LodModelDefinition def{};
        def.name = ReadFixedString(header->Name, W3D_NAME_LEN);
        def.sourceFromReferenceOnly = sourceFromReferenceOnly;

        const auto lodEntries = FindChildrenById(lodModelChunk, kChunkLodModelLod);
        for (const auto& lodEntryChunk : lodEntries) {
            if (!lodEntryChunk) {
                continue;
            }
            const auto parsed = ParseStructWithWarning<W3dLODStruct>(lodEntryChunk, ctx.result.warnings);
            if (parsed) {
                LodModelDefinition::EntryRef entry{};
                entry.payload = *parsed;
                entry.sourceChunk = lodEntryChunk.get();
                def.entries.push_back(std::move(entry));
            }
        }

        if (!def.entries.empty()) {
            out.push_back(std::move(def));
        }
    }

    return out;
}

std::vector<AggregateDefinition> ParseAggregateDefinitions(
    const W3DChunk& roots,
    BuildContext& ctx,
    bool sourceFromReferenceOnly)
{
    std::vector<std::shared_ptr<ChunkItem>> aggregateChunks;
    for (const auto& root : roots) {
        CollectChunksByIdRecursive(root, kChunkAggregate, aggregateChunks);
    }

    std::vector<AggregateDefinition> out;
    out.reserve(aggregateChunks.size());

    for (const auto& aggregateChunk : aggregateChunks) {
        if (!aggregateChunk) {
            continue;
        }

        const auto headerChunk = FindFirstChildById(aggregateChunk, kChunkAggregateHeader);
        const auto infoChunk = FindFirstChildById(aggregateChunk, kChunkAggregateInfo);
        if (!headerChunk || !infoChunk) {
            ctx.result.warnings.push_back({
                SceneBuildWarningCode::MissingPayload,
                BuildChunkPath(aggregateChunk.get()),
                "Aggregate chunk is missing AGGREGATE_HEADER or AGGREGATE_INFO."
                });
            continue;
        }

        const auto header =
            ParseStructWithWarning<W3dAggregateHeaderStruct>(headerChunk, ctx.result.warnings);
        const auto info =
            ParseStructWithWarning<W3dAggregateInfoStruct>(infoChunk, ctx.result.warnings);
        if (!header || !info) {
            continue;
        }

        AggregateDefinition def{};
        def.name = ReadFixedString(header->Name, W3D_NAME_LEN);
        def.baseModelName = ReadFixedString(info->BaseModelName, W3D_NAME_LEN * 2u);
        def.sourceFromReferenceOnly = sourceFromReferenceOnly;
        def.sourceChunk = aggregateChunk.get();

        if (const auto classInfoChunk = FindFirstChildById(aggregateChunk, kChunkAggregateClassInfo)) {
            if (const auto classInfo =
                ParseStructWithWarning<W3dAggregateMiscInfo>(classInfoChunk, ctx.result.warnings))
            {
                def.forceSubObjectLod =
                    (classInfo->Flags & static_cast<uint32_t>(W3D_AGGREGATE_FORCE_SUB_OBJ_LOD)) != 0u;
            }
        }

        const std::size_t entrySize = sizeof(W3dAggregateSubobjectStruct);
        const std::size_t headerSize = sizeof(W3dAggregateInfoStruct);
        if (infoChunk->data.size() < headerSize) {
            continue;
        }
        const std::size_t availableEntries = (infoChunk->data.size() - headerSize) / entrySize;
        const std::size_t count = std::min<std::size_t>(
            static_cast<std::size_t>(info->SubobjectCount),
            availableEntries);
        const auto* subObjects = reinterpret_cast<const W3dAggregateSubobjectStruct*>(
            infoChunk->data.data() + headerSize);

        def.subObjects.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            AggregateDefinition::SubObjectRef subObject{};
            subObject.renderObjName =
                ReadFixedString(subObjects[i].SubobjectName, W3D_NAME_LEN * 2u);
            subObject.boneName =
                ReadFixedString(subObjects[i].BoneName, W3D_NAME_LEN * 2u);
            subObject.sourceChunk = infoChunk.get();
            def.subObjects.push_back(std::move(subObject));
        }

        out.push_back(std::move(def));
    }

    return out;
}

ModelDefinitionLookup BuildModelDefinitionLookup(
    const std::vector<HModelDefinition>& hmodelDefs,
    const std::vector<HlodDefinition>& hlodDefs,
    const std::vector<LodModelDefinition>& lodModelDefs,
    const std::vector<AggregateDefinition>& aggregateDefs)
{
    ModelDefinitionLookup lookup{};

    auto addUnique = [](auto& map, const std::string& key, std::size_t index) {
        if (key.empty()) {
            return;
        }
        map.emplace(key, index);
    };

    for (std::size_t i = 0; i < hmodelDefs.size(); ++i) {
        addUnique(lookup.hmodelByName, NormalizeName(hmodelDefs[i].name), i);
        addUnique(lookup.hmodelByHierarchyName, NormalizeName(hmodelDefs[i].hierarchyName), i);
    }
    for (std::size_t i = 0; i < hlodDefs.size(); ++i) {
        addUnique(lookup.hlodByName, NormalizeName(hlodDefs[i].name), i);
        addUnique(lookup.hlodByHierarchyName, NormalizeName(hlodDefs[i].hierarchyName), i);
    }
    for (std::size_t i = 0; i < lodModelDefs.size(); ++i) {
        addUnique(lookup.lodModelByName, NormalizeName(lodModelDefs[i].name), i);
    }
    for (std::size_t i = 0; i < aggregateDefs.size(); ++i) {
        addUnique(lookup.aggregateByName, NormalizeName(aggregateDefs[i].name), i);
    }

    return lookup;
}

int ResolveHierarchyIndex(BuildContext& ctx, const std::string& hierarchyName, const std::string& fallbackName) {
    if (!hierarchyName.empty()) {
        const auto it = ctx.hierarchyByName.find(NormalizeName(hierarchyName));
        if (it != ctx.hierarchyByName.end()) {
            return it->second;
        }
    }

    if (!fallbackName.empty()) {
        const auto it = ctx.hierarchyByName.find(NormalizeName(fallbackName));
        if (it != ctx.hierarchyByName.end()) {
            return it->second;
        }
    }

    return -1;
}

std::vector<Mat4> BuildStaticHierarchyWorldTransforms(const RenderHierarchy& hierarchy) {
    std::vector<Mat4> worlds(hierarchy.pivots.size(), Mat4::Identity());
    std::vector<uint8_t> state(hierarchy.pivots.size(), 0u);

    std::function<void(int)> buildPivot = [&](int pivotIndex) {
        if (pivotIndex < 0 || pivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
            return;
        }
        if (state[static_cast<std::size_t>(pivotIndex)] == 2u) {
            return;
        }
        if (state[static_cast<std::size_t>(pivotIndex)] == 1u) {
            worlds[static_cast<std::size_t>(pivotIndex)] =
                hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].localTransform;
            state[static_cast<std::size_t>(pivotIndex)] = 2u;
            return;
        }

        state[static_cast<std::size_t>(pivotIndex)] = 1u;
        const auto& pivot = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)];
        Mat4 world = pivot.localTransform;
        if (pivot.parentIndex >= 0
            && pivot.parentIndex < static_cast<int>(hierarchy.pivots.size()))
        {
            buildPivot(pivot.parentIndex);
            world = Multiply(
                worlds[static_cast<std::size_t>(pivot.parentIndex)],
                pivot.localTransform);
        }

        worlds[static_cast<std::size_t>(pivotIndex)] = world;
        state[static_cast<std::size_t>(pivotIndex)] = 2u;
    };

    for (int i = 0; i < static_cast<int>(hierarchy.pivots.size()); ++i) {
        buildPivot(i);
    }

    return worlds;
}

std::optional<std::vector<Mat4>> ResolveStaticHierarchyWorldTransforms(
    BuildContext& ctx,
    const std::string& hierarchyName,
    const std::string& fallbackName,
    const ChunkItem* sourceChunk,
    const std::string& missingHierarchyMessage)
{
    const int hierarchyIndex = ResolveHierarchyIndex(ctx, hierarchyName, fallbackName);
    if (hierarchyIndex < 0
        || hierarchyIndex >= static_cast<int>(ctx.result.scene.hierarchies.size()))
    {
        ctx.WarnMissingHierarchy(
            sourceChunk,
            !hierarchyName.empty() ? hierarchyName : fallbackName,
            missingHierarchyMessage);
        return std::nullopt;
    }

    return BuildStaticHierarchyWorldTransforms(
        ctx.result.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)]);
}

struct AttachedHierarchyBinding {
    int hierarchyIndex = -1;
};

std::optional<AttachedHierarchyBinding> CreateAttachedHierarchyBinding(
    BuildContext& ctx,
    const std::string& sourceHierarchyName,
    const std::string& sourceFallbackName,
    int attachmentHierarchyIndex,
    int attachmentPivotIndex,
    const ChunkItem* sourceChunk,
    const ChunkItem* attachmentSourceChunk,
    const std::string& missingHierarchyMessage)
{
    const int sourceHierarchyIndex =
        ResolveHierarchyIndex(ctx, sourceHierarchyName, sourceFallbackName);
    if (sourceHierarchyIndex < 0
        || sourceHierarchyIndex >= static_cast<int>(ctx.result.scene.hierarchies.size()))
    {
        ctx.WarnMissingHierarchy(
            sourceChunk,
            !sourceHierarchyName.empty() ? sourceHierarchyName : sourceFallbackName,
            missingHierarchyMessage);
        return std::nullopt;
    }

    if (attachmentHierarchyIndex < 0
        || attachmentHierarchyIndex >= static_cast<int>(ctx.result.scene.hierarchies.size()))
    {
        ctx.result.warnings.push_back({
            SceneBuildWarningCode::InvalidIndex,
            BuildChunkPath(attachmentSourceChunk),
            "Aggregate attachment hierarchy index is out of range: "
                + std::to_string(attachmentHierarchyIndex)
            });
        return std::nullopt;
    }

    const auto& sourceHierarchy =
        ctx.result.scene.hierarchies[static_cast<std::size_t>(sourceHierarchyIndex)];
    const auto& attachmentHierarchy =
        ctx.result.scene.hierarchies[static_cast<std::size_t>(attachmentHierarchyIndex)];
    if (attachmentPivotIndex < 0
        || attachmentPivotIndex >= static_cast<int>(attachmentHierarchy.pivots.size()))
    {
        ctx.result.warnings.push_back({
            SceneBuildWarningCode::InvalidIndex,
            BuildChunkPath(attachmentSourceChunk),
            "Aggregate attachment pivot index is out of range: "
                + std::to_string(attachmentPivotIndex)
            });
        return std::nullopt;
    }

    RenderHierarchy attachedHierarchy = sourceHierarchy;
    attachedHierarchy.name =
        (!sourceHierarchy.name.empty() ? sourceHierarchy.name : sourceFallbackName);
    if (!sourceFallbackName.empty()) {
        attachedHierarchy.name += " [attached " + sourceFallbackName;
        attachedHierarchy.name += "]";
    }
    attachedHierarchy.attachedHierarchyIndex = attachmentHierarchyIndex;
    attachedHierarchy.attachedPivotIndex = attachmentPivotIndex;

    AttachedHierarchyBinding binding{};
    binding.hierarchyIndex = static_cast<int>(ctx.result.scene.hierarchies.size());
    ctx.result.scene.hierarchies.push_back(std::move(attachedHierarchy));
    return binding;
}

std::vector<LockedLodRangeSlot> BuildLockedLodRangeSlotsFromHlodDefinition(
    const HlodDefinition& def)
{
    std::vector<LockedLodRangeSlot> slots;
    slots.reserve(def.lodArrays.size());
    if (def.lodArrays.empty()) {
        return slots;
    }

    if (def.lodArrays.size() == 1u) {
        LockedLodRangeSlot slot{};
        slot.minDistance = 0.0f;
        slot.maxDistance = std::numeric_limits<float>::max();
        slot.maxScreenSize = def.lodArrays.front().maxScreenSize;
        slots.push_back(slot);
        return slots;
    }

    std::vector<float> distanceThresholds;
    distanceThresholds.reserve(def.lodArrays.size());
    for (std::size_t i = 0; i < def.lodArrays.size(); ++i) {
        distanceThresholds.push_back(ScreenSizeToDistance(def.lodArrays[i].maxScreenSize, i));
    }

    for (std::size_t i = 0; i < def.lodArrays.size(); ++i) {
        LockedLodRangeSlot slot{};
        slot.maxScreenSize = def.lodArrays[i].maxScreenSize;

        if (i == 0u) {
            slot.minDistance = distanceThresholds[0];
            slot.maxDistance = std::numeric_limits<float>::max();
        }
        else if (i + 1u == def.lodArrays.size()) {
            slot.minDistance = 0.0f;
            slot.maxDistance = distanceThresholds[i - 1u];
        }
        else {
            slot.minDistance = distanceThresholds[i];
            slot.maxDistance = distanceThresholds[i - 1u];
        }

        slots.push_back(slot);
    }

    return slots;
}

std::vector<LockedLodRangeSlot> BuildLockedLodRangeSlotsFromLodModelDefinition(
    const LodModelDefinition& def)
{
    std::vector<LockedLodRangeSlot> slots;
    slots.reserve(def.entries.size());
    for (const auto& entry : def.entries) {
        LockedLodRangeSlot slot{};
        slot.minDistance = entry.payload.LODMin;
        slot.maxDistance = entry.payload.LODMax;
        slots.push_back(slot);
    }
    return slots;
}

void AppendLodEntriesFromHLodDefinition(
    BuildContext& ctx,
    const HlodDefinition& def,
    RenderLodGroup& group)
{
    const std::vector<LockedLodRangeSlot> rangeSlots =
        BuildLockedLodRangeSlotsFromHlodDefinition(def);
    for (std::size_t rank = 0; rank < def.lodArrays.size(); ++rank) {
        const auto& lod = def.lodArrays[rank];
        const auto& slot = rangeSlots[rank];
        for (const auto& subObject : lod.subObjects) {
            RenderLodEntry entry{};
            entry.name = ReadFixedString(subObject.payload.Name, 2 * W3D_NAME_LEN);
            entry.hierarchyIndex = group.hierarchyIndex;
            entry.pivotIndex = static_cast<int>(subObject.payload.BoneIndex);
            entry.minDistance = slot.minDistance;
            entry.maxDistance = slot.maxDistance;
            entry.maxScreenSize = slot.maxScreenSize;
            entry.sourceBindingChunk = subObject.sourceChunk;

            const auto meshIt = ctx.meshByName.find(NormalizeName(entry.name));
            if (meshIt == ctx.meshByName.end()) {
                continue;
            }

            entry.meshIndex = meshIt->second;

            if (group.hierarchyIndex >= 0
                && group.hierarchyIndex < static_cast<int>(ctx.result.scene.hierarchies.size())) {
                const auto& pivots = ctx.result.scene.hierarchies[group.hierarchyIndex].pivots;
                if (entry.pivotIndex < 0 || entry.pivotIndex >= static_cast<int>(pivots.size())) {
                    ctx.result.warnings.push_back({
                        SceneBuildWarningCode::InvalidIndex,
                        "HLOD/" + def.name,
                        "Bone index is out of range for hierarchy: " + std::to_string(entry.pivotIndex)
                        });
                    entry.pivotIndex = -1;
                }
            }
            else {
                entry.pivotIndex = -1;
            }

            ctx.referencedMeshes.insert(entry.meshIndex);
            group.entries.push_back(std::move(entry));
        }
    }
}

void AppendLodEntriesFromLodModelDefinition(
    BuildContext& ctx,
    const LodModelDefinition& def,
    RenderLodGroup& group,
    int hierarchyIndexOverride,
    int pivotIndexOverride,
    const std::vector<LockedLodRangeSlot>* lockedRanges = nullptr)
{
    if (lockedRanges && !lockedRanges->empty()) {
        if (def.entries.empty()) {
            return;
        }

        for (std::size_t rank = 0; rank < lockedRanges->size(); ++rank) {
            const auto& slot = (*lockedRanges)[rank];
            const auto& source =
                def.entries[std::min(rank, def.entries.size() - 1)];

            RenderLodEntry entry{};
            entry.name = ReadFixedString(source.payload.RenderObjName, 2 * W3D_NAME_LEN);
            entry.hierarchyIndex = hierarchyIndexOverride;
            entry.pivotIndex = pivotIndexOverride;
            entry.minDistance = slot.minDistance;
            entry.maxDistance = slot.maxDistance;
            entry.maxScreenSize = slot.maxScreenSize;
            entry.sourceBindingChunk = source.sourceChunk;

            const auto meshIt = ctx.meshByName.find(NormalizeName(entry.name));
            if (meshIt == ctx.meshByName.end()) {
                continue;
            }

            entry.meshIndex = meshIt->second;
            ctx.referencedMeshes.insert(entry.meshIndex);
            group.entries.push_back(std::move(entry));
        }
        return;
    }

    for (const auto& source : def.entries) {
        RenderLodEntry entry{};
        entry.name = ReadFixedString(source.payload.RenderObjName, 2 * W3D_NAME_LEN);
        entry.hierarchyIndex = hierarchyIndexOverride;
        entry.pivotIndex = pivotIndexOverride;
        entry.minDistance = source.payload.LODMin;
        entry.maxDistance = source.payload.LODMax;
        entry.sourceBindingChunk = source.sourceChunk;

        const auto meshIt = ctx.meshByName.find(NormalizeName(entry.name));
        if (meshIt == ctx.meshByName.end()) {
            continue;
        }

        entry.meshIndex = meshIt->second;
        ctx.referencedMeshes.insert(entry.meshIndex);
        group.entries.push_back(std::move(entry));
    }
}

void AppendAttachedNodesFromHModelDefinition(
    BuildContext& ctx,
    const HModelDefinition& def,
    int attachmentHierarchyIndex,
    int attachmentPivotIndex,
    const std::string& sourceAttachBoneName,
    const ChunkItem* attachmentSourceChunk)
{
    const auto attachedHierarchy = CreateAttachedHierarchyBinding(
        ctx,
        def.hierarchyName,
        def.name,
        attachmentHierarchyIndex,
        attachmentPivotIndex,
        def.sourceChunk,
        attachmentSourceChunk,
        "Aggregate attached HModel hierarchy was not found");
    if (!attachedHierarchy.has_value()) {
        return;
    }

    for (const auto& source : def.nodes) {
        const auto meshIt = ctx.meshByName.find(NormalizeName(source.renderObjName));
        if (meshIt == ctx.meshByName.end()) {
            ctx.result.warnings.push_back({
                SceneBuildWarningCode::InvalidIndex,
                BuildChunkPath(source.sourceChunk),
                "Referenced HModel mesh not found: " + source.renderObjName
                });
            continue;
        }

        int pivotIndex = source.pivotIndex;
        if (pivotIndex < 0) {
            pivotIndex = -1;
        }
        else if (pivotIndex >= static_cast<int>(
            ctx.result.scene.hierarchies[static_cast<std::size_t>(attachedHierarchy->hierarchyIndex)].pivots.size()))
        {
            ctx.result.warnings.push_back({
                SceneBuildWarningCode::InvalidIndex,
                BuildChunkPath(source.sourceChunk),
                "Attached HModel pivot index is out of range for hierarchy: "
                    + std::to_string(source.pivotIndex)
                });
            continue;
        }

        AddMeshNodeInstance(
            ctx,
            source.renderObjName,
            meshIt->second,
            attachedHierarchy->hierarchyIndex,
            pivotIndex,
            attachmentSourceChunk,
            Mat4::Identity());
    }
}

void AppendAttachedLodEntriesFromHLodDefinition(
    BuildContext& ctx,
    const HlodDefinition& def,
    RenderLodGroup& group,
    int attachmentHierarchyIndex,
    int attachmentPivotIndex,
    const std::string& sourceAttachBoneName,
    const ChunkItem* attachmentSourceChunk,
    const std::vector<LockedLodRangeSlot>* lockedRanges)
{
    const auto attachedHierarchy = CreateAttachedHierarchyBinding(
        ctx,
        def.hierarchyName,
        def.name,
        attachmentHierarchyIndex,
        attachmentPivotIndex,
        def.sourceChunk,
        attachmentSourceChunk,
        "Aggregate attached HLOD hierarchy was not found");
    if (!attachedHierarchy.has_value()) {
        return;
    }
    group.hierarchyIndex = attachedHierarchy->hierarchyIndex;

    if (def.lodArrays.empty()) {
        return;
    }

    const std::vector<LockedLodRangeSlot> defaultRanges =
        BuildLockedLodRangeSlotsFromHlodDefinition(def);
    const std::size_t slotCount = (lockedRanges && !lockedRanges->empty())
        ? lockedRanges->size()
        : def.lodArrays.size();
    for (std::size_t rank = 0; rank < slotCount; ++rank) {
        const std::size_t lodIndex = std::min(rank, def.lodArrays.size() - 1);
        const auto& lod = def.lodArrays[lodIndex];
        const auto& slot = (lockedRanges && !lockedRanges->empty())
            ? (*lockedRanges)[std::min(rank, lockedRanges->size() - 1)]
            : defaultRanges[lodIndex];

        for (const auto& subObject : lod.subObjects) {
            const int sourcePivotIndex = static_cast<int>(subObject.payload.BoneIndex);
            if (sourcePivotIndex < 0
                || sourcePivotIndex >= static_cast<int>(
                    ctx.result.scene.hierarchies[static_cast<std::size_t>(attachedHierarchy->hierarchyIndex)].pivots.size()))
            {
                ctx.result.warnings.push_back({
                    SceneBuildWarningCode::InvalidIndex,
                    BuildChunkPath(subObject.sourceChunk),
                    "Attached HLOD bone index is out of range for hierarchy: "
                        + std::to_string(sourcePivotIndex)
                    });
                continue;
            }

            RenderLodEntry entry{};
            entry.name = ReadFixedString(subObject.payload.Name, 2 * W3D_NAME_LEN);
            entry.hierarchyIndex = attachedHierarchy->hierarchyIndex;
            entry.pivotIndex = sourcePivotIndex;
            entry.localTransform = Mat4::Identity();
            entry.minDistance = slot.minDistance;
            entry.maxDistance = slot.maxDistance;
            entry.maxScreenSize = slot.maxScreenSize;
            entry.sourceBindingChunk = attachmentSourceChunk;

            const auto meshIt = ctx.meshByName.find(NormalizeName(entry.name));
            if (meshIt == ctx.meshByName.end()) {
                continue;
            }

            entry.meshIndex = meshIt->second;
            ctx.referencedMeshes.insert(entry.meshIndex);
            group.entries.push_back(std::move(entry));
        }
    }
}

int BuildAggregateInstance(
    BuildContext& ctx,
    const AggregateDefinition& def,
    const std::vector<HModelDefinition>& hmodelDefs,
    const std::vector<HlodDefinition>& hlodDefs,
    const std::vector<LodModelDefinition>& lodModelDefs,
    const std::vector<AggregateDefinition>& aggregateDefs,
    const ModelDefinitionLookup& lookup,
    std::vector<std::string>& aggregateStack)
{
    const std::string aggregateKey = NormalizeName(
        !def.name.empty() ? def.name : BuildChunkPath(def.sourceChunk));
    if (std::find(aggregateStack.begin(), aggregateStack.end(), aggregateKey) != aggregateStack.end()) {
        ctx.result.warnings.push_back({
            SceneBuildWarningCode::UnsupportedChunk,
            BuildChunkPath(def.sourceChunk),
            "Cyclic aggregate reference detected: " + def.name
            });
        return -1;
    }

    aggregateStack.push_back(aggregateKey);

    int baseHierarchyIndex = -1;
    bool baseResolved = false;
    bool baseSupportsHierarchyBinding = false;
    std::vector<LockedLodRangeSlot> lockedSubObjectLodRanges;

    if (const AggregateDefinition* nestedAggregate =
        FindAggregateByName(aggregateDefs, lookup, def.baseModelName))
    {
        baseSupportsHierarchyBinding = true;
        baseHierarchyIndex = BuildAggregateInstance(
            ctx,
            *nestedAggregate,
            hmodelDefs,
            hlodDefs,
            lodModelDefs,
            aggregateDefs,
            lookup,
            aggregateStack);
        baseResolved = true;
    }
    else if (const HModelDefinition* hmodel =
        FindHModelByName(hmodelDefs, lookup, def.baseModelName, true))
    {
        baseSupportsHierarchyBinding = true;
        baseHierarchyIndex = ResolveHierarchyIndex(ctx, hmodel->hierarchyName, hmodel->name);
        if (baseHierarchyIndex < 0) {
            ctx.WarnMissingHierarchy(
                hmodel->sourceChunk,
                !hmodel->hierarchyName.empty() ? hmodel->hierarchyName : hmodel->name,
                "Aggregate base HModel hierarchy was not found");
        }
        AppendNodesFromHModelDefinition(ctx, *hmodel, baseHierarchyIndex);
        baseResolved = true;
    }
    else if (const HlodDefinition* hlod =
        FindHLodByName(hlodDefs, lookup, def.baseModelName, true))
    {
        baseSupportsHierarchyBinding = true;
        RenderLodGroup group{};
        group.name = def.name.empty() ? hlod->name : def.name;
        group.hierarchyIndex = ResolveHierarchyIndex(ctx, hlod->hierarchyName, hlod->name);
        if (group.hierarchyIndex < 0) {
            ctx.WarnMissingHierarchy(
                hlod->sourceChunk,
                !hlod->hierarchyName.empty() ? hlod->hierarchyName : hlod->name,
                "Aggregate base HLOD hierarchy was not found");
        }
        AppendLodEntriesFromHLodDefinition(ctx, *hlod, group);
        baseHierarchyIndex = group.hierarchyIndex;
        if (!group.entries.empty()) {
            ctx.result.scene.lodGroups.push_back(std::move(group));
        }
        if (def.forceSubObjectLod) {
            lockedSubObjectLodRanges = BuildLockedLodRangeSlotsFromHlodDefinition(*hlod);
        }
        baseResolved = true;
    }
    else if (const LodModelDefinition* lodModel =
        FindLodModelByName(lodModelDefs, lookup, def.baseModelName))
    {
        RenderLodGroup group{};
        group.name = def.name.empty() ? lodModel->name : def.name;
        group.hierarchyIndex = -1;
        AppendLodEntriesFromLodModelDefinition(ctx, *lodModel, group, -1, -1);
        if (!group.entries.empty()) {
            ctx.result.scene.lodGroups.push_back(std::move(group));
        }
        if (def.forceSubObjectLod) {
            lockedSubObjectLodRanges = BuildLockedLodRangeSlotsFromLodModelDefinition(*lodModel);
        }
        baseResolved = true;
    }
    else if (const auto meshIt = ctx.meshByName.find(NormalizeName(def.baseModelName));
        meshIt != ctx.meshByName.end())
    {
        AddMeshNodeInstance(
            ctx,
            def.baseModelName,
            meshIt->second,
            -1,
            -1,
            def.sourceChunk,
            Mat4::Identity());
        baseResolved = true;
    }
    else if (!def.baseModelName.empty()) {
        ctx.result.warnings.push_back({
            SceneBuildWarningCode::InvalidIndex,
            BuildChunkPath(def.sourceChunk),
            "Aggregate base model not found: " + def.baseModelName
            });
    }

    if (baseHierarchyIndex < 0) {
        if (baseResolved && !baseSupportsHierarchyBinding && !def.subObjects.empty()) {
            ctx.result.warnings.push_back({
                SceneBuildWarningCode::UnsupportedChunk,
                BuildChunkPath(def.sourceChunk),
                "Aggregate base model does not expose a hierarchy for subobject binding: " + def.baseModelName
                });
        }
        aggregateStack.pop_back();
        return baseHierarchyIndex;
    }

    const auto& hierarchy = ctx.result.scene.hierarchies[static_cast<std::size_t>(baseHierarchyIndex)];
    const auto* lockedSubObjectLodRangeView =
        (def.forceSubObjectLod && !lockedSubObjectLodRanges.empty())
        ? &lockedSubObjectLodRanges
        : nullptr;
    bool warnedUnsupportedLockedSubObjectLod = false;
    for (const auto& subObject : def.subObjects) {
        const int pivotIndex = ResolvePivotIndexByName(hierarchy, subObject.boneName);
        if (pivotIndex < 0) {
            ctx.result.warnings.push_back({
                SceneBuildWarningCode::InvalidIndex,
                BuildChunkPath(subObject.sourceChunk),
                "Aggregate bone not found on base hierarchy: " + subObject.boneName
                });
            continue;
        }

        if (const auto meshIt = ctx.meshByName.find(NormalizeName(subObject.renderObjName));
            meshIt != ctx.meshByName.end())
        {
            AddMeshNodeInstance(
                ctx,
                subObject.renderObjName,
                meshIt->second,
                baseHierarchyIndex,
                pivotIndex,
                subObject.sourceChunk,
                Mat4::Identity());
            continue;
        }

        if (const LodModelDefinition* lodModel =
            FindLodModelByName(lodModelDefs, lookup, subObject.renderObjName))
        {
            if (def.forceSubObjectLod
                && lockedSubObjectLodRangeView == nullptr
                && !warnedUnsupportedLockedSubObjectLod)
            {
                ctx.result.warnings.push_back({
                    SceneBuildWarningCode::UnsupportedChunk,
                    BuildChunkPath(def.sourceChunk),
                    "Aggregate subobject LOD locking is not supported for this base model yet."
                    });
                warnedUnsupportedLockedSubObjectLod = true;
            }

            RenderLodGroup group{};
            group.name = def.name.empty()
                ? subObject.renderObjName
                : (def.name + "." + subObject.renderObjName);
            group.hierarchyIndex = baseHierarchyIndex;
            AppendLodEntriesFromLodModelDefinition(
                ctx,
                *lodModel,
                group,
                baseHierarchyIndex,
                pivotIndex,
                lockedSubObjectLodRangeView);
            if (!group.entries.empty()) {
                ctx.result.scene.lodGroups.push_back(std::move(group));
            }
            continue;
        }

        if (FindAggregateByName(aggregateDefs, lookup, subObject.renderObjName)) {
            ctx.result.warnings.push_back({
                SceneBuildWarningCode::UnsupportedChunk,
                BuildChunkPath(subObject.sourceChunk),
                "Nested aggregate attachments are not supported yet: " + subObject.renderObjName
                });
            continue;
        }

        if (const HModelDefinition* hmodel =
            FindHModelByName(hmodelDefs, lookup, subObject.renderObjName, false))
        {
            AppendAttachedNodesFromHModelDefinition(
                ctx,
                *hmodel,
                baseHierarchyIndex,
                pivotIndex,
                subObject.boneName,
                subObject.sourceChunk);
            continue;
        }

        if (const HlodDefinition* hlod =
            FindHLodByName(hlodDefs, lookup, subObject.renderObjName, false))
        {
            if (def.forceSubObjectLod
                && lockedSubObjectLodRangeView == nullptr
                && !warnedUnsupportedLockedSubObjectLod)
            {
                ctx.result.warnings.push_back({
                    SceneBuildWarningCode::UnsupportedChunk,
                    BuildChunkPath(def.sourceChunk),
                    "Aggregate subobject LOD locking is not supported for this base model yet."
                    });
                warnedUnsupportedLockedSubObjectLod = true;
            }

            RenderLodGroup group{};
            group.name = def.name.empty()
                ? subObject.renderObjName
                : (def.name + "." + subObject.renderObjName);
            group.hierarchyIndex = baseHierarchyIndex;
            AppendAttachedLodEntriesFromHLodDefinition(
                ctx,
                *hlod,
                group,
                baseHierarchyIndex,
                pivotIndex,
                subObject.boneName,
                subObject.sourceChunk,
                lockedSubObjectLodRangeView);
            if (!group.entries.empty()) {
                ctx.result.scene.lodGroups.push_back(std::move(group));
            }
            continue;
        }

        ctx.result.warnings.push_back({
            SceneBuildWarningCode::InvalidIndex,
            BuildChunkPath(subObject.sourceChunk),
            "Aggregate subobject render object not found: " + subObject.renderObjName
            });
    }

    aggregateStack.pop_back();
    return baseHierarchyIndex;
}

void BuildAggregatesFromDefinitions(
    BuildContext& ctx,
    const std::vector<AggregateDefinition>& aggregateDefs,
    const std::vector<HModelDefinition>& hmodelDefs,
    const std::vector<HlodDefinition>& hlodDefs,
    const std::vector<LodModelDefinition>& lodModelDefs,
    const ModelDefinitionLookup& lookup)
{
    std::vector<std::string> aggregateStack;
    aggregateStack.reserve(8);

    for (const auto& def : aggregateDefs) {
        if (def.sourceFromReferenceOnly) {
            continue;
        }
        BuildAggregateInstance(
            ctx,
            def,
            hmodelDefs,
            hlodDefs,
            lodModelDefs,
            aggregateDefs,
            lookup,
            aggregateStack);
    }
}

void BuildLodGroupsFromHLodDefinitions(
    BuildContext& ctx,
    const std::vector<HlodDefinition>& hlodDefs)
{
    for (const auto& def : hlodDefs) {
        if (def.sourceFromReferenceOnly) {
            continue;
        }

        RenderLodGroup group{};
        group.name = def.name;
        group.hierarchyIndex = ResolveHierarchyIndex(ctx, def.hierarchyName, def.name);
        if (group.hierarchyIndex < 0) {
            ctx.WarnMissingHierarchy(
                def.sourceChunk,
                !def.hierarchyName.empty() ? def.hierarchyName : def.name,
                "HLOD hierarchy was not found");
        }
        AppendLodEntriesFromHLodDefinition(ctx, def, group);

        if (!group.entries.empty()) {
            ctx.result.scene.lodGroups.push_back(std::move(group));
        }
    }
}

void BuildLodGroupsFromLodModelDefinitions(
    BuildContext& ctx,
    const std::vector<LodModelDefinition>& defs)
{
    for (const auto& def : defs) {
        if (def.sourceFromReferenceOnly) {
            continue;
        }

        RenderLodGroup group{};
        group.name = def.name;
        group.hierarchyIndex = -1;
        AppendLodEntriesFromLodModelDefinition(ctx, def, group, -1, -1);

        if (!group.entries.empty()) {
            ctx.result.scene.lodGroups.push_back(std::move(group));
        }
    }
}

void BuildLooseNodes(BuildContext& ctx) {
    const bool hasPrimaryMeshes = std::any_of(
        ctx.result.scene.meshes.begin(),
        ctx.result.scene.meshes.end(),
        [](const RenderMesh& mesh) {
            return !mesh.sourceFromSupplemental;
        });

    for (std::size_t i = 0; i < ctx.result.scene.meshes.size(); ++i) {
        const int meshIndex = static_cast<int>(i);
        if (ctx.referencedMeshes.contains(meshIndex)) {
            continue;
        }
        if (ctx.referenceOnlyMeshes.contains(meshIndex)) {
            continue;
        }
        if (ctx.supplementalMeshes.contains(meshIndex) && hasPrimaryMeshes) {
            continue;
        }

        RenderNode node{};
        node.name = ctx.result.scene.meshes[i].fullName;
        node.meshIndex = static_cast<int>(i);
        node.hierarchyIndex = -1;
        node.pivotIndex = -1;
        node.localTransform = Mat4::Identity();
        node.sourceBindingChunk = ctx.result.scene.meshes[i].sourceMeshHeaderChunk;
        ctx.result.scene.looseNodes.push_back(std::move(node));
    }
}

void ParseMeshes(
    const W3DChunk& roots,
    BuildContext& ctx,
    bool supplemental,
    bool referenceOnly)
{
    std::vector<std::shared_ptr<ChunkItem>> meshChunks;
    for (const auto& root : roots) {
        CollectChunksByIdRecursive(root, kChunkMesh, meshChunks);
    }

    for (const auto& meshChunk : meshChunks) {
        if (!meshChunk) {
            continue;
        }

        const std::size_t meshCountBefore = ctx.result.scene.meshes.size();
        const auto mesh = BuildRenderMeshFromChunk(ctx, meshChunk);
        if (!mesh) {
            continue;
        }

        const int meshIndex = static_cast<int>(ctx.result.scene.meshes.size());
        ctx.meshByName.emplace(NormalizeName(mesh->fullName), meshIndex);
        ctx.result.scene.meshes.push_back(*mesh);

        const std::size_t meshCountAfter = ctx.result.scene.meshes.size();
        for (std::size_t i = meshCountBefore; i < meshCountAfter; ++i) {
            if (supplemental || referenceOnly) {
                ctx.result.scene.meshes[i].sourceFromSupplemental = true;
            }
            if (supplemental) {
                ctx.supplementalMeshes.insert(static_cast<int>(i));
            }
            if (referenceOnly) {
                ctx.referenceOnlyMeshes.insert(static_cast<int>(i));
            }
        }
    }
}

} // namespace

SceneBuildResult BuildRenderScene(
    const W3DChunk& primaryRoots,
    const SceneBuildOptions& options,
    const W3DChunk* skeletonSupplementalRoots,
    const W3DChunk* animationLibraryRoots,
    const W3DChunk* referenceOnlyRoots)
{
    BuildContext ctx{};
    ctx.options = options;
    ctx.InitializeExternalTextures();
    ctx.result.scene.profile = options.profile;
    const std::vector<std::shared_ptr<ChunkItem>> hierarchyRoots =
        CollectAllRoots(primaryRoots, skeletonSupplementalRoots, referenceOnlyRoots);
    ParseMeshes(primaryRoots, ctx, false, false);
    if (skeletonSupplementalRoots && !skeletonSupplementalRoots->empty()) {
        ParseMeshes(*skeletonSupplementalRoots, ctx, true, false);
    }
    if (referenceOnlyRoots && !referenceOnlyRoots->empty()) {
        ParseMeshes(*referenceOnlyRoots, ctx, false, true);
    }
    ParseHierarchies(hierarchyRoots, ctx);

    std::vector<ParsedAnimationDefinition> animations = ParseAnimationDefinitions(primaryRoots, ctx, false);
    if (skeletonSupplementalRoots && !skeletonSupplementalRoots->empty()) {
        std::vector<ParsedAnimationDefinition> moreAnimations =
            ParseAnimationDefinitions(*skeletonSupplementalRoots, ctx, false);
        animations.insert(
            animations.end(),
            std::make_move_iterator(moreAnimations.begin()),
            std::make_move_iterator(moreAnimations.end()));
    }
    if (referenceOnlyRoots && !referenceOnlyRoots->empty()) {
        std::vector<ParsedAnimationDefinition> referenceAnimations =
            ParseAnimationDefinitions(*referenceOnlyRoots, ctx, false);
        animations.insert(
            animations.end(),
            std::make_move_iterator(referenceAnimations.begin()),
            std::make_move_iterator(referenceAnimations.end()));
    }
    if (animationLibraryRoots && !animationLibraryRoots->empty()) {
        std::vector<ParsedAnimationDefinition> libraryAnimations =
            ParseAnimationDefinitions(*animationLibraryRoots, ctx, true);
        animations.insert(
            animations.end(),
            std::make_move_iterator(libraryAnimations.begin()),
            std::make_move_iterator(libraryAnimations.end()));
    }
    AssignAnimationsToHierarchies(ctx, animations);

    const std::vector<std::shared_ptr<ChunkItem>> modelRoots =
        CollectAllRoots(primaryRoots, skeletonSupplementalRoots, nullptr);

    auto hmodelDefs = ParseHModelDefinitions(modelRoots, ctx, false);
    if (referenceOnlyRoots && !referenceOnlyRoots->empty()) {
        std::vector<HModelDefinition> referenceHModels =
            ParseHModelDefinitions(*referenceOnlyRoots, ctx, true);
        hmodelDefs.insert(
            hmodelDefs.end(),
            std::make_move_iterator(referenceHModels.begin()),
            std::make_move_iterator(referenceHModels.end()));
    }
    BuildNodesFromHModelDefinitions(ctx, hmodelDefs);

    auto hlodDefs = ParseHLodDefinitions(modelRoots, ctx, false);
    if (referenceOnlyRoots && !referenceOnlyRoots->empty()) {
        std::vector<HlodDefinition> referenceHLods =
            ParseHLodDefinitions(*referenceOnlyRoots, ctx, true);
        hlodDefs.insert(
            hlodDefs.end(),
            std::make_move_iterator(referenceHLods.begin()),
            std::make_move_iterator(referenceHLods.end()));
    }
    BuildLodGroupsFromHLodDefinitions(ctx, hlodDefs);

    auto lodModelDefs = ParseLodModelDefinitions(modelRoots, ctx, false);
    if (referenceOnlyRoots && !referenceOnlyRoots->empty()) {
        std::vector<LodModelDefinition> referenceLodModels =
            ParseLodModelDefinitions(*referenceOnlyRoots, ctx, true);
        lodModelDefs.insert(
            lodModelDefs.end(),
            std::make_move_iterator(referenceLodModels.begin()),
            std::make_move_iterator(referenceLodModels.end()));
    }
    BuildLodGroupsFromLodModelDefinitions(ctx, lodModelDefs);

    auto aggregateDefs = ParseAggregateDefinitions(primaryRoots, ctx, false);
    if (skeletonSupplementalRoots && !skeletonSupplementalRoots->empty()) {
        std::vector<AggregateDefinition> supplementalAggregates =
            ParseAggregateDefinitions(*skeletonSupplementalRoots, ctx, false);
        aggregateDefs.insert(
            aggregateDefs.end(),
            std::make_move_iterator(supplementalAggregates.begin()),
            std::make_move_iterator(supplementalAggregates.end()));
    }
    if (referenceOnlyRoots && !referenceOnlyRoots->empty()) {
        std::vector<AggregateDefinition> referenceAggregates =
            ParseAggregateDefinitions(*referenceOnlyRoots, ctx, true);
        aggregateDefs.insert(
            aggregateDefs.end(),
            std::make_move_iterator(referenceAggregates.begin()),
            std::make_move_iterator(referenceAggregates.end()));
    }

    const ModelDefinitionLookup lookup =
        BuildModelDefinitionLookup(hmodelDefs, hlodDefs, lodModelDefs, aggregateDefs);
    BuildAggregatesFromDefinitions(ctx, aggregateDefs, hmodelDefs, hlodDefs, lodModelDefs, lookup);

    BuildLooseNodes(ctx);

    const bool hasRenderableInstances =
        !ctx.result.scene.looseNodes.empty() || !ctx.result.scene.lodGroups.empty();
    if (!hasRenderableInstances) {
        ctx.result.warnings.push_back({
            SceneBuildWarningCode::UnsupportedChunk,
            "root",
            aggregateDefs.empty()
                ? "No renderable mesh chunks were found in this file."
                : "No renderable scene instances were assembled. Aggregate dependencies may be missing."
            });
    }

    return ctx.result;
}

} // namespace OW3D::Render
