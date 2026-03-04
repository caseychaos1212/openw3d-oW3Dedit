#include "RenderViewportWidget.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <QApplication>
#include <QCursor>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEngine>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>

#include "D3D11RenderBackend.h"

#include "../../thirdparty/imgui/imgui.h"
#include "../../thirdparty/imgui/backends/imgui_impl_dx11.h"
#include "../../thirdparty/ImGuizmo/ImGuizmo.h"

namespace OW3D::Render {
namespace {

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

Vec3 CameraForward(float yaw, float pitch) {
    const float cp = std::cos(pitch);
    return {
        std::sin(yaw) * cp,
        std::sin(pitch),
        std::cos(yaw) * cp
    };
}

void Mat4ToFloatArray(const Mat4& m, float out[16]) {
    std::memcpy(out, m.m, sizeof(float) * 16);
}

Mat4 Mat4FromFloatArray(const float in[16]) {
    Mat4 out{};
    std::memcpy(out.m, in, sizeof(float) * 16);
    return out;
}

Quaternion QuaternionFromMatrix(const Mat4& m) {
    Quaternion q{};
    const float trace = m.m[0] + m.m[5] + m.m[10];

    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m.m[9] - m.m[6]) / s;
        q.y = (m.m[2] - m.m[8]) / s;
        q.z = (m.m[4] - m.m[1]) / s;
    }
    else if (m.m[0] > m.m[5] && m.m[0] > m.m[10]) {
        const float s = std::sqrt(1.0f + m.m[0] - m.m[5] - m.m[10]) * 2.0f;
        q.w = (m.m[9] - m.m[6]) / s;
        q.x = 0.25f * s;
        q.y = (m.m[1] + m.m[4]) / s;
        q.z = (m.m[2] + m.m[8]) / s;
    }
    else if (m.m[5] > m.m[10]) {
        const float s = std::sqrt(1.0f + m.m[5] - m.m[0] - m.m[10]) * 2.0f;
        q.w = (m.m[2] - m.m[8]) / s;
        q.x = (m.m[1] + m.m[4]) / s;
        q.y = 0.25f * s;
        q.z = (m.m[6] + m.m[9]) / s;
    }
    else {
        const float s = std::sqrt(1.0f + m.m[10] - m.m[0] - m.m[5]) * 2.0f;
        q.w = (m.m[4] - m.m[1]) / s;
        q.x = (m.m[2] + m.m[8]) / s;
        q.y = (m.m[6] + m.m[9]) / s;
        q.z = 0.25f * s;
    }

    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > 1e-6f) {
        const float inv = 1.0f / len;
        q.x *= inv;
        q.y *= inv;
        q.z *= inv;
        q.w *= inv;
    }
    else {
        q = {};
    }

    return q;
}

} // namespace

RenderViewportWidget::RenderViewportWidget(QWidget* parent)
    : QWidget(parent)
    , m_backend(std::make_unique<D3D11RenderBackend>())
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_camera.yaw = DegToRad(45.0f);
    m_camera.pitch = DegToRad(-22.0f);
    m_camera.distance = 24.0f;
    m_camera.fovDeg = 60.0f;
    m_camera.nearPlane = 0.1f;
    m_camera.farPlane = 10000.0f;

    m_settings.profile = ParityProfile::W3DViewD3D11Baseline;
    m_settings.enableFog = true;
    m_settings.enableLod = true;
    m_settings.lodBias = 1.0f;

    m_frameTimer = new QTimer(this);
    m_frameTimer->setInterval(16);
    connect(m_frameTimer, &QTimer::timeout, this, &RenderViewportWidget::TickFrame);

    m_statsTimer.start();
    m_deltaTimer.start();

    EmitSelectionStatus(QStringLiteral("Selection: none"));
}

RenderViewportWidget::~RenderViewportWidget() {
    if (m_frameTimer) {
        m_frameTimer->stop();
    }

    ShutdownImGui();

    if (m_backend) {
        m_backend->Shutdown();
    }
}

void RenderViewportWidget::SetSceneResult(const SceneBuildResult& sceneResult) {
    m_sceneResult = sceneResult;
    m_sceneDirty = true;

    ClearPivotOverrides();
    m_hiddenInstances.clear();
    m_selectedInstance.reset();
    m_selectedVisibleIndex = -1;

    if (m_backend) {
        m_backend->SetSelectedInstance(std::nullopt);
        m_backend->SetTransformOverrides({});
        m_backend->SetHiddenInstances({});
    }

    EmitWarnings();
    EmitSelectionStatus(QStringLiteral("Selection: none"));
}

void RenderViewportWidget::SetRenderSettings(const RenderSettings& settings) {
    m_settings = settings;
    if (m_backendInitialized && m_backend) {
        m_backend->SetRenderSettings(m_settings);
    }
}

void RenderViewportWidget::FocusScene() {
    const Vec3 center = ComputeSceneCenter();
    const float radius = ComputeSceneRadius(center);

    m_camera.target = center;
    m_camera.distance = std::max(6.0f, radius * 2.5f);
    m_camera.nearPlane = std::max(0.1f, radius * 0.002f);
    m_camera.farPlane = std::max(1000.0f, radius * 24.0f + m_camera.distance);
}

QPaintEngine* RenderViewportWidget::paintEngine() const {
    return nullptr;
}

void RenderViewportWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    EnsureBackendInitialized();
    if (m_frameTimer && !m_frameTimer->isActive()) {
        m_frameTimer->start();
    }
}

void RenderViewportWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (m_frameTimer && m_frameTimer->isActive()) {
        m_frameTimer->stop();
    }
}

void RenderViewportWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    EnsureBackendInitialized();
    if (m_backendInitialized && m_backend) {
        m_backend->Resize(
            static_cast<uint32_t>(std::max(1, width())),
            static_cast<uint32_t>(std::max(1, height())));
    }
}

