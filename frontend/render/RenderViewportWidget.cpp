#include "RenderViewportWidget.h"

#include <algorithm>
#include <cmath>

#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEngine>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>

#include "D3D11RenderBackend.h"

namespace OW3D::Render {
namespace {

Vec3 CameraForward(float yaw, float pitch) {
    const float cp = std::cos(pitch);
    return {
        std::sin(yaw) * cp,
        std::sin(pitch),
        std::cos(yaw) * cp
    };
}

} // namespace

RenderViewportWidget::RenderViewportWidget(QWidget* parent)
    : QWidget(parent)
    , m_backend(std::make_unique<D3D11RenderBackend>()) {
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
}

RenderViewportWidget::~RenderViewportWidget() {
    if (m_frameTimer) {
        m_frameTimer->stop();
    }
    if (m_backend) {
        m_backend->Shutdown();
    }
}

void RenderViewportWidget::SetSceneResult(const SceneBuildResult& sceneResult) {
    m_sceneResult = sceneResult;
    m_sceneDirty = true;
    EmitWarnings();
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
    if (event->button() == Qt::LeftButton) {
        m_dragMode = DragMode::Orbit;
    }
    else if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_dragMode = DragMode::Pan;
    }

    m_lastMousePos = event->pos();
    setFocus(Qt::MouseFocusReason);
    event->accept();
}

void RenderViewportWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

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
    if (event->button() == Qt::LeftButton
        || event->button() == Qt::MiddleButton
        || event->button() == Qt::RightButton) {
        m_dragMode = DragMode::None;
    }
    event->accept();
}

void RenderViewportWidget::wheelEvent(QWheelEvent* event) {
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

    m_backendInitialized = true;
    m_backend->SetRenderSettings(m_settings);
    m_backend->SetCamera(m_camera);
    m_sceneDirty = true;
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

    m_backend->SetRenderSettings(m_settings);
    m_backend->SetCamera(m_camera);
    m_backend->RenderFrame();

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

} // namespace OW3D::Render
