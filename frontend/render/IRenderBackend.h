#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "../../backend/render/RenderScene.h"
#include "../../backend/render/RenderTypes.h"

namespace OW3D::Render {

struct CameraState {
    Vec3 target{ 0.0f, 0.0f, 0.0f };
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 15.0f;
    float fovDeg = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 10000.0f;
};

struct RenderSettings {
    ParityProfile profile = ParityProfile::W3DViewD3D11Baseline;
    bool enableFog = true;
    bool enableLod = true;
    float lodBias = 1.0f;
    bool debugShowUv = false;
    bool lockLodLevel = false;
    int lockedLodLevel = 0;
    bool showCameraGizmo = true;
};

struct FrameStats {
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
};

enum class RenderInstanceKind : uint8_t {
    LodEntry,
    LooseNode
};

struct RenderInstanceKey {
    RenderInstanceKind kind = RenderInstanceKind::LooseNode;
    int primaryIndex = -1;
    int secondaryIndex = -1;

    bool operator==(const RenderInstanceKey& other) const = default;
};

struct RenderInstanceKeyHash {
    std::size_t operator()(const RenderInstanceKey& key) const noexcept {
        std::size_t h = static_cast<std::size_t>(key.kind);
        h = (h * 16777619u) ^ static_cast<std::size_t>(key.primaryIndex + 0x9E3779B9);
        h = (h * 16777619u) ^ static_cast<std::size_t>(key.secondaryIndex + 0x7F4A7C15);
        return h;
    }
};

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    virtual bool Initialize(void* nativeWindow, uint32_t width, uint32_t height) = 0;
    virtual void Shutdown() = 0;
    virtual bool UploadScene(const RenderScene& scene) = 0;
    virtual void Resize(uint32_t width, uint32_t height) = 0;
    virtual void SetCamera(const CameraState& camera) = 0;
    virtual void SetRenderSettings(const RenderSettings& settings) = 0;
    virtual void SetSelectedInstance(const std::optional<RenderInstanceKey>& selected) = 0;
    virtual void SetTransformOverrides(
        const std::unordered_map<RenderInstanceKey, Mat4, RenderInstanceKeyHash>& overrides) = 0;
    virtual void SetHiddenInstances(
        const std::unordered_set<RenderInstanceKey, RenderInstanceKeyHash>& hidden) = 0;
    virtual void RenderFrame(const std::function<void()>& overlayCallback = {}) = 0;
    virtual FrameStats GetFrameStats() const = 0;
    virtual void* NativeDeviceHandle() const = 0;
    virtual void* NativeDeviceContextHandle() const = 0;
};

} // namespace OW3D::Render