void RenderViewportWidget::mousePressEvent(QMouseEvent* event) {
    if (m_imguiInitialized) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(static_cast<float>(event->position().x()), static_cast<float>(event->position().y()));
        if (event->button() == Qt::LeftButton) {
            io.AddMouseButtonEvent(0, true);
        }
        else if (event->button() == Qt::RightButton) {
            io.AddMouseButtonEvent(1, true);
        }
        else if (event->button() == Qt::MiddleButton) {
            io.AddMouseButtonEvent(2, true);
        }

        if (io.WantCaptureMouse) {
            m_dragMode = DragMode::None;
            m_pendingPick = false;
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        if ((event->modifiers() & Qt::AltModifier) != 0) {
            m_dragMode = DragMode::Orbit;
        }
        else {
            m_pendingPick = !(m_gizmoOver || m_gizmoUsing);
            m_pickPressPos = event->pos();
        }
    }
    else if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_dragMode = DragMode::Pan;
    }

    m_lastMousePos = event->pos();
    setFocus(Qt::MouseFocusReason);
    event->accept();
}

void RenderViewportWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_imguiInitialized) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(static_cast<float>(event->position().x()), static_cast<float>(event->position().y()));
        if (io.WantCaptureMouse && m_dragMode == DragMode::None) {
            event->accept();
            return;
        }
    }

    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    if (m_pendingPick) {
        if ((event->pos() - m_pickPressPos).manhattanLength() > 4) {
            m_pendingPick = false;
        }
    }

    if (m_dragMode == DragMode::Orbit) {
        m_camera.yaw += static_cast<float>(delta.x()) * 0.0075f;
        m_camera.pitch += static_cast<float>(delta.y()) * 0.0075f;
        m_camera.pitch = std::clamp(m_camera.pitch, DegToRad(-89.0f), DegToRad(89.0f));
    }
    else if (m_dragMode == DragMode::Pan) {
        const float panScale = std::max(0.05f, m_camera.distance * 0.0018f);
        const Vec3 forward = Normalize(CameraForward(m_camera.yaw, m_camera.pitch));
        Vec3 right = Normalize(Cross({ 0.0f, 1.0f, 0.0f }, forward));
        if (Length(right) < 0.001f) {
            right = { 1.0f, 0.0f, 0.0f };
        }
        const Vec3 up = Normalize(Cross(forward, right));

        m_camera.target = m_camera.target
            + right * static_cast<float>(-delta.x()) * panScale
            + up * static_cast<float>(delta.y()) * panScale;
    }

    event->accept();
}

void RenderViewportWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (m_imguiInitialized) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(static_cast<float>(event->position().x()), static_cast<float>(event->position().y()));
        if (event->button() == Qt::LeftButton) {
            io.AddMouseButtonEvent(0, false);
        }
        else if (event->button() == Qt::RightButton) {
            io.AddMouseButtonEvent(1, false);
        }
        else if (event->button() == Qt::MiddleButton) {
            io.AddMouseButtonEvent(2, false);
        }

        if (io.WantCaptureMouse && m_dragMode == DragMode::None) {
            m_pendingPick = false;
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        if (m_dragMode == DragMode::Orbit) {
            m_dragMode = DragMode::None;
        }

        const bool shouldPick = m_pendingPick && !m_gizmoUsing && !m_gizmoOver;
        m_pendingPick = false;
        if (shouldPick) {
            PerformPick(event->pos());
        }
    }
    else if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        if (m_dragMode == DragMode::Pan) {
            m_dragMode = DragMode::None;
        }
    }

    event->accept();
}

void RenderViewportWidget::wheelEvent(QWheelEvent* event) {
    if (m_imguiInitialized) {
        ImGuiIO& io = ImGui::GetIO();
        const QPoint angle = event->angleDelta();
        io.AddMouseWheelEvent(0.0f, static_cast<float>(angle.y()) / 120.0f);
        if (io.WantCaptureMouse) {
            event->accept();
            return;
        }
    }

    const QPoint angle = event->angleDelta();
    if (!angle.isNull()) {
        const float notches = static_cast<float>(angle.y()) / 120.0f;
        const float scale = std::pow(0.88f, notches);
        m_camera.distance = std::clamp(m_camera.distance * scale, 0.25f, 200000.0f);
    }
    event->accept();
}

void RenderViewportWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_F) {
        FocusScene();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_B) {
        m_showSceneBrowser = !m_showSceneBrowser;
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_W) {
        m_gizmoMode = GizmoMode::Translate;
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_E) {
        m_gizmoMode = GizmoMode::Rotate;
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void RenderViewportWidget::EnsureBackendInitialized() {
    if (m_backendInitialized || !m_backend) {
        return;
    }

    if (!windowHandle()) {
        return;
    }

    const bool ok = m_backend->Initialize(
        reinterpret_cast<void*>(winId()),
        static_cast<uint32_t>(std::max(1, width())),
        static_cast<uint32_t>(std::max(1, height())));
    if (!ok) {
        return;
    }

    if (!m_imguiInitialized) {
        auto* d3dBackend = dynamic_cast<D3D11RenderBackend*>(m_backend.get());
        if (d3dBackend) {
            auto* device = static_cast<ID3D11Device*>(d3dBackend->NativeDeviceHandle());
            auto* context = static_cast<ID3D11DeviceContext*>(d3dBackend->NativeDeviceContextHandle());
            if (device && context) {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGui::StyleColorsDark();
                ImGuiIO& io = ImGui::GetIO();
                io.IniFilename = nullptr;
                io.LogFilename = nullptr;
                m_imguiInitialized = ImGui_ImplDX11_Init(device, context);
            }
        }
    }

    m_backendInitialized = true;
    m_backend->SetRenderSettings(m_settings);
    m_backend->SetCamera(m_camera);
    m_sceneDirty = true;
    m_deltaTimer.restart();
}

void RenderViewportWidget::ShutdownImGui() {
    if (!m_imguiInitialized) {
        return;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
    m_imguiInitialized = false;
}

void RenderViewportWidget::TickFrame() {
    EnsureBackendInitialized();
    if (!m_backendInitialized || !m_backend) {
        return;
    }

    if (m_sceneDirty) {
        m_backend->UploadScene(m_sceneResult.scene);
        m_sceneDirty = false;
    }

    m_camera.pitch = std::clamp(m_camera.pitch, DegToRad(-89.0f), DegToRad(89.0f));
    m_camera.distance = std::max(0.01f, m_camera.distance);

    Vec3 cameraPos = m_camera.target - Normalize(CameraForward(m_camera.yaw, m_camera.pitch)) * m_camera.distance;
    BuildVisibleInstances(cameraPos);
    SyncSelectedInstanceToVisibleList();

    bool viewOrTransformChanged = false;
    if (m_imguiInitialized) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(std::max(1, width())), static_cast<float>(std::max(1, height())));
        const float dt = std::max(1.0e-4f, static_cast<float>(m_deltaTimer.nsecsElapsed()) / 1.0e9f);
        io.DeltaTime = dt;
        m_deltaTimer.restart();

        const Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
        io.AddKeyEvent(ImGuiMod_Ctrl, (mods & Qt::ControlModifier) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (mods & Qt::ShiftModifier) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (mods & Qt::AltModifier) != 0);
        io.AddKeyEvent(ImGuiMod_Super, (mods & Qt::MetaModifier) != 0);

        const QPoint localMousePos = mapFromGlobal(QCursor::pos());
        io.AddMousePosEvent(static_cast<float>(localMousePos.x()), static_cast<float>(localMousePos.y()));

        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();

        const Mat4 view = LookAtLH(cameraPos, m_camera.target, { 0.0f, 1.0f, 0.0f });
        const float aspect = static_cast<float>(std::max(1, width())) / static_cast<float>(std::max(1, height()));
        const Mat4 projection = PerspectiveFovLH(
            DegToRad(m_camera.fovDeg),
            aspect,
            m_camera.nearPlane,
            m_camera.farPlane);

        viewOrTransformChanged = HandleGizmos(view, projection);

        ImGui::Render();
    }

    if (viewOrTransformChanged) {
        cameraPos = m_camera.target - Normalize(CameraForward(m_camera.yaw, m_camera.pitch)) * m_camera.distance;
        BuildVisibleInstances(cameraPos);
        SyncSelectedInstanceToVisibleList();
    }

    RebuildBackendOverrides();

    m_backend->SetRenderSettings(m_settings);
    m_backend->SetCamera(m_camera);

    if (m_imguiInitialized) {
        m_backend->RenderFrame([this]() {
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        });
    }
    else {
        m_backend->RenderFrame();
    }

    if (m_statsTimer.hasExpired(200)) {
        m_statsTimer.restart();
        const FrameStats stats = m_backend->GetFrameStats();
        emit frameStatsChanged(
            QStringLiteral("Draws: %1 | Tris: %2")
            .arg(stats.drawCalls)
            .arg(stats.triangles));
    }
}

void RenderViewportWidget::EmitWarnings() {
    QStringList lines;
    lines.reserve(static_cast<int>(m_sceneResult.warnings.size()));

    for (const auto& warning : m_sceneResult.warnings) {
        QString prefix;
        switch (warning.code) {
        case SceneBuildWarningCode::MissingPayload:
            prefix = QStringLiteral("MissingPayload");
            break;
        case SceneBuildWarningCode::InvalidIndex:
            prefix = QStringLiteral("InvalidIndex");
            break;
        case SceneBuildWarningCode::MissingTexture:
            prefix = QStringLiteral("MissingTexture");
            break;
        case SceneBuildWarningCode::CyclicHierarchy:
            prefix = QStringLiteral("CyclicHierarchy");
            break;
        case SceneBuildWarningCode::UnsupportedChunk:
        default:
            prefix = QStringLiteral("Unsupported");
            break;
        }

        lines.push_back(
            QStringLiteral("[%1] %2 :: %3")
            .arg(prefix,
                QString::fromStdString(warning.chunkPath),
                QString::fromStdString(warning.message)));
    }

    emit sceneWarningsChanged(lines);
}

