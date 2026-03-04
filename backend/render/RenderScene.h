#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "RenderTypes.h"

class ChunkItem;

namespace OW3D::Render {

enum class ParityProfile {
    W3DViewD3D11Baseline
};

enum class SceneBuildWarningCode {
    MissingPayload,
    InvalidIndex,
    MissingTexture,
    CyclicHierarchy,
    UnsupportedChunk
};

struct SceneBuildWarning {
    SceneBuildWarningCode code = SceneBuildWarningCode::UnsupportedChunk;
    std::string chunkPath;
    std::string message;
};

struct SceneBuildOptions {
    ParityProfile profile = ParityProfile::W3DViewD3D11Baseline;
    std::string textureSearchDirectory;
    std::vector<std::string> externalTextureNames;
    std::vector<uint32_t> externalTextureHashes;
    bool assembleWholeFileScene = true;
};

struct RenderTexture {
    std::string name;
    std::string resolvedPath;
    bool resolved = false;
};

struct RenderMaterial {
    std::string name;
    Vec4 diffuseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    int textureIndex = -1;
    bool alphaTest = false;
    bool translucent = false;
    bool twoSided = false;
    uint8_t uvAnimMode = 0; // 0=none, 1=scroll, 2=rotate
    float uvOffsetU = 0.0f;
    float uvOffsetV = 0.0f;
    float uvScrollU = 0.0f;
    float uvScrollV = 0.0f;
    float uvScaleU = 1.0f;
    float uvScaleV = 1.0f;
    float uvCenterU = 0.0f;
    float uvCenterV = 0.0f;
    float uvRotateRadPerSec = 0.0f;
};

struct RenderVertex {
    Vec3 position{};
    Vec3 normal{ 0.0f, 1.0f, 0.0f };
    Vec2 uv{};
    Vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
};

struct RenderMesh {
    std::string fullName;
    std::vector<RenderVertex> vertices;
    std::vector<uint32_t> indices;
    int materialIndex = -1;
    Vec3 boundsMin{};
    Vec3 boundsMax{};
    Vec3 boundsCenter{};
    float boundsRadius = 0.0f;
    bool twoSided = false;
    bool hidden = false;
    const ::ChunkItem* sourceMeshHeaderChunk = nullptr;
    bool sourceFromSupplemental = false;
};

struct RenderPivot {
    std::string name;
    int parentIndex = -1;
    Mat4 localTransform = Mat4::Identity();
};

struct RenderHierarchy {
    std::string name;
    std::vector<RenderPivot> pivots;
    const ::ChunkItem* sourceHierarchyChunk = nullptr;
    const ::ChunkItem* sourcePivotsChunk = nullptr;
};

struct RenderLodEntry {
    std::string name;
    int meshIndex = -1;
    int hierarchyIndex = -1;
    int pivotIndex = -1;
    float minDistance = 0.0f;
    float maxDistance = std::numeric_limits<float>::max();
    float maxScreenSize = 0.0f;
    const ::ChunkItem* sourceBindingChunk = nullptr;
};

struct RenderLodGroup {
    std::string name;
    int hierarchyIndex = -1;
    std::vector<RenderLodEntry> entries;
};

struct RenderNode {
    std::string name;
    int meshIndex = -1;
    int hierarchyIndex = -1;
    int pivotIndex = -1;
    Mat4 localTransform = Mat4::Identity();
    const ::ChunkItem* sourceBindingChunk = nullptr;
};

struct RenderFog {
    bool enabled = true;
    Vec3 color{ 0.55f, 0.60f, 0.67f };
    float nearDistance = 350.0f;
    float farDistance = 2400.0f;
};

struct RenderScene {
    ParityProfile profile = ParityProfile::W3DViewD3D11Baseline;
    std::vector<RenderTexture> textures;
    std::vector<RenderMaterial> materials;
    std::vector<RenderMesh> meshes;
    std::vector<RenderHierarchy> hierarchies;
    std::vector<RenderLodGroup> lodGroups;
    std::vector<RenderNode> looseNodes;
    RenderFog fog{};
    Vec3 ambientLight{ 0.35f, 0.35f, 0.35f };
    Vec3 directionalLightDir{ -0.5f, -1.0f, -0.35f };
    Vec3 directionalLightColor{ 0.85f, 0.85f, 0.85f };
};

struct SceneBuildResult {
    RenderScene scene;
    std::vector<SceneBuildWarning> warnings;
};

} // namespace OW3D::Render
