#pragma once

#include <QMainWindow>
#include <QElapsedTimer>
#include <QSet>
#include <array>
#include <memory>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "backend/ChunkData.h"
#include "backend/render/RenderScene.h"
#include "frontend/render/IRenderBackend.h"
#include <QString>
#include <QByteArray>

class QTreeWidget;
class QTableWidget;
class QStackedWidget;
class QSplitter;
class QScrollArea;
class QCloseEvent;
class QTimer;
class QCheckBox;
class QPlainTextEdit;
class QGroupBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSlider;
class QSpinBox;
class QTabWidget;
class QLabel;
class QTreeWidgetItem;
class MeshEditorWidget;
class RawTextEditorWidget;
class StringEditorWidget;
class HierarchyHeaderEditorWidget;
class AnimationHierarchyEditorWidget;
class TransformNodeEditorWidget;
class MapperArgsEditorWidget;
class MaterialEditorWidget;
class ShaderEditorWidget;
class SurfaceTypeEditorWidget;
class TriangleSurfaceTypeEditorWidget;
class TextureInfoEditorWidget;

namespace OW3D::Render {
class RenderViewportWidget;
}

class ChunkData; // forward declare

struct ArchiveRenderEntryInfo {
    QString name;
    uint32_t id = 0;
    uint32_t offset = 0;
    uint32_t size = 0;
    bool likelyW3d = false;
};

struct ArchiveTextureSourceInfo {
    uint32_t id = 0;
    QString archivePath;
    uint32_t offset = 0;
    uint32_t size = 0;
    QString name;
};

enum class RenderSessionAssetRole {
    Skeleton,
    AnimationLibrary
};

struct RenderSessionAsset {
    RenderSessionAssetRole role = RenderSessionAssetRole::Skeleton;
    QString filePath;
    QString sourceKey;
    QString sourceDisplayPath;
    QString displayLabel;
    std::vector<std::shared_ptr<ChunkItem>> roots;
    QSet<QString> hierarchyNames;
    int meshCount = 0;
    int animationCount = 0;
};

struct AggregateRenderDependencyAsset {
    QString sourceKey;
    QString sourceLabel;
    std::vector<std::shared_ptr<ChunkItem>> roots;
};

struct RenderAnimationClipIdentity {
    QString fullName;
    QString hierarchyName;
    QString sourceFileLabel;
    bool sourceFromAnimationLibrary = false;
    bool compressed = false;

    bool operator==(const RenderAnimationClipIdentity& other) const = default;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);