void RenderViewportWidget::BuildVisibleInstances(const Vec3& cameraPos) {
    m_visibleInstances.clear();

    const auto hierarchyWorld = BuildHierarchyWorldTransforms();
    auto getWorldForBinding = [&](int hierarchyIndex, int pivotIndex, const Mat4& fallback) -> Mat4 {
        if (hierarchyIndex < 0 || hierarchyIndex >= static_cast<int>(hierarchyWorld.size())) {
            return fallback;
        }
        const auto& worlds = hierarchyWorld[static_cast<std::size_t>(hierarchyIndex)];
        if (pivotIndex < 0 || pivotIndex >= static_cast<int>(worlds.size())) {
            return fallback;
        }
        return worlds[static_cast<std::size_t>(pivotIndex)];
    };

    for (std::size_t lodGroupIndex = 0; lodGroupIndex < m_sceneResult.scene.lodGroups.size(); ++lodGroupIndex) {
        const auto& lodGroup = m_sceneResult.scene.lodGroups[lodGroupIndex];
        int forcedEntryIndex = -1;
        if (m_settings.lockLodLevel && !lodGroup.entries.empty()) {
            forcedEntryIndex = std::clamp(
                m_settings.lockedLodLevel,
                0,
                static_cast<int>(lodGroup.entries.size()) - 1);
        }
        for (std::size_t entryIndex = 0; entryIndex < lodGroup.entries.size(); ++entryIndex) {
            if (forcedEntryIndex >= 0 && static_cast<int>(entryIndex) != forcedEntryIndex) {
                continue;
            }

            const auto& entry = lodGroup.entries[entryIndex];
            if (entry.meshIndex < 0 || entry.meshIndex >= static_cast<int>(m_sceneResult.scene.meshes.size())) {
                continue;
            }

            const auto& mesh = m_sceneResult.scene.meshes[static_cast<std::size_t>(entry.meshIndex)];
            if (mesh.hidden) {
                continue;
            }

            const Mat4 world = getWorldForBinding(entry.hierarchyIndex, entry.pivotIndex, Mat4::Identity());
            const Vec3 worldCenter = TransformPoint(world, mesh.boundsCenter);
            const float distanceToCamera = Length(worldCenter - cameraPos);

            if (forcedEntryIndex < 0 && m_settings.enableLod) {
                const float bias = std::max(0.01f, m_settings.lodBias);
                const float minDistance = entry.minDistance * bias;
                const float maxDistance = std::isfinite(entry.maxDistance)
                    ? entry.maxDistance * bias
                    : std::numeric_limits<float>::max();
                if (distanceToCamera < minDistance || distanceToCamera >= maxDistance) {
                    continue;
                }
            }

            VisibleInstance instance{};
            instance.key.kind = RenderInstanceKind::LodEntry;
            instance.key.primaryIndex = static_cast<int>(lodGroupIndex);
            instance.key.secondaryIndex = static_cast<int>(entryIndex);
            instance.meshIndex = entry.meshIndex;
            instance.hierarchyIndex = entry.hierarchyIndex;
            instance.pivotIndex = entry.pivotIndex;
            instance.world = world;
            instance.meshChunk = mesh.sourceMeshHeaderChunk;
            instance.meshFromSupplemental = mesh.sourceFromSupplemental;
            instance.hiddenByUser = m_hiddenInstances.contains(instance.key);

            if (instance.hierarchyIndex >= 0
                && instance.hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
                const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(instance.hierarchyIndex)];
                instance.pivotsChunk = hierarchy.sourcePivotsChunk;
            }

            if (instance.meshFromSupplemental) {
                instance.editable = false;
                instance.readOnlyReason = QStringLiteral("supplemental mesh (read-only)");
            }
            else if (!instance.pivotsChunk || instance.pivotIndex < 0) {
                instance.editable = false;
                instance.readOnlyReason = QStringLiteral("no writable pivot binding");
            }
            else {
                instance.editable = true;
            }

            m_visibleInstances.push_back(instance);
        }
    }

    for (std::size_t nodeIndex = 0; nodeIndex < m_sceneResult.scene.looseNodes.size(); ++nodeIndex) {
        const auto& node = m_sceneResult.scene.looseNodes[nodeIndex];
        if (node.meshIndex < 0 || node.meshIndex >= static_cast<int>(m_sceneResult.scene.meshes.size())) {
            continue;
        }

        const auto& mesh = m_sceneResult.scene.meshes[static_cast<std::size_t>(node.meshIndex)];
        if (mesh.hidden) {
            continue;
        }

        VisibleInstance instance{};
            instance.key.kind = RenderInstanceKind::LooseNode;
            instance.key.primaryIndex = static_cast<int>(nodeIndex);
            instance.key.secondaryIndex = -1;
            instance.meshIndex = node.meshIndex;
            instance.hierarchyIndex = node.hierarchyIndex;
            instance.pivotIndex = node.pivotIndex;
            instance.world = getWorldForBinding(node.hierarchyIndex, node.pivotIndex, node.localTransform);
            instance.meshChunk = mesh.sourceMeshHeaderChunk;
            instance.meshFromSupplemental = mesh.sourceFromSupplemental;
            instance.hiddenByUser = m_hiddenInstances.contains(instance.key);

        if (instance.hierarchyIndex >= 0
            && instance.hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(instance.hierarchyIndex)];
            instance.pivotsChunk = hierarchy.sourcePivotsChunk;
        }

        if (instance.meshFromSupplemental) {
            instance.editable = false;
            instance.readOnlyReason = QStringLiteral("supplemental mesh (read-only)");
        }
        else if (!instance.pivotsChunk || instance.pivotIndex < 0) {
            instance.editable = false;
            instance.readOnlyReason = QStringLiteral("no writable pivot binding");
        }
        else {
            instance.editable = true;
        }

        m_visibleInstances.push_back(instance);
    }
}

void RenderViewportWidget::RebuildBackendOverrides() {
    m_backendWorldOverrides.clear();

    if (!m_pivotLocalOverrides.empty()) {
        for (const auto& instance : m_visibleInstances) {
            m_backendWorldOverrides[instance.key] = instance.world;
        }
    }

    if (m_backend) {
        m_backend->SetSelectedInstance(m_selectedInstance);
        m_backend->SetTransformOverrides(m_backendWorldOverrides);
        m_backend->SetHiddenInstances(m_hiddenInstances);
    }
}

void RenderViewportWidget::PerformPick(const QPoint& pos) {
    if (m_visibleInstances.empty()) {
        SetSelectedVisibleInstance(-1, false);
        return;
    }

    Vec3 rayOrigin{};
    Vec3 rayDir{};
    if (!BuildRayFromScreen(pos, rayOrigin, rayDir)) {
        return;
    }

    float bestT = std::numeric_limits<float>::max();
    int bestIndex = -1;

    for (int i = 0; i < static_cast<int>(m_visibleInstances.size()); ++i) {
        const auto& instance = m_visibleInstances[static_cast<std::size_t>(i)];
        if (instance.hiddenByUser) {
            continue;
        }
        if (instance.meshIndex < 0 || instance.meshIndex >= static_cast<int>(m_sceneResult.scene.meshes.size())) {
            continue;
        }

        const auto& mesh = m_sceneResult.scene.meshes[static_cast<std::size_t>(instance.meshIndex)];
        const Vec3 worldCenter = TransformPoint(instance.world, mesh.boundsCenter);
        if (!RayIntersectsSphere(rayOrigin, rayDir, worldCenter, std::max(0.1f, mesh.boundsRadius))) {
            continue;
        }

        for (std::size_t tri = 0; tri + 2 < mesh.indices.size(); tri += 3) {
            const uint32_t ia = mesh.indices[tri + 0];
            const uint32_t ib = mesh.indices[tri + 1];
            const uint32_t ic = mesh.indices[tri + 2];
            if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) {
                continue;
            }

            const Vec3 v0 = TransformPoint(instance.world, mesh.vertices[ia].position);
            const Vec3 v1 = TransformPoint(instance.world, mesh.vertices[ib].position);
            const Vec3 v2 = TransformPoint(instance.world, mesh.vertices[ic].position);

            float t = 0.0f;
            if (RayIntersectsTriangle(rayOrigin, rayDir, v0, v1, v2, t)) {
                if (t > 0.0f && t < bestT) {
                    bestT = t;
                    bestIndex = i;
                }
            }
        }
    }

    SetSelectedVisibleInstance(bestIndex, true);
}

