#pragma once

#include "IRenderBackend.h"

namespace OW3D::Render {

// Post-v1 experiment backend stub. Kept behind IRenderBackend so SceneBuilder/viewport APIs stay stable.
class VulkanRenderBackend final : public IRenderBackend {
public:
    bool Initialize(void* nativeWindow, uint32_t width, uint32_t height) override;
    void Shutdown() override;
    bool UploadScene(const RenderScene& scene) override;
    void Resize(uint32_t width, uint32_t height) override;
    void SetCamera(const CameraState& camera) override;
    void SetRenderSettings(const RenderSettings& settings) override;
    void RenderFrame() override;
    FrameStats GetFrameStats() const override;

private:
    RenderScene m_scene{};
    CameraState m_camera{};
    RenderSettings m_settings{};
};

} // namespace OW3D::Render

