#pragma once

#include <cstdint>

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
};

struct FrameStats {
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
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
    virtual void RenderFrame() = 0;
    virtual FrameStats GetFrameStats() const = 0;
};

} // namespace OW3D::Render