void RenderViewportWidget::DrawSceneBrowserOverlay() {
    if (!m_showSceneBrowser) {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.45f);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoNav;
        if (ImGui::Begin("Scene Browser Toggle", nullptr, flags)) {
            if (ImGui::Button("Scene Browser")) {
                m_showSceneBrowser = true;
            }
        }
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(
        ImVec2(360.0f, std::max(220.0f, static_cast<float>(height()) * 0.62f)),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.86f);

    if (!ImGui::Begin("Scene Browser", &m_showSceneBrowser, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const int totalInstances = static_cast<int>(m_visibleInstances.size());
    const int hiddenInstances = static_cast<int>(std::count_if(
        m_visibleInstances.begin(),
        m_visibleInstances.end(),
        [](const VisibleInstance& instance) { return instance.hiddenByUser; }));
    ImGui::Text("Scene instances: %d", totalInstances);
    ImGui::Text("Drawn: %d  Hidden: %d", totalInstances - hiddenInstances, hiddenInstances);
    bool visibilityChanged = false;
    if (hiddenInstances > 0) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Unhide All")) {
            m_hiddenInstances.clear();
            visibilityChanged = true;
        }
    }
    ImGui::TextUnformatted("Select by mesh name. B toggles this panel.");
    ImGui::Separator();
    ImGui::BeginChild("##SceneBrowserList", ImVec2(0.0f, 0.0f), false);

    int requestedSelection = -1;

    auto hierarchyNameFor = [&](int hierarchyIndex) -> QString {
        if (hierarchyIndex >= 0
            && hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            const QString name =
                QString::fromStdString(m_sceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)].name);
            if (!name.isEmpty()) {
                return name;
            }
        }
        return QStringLiteral("Hierarchy %1").arg(hierarchyIndex);
    };

    auto pivotNameFor = [&](int hierarchyIndex, int pivotIndex) -> QString {
        if (hierarchyIndex >= 0
            && hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
            if (pivotIndex >= 0 && pivotIndex < static_cast<int>(hierarchy.pivots.size())) {
                const QString name = QString::fromStdString(hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].name);
                if (!name.isEmpty()) {
                    return name;
                }
            }
        }
        return QStringLiteral("Pivot %1").arg(pivotIndex);
    };

    auto meshNameFor = [&](const VisibleInstance& instance) -> QString {
        if (instance.meshIndex >= 0 && instance.meshIndex < static_cast<int>(m_sceneResult.scene.meshes.size())) {
            const QString fullName =
                QString::fromStdString(m_sceneResult.scene.meshes[static_cast<std::size_t>(instance.meshIndex)].fullName);
            if (!fullName.isEmpty()) {
                return fullName;
            }
        }
        return QStringLiteral("Mesh %1").arg(instance.meshIndex);
    };

    auto drawInstanceItem = [&](int visibleIndex) {
        const auto& instance = m_visibleInstances[static_cast<std::size_t>(visibleIndex)];
        bool isVisibleInViewport = !m_hiddenInstances.contains(instance.key);
        QString label = meshNameFor(instance);
        label += (instance.key.kind == RenderInstanceKind::LodEntry)
            ? QStringLiteral(" [LOD]")
            : QStringLiteral(" [Node]");
        if (!isVisibleInViewport) {
            label += QStringLiteral(" [HIDDEN]");
        }
        if (!instance.editable) {
            label += QStringLiteral(" [RO]");
        }

        const QByteArray labelUtf8 = label.toUtf8();
        ImGui::PushID(visibleIndex);
        if (ImGui::Checkbox("##Visible", &isVisibleInViewport)) {
            if (isVisibleInViewport) {
                m_hiddenInstances.erase(instance.key);
            }
            else {
                m_hiddenInstances.insert(instance.key);
            }
            visibilityChanged = true;
        }
        ImGui::SameLine();

        const bool dimmed = !isVisibleInViewport;
        if (dimmed) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.6f);
        }
        if (!instance.editable) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.86f, 0.74f, 0.55f, 1.0f));
        }
        if (ImGui::Selectable(labelUtf8.constData(), visibleIndex == m_selectedVisibleIndex)) {
            requestedSelection = visibleIndex;
        }
        if (ImGui::IsItemHovered()) {
            if (!instance.readOnlyReason.isEmpty()) {
                const QByteArray reason = instance.readOnlyReason.toUtf8();
                ImGui::SetTooltip("%s", reason.constData());
            }
            else if (!isVisibleInViewport) {
                ImGui::SetTooltip("Hidden in viewport");
            }
        }
        if (!instance.editable) {
            ImGui::PopStyleColor();
        }
        if (dimmed) {
            ImGui::PopStyleVar();
        }
        ImGui::PopID();
    };

    std::vector<int> looseInstances;
    std::vector<int> hierarchyIndices;
    looseInstances.reserve(m_visibleInstances.size());
    hierarchyIndices.reserve(m_sceneResult.scene.hierarchies.size());

    for (int i = 0; i < static_cast<int>(m_visibleInstances.size()); ++i) {
        const auto& instance = m_visibleInstances[static_cast<std::size_t>(i)];
        const int hierarchyIndex = instance.hierarchyIndex;
        if (hierarchyIndex >= 0 && hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            if (std::find(hierarchyIndices.begin(), hierarchyIndices.end(), hierarchyIndex) == hierarchyIndices.end()) {
                hierarchyIndices.push_back(hierarchyIndex);
            }
        }
        else {
            looseInstances.push_back(i);
        }
    }

    std::sort(
        hierarchyIndices.begin(),
        hierarchyIndices.end(),
        [&](int a, int b) {
            return QString::compare(hierarchyNameFor(a), hierarchyNameFor(b), Qt::CaseInsensitive) < 0;
        });

    if (looseInstances.empty() && hierarchyIndices.empty()) {
        ImGui::TextUnformatted("No instances available.");
    }

    if (!looseInstances.empty()) {
        if (ImGui::TreeNodeEx("##LooseInstances", ImGuiTreeNodeFlags_DefaultOpen, "Unbound (%d)", static_cast<int>(looseInstances.size()))) {
            for (const int visibleIndex : looseInstances) {
                drawInstanceItem(visibleIndex);
            }
            ImGui::TreePop();
        }
    }

    for (const int hierarchyIndex : hierarchyIndices) {
        std::vector<int> hierarchyInstances;
        for (int i = 0; i < static_cast<int>(m_visibleInstances.size()); ++i) {
            if (m_visibleInstances[static_cast<std::size_t>(i)].hierarchyIndex == hierarchyIndex) {
                hierarchyInstances.push_back(i);
            }
        }

        const QString hierarchyLabelText = QStringLiteral("%1 (%2)")
            .arg(hierarchyNameFor(hierarchyIndex))
            .arg(static_cast<int>(hierarchyInstances.size()));
        const QByteArray hierarchyLabel = hierarchyLabelText.toUtf8();

        ImGui::PushID(hierarchyIndex);
        if (ImGui::TreeNodeEx("##HierarchyNode", ImGuiTreeNodeFlags_DefaultOpen, "%s", hierarchyLabel.constData())) {
            const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
            std::vector<std::vector<int>> pivotChildren(hierarchy.pivots.size());
            for (int pivotIndex = 0; pivotIndex < static_cast<int>(hierarchy.pivots.size()); ++pivotIndex) {
                const int parent = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].parentIndex;
                if (parent >= 0 && parent < static_cast<int>(hierarchy.pivots.size())) {
                    pivotChildren[static_cast<std::size_t>(parent)].push_back(pivotIndex);
                }
            }

            for (auto& children : pivotChildren) {
                std::sort(
                    children.begin(),
                    children.end(),
                    [&](int a, int b) {
                        return QString::compare(
                            pivotNameFor(hierarchyIndex, a),
                            pivotNameFor(hierarchyIndex, b),
                            Qt::CaseInsensitive) < 0;
                    });
            }

            std::vector<std::vector<int>> pivotInstances(hierarchy.pivots.size());
            std::vector<int> unboundInstances;
            for (const int visibleIndex : hierarchyInstances) {
                const auto& instance = m_visibleInstances[static_cast<std::size_t>(visibleIndex)];
                if (instance.pivotIndex >= 0
                    && instance.pivotIndex < static_cast<int>(hierarchy.pivots.size())) {
                    pivotInstances[static_cast<std::size_t>(instance.pivotIndex)].push_back(visibleIndex);
                }
                else {
                    unboundInstances.push_back(visibleIndex);
                }
            }

            if (!unboundInstances.empty()) {
                if (ImGui::TreeNodeEx("##HierarchyUnbound", ImGuiTreeNodeFlags_DefaultOpen, "Unbound (%d)", static_cast<int>(unboundInstances.size()))) {
                    for (const int visibleIndex : unboundInstances) {
                        drawInstanceItem(visibleIndex);
                    }
                    ImGui::TreePop();
                }
            }

            std::vector<uint8_t> drawn(hierarchy.pivots.size(), 0);
            std::function<void(int)> drawPivot = [&](int pivotIndex) {
                if (pivotIndex < 0 || pivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
                    return;
                }
                if (drawn[static_cast<std::size_t>(pivotIndex)] != 0) {
                    return;
                }
                drawn[static_cast<std::size_t>(pivotIndex)] = 1;

                const auto& instances = pivotInstances[static_cast<std::size_t>(pivotIndex)];
                const auto& children = pivotChildren[static_cast<std::size_t>(pivotIndex)];

                const QString pivotLabelText = QStringLiteral("%1 (%2)")
                    .arg(pivotNameFor(hierarchyIndex, pivotIndex))
                    .arg(static_cast<int>(instances.size()));
                const QByteArray pivotLabel = pivotLabelText.toUtf8();

                ImGui::PushID(pivotIndex);
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
                if (instances.empty() && children.empty()) {
                    flags |= ImGuiTreeNodeFlags_Leaf;
                }
                if (ImGui::TreeNodeEx("##PivotNode", flags, "%s", pivotLabel.constData())) {
                    for (const int visibleIndex : instances) {
                        drawInstanceItem(visibleIndex);
                    }
                    for (const int childPivotIndex : children) {
                        drawPivot(childPivotIndex);
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            };

            for (int pivotIndex = 0; pivotIndex < static_cast<int>(hierarchy.pivots.size()); ++pivotIndex) {
                const int parent = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].parentIndex;
                if (parent < 0 || parent >= static_cast<int>(hierarchy.pivots.size())) {
                    drawPivot(pivotIndex);
                }
            }
            for (int pivotIndex = 0; pivotIndex < static_cast<int>(hierarchy.pivots.size()); ++pivotIndex) {
                if (drawn[static_cast<std::size_t>(pivotIndex)] == 0) {
                    drawPivot(pivotIndex);
                }
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::End();

    if (visibilityChanged
        && m_selectedVisibleIndex >= 0
        && m_selectedVisibleIndex < static_cast<int>(m_visibleInstances.size())) {
        SetSelectedVisibleInstance(m_selectedVisibleIndex, false);
    }
    if (requestedSelection >= 0) {
        SetSelectedVisibleInstance(requestedSelection, true);
    }
}

bool RenderViewportWidget::HandleGizmos(const Mat4& view, const Mat4& projection) {
    bool changed = false;

    m_gizmoOver = false;
    m_gizmoUsing = false;

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(0.0f, 0.0f, static_cast<float>(width()), static_cast<float>(height()));

    float viewMatrix[16];
    float projectionMatrix[16];
    Mat4ToFloatArray(view, viewMatrix);
    Mat4ToFloatArray(projection, projectionMatrix);

    float viewMatrixBefore[16];
    std::memcpy(viewMatrixBefore, viewMatrix, sizeof(viewMatrixBefore));
    if (m_settings.showCameraGizmo) {
        ImGuizmo::ViewManipulate(
            viewMatrix,
            m_camera.distance,
            ImVec2(static_cast<float>(std::max(0, width() - 140)), 20.0f),
            ImVec2(120.0f, 120.0f),
            0x30303080u);
    }

    DrawSceneBrowserOverlay();

    if (std::memcmp(viewMatrixBefore, viewMatrix, sizeof(viewMatrixBefore)) != 0) {
        const Mat4 manipulatedView = Mat4FromFloatArray(viewMatrix);
        const Mat4 invView = Inverse(manipulatedView);
        const Vec3 forward = Normalize({ invView.m[8], invView.m[9], invView.m[10] });
        if (Length(forward) > 1e-4f) {
            m_camera.yaw = std::atan2(forward.x, forward.z);
            m_camera.pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
            changed = true;
        }
    }

    if (m_selectedVisibleIndex >= 0
        && m_selectedVisibleIndex < static_cast<int>(m_visibleInstances.size())) {
        const auto& selected = m_visibleInstances[static_cast<std::size_t>(m_selectedVisibleIndex)];
        if (!m_hiddenInstances.contains(selected.key)
            && selected.editable
            && selected.hierarchyIndex >= 0
            && selected.pivotIndex >= 0
            && selected.hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            float modelMatrix[16];
            Mat4ToFloatArray(selected.world, modelMatrix);

            const Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
            const bool snap = (mods & Qt::ControlModifier) != 0;
            float snapValues[3] = { 1.0f, 1.0f, 1.0f };
            ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
            ImGuizmo::MODE mode = ImGuizmo::WORLD;
            if (m_gizmoMode == GizmoMode::Rotate) {
                operation = ImGuizmo::ROTATE;
                mode = ImGuizmo::LOCAL;
                snapValues[0] = 15.0f;
                snapValues[1] = 15.0f;
                snapValues[2] = 15.0f;
            }

            ImGuizmo::Manipulate(
                viewMatrix,
                projectionMatrix,
                operation,
                mode,
                modelMatrix,
                nullptr,
                snap ? snapValues : nullptr);

            m_gizmoOver = ImGuizmo::IsOver();
            m_gizmoUsing = ImGuizmo::IsUsing();

            if (m_gizmoUsing) {
                const auto worlds = BuildHierarchyWorldTransforms();
                Mat4 parentWorld = Mat4::Identity();
                const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(selected.hierarchyIndex)];
                if (selected.pivotIndex >= 0
                    && selected.pivotIndex < static_cast<int>(hierarchy.pivots.size())) {
                    const int parentIndex = hierarchy.pivots[static_cast<std::size_t>(selected.pivotIndex)].parentIndex;
                    if (parentIndex >= 0
                        && parentIndex < static_cast<int>(worlds[static_cast<std::size_t>(selected.hierarchyIndex)].size())) {
                        parentWorld = worlds[static_cast<std::size_t>(selected.hierarchyIndex)][static_cast<std::size_t>(parentIndex)];
                    }
                }

                const Mat4 manipulatedWorld = Mat4FromFloatArray(modelMatrix);
                const Mat4 newLocal = Multiply(Inverse(parentWorld), manipulatedWorld);

                PivotKey key{};
                key.hierarchyIndex = selected.hierarchyIndex;
                key.pivotIndex = selected.pivotIndex;
                m_pivotLocalOverrides[key] = newLocal;
                changed = true;
            }
        }
    }

    if (!m_gizmoUsing && m_gizmoWasUsing) {
        CommitPivotOverrideIfNeeded();
        changed = true;
    }

    m_gizmoWasUsing = m_gizmoUsing;
    return changed;
}

void RenderViewportWidget::CommitPivotOverrideIfNeeded() {
    if (m_selectedVisibleIndex < 0
        || m_selectedVisibleIndex >= static_cast<int>(m_visibleInstances.size())) {
        ClearPivotOverrides();
        return;
    }

    const auto selected = m_visibleInstances[static_cast<std::size_t>(m_selectedVisibleIndex)];
    if (!selected.editable || !selected.pivotsChunk || selected.pivotIndex < 0) {
        ClearPivotOverrides();
        return;
    }

    PivotKey key{};
    key.hierarchyIndex = selected.hierarchyIndex;
    key.pivotIndex = selected.pivotIndex;
    const auto it = m_pivotLocalOverrides.find(key);
    if (it == m_pivotLocalOverrides.end()) {
        ClearPivotOverrides();
        return;
    }

    const Mat4 local = it->second;
    const Quaternion q = QuaternionFromMatrix(local);
    const float tx = local.m[12];
    const float ty = local.m[13];
    const float tz = local.m[14];

    void* pivotsChunkPtr = const_cast<::ChunkItem*>(selected.pivotsChunk);
    const int pivotIndex = selected.pivotIndex;

    QTimer::singleShot(0, this, [this, pivotsChunkPtr, pivotIndex, tx, ty, tz, q]() {
        emit pivotTransformCommitRequested(
            pivotsChunkPtr,
            pivotIndex,
            tx,
            ty,
            tz,
            q.x,
            q.y,
            q.z,
            q.w);
    });

    ClearPivotOverrides();
}

void RenderViewportWidget::ClearPivotOverrides() {
    m_pivotLocalOverrides.clear();
    m_backendWorldOverrides.clear();
    if (m_backend) {
        m_backend->SetTransformOverrides({});
    }
}

void RenderViewportWidget::SyncSelectedInstanceToVisibleList() {
    m_selectedVisibleIndex = -1;
    if (!m_selectedInstance.has_value()) {
        return;
    }

    for (int i = 0; i < static_cast<int>(m_visibleInstances.size()); ++i) {
        if (m_visibleInstances[static_cast<std::size_t>(i)].key == *m_selectedInstance) {
            m_selectedVisibleIndex = i;
            break;
        }
    }
}

void RenderViewportWidget::SetSelectedVisibleInstance(int index, bool emitChunkSignal) {
    if (index < 0 || index >= static_cast<int>(m_visibleInstances.size())) {
        m_selectedVisibleIndex = -1;
        m_selectedInstance.reset();
        m_gizmoWasUsing = false;
        ClearPivotOverrides();
        EmitSelectionStatus(QStringLiteral("Selection: none"));
        return;
    }

    if (m_selectedVisibleIndex != index) {
        m_gizmoWasUsing = false;
        ClearPivotOverrides();
    }

    m_selectedVisibleIndex = index;
    const auto& selected = m_visibleInstances[static_cast<std::size_t>(index)];
    m_selectedInstance = selected.key;
    const bool hiddenByUser = m_hiddenInstances.contains(selected.key);

    if (emitChunkSignal && selected.meshChunk) {
        emit sceneChunkActivated(const_cast<::ChunkItem*>(selected.meshChunk));
    }

    if (selected.editable) {
        EmitSelectionStatus(
            hiddenByUser
            ? QStringLiteral("Selection: editable (pivot %1, hidden)").arg(selected.pivotIndex)
            : QStringLiteral("Selection: editable (pivot %1)").arg(selected.pivotIndex));
    }
    else {
        const QString baseStatus =
            QStringLiteral("Selection: read-only (%1)").arg(selected.readOnlyReason);
        EmitSelectionStatus(hiddenByUser ? (baseStatus + QStringLiteral(", hidden")) : baseStatus);
    }
}

void RenderViewportWidget::EmitSelectionStatus(const QString& text) {
    emit selectionStatusChanged(text);
}

Vec3 RenderViewportWidget::ComputeSceneCenter() const {
    if (m_sceneResult.scene.meshes.empty()) {
        return { 0.0f, 0.0f, 0.0f };
    }

    Vec3 sum{};
    for (const auto& mesh : m_sceneResult.scene.meshes) {
        sum = sum + mesh.boundsCenter;
    }

    const float invCount = 1.0f / static_cast<float>(m_sceneResult.scene.meshes.size());
    return sum * invCount;
}

float RenderViewportWidget::ComputeSceneRadius(const Vec3& center) const {
    float radius = 10.0f;
    for (const auto& mesh : m_sceneResult.scene.meshes) {
        const float d = Length(mesh.boundsCenter - center) + std::max(1.0f, mesh.boundsRadius);
        radius = std::max(radius, d);
    }
    return radius;
}

bool RenderViewportWidget::BuildRayFromScreen(const QPoint& pos, Vec3& rayOrigin, Vec3& rayDir) const {
    const int w = std::max(1, width());
    const int h = std::max(1, height());
    if (w <= 0 || h <= 0) {
        return false;
    }

    const float ndcX = (2.0f * (static_cast<float>(pos.x()) + 0.5f) / static_cast<float>(w)) - 1.0f;
    const float ndcY = 1.0f - (2.0f * (static_cast<float>(pos.y()) + 0.5f) / static_cast<float>(h));

    const Vec3 forward = Normalize(CameraForward(m_camera.yaw, m_camera.pitch));
    Vec3 right = Normalize(Cross({ 0.0f, 1.0f, 0.0f }, forward));
    if (Length(right) < 0.001f) {
        right = { 1.0f, 0.0f, 0.0f };
    }
    const Vec3 up = Normalize(Cross(forward, right));

    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    const float tanHalfFov = std::tan(DegToRad(m_camera.fovDeg) * 0.5f);

    rayDir = Normalize(
        forward
        + right * (ndcX * aspect * tanHalfFov)
        + up * (ndcY * tanHalfFov));
    rayOrigin = m_camera.target - forward * m_camera.distance;
    return true;
}

bool RenderViewportWidget::RayIntersectsSphere(
    const Vec3& rayOrigin,
    const Vec3& rayDir,
    const Vec3& center,
    float radius) const
{
    const Vec3 toCenter = center - rayOrigin;
    const float projection = Dot(toCenter, rayDir);
    const float distSq = Dot(toCenter, toCenter) - projection * projection;
    return distSq <= (radius * radius);
}

bool RenderViewportWidget::RayIntersectsTriangle(
    const Vec3& rayOrigin,
    const Vec3& rayDir,
    const Vec3& v0,
    const Vec3& v1,
    const Vec3& v2,
    float& outT) const
{
    constexpr float kEpsilon = 1.0e-6f;
    const Vec3 edge1 = v1 - v0;
    const Vec3 edge2 = v2 - v0;
    const Vec3 pvec = Cross(rayDir, edge2);
    const float det = Dot(edge1, pvec);

    if (std::fabs(det) < kEpsilon) {
        return false;
    }

    const float invDet = 1.0f / det;
    const Vec3 tvec = rayOrigin - v0;
    const float u = Dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    const Vec3 qvec = Cross(tvec, edge1);
    const float v = Dot(rayDir, qvec) * invDet;
    if (v < 0.0f || (u + v) > 1.0f) {
        return false;
    }

    const float t = Dot(edge2, qvec) * invDet;
    if (t <= kEpsilon) {
        return false;
    }

    outT = t;
    return true;
}

std::vector<std::vector<Mat4>> RenderViewportWidget::BuildHierarchyWorldTransforms() const {
    std::vector<std::vector<Mat4>> hierarchyWorld;
    hierarchyWorld.resize(m_sceneResult.scene.hierarchies.size());

    for (std::size_t h = 0; h < m_sceneResult.scene.hierarchies.size(); ++h) {
        const auto& hierarchy = m_sceneResult.scene.hierarchies[h];
        auto& worlds = hierarchyWorld[h];
        worlds.resize(hierarchy.pivots.size(), Mat4::Identity());

        std::vector<uint8_t> state(hierarchy.pivots.size(), 0);
        std::function<void(int)> buildPivot = [&](int pivotIndex) {
            if (pivotIndex < 0 || pivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
                return;
            }
            if (state[pivotIndex] == 2) {
                return;
            }
            if (state[pivotIndex] == 1) {
                worlds[static_cast<std::size_t>(pivotIndex)] = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].localTransform;
                state[pivotIndex] = 2;
                return;
            }

            state[pivotIndex] = 1;
            Mat4 local = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].localTransform;

            PivotKey key{};
            key.hierarchyIndex = static_cast<int>(h);
            key.pivotIndex = pivotIndex;
            if (const auto it = m_pivotLocalOverrides.find(key); it != m_pivotLocalOverrides.end()) {
                local = it->second;
            }

            const int parentIndex = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].parentIndex;
            if (parentIndex >= 0 && parentIndex < static_cast<int>(hierarchy.pivots.size())) {
                buildPivot(parentIndex);
                worlds[static_cast<std::size_t>(pivotIndex)] =
                    Multiply(worlds[static_cast<std::size_t>(parentIndex)], local);
            }
            else {
                worlds[static_cast<std::size_t>(pivotIndex)] = local;
            }
            state[pivotIndex] = 2;
        };

        for (int i = 0; i < static_cast<int>(hierarchy.pivots.size()); ++i) {
            buildPivot(i);
        }
    }

    return hierarchyWorld;
}

} // namespace OW3D::Render
