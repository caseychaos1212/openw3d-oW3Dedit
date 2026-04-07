#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QElapsedTimer>
#include <QImage>
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
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void SetSceneResult(const SceneBuildResult& sceneResult);
    void SetRenderSettings(const RenderSettings& settings);
    void SetAnimationPlayback(const AnimationPlaybackState& playback);
    void SetAnimationEditDraft(const std::optional<RenderAnimationEditDraft>& draft);
    void SetAnimationEditingState(bool editKeysEnabled, bool clipEditable, const QString& readOnlyReason);
    void SetCameraState(const CameraState& camera);
    void SetContinuousRenderingEnabled(bool enabled);
    void SetOverlayUiEnabled(bool enabled);
    void FocusScene();
    void OpenManualPivotRotationDialog();
    void RenderOnce();
    bool CaptureCurrentFrame(QImage& outImage);

    const CameraState& camera() const { return m_camera; }

signals:
    void sceneWarningsChanged(const QStringList& warnings);
    void frameStatsChanged(const QString& statsText);
    void sceneChunkActivated(void* chunkPtr);
    void selectionStatusChanged(const QString& statusText);
    void pivotSelectionChanged(int hierarchyIndex, int pivotIndex);
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
    void animationKeyframeCommitRequested(
        int hierarchyIndex,
        int pivotIndex,
        int frameIndex,
        float tx,
        float ty,
        float tz,
        float qx,
        float qy,
        float qz,
        float qw);
    void animationKeyframeDeleteRequested(int hierarchyIndex, int pivotIndex, int frameIndex);
    void animationPlaybackPauseRequested();

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

    struct PivotEditability {
        bool editable = false;
        QString reason;
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
    void DrawPivotMarkersOverlay(const std::vector<std::vector<Mat4>>& hierarchyWorld, const Mat4& viewProjection);
    void DrawTransformInspectorOverlay();
    void CommitPivotOverrideIfNeeded();
    void CommitDisplayedLocalTransform(const PivotKey& key, const Mat4& displayedLocal);
    void ClearPivotOverrides();
    bool TryGetSelectedEditablePivot(
        PivotKey& outKey,
        const ::ChunkItem*& outPivotsChunk,
        int& outRepresentativeVisibleIndex,
        Mat4& outLocal,
        PivotEditability& outEditability,
        QString* outError = nullptr) const;
    void SyncCameraInspectorStateFromCamera();
    void ApplyCameraInspectorEdits();
    void SyncTransformInspectorState(const PivotKey& key, const Mat4& local);
    void ApplyTransformInspectorEdits();
    void SyncSelectedInstanceToVisibleList();
    void SetSelectedVisibleInstance(int index, bool emitChunkSignal);
    void SetSelectedPivot(const PivotKey& key, const ::ChunkItem* pivotsChunk, int representativeVisibleIndex, bool emitChunkSignal);
    void EmitSelectionStatus(const QString& text);
    void AutoSelectFirstEditablePivotIfNeeded();
    PivotEditability EvaluatePivotEditability(
        int hierarchyIndex,
        int pivotIndex,
        const ::ChunkItem* pivotsChunk) const;
    const RenderAnimationClip* CurrentActiveAnimationClip() const;
    const RenderAnimationClip* ActiveClipForHierarchy(int hierarchyIndex, float* outAnimationFrame = nullptr) const;
    int CurrentAnimationFrameIndex() const;
    int FindRepresentativeVisibleInstance(const PivotKey& key) const;
    Mat4 ComputeUnderlyingLocalTransform(int hierarchyIndex, int pivotIndex) const;
    Mat4 ComputeDisplayedLocalTransform(int hierarchyIndex, int pivotIndex) const;
    void PreviewTransformInspectorEdits();
    void ResetTransformInspectorToOriginal();
    bool IsAnimationEditModeActive() const;
    void MaybePauseAnimationForEditing();

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
    AnimationPlaybackState m_animationPlayback{};
    std::optional<RenderAnimationEditDraft> m_animationEditDraft;
    bool m_animationEditKeysEnabled = false;
    bool m_animationClipEditable = false;
    QString m_animationClipReadOnlyReason;

    QElapsedTimer m_statsTimer;
    QElapsedTimer m_deltaTimer;
    DragMode m_dragMode = DragMode::None;
    QPoint m_lastMousePos{};

    bool m_backendInitialized = false;
    bool m_sceneDirty = false;
    QTimer* m_frameTimer = nullptr;
    bool m_continuousRenderingEnabled = true;
    bool m_overlayUiEnabled = true;

    bool m_imguiInitialized = false;
    bool m_pendingPick = false;
    QPoint m_pickPressPos{};

    GizmoMode m_gizmoMode = GizmoMode::Translate;
    bool m_showSceneBrowser = true;
    bool m_showTransformInspector = true;
    bool m_gizmoOver = false;
    bool m_gizmoUsing = false;
    bool m_gizmoWasUsing = false;

    std::vector<VisibleInstance> m_visibleInstances;
    std::optional<RenderInstanceKey> m_selectedInstance;
    int m_selectedVisibleIndex = -1;
    std::optional<PivotKey> m_selectedPivot;
    const ::ChunkItem* m_selectedPivotsChunk = nullptr;
    int m_selectedPivotRepresentativeVisibleIndex = -1;
    bool m_autoSelectEditablePivotPending = false;

    std::unordered_map<PivotKey, Mat4, PivotKeyHash> m_pivotLocalOverrides;
    std::unordered_map<RenderInstanceKey, Mat4, RenderInstanceKeyHash> m_backendWorldOverrides;
    std::unordered_set<RenderInstanceKey, RenderInstanceKeyHash> m_hiddenInstances;
    Vec3 m_cameraInspectorTarget{};
    float m_cameraInspectorYawDegrees = 0.0f;
    float m_cameraInspectorPitchDegrees = 0.0f;
    float m_cameraInspectorDistance = 0.0f;
    bool m_transformInspectorHasSelection = false;
    PivotKey m_transformInspectorPivot{};
    Vec3 m_transformInspectorTranslation{};
    Vec3 m_transformInspectorRotationDegrees{};
};

} // namespace OW3D::Render
