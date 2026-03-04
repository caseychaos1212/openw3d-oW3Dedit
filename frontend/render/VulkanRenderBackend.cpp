#include "VulkanRenderBackend.h"

namespace OW3D::Render {

bool VulkanRenderBackend::Initialize(void*, uint32_t, uint32_t) {
    return false;
}

void VulkanRenderBackend::Shutdown() {
    m_scene = {};
}

bool VulkanRenderBackend::UploadScene(const RenderScene& scene) {
    m_scene = scene;
    return true;
}

void VulkanRenderBackend::Resize(uint32_t, uint32_t) {
}

void VulkanRenderBackend::SetCamera(const CameraState& camera) {
    m_camera = camera;
}

void VulkanRenderBackend::SetRenderSettings(const RenderSettings& settings) {
    m_settings = settings;
}

void VulkanRenderBackend::RenderFrame() {
}

FrameStats VulkanRenderBackend::GetFrameStats() const {
    return {};
}

} // namespace OW3D::Render

