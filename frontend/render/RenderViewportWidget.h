#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QElapsedTimer>
#include <QPoint>
#include <QStringList>
#include <QWidget>

class ChunkItem;
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
    void sceneChunkActivated(void* chunkPtr);
    void selectionStatusChanged(const QString& statusText);
    void pivotTransformCommitRequested(
        void* pivotsChunkPtr,
        int pivotIndex,
        float tx,
        float ty,
        float tz,
        float qx,
        float qy,
        float qz,
        float qw);

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

    enum class GizmoMode {
        Translate,
        Rotate
    };

    struct PivotKey {
        int hierarchyIndex = -1;
        int pivotIndex = -1;

        bool operator==(const PivotKey& other) const = default;
    };

    struct PivotKeyHash {
        std::size_t operator()(const PivotKey& key) const noexcept {
            std::size_t h = static_cast<std::size_t>(key.hierarchyIndex + 0x9E3779B9);
            h = (h * 16777619u) ^ static_cast<std::size_t>(key.pivotIndex + 0x7F4A7C15);
            return h;
        }
    };

    struct VisibleInstance {
        RenderInstanceKey key{};
        int meshIndex = -1;
        int hierarchyIndex = -1;
        int pivotIndex = -1;
        Mat4 world = Mat4::Identity();
        const ::ChunkItem* meshChunk = nullptr;
        const ::ChunkItem* pivotsChunk = nullptr;
        bool meshFromSupplemental = false;
        bool editable = false;
        bool hiddenByUser = false;
        QString readOnlyReason;
    };

    void EnsureBackendInitialized();
    void ShutdownImGui();
    void TickFrame();
    void EmitWarnings();
    void BuildVisibleInstances(const Vec3& cameraPos);
    void RebuildBackendOverrides();
    void PerformPick(const QPoint& pos);
    bool HandleGizmos(const Mat4& view, const Mat4& projection);
    void DrawSceneBrowserOverlay();
    void CommitPivotOverrideIfNeeded();
    void ClearPivotOverrides();
    void SyncSelectedInstanceToVisibleList();
    void SetSelectedVisibleInstance(int index, bool emitChunkSignal);
    void EmitSelectionStatus(const QString& text);

    Vec3 ComputeSceneCenter() const;
    float ComputeSceneRadius(const Vec3& center) const;

    bool BuildRayFromScreen(const QPoint& pos, Vec3& rayOrigin, Vec3& rayDir) const;
    bool RayIntersectsSphere(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& center, float radius) const;
    bool RayIntersectsTriangle(
        const Vec3& rayOrigin,
        const Vec3& rayDir,
        const Vec3& v0,
        const Vec3& v1,
        const Vec3& v2,
        float& outT) const;

    std::vector<std::vector<Mat4>> BuildHierarchyWorldTransforms() const;

    std::unique_ptr<IRenderBackend> m_backend;
    SceneBuildResult m_sceneResult{};

    CameraState m_camera{};
    RenderSettings m_settings{};

    QElapsedTimer m_statsTimer;
    QElapsedTimer m_deltaTimer;
    DragMode m_dragMode = DragMode::None;
    QPoint m_lastMousePos{};

    bool m_backendInitialized = false;
    bool m_sceneDirty = false;
    QTimer* m_frameTimer = nullptr;

    bool m_imguiInitialized = false;
    bool m_pendingPick = false;
    QPoint m_pickPressPos{};

    GizmoMode m_gizmoMode = GizmoMode::Translate;
    bool m_showSceneBrowser = true;
    bool m_gizmoOver = false;
    bool m_gizmoUsing = false;
    bool m_gizmoWasUsing = false;

    std::vector<VisibleInstance> m_visibleInstances;
    std::optional<RenderInstanceKey> m_selectedInstance;
    int m_selectedVisibleIndex = -1;

    std::unordered_map<PivotKey, Mat4, PivotKeyHash> m_pivotLocalOverrides;
    std::unordered_map<RenderInstanceKey, Mat4, RenderInstanceKeyHash> m_backendWorldOverrides;
    std::unordered_set<RenderInstanceKey, RenderInstanceKeyHash> m_hiddenInstances;
};

} // namespace OW3D::Render
