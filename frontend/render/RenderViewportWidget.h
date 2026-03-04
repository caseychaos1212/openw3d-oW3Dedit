#pragma once

#include <memory>

#include <QElapsedTimer>
#include <QPoint>
#include <QStringList>
#include <QWidget>

class QTimer;

#include "IRenderBackend.h"

namespace OW3D::Render {

class RenderViewportWidget final : public QWidget {
    Q_OBJECT

public:
    explicit RenderViewportWidget(QWidget* parent = nullptr);
    ~RenderViewportWidget() override;

    void SetSceneResult(const SceneBuildResult& sceneResult);
    void SetRenderSettings(const RenderSettings& settings);
    void FocusScene();

    const CameraState& camera() const { return m_camera; }

signals:
    void sceneWarningsChanged(const QStringList& warnings);
    void frameStatsChanged(const QString& statsText);

protected:
    QPaintEngine* paintEngine() const override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class DragMode {
        None,
        Orbit,
        Pan
    };

    void EnsureBackendInitialized();
    void TickFrame();
    void EmitWarnings();
    Vec3 ComputeSceneCenter() const;
    float ComputeSceneRadius(const Vec3& center) const;

    std::unique_ptr<IRenderBackend> m_backend;
    SceneBuildResult m_sceneResult{};

    CameraState m_camera{};
    RenderSettings m_settings{};

    QElapsedTimer m_statsTimer;
    DragMode m_dragMode = DragMode::None;
    QPoint m_lastMousePos{};

    bool m_backendInitialized = false;
    bool m_sceneDirty = false;
    QTimer* m_frameTimer = nullptr;
};

} // namespace OW3D::Render
