#include "SceneBuilder.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <limits>
#include <optional>
#include <string>
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
constexpr uint32_t kChunkMeshHeader3 = 0x001F;
constexpr uint32_t kChunkTriangles = 0x0020;
constexpr uint32_t kChunkTextureWrapper = 0x0031;
constexpr uint32_t kChunkTextureName = 0x0032;
constexpr uint32_t kChunkMaterialPass = 0x0038;
constexpr uint32_t kChunkTextureStage = 0x0048;
constexpr uint32_t kChunkTextureIds = 0x0049;
constexpr uint32_t kChunkStageTexCoords = 0x004A;
constexpr uint32_t kChunkPrelitUnlit = 0x0023;
constexpr uint32_t kChunkPrelitVertex = 0x0024;
constexpr uint32_t kChunkPrelitLightmapMultiPass = 0x0025;
constexpr uint32_t kChunkPrelitLightmapMultiTexture = 0x0026;
constexpr uint32_t kChunkHierarchy = 0x0100;
constexpr uint32_t kChunkHierarchyHeader = 0x0101;
constexpr uint32_t kChunkPivots = 0x0102;
constexpr uint32_t kChunkLodModel = 0x0400;
constexpr uint32_t kChunkLodModelHeader = 0x0401;
constexpr uint32_t kChunkLodModelLod = 0x0402;
constexpr uint32_t kChunkHLod = 0x0700;
constexpr uint32_t kChunkHLodHeader = 0x0701;
constexpr uint32_t kChunkHLodLodArray = 0x0702;
constexpr uint32_t kChunkHLodSubObjectArrayHeader = 0x0703;
constexpr uint32_t kChunkHLodSubObject = 0x0704;

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

bool FileExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

std::string ResolveTexturePath(const std::string& textureName, const SceneBuildOptions& options) {
    if (textureName.empty()) {
        return {};
    }

    std::filesystem::path input(textureName);
    if (input.is_absolute() && FileExists(input)) {
        return input.string();
    }

    std::filesystem::path base = options.textureSearchDirectory.empty()
        ? std::filesystem::current_path()
        : std::filesystem::path(options.textureSearchDirectory);

    if (input.has_extension()) {
        const auto candidate = base / input;
        if (FileExists(candidate)) {
            return candidate.string();
        }
    }
    else {
        static const std::array<const char*, 6> kTextureExtensions = {
            ".dds", ".tga", ".png", ".jpg", ".jpeg", ".bmp"
        };
        for (const char* ext : kTextureExtensions) {
            auto candidate = base / input;
            candidate += ext;
            if (FileExists(candidate)) {
                return candidate.string();
            }
        }
    }

    return {};
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
    std::vector<W3dHLodSubObjectStruct> subObjects;
};

struct HlodDefinition {
    std::string name;
    std::string hierarchyName;
    std::vector<HlodLodArray> lodArrays;
};

struct LodModelDefinition {
    std::string name;
    std::vector<W3dLODStruct> entries;
};

struct BuildContext {
    SceneBuildOptions options{};
    SceneBuildResult result{};

    std::unordered_map<std::string, int> textureByName;
    std::unordered_map<std::string, int> materialByKey;
    std::unordered_map<std::string, int> meshByName;
    std::unordered_map<std::string, int> hierarchyByName;
    std::unordered_set<int> referencedMeshes;
    std::unordered_set<int> supplementalMeshes;
    std::unordered_set<std::string> externalTextureByPath;
    std::unordered_set<std::string> externalTextureByBaseName;
    std::unordered_set<uint32_t> externalTextureHashes;

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
        texture.resolvedPath = ResolveTexturePath(textureName, options);
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