private slots:
    void newFile();
    void openFile(const QString& path = QString());
    void ClearChunkTree();
    void handleTreeSelection();
    void saveFile();
    void saveFileAs();
    void onChunkEdited();
    void onAnimationHeaderRenamed(
        const QString& oldAnimationName,
        const QString& newAnimationName,
        const QString& oldHierarchyName,
        const QString& newHierarchyName);
    void onMeshRenamed(const QString& oldMeshName,
        const QString& newMeshName,
        const QString& oldContainerName,
        const QString& newContainerName);
    void LoadRecentFiles();
    void SaveRecentFiles();
    void UpdateRecentFilesMenu();
    void AddRecentFile(const QString& path);
    void OpenRecentFile();  // will be connected to recent file actions
    void on_actionExportChunkList_triggered();
    void on_actionExportJsonBatch_triggered();
    void on_actionValidateRoundTripBatch_triggered();
    void on_actionCopyPureHumanAnimationsBySkeleton_triggered();
    void on_actionExportSkeletonAAnimationGifs_triggered();
    void exportJson();
    void importJson();
    void showHierarchyBrowser();
    void addTopLevelChunk();
    void insertChunkBefore();
    void insertChunkAfter();
    void addChildChunk();
    void deleteSelectedChunk();
    void moveChunkUp();
    void moveChunkDown();
    void moveHierarchyBoneToEnd();
    void addRenderSkeletons();
    void addRenderAnimations();
    void selectRenderTextureFolder();
    void clearRenderTextureFolder();
    void removeSelectedRenderSessionAsset();
    void clearRenderAnimationLibraries();
    void handleRenderAnimationSelectionChanged();
    void handleRenderAnimationEditKeysChanged(bool checked);
    void handleRenderPrepSourceClipChanged(int index);
    void handleRenderPrepStaticPoseFrameChanged(int value);
    void applyRenderAnimationClipPrep();
    void applyRenderAnimationStaticPoseToClip();
    void applyRenderAnimationFitSourceToClip();
    void handleRenderBlendSourceClipChanged(int index);
    void handleRenderBlendTimingModeChanged(int index);
    void handleRenderBlendPivotItemChanged(QTreeWidgetItem* item, int column);
    void handleRenderBlendPivotCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void handleRenderBlendIncludeDescendantsChanged(bool checked);
    void handleRenderBlendTruncateChanged(bool checked);
    void handleRenderBlendNormalizeFrameRateChanged(bool checked);
    void handleRenderBlendPinLeftHandChanged(bool checked);
    void handleRenderBlendPinRightHandChanged(bool checked);
    void handleRenderBlendHandPinReferenceFrameChanged(int value);
    void handleRenderBlendTranslationPercentChanged(int value);
    void handleRenderBlendRotationPercentChanged(int value);
    void handleRenderBlendPivotAxisWeightChanged(int value);
    void resetRenderBlendSelectedPivotWeights();
    void handleRenderBlendStartFrameChanged(int value);
    void handleRenderBlendEndFrameChanged(int value);
    void applyRenderAnimationPivotOverride();
    void toggleRenderAnimationPlayback();
    void stopRenderAnimationPlayback();
    void exportActiveRenderAnimationGif();
    void resetActiveRenderAnimationDraft();
    void handleRenderAnimationLoopChanged(bool checked);
    void handleRenderAnimationSpeedChanged(double value);
    void handleRenderAnimationFrameSliderChanged(int value);
    void handleRenderAnimationPlaybackTimerTick();
    void handleViewportPivotSelectionChanged(int hierarchyIndex, int pivotIndex);
    void handleViewportAnimationKeyframeCommit(
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
    void handleViewportAnimationKeyframeDelete(int hierarchyIndex, int pivotIndex, int frameIndex);
    void handleViewportAnimationPlaybackPauseRequested();
    void handleRenderAnimationPivotSheetCellClicked(int row, int column);
    void selectChunkInTree(void* chunkPtr);
    void handleChunkSourceTabChanged(int index);

protected:
    void closeEvent(QCloseEvent* event) override;
private:
    enum class ValidatorRunMode {
        Both,
        StructuredPreferred,
        HexOnly
    };

    void populateTree();
    void rebuildChunkSourceTabs();
    JsonSerializationMode loadDefaultSerializationModeSetting() const;
    void saveDefaultSerializationModeSetting(JsonSerializationMode mode) const;
    bool promptSerializationMode(
        const QString& title,
        const QString& prompt,
        JsonSerializationMode& outMode);
    ValidatorRunMode loadValidatorRunModeSetting() const;
    void saveValidatorRunModeSetting(ValidatorRunMode mode) const;
    bool promptValidatorRunMode(ValidatorRunMode& outMode);
    void rebuildRenderScene();
    void applyRenderSettingsToViewport();
    void clearArchiveRenderContext();
    void clearExternalRenderContext();
    void resetRenderAnimationPlayback();
    void syncRenderAnimationPlaybackToViewport();
    void syncActiveRenderAnimationDraftToViewport();
    void syncRenderAnimationEditingStateToViewport();
    void syncRenderAnimationUi();
    void refreshRenderAssetList();
    void refreshRenderAnimationList();
    void refreshRenderPlaybackControls();
    void refreshRenderPlaybackSelection();
    void refreshRenderAnimationPivotSheet();
    void refreshRenderAnimationPivotSheetFrameHighlight();
    void refreshRenderAnimationPrepControls();
    void refreshRenderBlendControls();
    void refreshRenderBlendPivotWeightEditor();
    void refreshRenderBlendStatus();
    void setRenderActiveAnimationIndex(int animationIndex, bool startPlaying, bool resetTime);
    int findRenderAnimationIndexByIdentity(const RenderAnimationClipIdentity& identity) const;
    int findCompatibleRenderHierarchyIndexForAnimation(int animationIndex) const;
    bool tryLoadRenderSessionAsset(
        const QString& filePath,
        RenderSessionAssetRole role,
        RenderSessionAsset& outAsset,
        QString* outError = nullptr) const;
    bool tryLoadRenderSessionArchiveAsset(
        const QString& archivePath,
        RenderSessionAssetRole role,
        RenderSessionAsset& outAsset,
        QString* outError = nullptr);
    void handleViewportChunkActivated(void* chunkPtr);
    void handleViewportPivotTransformCommit(
        void* pivotsChunkPtr,
        int pivotIndex,
        float tx,
        float ty,
        float tz,
        float qx,
        float qy,
        float qz,
        float qw);
    OW3D::Render::RenderAnimationEditDraft* ensureRenderAnimationEditDraft(
        const OW3D::Render::RenderAnimationClip& clip,
        const std::shared_ptr<ChunkItem>& animationChunk);
    const OW3D::Render::RenderAnimationEditDraft* findRenderAnimationEditDraftForClip(
        const OW3D::Render::RenderAnimationClip& clip) const;
    const OW3D::Render::RenderAnimationEditDraft* findActiveRenderAnimationEditDraft() const;
    bool flushPendingRenderAnimationDrafts(QString* outError = nullptr);
    void undoRenderTransform();
    void redoRenderTransform();
    const std::vector<std::shared_ptr<ChunkItem>>* chunkRootsForSourceKey(const QString& sourceKey) const;
    std::shared_ptr<ChunkItem> findChunkInActiveSource(const void* targetPtr) const;
    QString chunkSourceKeyForTree(const QTreeWidget* tree) const;
    QString activeChunkSourceLabel() const;
    bool activeChunkSourceEditable() const;
    void syncActiveChunkSourceTree();

    QTabWidget* chunkTreeTabs = nullptr;
    QTreeWidget* treeWidget = nullptr;
    QTableWidget* tableWidget = nullptr;
    QSplitter* splitter = nullptr;
    QSplitter* detailSplitter = nullptr;
    QScrollArea* editorScrollArea = nullptr;
    QCheckBox* rawHexToggle = nullptr;
    QPlainTextEdit* rawHexEdit = nullptr;
    QGroupBox* rawHexContainer = nullptr;
    std::unique_ptr<ChunkData> chunkData;
    QString recentFilesPath;
    QStringList recentFiles;
    QMenu* recentFilesMenu = nullptr;
    QString lastDirectory;
    QStackedWidget* editorStack = nullptr;
    MeshEditorWidget* meshEditor = nullptr;
    RawTextEditorWidget* meshUserTextEditor = nullptr;
    StringEditorWidget* textureNameEditor = nullptr;
    HierarchyHeaderEditorWidget* hierarchyHeaderEditor = nullptr;
    AnimationHierarchyEditorWidget* animationHierarchyEditor = nullptr;
    StringEditorWidget* materialNameEditor = nullptr;
    TransformNodeEditorWidget* transformNodeEditor = nullptr;
    MapperArgsEditorWidget* stage0ArgsEditor = nullptr;
    MapperArgsEditorWidget* stage1ArgsEditor = nullptr;
    MaterialEditorWidget* materialEditor = nullptr;
    ShaderEditorWidget* shaderEditor = nullptr;
    SurfaceTypeEditorWidget* surfaceTypeEditor = nullptr;
    TriangleSurfaceTypeEditorWidget* triangleSurfaceTypeEditor = nullptr;
    TextureInfoEditorWidget* textureInfoEditor = nullptr;
    QWidget* editorPlaceholder = nullptr;
    QLabel* editorPlaceholderLabel = nullptr;
    QWidget* renderPane = nullptr;
    QSplitter* renderSplitter = nullptr;
    OW3D::Render::RenderViewportWidget* renderViewport = nullptr;
    QCheckBox* renderFogToggle = nullptr;
    QCheckBox* renderLodToggle = nullptr;
    QDoubleSpinBox* renderLodBiasSpin = nullptr;
    QCheckBox* renderUvDebugToggle = nullptr;
    QCheckBox* renderLodLockToggle = nullptr;
    QCheckBox* renderCameraGizmoToggle = nullptr;
    QCheckBox* renderPivotMarkersToggle = nullptr;
    QSpinBox* renderLodLevelSpin = nullptr;
    QLabel* renderStatsLabel = nullptr;
    QLabel* renderSelectionLabel = nullptr;
    QTabWidget* renderTabs = nullptr;
    QPlainTextEdit* renderWarningsEdit = nullptr;
    QTreeWidget* renderAssetsTree = nullptr;
    QTreeWidget* renderAnimationsTree = nullptr;
    QPushButton* renderAddSkeletonButton = nullptr;
    QPushButton* renderAddAnimationsButton = nullptr;
    QPushButton* renderRemoveAssetButton = nullptr;
    QPushButton* renderClearAnimationsButton = nullptr;
    QPushButton* renderPlayPauseButton = nullptr;
    QPushButton* renderStopButton = nullptr;
    QPushButton* renderAnimationExportGifButton = nullptr;
    QPushButton* renderAnimationResetDraftButton = nullptr;
    QCheckBox* renderAnimationLoopToggle = nullptr;
    QCheckBox* renderAnimationEditKeysToggle = nullptr;
    QDoubleSpinBox* renderAnimationSpeedSpin = nullptr;
    QSlider* renderAnimationFrameSlider = nullptr;
    QLabel* renderAnimationFrameLabel = nullptr;
    QLabel* renderAnimationClipLabel = nullptr;
    QLabel* renderAnimationMetadataLabel = nullptr;
    QLabel* renderAnimationEditStatusLabel = nullptr;
    QGroupBox* renderAnimationPrepGroup = nullptr;
    QComboBox* renderAnimationPrepSourceCombo = nullptr;
    QSpinBox* renderAnimationPrepStaticPoseFrameSpin = nullptr;
    QPushButton* renderAnimationPrepApplyButton = nullptr;
    QPushButton* renderAnimationPrepFreezePoseButton = nullptr;
    QPushButton* renderAnimationPrepFitSourceButton = nullptr;
    QLabel* renderAnimationPrepStatusLabel = nullptr;
    QGroupBox* renderAnimationPivotSheetGroup = nullptr;
    QLabel* renderAnimationPivotSheetLabel = nullptr;
    QLabel* renderAnimationPivotSheetStatusLabel = nullptr;
    QTableWidget* renderAnimationPivotSheetTable = nullptr;
    QGroupBox* renderAnimationBlendGroup = nullptr;
    QComboBox* renderAnimationBlendSourceCombo = nullptr;
    QComboBox* renderAnimationBlendTimingCombo = nullptr;
    QTreeWidget* renderAnimationBlendPivotTree = nullptr;
    QGroupBox* renderAnimationBlendPivotWeightsGroup = nullptr;
    QLabel* renderAnimationBlendPivotWeightsLabel = nullptr;
    std::array<QSpinBox*, 6> renderAnimationBlendPivotAxisPercentSpins{};
    QPushButton* renderAnimationBlendResetPivotWeightsButton = nullptr;
    QCheckBox* renderAnimationBlendIncludeDescendantsToggle = nullptr;
    QCheckBox* renderAnimationBlendTruncateToggle = nullptr;
    QCheckBox* renderAnimationBlendNormalizeFrameRateToggle = nullptr;
    QCheckBox* renderAnimationBlendPinLeftHandToggle = nullptr;
    QCheckBox* renderAnimationBlendPinRightHandToggle = nullptr;
    QSpinBox* renderAnimationBlendHandPinReferenceFrameSpin = nullptr;
    QSpinBox* renderAnimationBlendTranslationPercentSpin = nullptr;
    QSpinBox* renderAnimationBlendRotationPercentSpin = nullptr;
    QSpinBox* renderAnimationBlendStartFrameSpin = nullptr;
    QSpinBox* renderAnimationBlendEndFrameSpin = nullptr;
    QPushButton* renderAnimationBlendApplyButton = nullptr;
    QLabel* renderAnimationBlendStatusLabel = nullptr;
    QTimer* renderAnimationPlaybackTimer = nullptr;
    QElapsedTimer renderAnimationPlaybackElapsed;
    std::shared_ptr<ChunkItem> currentChunk;
    QLabel* detailSourceLabel = nullptr;
    QString currentFilePath;
    bool dirty = false;
    QByteArray detailSplitterStateCache;
    QString currentArchiveRenderPath;
    uint32_t currentArchiveRenderEntryId = 0;
    QString currentArchiveRenderEntryPath;
    std::vector<ArchiveRenderEntryInfo> currentArchiveRenderEntries;
    std::vector<std::string> currentArchiveTextureEntries;
    std::vector<uint32_t> currentArchiveTextureEntryIds;
    std::unordered_map<uint32_t, ArchiveTextureSourceInfo> currentArchiveTextureSourcesById;
    std::unordered_map<uint32_t, QSet<QString>> currentArchiveRenderEntryReferenceNamesById;
    std::vector<std::shared_ptr<ChunkItem>> currentArchiveSupplementalRoots;
    std::unordered_set<uint32_t> currentArchiveLoadedSupplementalEntryIds;
    std::vector<RenderSessionAsset> currentExternalRenderAssets;
    QSet<QString> currentExternalRenderAssetPaths;
    std::vector<AggregateRenderDependencyAsset> currentAggregateRenderDependencyAssets;
    bool currentRenderTriedSkeletonAutoload = false;
    QString currentRenderSuppressedMissingHierarchyKey;
    QString currentRenderSuppressedMissingMeshKey;
    QString currentRenderTextureDirectory;
    QString currentRenderSuppressedMissingTextureKey;
    QString currentRenderAggregateDependencyAttemptKey;
    QString currentRenderHierarchyDependencyAttemptKey;
    OW3D::Render::SceneBuildResult currentRenderSceneResult;
    OW3D::Render::AnimationPlaybackState currentRenderAnimationPlayback;
    std::unordered_map<const void*, OW3D::Render::RenderAnimationEditDraft> currentRenderAnimationDrafts;
    std::optional<RenderAnimationClipIdentity> currentRenderActiveClipIdentity;
    bool currentRenderAnimationEditKeysEnabled = false;
    bool suppressRenderAnimationFrameSliderChange = false;
    bool suppressRenderAnimationBlendUiSignals = false;
    bool suppressRenderAnimationPrepUiSignals = false;
    bool suppressRenderAnimationPivotSheetSignals = false;
    int currentRenderSelectedPivotHierarchyIndex = -1;
    int currentRenderSelectedPivotIndex = -1;

    struct RenderAnimationPrepState {
        int targetAnimationIndex = -1;
        std::optional<RenderAnimationClipIdentity> targetClipIdentity;
        int staticPoseFrame = 0;
    };
    RenderAnimationPrepState currentRenderAnimationPrepState;

    struct RenderAnimationBlendState {
        enum class TimingMode {
            PreserveSourceRate,
            FitSourceToRange
        };

        int overlayAnimationIndex = -1;
        std::optional<RenderAnimationClipIdentity> overlayClipIdentity;
        QString hierarchyKey;
        QSet<int> pivotIndices;
        TimingMode timingMode = TimingMode::PreserveSourceRate;
        bool includeDescendants = true;
        bool truncateToShorter = false;
        bool normalizeBaseToSourceFrameRate = false;
        bool pinLeftHand = false;
        bool pinRightHand = false;
        int handPinReferenceFrame = 0;
        int translationBlendPercent = 100;
        int rotationBlendPercent = 100;
        std::unordered_map<int, std::array<int, 6>> pivotAxisBlendPercents;
        int selectedPivotIndex = -1;
        int startFrame = 0;
        int endFrame = 0;
        bool frameRangeInitialized = false;
    };
    RenderAnimationBlendState currentRenderAnimationBlendState;

    struct RenderEditUndoEntry {
        enum class Kind {
            ChunkTree,
            AnimationDrafts
        };

        Kind kind = Kind::ChunkTree;
        void* targetChunkPtr = nullptr;
        std::shared_ptr<ChunkItem> beforeSnapshot;
        std::shared_ptr<ChunkItem> afterSnapshot;
        std::unordered_map<const void*, OW3D::Render::RenderAnimationEditDraft> beforeDrafts;
        std::unordered_map<const void*, OW3D::Render::RenderAnimationEditDraft> afterDrafts;
    };
    std::vector<RenderEditUndoEntry> renderTransformUndoStack;
    std::vector<RenderEditUndoEntry> renderTransformRedoStack;
    bool applyingRenderTransformUndoRedo = false;

    struct ChunkSourceTab {
        QString sourceKey;
        QString label;
        bool editable = false;
        QTreeWidget* treeWidget = nullptr;
    };
    std::vector<ChunkSourceTab> chunkSourceTabs;

    void updateEditorForChunk(const std::shared_ptr<ChunkItem>& chunk);
    void updateRawHex(const std::shared_ptr<ChunkItem>& chunk);
    void setDirty(bool value);
    void updateWindowTitle();
    bool confirmDiscardChanges();
    void clearDetails();
    bool createBackupFile(const QString& filePath);
    void renameFileReferences(const QString& oldBaseName,
        const QString& newBaseName,
        const QString& oldFileName,
        const QString& newFileName);
};
