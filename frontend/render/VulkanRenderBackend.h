#pragma once

#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

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
    void SetSelectedInstance(const std::optional<RenderInstanceKey>& selected) override;
    void SetTransformOverrides(
        const std::unordered_map<RenderInstanceKey, Mat4, RenderInstanceKeyHash>& overrides) override;
    void SetHiddenInstances(
        const std::unordered_set<RenderInstanceKey, RenderInstanceKeyHash>& hidden) override;
    void RenderFrame(const std::function<void()>& overlayCallback = {}) override;
    FrameStats GetFrameStats() const override;
    void* NativeDeviceHandle() const override { return nullptr; }
    void* NativeDeviceContextHandle() const override { return nullptr; }

private:
    RenderScene m_scene{};
    CameraState m_camera{};
    RenderSettings m_settings{};
};

} // namespace OW3D::Render

