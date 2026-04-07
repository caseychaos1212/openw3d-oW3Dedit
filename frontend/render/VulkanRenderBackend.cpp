#include "VulkanRenderBackend.h"

#include <QImage>

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

void VulkanRenderBackend::SetAnimationPlayback(const AnimationPlaybackState& playback) {
    m_animationPlayback = playback;
}

void VulkanRenderBackend::SetAnimationEditDraft(const std::optional<RenderAnimationEditDraft>&) {
}

void VulkanRenderBackend::SetSelectedInstance(const std::optional<RenderInstanceKey>&) {
}

void VulkanRenderBackend::SetTransformOverrides(
    const std::unordered_map<RenderInstanceKey, Mat4, RenderInstanceKeyHash>&)
{
}

void VulkanRenderBackend::SetPivotLocalOverrides(
    const std::unordered_map<RenderPivotOverrideKey, Mat4, RenderPivotOverrideKeyHash>&)
{
}

void VulkanRenderBackend::SetHiddenInstances(
    const std::unordered_set<RenderInstanceKey, RenderInstanceKeyHash>&)
{
}

void VulkanRenderBackend::RenderFrame(const std::function<void()>&) {
}

bool VulkanRenderBackend::CaptureFrame(QImage& outImage) {
    outImage = QImage();
    return false;
}

FrameStats VulkanRenderBackend::GetFrameStats() const {
    return {};
}

} // namespace OW3D::Render

