#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
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
    MissingHierarchy,
    CyclicHierarchy,
    UnsupportedChunk
};

struct SceneBuildWarning {
    SceneBuildWarningCode code = SceneBuildWarningCode::UnsupportedChunk;
    std::string chunkPath;
    std::string message;
};

struct AnimationPlaybackState {
    int activeAnimationIndex = -1;
    float timeSeconds = 0.0f;
    bool playing = false;
    bool loop = true;
    float speed = 1.0f;
};

struct SceneBuildOptions {
    ParityProfile profile = ParityProfile::W3DViewD3D11Baseline;
    std::string textureSearchDirectory;
    std::vector<std::string> additionalTextureSearchDirectories;
    std::vector<std::string> externalTextureNames;
    std::vector<uint32_t> externalTextureHashes;
    std::unordered_map<const ChunkItem*, std::string> rootSourceLabels;
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
    Vec3 secondaryPosition{};
    Vec3 secondaryNormal{ 0.0f, 1.0f, 0.0f };
    Vec2 uv{};
    Vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    std::array<uint16_t, 4> boneIndices{ 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu };
    std::array<float, 4> boneWeights{ 0.0f, 0.0f, 0.0f, 0.0f };
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
    bool skinned = false;
    bool hasSecondaryVertexStream = false;
    uint8_t bonesPerVertex = 0;
    const ::ChunkItem* sourceMeshHeaderChunk = nullptr;
    bool sourceFromSupplemental = false;
};

struct RenderPivot {
    std::string name;
    int parentIndex = -1;
    Mat4 localTransform = Mat4::Identity();
    Vec3 baseTranslation{};
    Vec4 baseRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
};

struct RenderHierarchy {
    std::string name;
    std::vector<RenderPivot> pivots;
    std::vector<int> compatibleAnimationIndices;
    const ::ChunkItem* sourceHierarchyChunk = nullptr;
    const ::ChunkItem* sourcePivotsChunk = nullptr;
};

struct RenderFloatKeyframe {
    float frame = 0.0f;
    float value = 0.0f;
    bool hold = false;
};

struct RenderQuatKeyframe {
    float frame = 0.0f;
    Vec4 value{ 0.0f, 0.0f, 0.0f, 1.0f };
    bool hold = false;
};

struct RenderPivotAnimation {
    std::vector<RenderFloatKeyframe> translationX;
    std::vector<RenderFloatKeyframe> translationY;
    std::vector<RenderFloatKeyframe> translationZ;
    std::vector<RenderQuatKeyframe> rotation;
};

struct RenderDensePivotAnimationSamples {
    std::vector<float> translationX;
    std::vector<float> translationY;
    std::vector<float> translationZ;
    std::vector<Vec4> rotation;
};

struct RenderAnimationClip {
    std::string fullName;
    std::string hierarchyName;
    std::string sourceFileLabel;
    uint32_t numFrames = 0;
    float frameRate = 0.0f;
    bool compressed = false;
    bool supportedForPlayback = true;
    bool sourceFromAnimationLibrary = false;
    const ::ChunkItem* sourceAnimationChunk = nullptr;
    std::vector<RenderPivotAnimation> pivots;
};

struct RenderAnimationEditDraft {
    const ::ChunkItem* sourceAnimationChunk = nullptr;
    uint32_t numFrames = 0;
    std::unordered_map<int, RenderDensePivotAnimationSamples> pivotSamples;
};

struct RenderLodEntry {
    std::string name;
    int meshIndex = -1;
    int hierarchyIndex = -1;
    int pivotIndex = -1;
    Mat4 localTransform = Mat4::Identity();
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
    std::vector<RenderAnimationClip> animations;
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