    int EnsureMaterial(int textureIndex, bool twoSided, const std::string& nameHint) {
        const std::string key = std::to_string(textureIndex) + ":" + (twoSided ? "2" : "1");
        const auto existing = materialByKey.find(key);
        if (existing != materialByKey.end()) {
            return existing->second;
        }

        RenderMaterial material{};
        material.name = nameHint;
        material.textureIndex = textureIndex;
        material.twoSided = twoSided;

        const int index = static_cast<int>(result.scene.materials.size());
        result.scene.materials.push_back(material);
        materialByKey.emplace(key, index);
        return index;
    }
};

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
    std::shared_ptr<ChunkItem> primaryTextureStageChunk;
    if (materialRoot) {
        std::vector<std::shared_ptr<ChunkItem>> materialPassChunks;
        CollectChunksByIdRecursive(materialRoot, kChunkMaterialPass, materialPassChunks);
        if (!materialPassChunks.empty()) {
            const auto textureStages = FindChildrenById(materialPassChunks.front(), kChunkTextureStage);
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

    std::vector<int> localTextureIndices;
    std::vector<std::shared_ptr<ChunkItem>> textureWrappers;
    CollectChunksByIdRecursive(materialRoot ? materialRoot : meshChunk, kChunkTextureWrapper, textureWrappers);
    for (const auto& textureWrapper : textureWrappers) {
        const auto texNameChunk = FindFirstChildById(textureWrapper, kChunkTextureName);
        if (!texNameChunk) {
            continue;
        }
        const std::string texName = ReadNullTerminatedChunkString(texNameChunk);
        localTextureIndices.push_back(ctx.EnsureTexture(texName, texNameChunk.get()));
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

    const auto headerValue = *header;
    const std::string meshName = ReadFixedString(headerValue.MeshName, W3D_NAME_LEN);
    const std::string containerName = ReadFixedString(headerValue.ContainerName, W3D_NAME_LEN);

    std::string fullName = meshName;
    if (!containerName.empty()) {
        fullName = containerName + "." + meshName;
    }
    if (fullName.empty()) {
        fullName = "mesh_" + std::to_string(ctx.result.scene.meshes.size());
    }

    std::unordered_map<int, std::vector<uint32_t>> groupedIndices;
    groupedIndices.reserve(4);

    auto resolveTextureIndexForTriangle = [&](std::size_t triIndex) -> int {
        if (triTextureAssignments.empty()) {
            return -1;
        }
        uint32_t texId = triTextureAssignments[0];
        if (triTextureAssignments.size() > 1 && triIndex < triTextureAssignments.size()) {
            texId = triTextureAssignments[triIndex];
        }
        if (texId == 0xFFFFFFFFu) {
            return -1;
        }
        if (texId >= localTextureIndices.size()) {
            ctx.result.warnings.push_back({
                SceneBuildWarningCode::InvalidIndex,
                BuildChunkPath(trisChunk.get()),
                "Triangle texture index is out of range."
                });
            return -1;
        }
        return localTextureIndices[texId];
    };

    std::vector<RenderVertex> baseVertices;
    baseVertices.resize(vertices->size());
    for (std::size_t i = 0; i < vertices->size(); ++i) {
        const auto& v = (*vertices)[i];
        auto& dst = baseVertices[i];
        dst.position = { v.X, v.Y, v.Z };

        if (i < normals.size()) {
            const auto& n = normals[i];
            dst.normal = { n.X, n.Y, n.Z };
        }

        if (i < texCoords.size()) {
            const auto& uv = texCoords[i];
            dst.uv = { uv.U, 1.0f - uv.V };
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

        const int materialTextureIndex = resolveTextureIndexForTriangle(triIndex);
        auto& indices = groupedIndices[materialTextureIndex];
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }

    if (groupedIndices.empty()) {
        return std::nullopt;
    }

    const bool twoSided =
        (headerValue.Attributes & static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_TWO_SIDED)) != 0;
    const bool hidden =
        (headerValue.Attributes & static_cast<uint32_t>(MeshAttr::W3D_MESH_FLAG_HIDDEN)) != 0;

    const Vec3 boundsMin = { headerValue.Min.X, headerValue.Min.Y, headerValue.Min.Z };
    const Vec3 boundsMax = { headerValue.Max.X, headerValue.Max.Y, headerValue.Max.Z };
    const Vec3 boundsCenter = { headerValue.SphCenter.X, headerValue.SphCenter.Y, headerValue.SphCenter.Z };

    std::optional<RenderMesh> primaryMesh;
    int groupSuffix = 0;
    for (const auto& pair : groupedIndices) {
        const int textureIndex = pair.first;
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

        const std::string materialNameHint = subMesh.fullName + "_mat";
        subMesh.materialIndex = ctx.EnsureMaterial(textureIndex, twoSided, materialNameHint);

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
        hierarchy.pivots.reserve(pivots->size());

        for (std::size_t i = 0; i < pivots->size(); ++i) {
            const auto& src = (*pivots)[i];
            RenderPivot pivot{};
            pivot.name = ReadFixedString(src.Name, W3D_NAME_LEN);
            pivot.parentIndex = (src.ParentIdx == 0xFFFFFFFFu)
                ? -1
                : static_cast<int>(src.ParentIdx);
            pivot.localTransform = TransformFromTranslationRotation(
                { src.Translation.X, src.Translation.Y, src.Translation.Z },
                src.Rotation.Q[0],
                src.Rotation.Q[1],
                src.Rotation.Q[2],
                src.Rotation.Q[3]);

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
            ctx.hierarchyByName[NormalizeName(hierarchy.name)] = hierarchyIndex;
        }
        ctx.result.scene.hierarchies.push_back(std::move(hierarchy));
    }
}

std::vector<HlodDefinition> ParseHLodDefinitions(const W3DChunk& roots, BuildContext& ctx) {
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
                lodArray.subObjects.push_back(*parsedSub);
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

std::vector<LodModelDefinition> ParseLodModelDefinitions(const W3DChunk& roots, BuildContext& ctx) {
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

        const auto lodEntries = FindChildrenById(lodModelChunk, kChunkLodModelLod);
        for (const auto& lodEntryChunk : lodEntries) {
            if (!lodEntryChunk) {
                continue;
            }
            const auto parsed = ParseStructWithWarning<W3dLODStruct>(lodEntryChunk, ctx.result.warnings);
            if (parsed) {
                def.entries.push_back(*parsed);
            }
        }

        if (!def.entries.empty()) {
            out.push_back(std::move(def));
        }
    }

    return out;
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

void BuildLodGroupsFromHLodDefinitions(
    BuildContext& ctx,
    const std::vector<HlodDefinition>& hlodDefs)
{
    for (const auto& def : hlodDefs) {
        RenderLodGroup group{};
        group.name = def.name;
        group.hierarchyIndex = ResolveHierarchyIndex(ctx, def.hierarchyName, def.name);

        std::vector<std::size_t> order;
        order.reserve(def.lodArrays.size());
        for (std::size_t i = 0; i < def.lodArrays.size(); ++i) {
            order.push_back(i);
        }
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return def.lodArrays[a].maxScreenSize > def.lodArrays[b].maxScreenSize;
        });

        float rangeMin = 0.0f;
        for (std::size_t rank = 0; rank < order.size(); ++rank) {
            const auto& lod = def.lodArrays[order[rank]];
            const float nominalRange = ScreenSizeToDistance(lod.maxScreenSize, rank);
            const float rangeMax = (rank + 1 < order.size())
                ? std::max(rangeMin + 1.0f, nominalRange)
                : std::numeric_limits<float>::max();

            for (const auto& subObject : lod.subObjects) {
                RenderLodEntry entry{};
                entry.name = ReadFixedString(subObject.Name, 2 * W3D_NAME_LEN);
                entry.hierarchyIndex = group.hierarchyIndex;
                entry.pivotIndex = static_cast<int>(subObject.BoneIndex);
                entry.minDistance = rangeMin;
                entry.maxDistance = rangeMax;
                entry.maxScreenSize = lod.maxScreenSize;

                const auto meshIt = ctx.meshByName.find(NormalizeName(entry.name));
                if (meshIt == ctx.meshByName.end()) {
                    // Match WW3D behavior: unresolved render-object refs are skipped.
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

            rangeMin = rangeMax;
        }

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
        RenderLodGroup group{};
        group.name = def.name;
        group.hierarchyIndex = -1;

        for (const auto& source : def.entries) {
            RenderLodEntry entry{};
            entry.name = ReadFixedString(source.RenderObjName, 2 * W3D_NAME_LEN);
            entry.hierarchyIndex = -1;
            entry.pivotIndex = -1;
            entry.minDistance = source.LODMin;
            entry.maxDistance = source.LODMax;

            const auto meshIt = ctx.meshByName.find(NormalizeName(entry.name));
            if (meshIt == ctx.meshByName.end()) {
                // Match WW3D behavior: unresolved render-object refs are skipped.
                continue;
            }

            entry.meshIndex = meshIt->second;
            ctx.referencedMeshes.insert(entry.meshIndex);
            group.entries.push_back(std::move(entry));
        }

        if (!group.entries.empty()) {
            ctx.result.scene.lodGroups.push_back(std::move(group));
        }
    }
}

void BuildLooseNodes(BuildContext& ctx) {
    for (std::size_t i = 0; i < ctx.result.scene.meshes.size(); ++i) {
        const int meshIndex = static_cast<int>(i);
        if (ctx.referencedMeshes.contains(meshIndex)) {
            continue;
        }
        if (ctx.supplementalMeshes.contains(meshIndex)) {
            continue;
        }

        RenderNode node{};
        node.name = ctx.result.scene.meshes[i].fullName;
        node.meshIndex = static_cast<int>(i);
        node.hierarchyIndex = -1;
        node.pivotIndex = -1;
        node.localTransform = Mat4::Identity();
        ctx.result.scene.looseNodes.push_back(std::move(node));
    }
}

void ParseMeshes(const W3DChunk& roots, BuildContext& ctx, bool supplemental) {
    std::vector<std::shared_ptr<ChunkItem>> meshChunks;
    for (const auto& root : roots) {
        CollectChunksByIdRecursive(root, kChunkMesh, meshChunks);
    }

    for (const auto& meshChunk : meshChunks) {
        if (!meshChunk) {
            continue;
        }

        const auto mesh = BuildRenderMeshFromChunk(ctx, meshChunk);
        if (!mesh) {
            continue;
        }

        const int meshIndex = static_cast<int>(ctx.result.scene.meshes.size());
        ctx.meshByName.emplace(NormalizeName(mesh->fullName), meshIndex);
        ctx.result.scene.meshes.push_back(*mesh);
        if (supplemental) {
            ctx.supplementalMeshes.insert(meshIndex);
        }
    }
}

} // namespace

SceneBuildResult BuildRenderScene(
    const W3DChunk& root,
    const SceneBuildOptions& options,
    const W3DChunk* supplementalRoot)
{
    BuildContext ctx{};
    ctx.options = options;
    ctx.InitializeExternalTextures();
    ctx.result.scene.profile = options.profile;

    ParseMeshes(root, ctx, false);
    if (supplementalRoot && !supplementalRoot->empty()) {
        ParseMeshes(*supplementalRoot, ctx, true);
    }
    ParseHierarchies(root, ctx);

    const auto hlodDefs = ParseHLodDefinitions(root, ctx);
    BuildLodGroupsFromHLodDefinitions(ctx, hlodDefs);

    const auto lodModelDefs = ParseLodModelDefinitions(root, ctx);
    BuildLodGroupsFromLodModelDefinitions(ctx, lodModelDefs);

    BuildLooseNodes(ctx);

    if (ctx.result.scene.meshes.empty()) {
        ctx.result.warnings.push_back({
            SceneBuildWarningCode::UnsupportedChunk,
            "root",
            "No renderable mesh chunks were found in this file."
            });
    }

    return ctx.result;
}

} // namespace OW3D::Render
