
#include "MainWindow.h"
#include "backend/ChunkData.h"
#include "backend/ChunkNames.h"
#include "backend/ChunkInterpreter.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QScrollArea>
#include <QTreeWidget>
#include <QTableWidget>
#include <QAbstractItemView>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QGridLayout>
#include <QProgressBar>
#include <QStringList>
#include <QKeySequence>
#include <QDialog>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QSizePolicy>
#include <iostream>
#include <QStandardPaths>
#include <QSettings>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QProgressDialog>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QTimer>
#include <QCloseEvent>
#include "backend/W3DMesh.h"
#include "backend/W3DStructs.h"
#include "backend/ChunkMutators.h"
#include "backend/ParseUtils.h"
#include "backend/render/AnimationPose.h"
#include "backend/render/SceneBuilder.h"
#include "EditorWidgets.h"
#include "frontend/render/RenderViewportWidget.h"
#include "ui_MainWindow.h"
#include "ui_RenderPanel.h"
#include "ui_HierarchyHeaderEditorWidget.h"
#include "ui_AnimationHierarchyEditorWidget.h"
#include "ui_TransformNodeEditorWidget.h"
#include "ui_SurfaceTypeEditorWidget.h"
#include "ui_TriangleSurfaceTypeEditorWidget.h"
#include "ui_MeshEditorWidget.h"
#include "ui_TextureInfoEditorWidget.h"
#include "ui_ShaderEditorWidget.h"
#include "ui_MaterialEditorWidget.h"
#include <map>
#include <nlohmann/json.hpp>
#include <array>
#include <vector>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <exception>
#include <variant>
#include <type_traits>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <functional>
#include <cctype>
#include <filesystem>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QSet>

#include "thirdparty/SimpleGifWriter.h"

using ordered_json = nlohmann::ordered_json;

static std::size_t TruncatedLength(const char* data, std::size_t maxLen) {
    std::size_t len = 0;
    while (len < maxLen && data[len] != '\0') {
        ++len;
    }
    return len;
}

static QString ReadFixedString(const char* data, std::size_t maxLen) {
    return QString::fromLatin1(data, static_cast<int>(TruncatedLength(data, maxLen)));
}

static std::shared_ptr<ChunkItem> FindFirstChildById(
    const std::shared_ptr<ChunkItem>& parent,
    uint32_t childId)
{
    if (!parent) {
        return nullptr;
    }
    for (const auto& child : parent->children) {
        if (child && child->id == childId) {
            return child;
        }
    }
    return nullptr;
}

static void CollectChunksByIdRecursive(
    const std::shared_ptr<ChunkItem>& node,
    uint32_t id,
    std::vector<std::shared_ptr<ChunkItem>>& out)
{
    if (!node) {
        return;
    }
    if (node->id == id) {
        out.push_back(node);
    }
    for (const auto& child : node->children) {
        CollectChunksByIdRecursive(child, id, out);
    }
}

static QSet<QString> CollectHierarchyNamesFromRoots(
    const std::vector<std::shared_ptr<ChunkItem>>& roots)
{
    std::vector<std::shared_ptr<ChunkItem>> hierarchyChunks;
    for (const auto& root : roots) {
        CollectChunksByIdRecursive(root, 0x0100, hierarchyChunks);
    }

    QSet<QString> names;
    for (const auto& hierarchyChunk : hierarchyChunks) {
        const auto headerChunk = FindFirstChildById(hierarchyChunk, 0x0101);
        if (!headerChunk) {
            continue;
        }
        auto parsed = ParseChunkStruct<W3dHierarchyStruct>(headerChunk);
        if (const auto* header = std::get_if<W3dHierarchyStruct>(&parsed)) {
            const QString name = ReadFixedString(header->Name, W3D_NAME_LEN).trimmed().toLower();
            if (!name.isEmpty()) {
                names.insert(name);
            }
        }
    }

    return names;
}

static int CollectAnimationCountFromRoots(
    const std::vector<std::shared_ptr<ChunkItem>>& roots)
{
    std::vector<std::shared_ptr<ChunkItem>> rawAnimationChunks;
    std::vector<std::shared_ptr<ChunkItem>> compressedAnimationChunks;
    for (const auto& root : roots) {
        CollectChunksByIdRecursive(root, 0x0200, rawAnimationChunks);
        CollectChunksByIdRecursive(root, 0x0280, compressedAnimationChunks);
    }
    return static_cast<int>(rawAnimationChunks.size() + compressedAnimationChunks.size());
}

static int CollectMeshCountFromRoots(
    const std::vector<std::shared_ptr<ChunkItem>>& roots)
{
    std::vector<std::shared_ptr<ChunkItem>> meshChunks;
    for (const auto& root : roots) {
        CollectChunksByIdRecursive(root, 0x0000, meshChunks);
    }
    return static_cast<int>(meshChunks.size());
}

namespace {
QString SanitizePathComponent(QString component);
int GifDelayCentisecondsFromClipFrameRate(float frameRate);
}

static std::string NormalizeRenderPivotNameKey(const std::string& value);

static void AddRenderReferenceNameVariants(
    const QString& name,
    QSet<QString>& outNames)
{
    const QString normalized = QDir::fromNativeSeparators(name).trimmed().toLower();
    if (normalized.isEmpty()) {
        return;
    }

    outNames.insert(normalized);

    const QFileInfo info(normalized);
    const QString fileName = info.fileName().trimmed().toLower();
    if (!fileName.isEmpty()) {
        outNames.insert(fileName);

        const QString stem = info.completeBaseName().trimmed().toLower();
        if (!stem.isEmpty()) {
            outNames.insert(stem);
        }
    }

    const int firstDot = normalized.indexOf(QLatin1Char('.'));
    if (firstDot > 0) {
        const QString prefix = normalized.left(firstDot).trimmed();
        if (!prefix.isEmpty()) {
            outNames.insert(prefix);
        }
    }
}

static void CollectRenderReferenceNamesRecursive(
    const std::shared_ptr<ChunkItem>& node,
    QSet<QString>& outNames)
{
    if (!node) {
        return;
    }

    switch (node->id) {
    case 0x0101: { // W3D_CHUNK_HIERARCHY_HEADER
        auto parsed = ParseChunkStruct<W3dHierarchyStruct>(node);
        if (const auto* header = std::get_if<W3dHierarchyStruct>(&parsed)) {
            AddRenderReferenceNameVariants(
                ReadFixedString(header->Name, W3D_NAME_LEN),
                outNames);
        }
        break;
    }
    case 0x001F: { // W3D_CHUNK_MESH_HEADER3
        auto parsed = ParseChunkStruct<W3dMeshHeader3Struct>(node);
        if (const auto* header = std::get_if<W3dMeshHeader3Struct>(&parsed)) {
            const QString meshName = ReadFixedString(header->MeshName, W3D_NAME_LEN);
            const QString containerName = ReadFixedString(header->ContainerName, W3D_NAME_LEN);
            AddRenderReferenceNameVariants(meshName, outNames);
            AddRenderReferenceNameVariants(containerName, outNames);
            if (!meshName.trimmed().isEmpty() && !containerName.trimmed().isEmpty()) {
                AddRenderReferenceNameVariants(containerName + QLatin1Char('.') + meshName, outNames);
            }
        }
        break;
    }
    case 0x0301: { // W3D_CHUNK_HMODEL_HEADER
        auto parsed = ParseChunkStruct<W3dHModelHeaderStruct>(node);
        if (const auto* header = std::get_if<W3dHModelHeaderStruct>(&parsed)) {
            AddRenderReferenceNameVariants(
                ReadFixedString(header->Name, W3D_NAME_LEN),
                outNames);
            AddRenderReferenceNameVariants(
                ReadFixedString(header->HierarchyName, W3D_NAME_LEN),
                outNames);
        }
        break;
    }
    case 0x0302: // W3D_CHUNK_HMODEL_NODE
    case 0x0303: // W3D_CHUNK_HMODEL_COLLISION_NODE
    case 0x0304: // W3D_CHUNK_HMODEL_SKIN_NODE
    case 0x0306: { // W3D_CHUNK_HMODEL_SHADOW_NODE
        auto parsed = ParseChunkStruct<W3dHModelNodeStruct>(node);
        if (const auto* hmodelNode = std::get_if<W3dHModelNodeStruct>(&parsed)) {
            AddRenderReferenceNameVariants(
                ReadFixedString(hmodelNode->RenderObjName, W3D_NAME_LEN),
                outNames);
        }
        break;
    }
    case 0x0401: { // W3D_CHUNK_LODMODEL_HEADER
        auto parsed = ParseChunkStruct<W3dLODModelHeaderStruct>(node);
        if (const auto* header = std::get_if<W3dLODModelHeaderStruct>(&parsed)) {
            AddRenderReferenceNameVariants(
                ReadFixedString(header->Name, W3D_NAME_LEN),
                outNames);
        }
        break;
    }
    case 0x0402: { // W3D_CHUNK_LOD
        auto parsed = ParseChunkStruct<W3dLODStruct>(node);
        if (const auto* lod = std::get_if<W3dLODStruct>(&parsed)) {
            AddRenderReferenceNameVariants(
                ReadFixedString(lod->RenderObjName, 2 * W3D_NAME_LEN),
                outNames);
        }
        break;
    }
    case 0x0601: { // W3D_CHUNK_AGGREGATE_HEADER
        auto parsed = ParseChunkStruct<W3dAggregateHeaderStruct>(node);
        if (const auto* header = std::get_if<W3dAggregateHeaderStruct>(&parsed)) {
            AddRenderReferenceNameVariants(
                ReadFixedString(header->Name, W3D_NAME_LEN),
                outNames);
        }
        break;
    }
    case 0x0602: { // W3D_CHUNK_AGGREGATE_INFO
        auto parsed = ParseChunkStruct<W3dAggregateInfoStruct>(node);
        if (const auto* info = std::get_if<W3dAggregateInfoStruct>(&parsed)) {
            AddRenderReferenceNameVariants(
                ReadFixedString(info->BaseModelName, 2 * W3D_NAME_LEN),
                outNames);

            const std::size_t headerSize = sizeof(W3dAggregateInfoStruct);
            const std::size_t entrySize = sizeof(W3dAggregateSubobjectStruct);
            if (node->data.size() >= headerSize && entrySize > 0) {
                const std::size_t availableEntries = (node->data.size() - headerSize) / entrySize;
                const std::size_t count =
                    std::min<std::size_t>(info->SubobjectCount, availableEntries);
                const auto* subObjects =
                    reinterpret_cast<const W3dAggregateSubobjectStruct*>(
                        node->data.data() + static_cast<qsizetype>(headerSize));
                for (std::size_t i = 0; i < count; ++i) {
                    AddRenderReferenceNameVariants(
                        ReadFixedString(subObjects[i].SubobjectName, 2 * W3D_NAME_LEN),
                        outNames);
                }
            }
        }
        break;
    }
    case 0x0701: { // W3D_CHUNK_HLOD_HEADER
        auto parsed = ParseChunkStruct<W3dHLodHeaderStruct>(node);
        if (const auto* header = std::get_if<W3dHLodHeaderStruct>(&parsed)) {
            AddRenderReferenceNameVariants(
                ReadFixedString(header->Name, W3D_NAME_LEN),
                outNames);
            AddRenderReferenceNameVariants(
                ReadFixedString(header->HierarchyName, W3D_NAME_LEN),
                outNames);
        }
        break;
    }
    case 0x0704: { // W3D_CHUNK_HLOD_SUB_OBJECT
        auto parsed = ParseChunkStruct<W3dHLodSubObjectStruct>(node);
        if (const auto* subObject = std::get_if<W3dHLodSubObjectStruct>(&parsed)) {
            AddRenderReferenceNameVariants(
                ReadFixedString(subObject->Name, 2 * W3D_NAME_LEN),
                outNames);
        }
        break;
    }
    default:
        break;
    }

    for (const auto& child : node->children) {
        CollectRenderReferenceNamesRecursive(child, outNames);
    }
}

static QSet<QString> CollectRenderReferenceNamesFromRoots(
    const std::vector<std::shared_ptr<ChunkItem>>& roots)
{
    QSet<QString> names;
    for (const auto& root : roots) {
        CollectRenderReferenceNamesRecursive(root, names);
    }
    return names;
}

static bool ReferenceNamesContainAnyMatchToken(
    const QSet<QString>& referenceNames,
    const QSet<QString>& matchTokens)
{
    for (const QString& name : referenceNames) {
        if (matchTokens.contains(name)) {
            return true;
        }
    }
    return false;
}

static QString NormalizeAbsolutePathKey(const QString& filePath) {
    return QDir::cleanPath(QFileInfo(filePath).absoluteFilePath()).toLower();
}

static QString BuildArchiveEntrySourceKey(const QString& archivePath, uint32_t entryId) {
    return NormalizeAbsolutePathKey(archivePath)
        + QStringLiteral("::")
        + QString::number(static_cast<qulonglong>(entryId));
}

static QString RenderSessionAssetSourceKey(const RenderSessionAsset& asset) {
    if (!asset.sourceKey.isEmpty()) {
        return asset.sourceKey;
    }
    return NormalizeAbsolutePathKey(asset.filePath);
}

static QString RenderSessionAssetSourceDisplayPath(const RenderSessionAsset& asset) {
    if (!asset.sourceDisplayPath.isEmpty()) {
        return asset.sourceDisplayPath;
    }
    return asset.filePath;
}

static const QString& PrimaryChunkSourceKey() {
    static const QString kKey = QStringLiteral("__primary__");
    return kKey;
}

static QString RenderSessionAssetRoleLabel(const RenderSessionAsset& asset) {
    switch (asset.role) {
    case RenderSessionAssetRole::Skeleton:
        if (!asset.hierarchyNames.isEmpty() && asset.meshCount > 0) {
            return QObject::tr("Model+Skeleton");
        }
        if (!asset.hierarchyNames.isEmpty()) {
            return QObject::tr("Skeleton");
        }
        if (asset.meshCount > 0) {
            return QObject::tr("Model");
        }
        return QObject::tr("Reference");
    case RenderSessionAssetRole::AnimationLibrary:
        return QObject::tr("Animations");
    }
    return QObject::tr("Unknown");
}

static RenderAnimationClipIdentity BuildRenderAnimationClipIdentity(
    const OW3D::Render::RenderAnimationClip& clip)
{
    RenderAnimationClipIdentity identity{};
    identity.fullName = QString::fromStdString(clip.fullName);
    identity.hierarchyName = QString::fromStdString(clip.hierarchyName);
    identity.sourceFileLabel = QString::fromStdString(clip.sourceFileLabel);
    identity.sourceFromAnimationLibrary = clip.sourceFromAnimationLibrary;
    identity.compressed = clip.compressed;
    return identity;
}

constexpr int kRenderAssetPathRole = Qt::UserRole;
constexpr int kRenderAssetRoleRole = Qt::UserRole + 1;
constexpr int kRenderAnimationIndexRole = Qt::UserRole;
constexpr int kRenderBlendPivotIndexRole = Qt::UserRole;
constexpr int kRenderBlendAxisCount = 6;
constexpr int kRenderBlendPositionXAxis = 0;
constexpr int kRenderBlendPositionYAxis = 1;
constexpr int kRenderBlendPositionZAxis = 2;
constexpr int kRenderBlendRotationXAxis = 3;
constexpr int kRenderBlendRotationYAxis = 4;
constexpr int kRenderBlendRotationZAxis = 5;

static std::array<int, kRenderBlendAxisCount> DefaultRenderBlendPivotAxisPercents(
    int translationPercent,
    int rotationPercent)
{
    const int clampedTranslation = std::clamp(translationPercent, 0, 100);
    const int clampedRotation = std::clamp(rotationPercent, 0, 100);
    return {
        clampedTranslation,
        clampedTranslation,
        clampedTranslation,
        clampedRotation,
        clampedRotation,
        clampedRotation
    };
}

static std::array<int, kRenderBlendAxisCount> ResolveRenderBlendPivotAxisPercents(
    int translationPercent,
    int rotationPercent,
    const std::unordered_map<int, std::array<int, kRenderBlendAxisCount>>& perPivotPercents,
    int pivotIndex)
{
    if (const auto it = perPivotPercents.find(pivotIndex); it != perPivotPercents.end()) {
        return it->second;
    }
    return DefaultRenderBlendPivotAxisPercents(translationPercent, rotationPercent);
}

static bool RenderBlendPivotAxisPercentsHaveAnyBlend(
    const std::array<int, kRenderBlendAxisCount>& axisPercents)
{
    return std::any_of(
        axisPercents.begin(),
        axisPercents.end(),
        [](int value) { return value > 0; });
}

static bool RenderBlendPivotAxisPercentsMatchDefaults(
    const std::array<int, kRenderBlendAxisCount>& axisPercents,
    int translationPercent,
    int rotationPercent)
{
    return axisPercents == DefaultRenderBlendPivotAxisPercents(
        translationPercent,
        rotationPercent);
}

static QString FormatRenderBlendPivotAxisPercentsSummary(
    const std::array<int, kRenderBlendAxisCount>& axisPercents)
{
    return QObject::tr("Pos X %1%%, Y %2%%, Z %3%% | Rot X %4%%, Y %5%%, Z %6%%")
        .arg(axisPercents[static_cast<std::size_t>(kRenderBlendPositionXAxis)])
        .arg(axisPercents[static_cast<std::size_t>(kRenderBlendPositionYAxis)])
        .arg(axisPercents[static_cast<std::size_t>(kRenderBlendPositionZAxis)])
        .arg(axisPercents[static_cast<std::size_t>(kRenderBlendRotationXAxis)])
        .arg(axisPercents[static_cast<std::size_t>(kRenderBlendRotationYAxis)])
        .arg(axisPercents[static_cast<std::size_t>(kRenderBlendRotationZAxis)]);
}

static uint32_t EffectiveRenderAnimationFrameCount(
    const OW3D::Render::RenderAnimationClip& clip,
    const OW3D::Render::RenderAnimationEditDraft* draft = nullptr)
{
    return OW3D::Render::ResolveAnimationFrameCount(&clip, draft);
}

static float EffectiveRenderAnimationFrameRate(
    const OW3D::Render::RenderAnimationClip& clip,
    const OW3D::Render::RenderAnimationEditDraft* draft = nullptr)
{
    return OW3D::Render::ResolveAnimationFrameRate(&clip, draft);
}

static bool HasPendingRenderAnimationDraftChanges(
    const OW3D::Render::RenderAnimationClip* clip,
    const OW3D::Render::RenderAnimationEditDraft* draft)
{
    if (!clip || !draft) {
        return false;
    }
    return !draft->pivotSamples.empty()
        || draft->numFrames != clip->numFrames
        || std::fabs(draft->frameRate - clip->frameRate) > 1.0e-4f;
}

static float AnimationClipDurationSeconds(
    const OW3D::Render::RenderAnimationClip& clip,
    const OW3D::Render::RenderAnimationEditDraft* draft = nullptr)
{
    const uint32_t numFrames = EffectiveRenderAnimationFrameCount(clip, draft);
    const float frameRate = EffectiveRenderAnimationFrameRate(clip, draft);
    if (frameRate <= 0.0f || numFrames <= 1u) {
        return 0.0f;
    }
    return static_cast<float>(numFrames - 1u) / frameRate;
}

static uint32_t ComputeResampledAnimationFrameCount(
    uint32_t sourceNumFrames,
    float sourceFrameRate,
    float targetFrameRate)
{
    if (sourceNumFrames == 0u) {
        return 0u;
    }
    if (sourceNumFrames <= 1u || sourceFrameRate <= 0.0f || targetFrameRate <= 0.0f) {
        return sourceNumFrames;
    }

    const double intervalCount =
        static_cast<double>(sourceNumFrames - 1u)
        * static_cast<double>(targetFrameRate)
        / static_cast<double>(sourceFrameRate);
    const auto roundedIntervals =
        static_cast<uint32_t>(std::max<long long>(0ll, std::llround(intervalCount)));
    return roundedIntervals + 1u;
}

static int MapAnimationFrameIndexToFrameRate(
    int frameIndex,
    float sourceFrameRate,
    float targetFrameRate,
    uint32_t targetNumFrames)
{
    const int targetMaxFrame = std::max(0, static_cast<int>(targetNumFrames) - 1);
    if (sourceFrameRate <= 0.0f || targetFrameRate <= 0.0f) {
        return std::clamp(frameIndex, 0, targetMaxFrame);
    }

    const double mappedFrame =
        static_cast<double>(std::max(frameIndex, 0))
        * static_cast<double>(targetFrameRate)
        / static_cast<double>(sourceFrameRate);
    return std::clamp(
        static_cast<int>(std::llround(mappedFrame)),
        0,
        targetMaxFrame);
}

template <typename KeyframeT>
static bool AnimationTrackHasFrameKey(
    const std::vector<KeyframeT>& track,
    int frameIndex,
    float epsilon = 1.0e-3f)
{
    return std::any_of(
        track.begin(),
        track.end(),
        [frameIndex, epsilon](const KeyframeT& key) {
            return std::fabs(key.frame - static_cast<float>(frameIndex)) <= epsilon;
        });
}

static QString BuildPivotAnimationSourceKeyFlags(
    const OW3D::Render::RenderPivotAnimation* pivotAnimation,
    int frameIndex)
{
    if (!pivotAnimation || frameIndex < 0) {
        return QString();
    }

    const bool translationKey =
        AnimationTrackHasFrameKey(pivotAnimation->translationX, frameIndex)
        || AnimationTrackHasFrameKey(pivotAnimation->translationY, frameIndex)
        || AnimationTrackHasFrameKey(pivotAnimation->translationZ, frameIndex);
    const bool rotationKey =
        AnimationTrackHasFrameKey(pivotAnimation->rotation, frameIndex);

    if (translationKey && rotationKey) {
        return QStringLiteral("TR");
    }
    if (translationKey) {
        return QStringLiteral("T");
    }
    if (rotationKey) {
        return QStringLiteral("R");
    }
    return QString();
}

struct PivotOverrideBaseTimingPreview {
    uint32_t frameCount = 0u;
    float frameRate = 0.0f;
    int selectedStartFrame = 0;
    int selectedEndFrame = 0;
    int appliedStartFrame = 0;
    int appliedEndFrame = 0;
    bool normalizationRequested = false;
    bool normalizationApplied = false;
    bool normalizationAlreadyMatchesSource = false;
    bool normalizationRequiresValidFrameRates = false;
};

static PivotOverrideBaseTimingPreview ComputePivotOverrideBaseTimingPreview(
    uint32_t baseNumFrames,
    float baseFrameRate,
    int requestedStartFrame,
    int requestedEndFrame,
    bool preserveSourceRate,
    bool normalizeBaseToSourceFrameRate,
    float sourceFrameRate)
{
    PivotOverrideBaseTimingPreview preview{};
    preview.frameCount = baseNumFrames;
    preview.frameRate = baseFrameRate;

    const int maxFrame = std::max(0, static_cast<int>(baseNumFrames) - 1);
    preview.selectedStartFrame =
        std::clamp(requestedStartFrame, 0, maxFrame);
    preview.selectedEndFrame =
        std::clamp(std::max(requestedStartFrame, requestedEndFrame), 0, maxFrame);
    preview.appliedStartFrame = preview.selectedStartFrame;
    preview.appliedEndFrame = preview.selectedEndFrame;

    if (!preserveSourceRate || !normalizeBaseToSourceFrameRate) {
        return preview;
    }

    preview.normalizationRequested = true;
    if (baseFrameRate <= 0.0f || sourceFrameRate <= 0.0f) {
        preview.normalizationRequiresValidFrameRates = true;
        return preview;
    }

    if (std::fabs(baseFrameRate - sourceFrameRate) <= 1.0e-4f) {
        preview.normalizationAlreadyMatchesSource = true;
        return preview;
    }

    preview.normalizationApplied = true;
    preview.frameRate = sourceFrameRate;
    preview.frameCount = ComputeResampledAnimationFrameCount(
        baseNumFrames,
        baseFrameRate,
        sourceFrameRate);
    preview.appliedStartFrame = MapAnimationFrameIndexToFrameRate(
        preview.selectedStartFrame,
        baseFrameRate,
        preview.frameRate,
        preview.frameCount);
    preview.appliedEndFrame = MapAnimationFrameIndexToFrameRate(
        preview.selectedEndFrame,
        baseFrameRate,
        preview.frameRate,
        preview.frameCount);
    if (preview.appliedEndFrame < preview.appliedStartFrame) {
        preview.appliedEndFrame = preview.appliedStartFrame;
    }
    return preview;
}

static void RetimeAnimationEditDraft(
    const OW3D::Render::RenderAnimationClip& clip,
    OW3D::Render::RenderAnimationEditDraft& draft,
    uint32_t targetNumFrames,
    float targetFrameRate)
{
    const uint32_t sourceNumFrames = EffectiveRenderAnimationFrameCount(clip, &draft);
    const float sourceFrameRate = EffectiveRenderAnimationFrameRate(clip, &draft);
    if (sourceNumFrames == targetNumFrames
        && std::fabs(sourceFrameRate - targetFrameRate) <= 1.0e-4f)
    {
        draft.numFrames = targetNumFrames;
        draft.frameRate = targetFrameRate;
        return;
    }

    const OW3D::Render::RenderAnimationEditDraft sourceDraft = draft;
    std::vector<int> pivotIndices;
    pivotIndices.reserve(clip.pivots.size() + sourceDraft.pivotSamples.size());
    for (int pivotIndex = 0; pivotIndex < static_cast<int>(clip.pivots.size()); ++pivotIndex) {
        pivotIndices.push_back(pivotIndex);
    }
    for (const auto& [pivotIndex, samples] : sourceDraft.pivotSamples) {
        (void)samples;
        if (pivotIndex >= static_cast<int>(clip.pivots.size())) {
            pivotIndices.push_back(pivotIndex);
        }
    }

    draft.sourceAnimationChunk = clip.sourceAnimationChunk;
    draft.numFrames = targetNumFrames;
    draft.frameRate = targetFrameRate;
    draft.pivotSamples.clear();

    for (const int pivotIndex : pivotIndices) {
        OW3D::Render::RenderDensePivotAnimationSamples samples{};
        samples.translationX.resize(targetNumFrames, 0.0f);
        samples.translationY.resize(targetNumFrames, 0.0f);
        samples.translationZ.resize(targetNumFrames, 0.0f);
        samples.rotation.resize(
            targetNumFrames,
            OW3D::Render::Vec4{ 0.0f, 0.0f, 0.0f, 1.0f });

        for (uint32_t frame = 0; frame < targetNumFrames; ++frame) {
            float sourceFrame = 0.0f;
            if (sourceNumFrames > 1u
                && sourceFrameRate > 0.0f
                && targetFrameRate > 0.0f)
            {
                sourceFrame = static_cast<float>(
                    static_cast<double>(frame)
                    * static_cast<double>(sourceFrameRate)
                    / static_cast<double>(targetFrameRate));
                sourceFrame = std::clamp(
                    sourceFrame,
                    0.0f,
                    static_cast<float>(sourceNumFrames - 1u));
            }
            else if (targetNumFrames > 1u) {
                sourceFrame = OW3D::Render::AnimationFrameFromNormalizedProgress(
                    sourceNumFrames,
                    static_cast<float>(frame)
                        / static_cast<float>(targetNumFrames - 1u));
            }

            const OW3D::Render::Vec3 translation =
                OW3D::Render::SamplePivotAnimationTranslation(
                    &clip,
                    pivotIndex,
                    sourceFrame,
                    &sourceDraft);
            const OW3D::Render::Vec4 rotation =
                OW3D::Render::SamplePivotAnimationRotation(
                    &clip,
                    pivotIndex,
                    sourceFrame,
                    &sourceDraft);
            samples.translationX[frame] = translation.x;
            samples.translationY[frame] = translation.y;
            samples.translationZ[frame] = translation.z;
            samples.rotation[frame] =
                OW3D::Render::AnimationNormalizeQuat(rotation);
        }

        draft.pivotSamples[pivotIndex] = std::move(samples);
    }
}

static std::vector<int> CollectRenderAnimationDraftPivotIndices(
    const OW3D::Render::RenderAnimationClip& clip,
    const OW3D::Render::RenderAnimationEditDraft* draft)
{
    std::vector<int> pivotIndices;
    pivotIndices.reserve(clip.pivots.size() + (draft ? draft->pivotSamples.size() : 0u));
    for (int pivotIndex = 0; pivotIndex < static_cast<int>(clip.pivots.size()); ++pivotIndex) {
        pivotIndices.push_back(pivotIndex);
    }
    if (draft) {
        for (const auto& [pivotIndex, samples] : draft->pivotSamples) {
            (void)samples;
            if (pivotIndex >= static_cast<int>(clip.pivots.size())) {
                pivotIndices.push_back(pivotIndex);
            }
        }
    }
    std::sort(pivotIndices.begin(), pivotIndices.end());
    pivotIndices.erase(std::unique(pivotIndices.begin(), pivotIndices.end()), pivotIndices.end());
    return pivotIndices;
}

static void PrepareAnimationEditDraftFromStaticPose(
    const OW3D::Render::RenderAnimationClip& clip,
    OW3D::Render::RenderAnimationEditDraft& draft,
    uint32_t targetNumFrames,
    float targetFrameRate,
    float sourcePoseFrame)
{
    const OW3D::Render::RenderAnimationEditDraft sourceDraft = draft;
    const auto pivotIndices = CollectRenderAnimationDraftPivotIndices(clip, &sourceDraft);

    draft.sourceAnimationChunk = clip.sourceAnimationChunk;
    draft.numFrames = targetNumFrames;
    draft.frameRate = targetFrameRate;
    draft.pivotSamples.clear();

    for (const int pivotIndex : pivotIndices) {
        OW3D::Render::RenderDensePivotAnimationSamples samples{};
        samples.translationX.resize(targetNumFrames, 0.0f);
        samples.translationY.resize(targetNumFrames, 0.0f);
        samples.translationZ.resize(targetNumFrames, 0.0f);
        samples.rotation.resize(
            targetNumFrames,
            OW3D::Render::Vec4{ 0.0f, 0.0f, 0.0f, 1.0f });

        const OW3D::Render::Vec3 translation =
            OW3D::Render::SamplePivotAnimationTranslation(
                &clip,
                pivotIndex,
                sourcePoseFrame,
                &sourceDraft);
        const OW3D::Render::Vec4 rotation =
            OW3D::Render::AnimationNormalizeQuat(
                OW3D::Render::SamplePivotAnimationRotation(
                    &clip,
                    pivotIndex,
                    sourcePoseFrame,
                    &sourceDraft));

        for (uint32_t frameIndex = 0; frameIndex < targetNumFrames; ++frameIndex) {
            samples.translationX[frameIndex] = translation.x;
            samples.translationY[frameIndex] = translation.y;
            samples.translationZ[frameIndex] = translation.z;
            samples.rotation[frameIndex] = rotation;
        }

        draft.pivotSamples[pivotIndex] = std::move(samples);
    }
}

static void PrepareAnimationEditDraftFromFittedSourceClip(
    const OW3D::Render::RenderAnimationClip& targetClip,
    OW3D::Render::RenderAnimationEditDraft& targetDraft,
    const OW3D::Render::RenderAnimationClip& sourceClip,
    const OW3D::Render::RenderAnimationEditDraft* sourceDraft,
    uint32_t targetNumFrames,
    float targetFrameRate)
{
    const OW3D::Render::RenderAnimationEditDraft existingTargetDraft = targetDraft;
    std::vector<int> pivotIndices =
        CollectRenderAnimationDraftPivotIndices(targetClip, &existingTargetDraft);
    std::vector<int> sourcePivotIndices =
        CollectRenderAnimationDraftPivotIndices(sourceClip, sourceDraft);
    pivotIndices.insert(
        pivotIndices.end(),
        sourcePivotIndices.begin(),
        sourcePivotIndices.end());
    std::sort(pivotIndices.begin(), pivotIndices.end());
    pivotIndices.erase(std::unique(pivotIndices.begin(), pivotIndices.end()), pivotIndices.end());

    const uint32_t sourceNumFrames =
        EffectiveRenderAnimationFrameCount(sourceClip, sourceDraft);

    targetDraft.sourceAnimationChunk = targetClip.sourceAnimationChunk;
    targetDraft.numFrames = targetNumFrames;
    targetDraft.frameRate = targetFrameRate;
    targetDraft.pivotSamples.clear();

    for (const int pivotIndex : pivotIndices) {
        OW3D::Render::RenderDensePivotAnimationSamples samples{};
        samples.translationX.resize(targetNumFrames, 0.0f);
        samples.translationY.resize(targetNumFrames, 0.0f);
        samples.translationZ.resize(targetNumFrames, 0.0f);
        samples.rotation.resize(
            targetNumFrames,
            OW3D::Render::Vec4{ 0.0f, 0.0f, 0.0f, 1.0f });

        for (uint32_t frameIndex = 0; frameIndex < targetNumFrames; ++frameIndex) {
            const float progress = targetNumFrames <= 1u
                ? 0.0f
                : static_cast<float>(frameIndex) / static_cast<float>(targetNumFrames - 1u);
            const float sourceFrame =
                OW3D::Render::AnimationFrameFromNormalizedProgress(
                    sourceNumFrames,
                    progress);
            const OW3D::Render::Vec3 translation =
                OW3D::Render::SamplePivotAnimationTranslation(
                    &sourceClip,
                    pivotIndex,
                    sourceFrame,
                    sourceDraft);
            const OW3D::Render::Vec4 rotation =
                OW3D::Render::AnimationNormalizeQuat(
                    OW3D::Render::SamplePivotAnimationRotation(
                        &sourceClip,
                        pivotIndex,
                        sourceFrame,
                        sourceDraft));

            samples.translationX[frameIndex] = translation.x;
            samples.translationY[frameIndex] = translation.y;
            samples.translationZ[frameIndex] = translation.z;
            samples.rotation[frameIndex] = rotation;
        }

        targetDraft.pivotSamples[pivotIndex] = std::move(samples);
    }
}

struct RenderHandPinChain {
    QString label;
    int effectorPivotIndex = -1;
    std::vector<int> jointPivotIndices;
};

struct RenderHandPinTarget {
    RenderHandPinChain chain;
    OW3D::Render::Vec3 worldPosition{};
};

static bool RenderPivotNameMatchesAnyVariant(
    const std::string& pivotName,
    std::initializer_list<const char*> variants)
{
    const std::string normalized = NormalizeRenderPivotNameKey(pivotName);
    for (const char* variant : variants) {
        if (normalized == NormalizeRenderPivotNameKey(variant ? variant : "")) {
            return true;
        }
    }
    return false;
}

static int FindRenderPivotIndexByNameVariants(
    const std::unordered_map<std::string, int>& pivotIndexByName,
    std::initializer_list<const char*> variants)
{
    for (const char* variant : variants) {
        const auto it = pivotIndexByName.find(
            NormalizeRenderPivotNameKey(variant ? variant : ""));
        if (it != pivotIndexByName.end()) {
            return it->second;
        }
    }
    return -1;
}

static std::optional<RenderHandPinChain> ResolveRenderHandPinChain(
    const OW3D::Render::RenderHierarchy& hierarchy,
    const std::unordered_map<std::string, int>& pivotIndexByName,
    bool leftHand)
{
    const int effectorPivotIndex = leftHand
        ? FindRenderPivotIndexByNameVariants(
            pivotIndexByName,
            { "C_L_HAND", "C L HAND" })
        : FindRenderPivotIndexByNameVariants(
            pivotIndexByName,
            { "C_R_HAND", "C R HAND" });
    if (effectorPivotIndex < 0
        || effectorPivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
        return std::nullopt;
    }

    std::vector<int> jointPivotIndicesReversed;
    for (int parentIndex = hierarchy.pivots[static_cast<std::size_t>(effectorPivotIndex)].parentIndex;
        parentIndex >= 0 && parentIndex < static_cast<int>(hierarchy.pivots.size());
        parentIndex = hierarchy.pivots[static_cast<std::size_t>(parentIndex)].parentIndex)
    {
        const auto& pivot = hierarchy.pivots[static_cast<std::size_t>(parentIndex)];
        const bool matchesClavicle = leftHand
            ? RenderPivotNameMatchesAnyVariant(pivot.name, { "C_L_CLAVICLE", "C L CLAVICLE" })
            : RenderPivotNameMatchesAnyVariant(pivot.name, { "C_R_CLAVICLE", "C R CLAVICLE" });
        const bool matchesUpperArm = leftHand
            ? RenderPivotNameMatchesAnyVariant(pivot.name, { "C_L_UPPERARM", "C L UPPERARM" })
            : RenderPivotNameMatchesAnyVariant(pivot.name, { "C_R_UPPERARM", "C R UPPERARM" });
        const bool matchesForearm = leftHand
            ? RenderPivotNameMatchesAnyVariant(pivot.name, { "C_L_FOREARM", "C L FOREARM" })
            : RenderPivotNameMatchesAnyVariant(pivot.name, { "C_R_FOREARM", "C R FOREARM" });
        if (matchesClavicle || matchesUpperArm || matchesForearm) {
            jointPivotIndicesReversed.push_back(parentIndex);
        }
    }

    if (jointPivotIndicesReversed.empty()) {
        return std::nullopt;
    }

    std::reverse(jointPivotIndicesReversed.begin(), jointPivotIndicesReversed.end());
    RenderHandPinChain chain{};
    chain.label = leftHand ? QObject::tr("Left hand") : QObject::tr("Right hand");
    chain.effectorPivotIndex = effectorPivotIndex;
    chain.jointPivotIndices = std::move(jointPivotIndicesReversed);
    return chain;
}

static OW3D::Render::AnimationPlaybackState BuildRenderAnimationPlaybackForFrame(
    int activeAnimationIndex,
    int frameIndex,
    float frameRate)
{
    OW3D::Render::AnimationPlaybackState playback{};
    playback.activeAnimationIndex = activeAnimationIndex;
    playback.loop = false;
    playback.playing = false;
    playback.speed = 1.0f;
    playback.timeSeconds = frameRate > 0.0f
        ? static_cast<float>(frameIndex) / frameRate
        : 0.0f;
    return playback;
}

static OW3D::Render::Vec3 ExtractRenderTransformTranslation(const OW3D::Render::Mat4& transform)
{
    return OW3D::Render::AnimationRigidTransformTranslation(transform);
}

static OW3D::Render::Vec4 QuaternionFromAxisAngle(
    OW3D::Render::Vec3 axis,
    float angleRadians)
{
    axis = OW3D::Render::Normalize(axis);
    if (OW3D::Render::Length(axis) <= 1.0e-6f || std::fabs(angleRadians) <= 1.0e-6f) {
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    }

    const float halfAngle = angleRadians * 0.5f;
    const float sinHalf = std::sin(halfAngle);
    return OW3D::Render::AnimationNormalizeQuat({
        axis.x * sinHalf,
        axis.y * sinHalf,
        axis.z * sinHalf,
        std::cos(halfAngle)
    });
}

static OW3D::Render::Vec3 FallbackRotationAxis(const OW3D::Render::Vec3& direction)
{
    OW3D::Render::Vec3 axis =
        OW3D::Render::Cross(direction, { 0.0f, 0.0f, 1.0f });
    if (OW3D::Render::Length(axis) <= 1.0e-6f) {
        axis = OW3D::Render::Cross(direction, { 0.0f, 1.0f, 0.0f });
    }
    if (OW3D::Render::Length(axis) <= 1.0e-6f) {
        axis = { 1.0f, 0.0f, 0.0f };
    }
    return OW3D::Render::Normalize(axis);
}

static OW3D::Render::Mat4 RotateRigidTransformInWorldSpace(
    const OW3D::Render::Mat4& currentWorld,
    const OW3D::Render::Vec4& worldRotationDelta)
{
    OW3D::Render::Mat4 rotated =
        OW3D::Render::Multiply(
            OW3D::Render::QuaternionToMatrix(
                worldRotationDelta.x,
                worldRotationDelta.y,
                worldRotationDelta.z,
                worldRotationDelta.w),
            OW3D::Render::AnimationOrthonormalizeRigidTransform(currentWorld));
    rotated.m[12] = currentWorld.m[12];
    rotated.m[13] = currentWorld.m[13];
    rotated.m[14] = currentWorld.m[14];
    return OW3D::Render::AnimationOrthonormalizeRigidTransform(rotated);
}

static int ComputePivotOverrideEffectiveEndFrame(
    uint32_t baseNumFrames,
    float baseFrameRate,
    uint32_t sourceNumFrames,
    float sourceFrameRate,
    int startFrame,
    int requestedEndFrame,
    bool preserveSourceRate,
    bool truncateToShorter)
{
    const int maxFrame = std::max(0, static_cast<int>(baseNumFrames) - 1);
    const int clampedStartFrame = std::clamp(startFrame, 0, maxFrame);
    const int clampedEndFrame =
        std::clamp(std::max(startFrame, requestedEndFrame), 0, maxFrame);
    if (!preserveSourceRate || !truncateToShorter) {
        return clampedEndFrame;
    }
    if (sourceNumFrames == 0u) {
        return clampedStartFrame - 1;
    }

    int effectiveEndFrame = clampedStartFrame + static_cast<int>(sourceNumFrames) - 1;
    if (baseFrameRate > 0.0f && sourceFrameRate > 0.0f) {
        const double maxBaseFrameOffset =
            static_cast<double>(sourceNumFrames - 1u)
            * static_cast<double>(baseFrameRate)
            / static_cast<double>(sourceFrameRate);
        effectiveEndFrame =
            clampedStartFrame + std::max(0, static_cast<int>(std::floor(maxBaseFrameOffset + 1.0e-6)));
    }

    return std::min(clampedEndFrame, effectiveEndFrame);
}

static bool SceneHasCompatibleHierarchyForAnimation(
    const OW3D::Render::RenderScene& scene,
    int animationIndex)
{
    for (const auto& hierarchy : scene.hierarchies) {
        if (std::find(
            hierarchy.compatibleAnimationIndices.begin(),
            hierarchy.compatibleAnimationIndices.end(),
            animationIndex) != hierarchy.compatibleAnimationIndices.end())
        {
            return true;
        }
    }
    return false;
}

static int FindFirstPlayableAnimationIndex(
    const OW3D::Render::RenderScene& scene)
{
    for (int i = 0; i < static_cast<int>(scene.animations.size()); ++i) {
        const auto& clip = scene.animations[static_cast<std::size_t>(i)];
        if (clip.supportedForPlayback
            && SceneHasCompatibleHierarchyForAnimation(scene, i))
        {
            return i;
        }
    }
    return -1;
}

static int FindCompatibleHierarchyIndexForAnimation(
    const OW3D::Render::RenderScene& scene,
    int animationIndex,
    const std::string* preferredHierarchyName = nullptr)
{
    const auto normalizeHierarchyName = [](const std::string& value) {
        return QString::fromStdString(value).trimmed().toLower();
    };

    int firstCompatibleIndex = -1;
    const QString preferredNormalized = preferredHierarchyName
        ? normalizeHierarchyName(*preferredHierarchyName)
        : QString();

    for (std::size_t i = 0; i < scene.hierarchies.size(); ++i) {
        const auto& hierarchy = scene.hierarchies[i];
        if (std::find(
            hierarchy.compatibleAnimationIndices.begin(),
            hierarchy.compatibleAnimationIndices.end(),
            animationIndex) == hierarchy.compatibleAnimationIndices.end()) {
            continue;
        }

        if (firstCompatibleIndex < 0) {
            firstCompatibleIndex = static_cast<int>(i);
        }
        if (!preferredNormalized.isEmpty()
            && normalizeHierarchyName(hierarchy.name) == preferredNormalized) {
            return static_cast<int>(i);
        }
    }

    return firstCompatibleIndex;
}

static QString BuildRenderHierarchyIdentityKey(const OW3D::Render::RenderHierarchy& hierarchy)
{
    return QStringLiteral("%1#%2")
        .arg(QString::fromStdString(hierarchy.name).trimmed().toLower())
        .arg(static_cast<qulonglong>(hierarchy.pivots.size()));
}

static std::string NormalizeRenderPivotNameKey(const std::string& value)
{
    return QString::fromStdString(value).trimmed().toLower().toStdString();
}

static std::unordered_map<std::string, int> BuildRenderPivotIndexByName(
    const OW3D::Render::RenderHierarchy& hierarchy)
{
    std::unordered_map<std::string, int> out;
    out.reserve(hierarchy.pivots.size());
    for (int pivotIndex = 0; pivotIndex < static_cast<int>(hierarchy.pivots.size()); ++pivotIndex) {
        const std::string key =
            NormalizeRenderPivotNameKey(hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].name);
        if (!key.empty() && !out.contains(key)) {
            out.emplace(key, pivotIndex);
        }
    }
    return out;
}

static int ResolveMappedRenderPivotIndex(
    const OW3D::Render::RenderHierarchy& targetHierarchy,
    const std::unordered_map<std::string, int>& targetPivotIndexByName,
    const OW3D::Render::RenderHierarchy& selectionHierarchy,
    int selectionPivotIndex)
{
    if (selectionPivotIndex < 0
        || selectionPivotIndex >= static_cast<int>(selectionHierarchy.pivots.size())) {
        return -1;
    }

    const std::string selectionKey =
        NormalizeRenderPivotNameKey(
            selectionHierarchy.pivots[static_cast<std::size_t>(selectionPivotIndex)].name);
    if (!selectionKey.empty()) {
        const auto it = targetPivotIndexByName.find(selectionKey);
        if (it != targetPivotIndexByName.end()) {
            return it->second;
        }
        return -1;
    }

    return selectionPivotIndex >= 0
        && selectionPivotIndex < static_cast<int>(targetHierarchy.pivots.size())
        ? selectionPivotIndex
        : -1;
}

static QSet<int> ExpandBlendPivotSelection(
    const OW3D::Render::RenderHierarchy& hierarchy,
    const QSet<int>& selectedPivots,
    bool includeDescendants)
{
    QSet<int> expanded = selectedPivots;
    if (!includeDescendants || selectedPivots.isEmpty()) {
        return expanded;
    }

    std::vector<std::vector<int>> children(hierarchy.pivots.size());
    for (int pivotIndex = 0; pivotIndex < static_cast<int>(hierarchy.pivots.size()); ++pivotIndex) {
        const int parentIndex = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].parentIndex;
        if (parentIndex >= 0 && parentIndex < static_cast<int>(hierarchy.pivots.size())) {
            children[static_cast<std::size_t>(parentIndex)].push_back(pivotIndex);
        }
    }

    std::vector<int> stack;
    stack.reserve(selectedPivots.size());
    for (const int pivotIndex : selectedPivots) {
        if (pivotIndex >= 0 && pivotIndex < static_cast<int>(hierarchy.pivots.size())) {
            stack.push_back(pivotIndex);
        }
    }

    while (!stack.empty()) {
        const int pivotIndex = stack.back();
        stack.pop_back();
        for (const int childIndex : children[static_cast<std::size_t>(pivotIndex)]) {
            if (!expanded.contains(childIndex)) {
                expanded.insert(childIndex);
                stack.push_back(childIndex);
            }
        }
    }

    return expanded;
}

static QStringList CollectMissingRenderHierarchyNames(
    const OW3D::Render::SceneBuildResult& result)
{
    QSet<QString> names;
    for (const auto& warning : result.warnings) {
        if (warning.code != OW3D::Render::SceneBuildWarningCode::MissingHierarchy) {
            continue;
        }

        const QString message = QString::fromStdString(warning.message);
        const int sep = message.lastIndexOf(QStringLiteral(": "));
        const QString name = (sep >= 0)
            ? message.mid(sep + 2).trimmed()
            : message.trimmed();
        if (!name.isEmpty()) {
            names.insert(name.toLower());
        }
    }

    QStringList out = names.values();
    std::sort(out.begin(), out.end(), [](const QString& a, const QString& b) {
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });
    return out;
}

static QString BuildArchiveEntryDisplayPath(
    const QString& archivePath,
    const QString& archiveEntryPath)
{
    const QString normalizedArchivePath =
        QDir::cleanPath(QFileInfo(archivePath).absoluteFilePath());
    const QString normalizedEntryPath =
        QDir::fromNativeSeparators(archiveEntryPath).trimmed();

    if (normalizedArchivePath.isEmpty()) {
        return normalizedEntryPath;
    }
    if (normalizedEntryPath.isEmpty()) {
        return QDir::toNativeSeparators(normalizedArchivePath);
    }

    return QDir::toNativeSeparators(normalizedArchivePath)
        + QStringLiteral("::")
        + QDir::toNativeSeparators(normalizedEntryPath);
}

static QString BuildPrimaryRenderSourceName(
    const QString& currentFilePath,
    const QString& currentArchiveRenderPath,
    const QString& currentArchiveRenderEntryPath)
{
    if (!currentArchiveRenderPath.isEmpty()) {
        const QString entryPath =
            QDir::fromNativeSeparators(currentArchiveRenderEntryPath).trimmed();
        if (!entryPath.isEmpty()) {
            const QString fileName = QFileInfo(entryPath).fileName();
            return fileName.isEmpty() ? entryPath : fileName;
        }
    }

    return currentFilePath.isEmpty()
        ? QObject::tr("Current Scene")
        : QFileInfo(currentFilePath).fileName();
}

static QStringList CollectMissingRenderTextureNames(
    const OW3D::Render::SceneBuildResult& result)
{
    QSet<QString> names;
    for (const auto& warning : result.warnings) {
        if (warning.code != OW3D::Render::SceneBuildWarningCode::MissingTexture) {
            continue;
        }

        const QString message = QString::fromStdString(warning.message);
        const int sep = message.lastIndexOf(QStringLiteral(": "));
        const QString name = (sep >= 0)
            ? message.mid(sep + 2).trimmed()
            : message.trimmed();
        if (!name.isEmpty()) {
            names.insert(name);
        }
    }

    QStringList out = names.values();
    std::sort(out.begin(), out.end(), [](const QString& a, const QString& b) {
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });
    return out;
}

static QStringList CollectMissingAggregateRenderObjectNames(
    const OW3D::Render::SceneBuildResult& result)
{
    static const QString kMissingAggregateBasePrefix =
        QStringLiteral("Aggregate base model not found: ");
    static const QString kMissingAggregateSubObjectPrefix =
        QStringLiteral("Aggregate subobject render object not found: ");

    QSet<QString> names;
    for (const auto& warning : result.warnings) {
        if (warning.code != OW3D::Render::SceneBuildWarningCode::InvalidIndex) {
            continue;
        }

        const QString message = QString::fromStdString(warning.message).trimmed();
        if (message.startsWith(kMissingAggregateBasePrefix, Qt::CaseInsensitive)) {
            const QString name = message.mid(kMissingAggregateBasePrefix.size()).trimmed();
            if (!name.isEmpty()) {
                names.insert(name);
            }
            continue;
        }
        if (message.startsWith(kMissingAggregateSubObjectPrefix, Qt::CaseInsensitive)) {
            const QString name = message.mid(kMissingAggregateSubObjectPrefix.size()).trimmed();
            if (!name.isEmpty()) {
                names.insert(name);
            }
        }
    }

    QStringList out = names.values();
    std::sort(out.begin(), out.end(), [](const QString& a, const QString& b) {
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });
    return out;
}

static bool HasKnownW3DExtension(const QString& fileName) {
    return fileName.endsWith(QStringLiteral(".w3d"), Qt::CaseInsensitive)
        || fileName.endsWith(QStringLiteral(".wlt"), Qt::CaseInsensitive);
}

static void AddAggregateDependencyMatchTokens(const QString& referenceName, QSet<QString>& outTokens) {
    QString normalized = QDir::fromNativeSeparators(referenceName).trimmed().toLower();
    if (normalized.isEmpty()) {
        return;
    }

    outTokens.insert(normalized);

    const QFileInfo info(normalized);
    const QString fileName = info.fileName().trimmed().toLower();
    if (!fileName.isEmpty()) {
        outTokens.insert(fileName);
        if (!fileName.contains(QLatin1Char('.')) || HasKnownW3DExtension(fileName)) {
            const QString stem = info.completeBaseName().trimmed().toLower();
            if (!stem.isEmpty()) {
                outTokens.insert(stem);
            }
        }
    }
}

static QSet<QString> BuildAggregateDependencyMatchTokens(const QStringList& referenceNames) {
    QSet<QString> tokens;
    for (const QString& name : referenceNames) {
        AddAggregateDependencyMatchTokens(name, tokens);
    }
    return tokens;
}

static bool MatchesAggregateDependencyPath(const QString& candidatePath, const QSet<QString>& tokens) {
    if (tokens.isEmpty()) {
        return false;
    }

    const QString normalized = QDir::fromNativeSeparators(candidatePath).trimmed().toLower();
    if (normalized.isEmpty()) {
        return false;
    }
    if (tokens.contains(normalized)) {
        return true;
    }

    const QFileInfo info(normalized);
    const QString fileName = info.fileName().trimmed().toLower();
    if (!fileName.isEmpty() && tokens.contains(fileName)) {
        return true;
    }

    const QString stem = info.completeBaseName().trimmed().toLower();
    return !stem.isEmpty() && tokens.contains(stem);
}

static bool SceneHasRenderableMeshData(const OW3D::Render::SceneBuildResult& result)
{
    return !result.scene.looseNodes.empty()
        || !result.scene.lodGroups.empty();
}

static QString BuildPrimaryRenderSourceLabel(
    const QString& currentFilePath,
    const QString& currentArchiveRenderPath,
    const QString& currentArchiveRenderEntryPath,
    const ChunkData* chunkData)
{
    const QString baseLabel = BuildPrimaryRenderSourceName(
        currentFilePath,
        currentArchiveRenderPath,
        currentArchiveRenderEntryPath);

    QString suffix = QObject::tr("[primary]");
    if (chunkData) {
        const auto& roots = chunkData->getChunks();
        const int hierarchyCount = CollectHierarchyNamesFromRoots(roots).size();
        const int meshCount = CollectMeshCountFromRoots(roots);
        const int animationCount = CollectAnimationCountFromRoots(roots);

        if (meshCount > 0) {
            suffix = QObject::tr("[model]");
        }
        else if (hierarchyCount > 0) {
            suffix = QObject::tr("[skeleton]");
        }
        else if (animationCount > 0) {
            suffix = QObject::tr("[animation]");
        }
    }

    return baseLabel + QLatin1Char(' ') + suffix;
}

static bool LoadSupplementalRenderRootsFromFile(
    const QString& filePath,
    std::vector<std::shared_ptr<ChunkItem>>& outRoots,
    QString* outError = nullptr)
{
    outRoots.clear();

    ChunkData supplementalData;
    if (!supplementalData.loadFromFile(filePath.toStdString())
        || supplementalData.getChunks().empty())
    {
        if (outError) {
            *outError = QObject::tr("Failed to load supplemental W3D/WLT.");
        }
        return false;
    }

    const auto& parsedRoots = supplementalData.getChunks();
    outRoots.assign(parsedRoots.begin(), parsedRoots.end());
    return !outRoots.empty();
}

static QString BuildMapperArgsReference() {
    struct Entry { const char* name; const char* args; };
    static const Entry kEntries[] = {
        { "UV", "" },
        { "Environment", "" },
        { "ClassicEnvironment", "" },
        { "Screen", "UPerSec=0.0\nVPerSec=0.0\nUOffset=0.0\nVOffset=0.0\nClampFix=false\nUScale=1.0\nVScale=1.0" },
        { "LinearOffset", "UPerSec=0.0\nVPerSec=0.0\nUOffset=0.0\nVOffset=0.0\nClampFix=false\nUScale=1.0\nVScale=1.0" },
        { "Silhouette", "Not Supported" },
        { "Scale", "UScale=1.0\nVScale=1.0" },
        { "Grid", "FPS=1.0; The frames per second\nLog2Width=1; So 0=width 1, 1=width 2, 2=width 4. The default means animate using a texture divided up into quarters.\nLast=GridWidth*GridWidth; The last frame to use.\nOffset=0" },
        { "Rotate", "Speed=0.1; In Hertz. 1 = 1 rotate per second\nUCenter=0.0\nVCenter=0.0\nUScale=1.0\nVScale=1.0" },
        { "Sine", "UAmp=1.0\nUFreq=1.0\nUPhase=0.0\nVAmp=1.0\nVFreq=1.0\nVPhase=0.0\nUScale=1.0\nVScale=1.0" },
        { "Step", "UStep=0.0\nVStep=0.0\nSPS=0.0; Steps per second\nClampFix=false\nUScale=1.0\nVScale=1.0" },
        { "ZigZag", "UPerSec=0.0\nVPerSec=0.0\nPeriod=0.0; Time it takes to make a zigzag in seconds\nUScale=1.0\nVScale=1.0" },
        { "WSClassicEnv", "Axis=Z; Axis to use for this, X, Y, Z" },
        { "WSEnvironment", "Axis=Z; Axis to use for this, X, Y, Z" },
        { "GridClassicEnv", "FPS=1.0; The frames per second\nLog2Width=1; So 0=width 1, 1=width 2, 2=width 4. The default means animate using a texture divided up into quarters.\nLast=GridWidth*GridWidth; The last frame to use.\nOffset=0" },
        { "GridEnvironment", "FPS=1.0; The frames per second\nLog2Width=1; So 0=width 1, 1=width 2, 2=width 4. The default means animate using a texture divided up into quarters.\nLast=GridWidth*GridWidth; The last frame to use.\nOffset=0" },
        { "Random", "FPS=0.0; Frames per second\nUPerSec=0.0\nVPerSec=0.0\nUScale=1.0\nVScale=1.0" },
        { "Edge", "VPerSec=0.0\nUseReflect=false\nVStart=0.0" },
        { "BumpEnv", "BumpRotation = 0.1; In Hertz. 1 = 1 rotate per second  (DEFAULT = 0.0)\nBumpScale = scale factor applied to the bumps\t(DEFAULT = 1.0)\nUPerSec=0.0\nVPerSec=0.0\nUScale=1.0\nVScale=1.0" },
        { "GridWSClassicEnv", "FPS=1.0; The frames per second\nLog2Width=1; So 0=width 1, 1=width 2, 2=width 4. The default means animate using a texture divided up into quarters.\nLast=GridWidth*GridWidth; The last frame to use.\nOffset=0\nAxis=Z; Axis to use for this, X, Y, Z" },
        { "GridWSEnv", "FPS=1.0; The frames per second\nLog2Width=1; So 0=width 1, 1=width 2, 2=width 4. The default means animate using a texture divided up into quarters.\nLast=GridWidth*GridWidth; The last frame to use.\nOffset=0\nAxis=Z; Axis to use for this, X, Y, Z" },
    };

    QString out = QStringLiteral("Mapping args reference (original Max tools defaults)\n\n");
    for (const auto& entry : kEntries) {
        out += QString::fromLatin1(entry.name);
        if (entry.args && entry.args[0] != '\0') {
            out += QStringLiteral(":\n");
            out += QString::fromLatin1(entry.args);
        }
        else {
            out += QStringLiteral(": (no args)");
        }
        out += QStringLiteral("\n\n");
    }
    out += QStringLiteral("Notes: keys are case-sensitive; omit parentheses in values; types are float/int/bool.");
    return out;
}

static QString FormatHexBlock(const std::vector<uint8_t>& data) {
    if (data.empty()) return {};
    constexpr std::size_t kBytesPerLine = 16;
    QString out;
    for (std::size_t i = 0; i < data.size(); i += kBytesPerLine) {
        out += QStringLiteral("%1: ")
            .arg(static_cast<uint32_t>(i), 4, 16, QLatin1Char('0')).toUpper();
        const std::size_t end = std::min(i + kBytesPerLine, data.size());
        for (std::size_t j = i; j < end; ++j) {
            out += QStringLiteral("%1").arg(data[j], 2, 16, QLatin1Char('0')).toUpper();
            if (j + 1 < end) out += QLatin1Char(' ');
        }
        out += QLatin1Char('\n');
    }
    if (!out.isEmpty()) out.chop(1);
    return out;
}

constexpr uint32_t MeshAttrValue(MeshAttr attr) {
    return static_cast<uint32_t>(attr);
}

constexpr uint16_t TextureAttrValue(TextureAttr attr) {
    return static_cast<uint16_t>(attr);
}

static int ClampToInt(uint32_t value) {
    if (value > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(value);
}

static QString RenameIfMatches(const QString& current,
    const QString& oldBase,
    const QString& newBase,
    const QString& oldFile,
    const QString& newFile) {
    if (current.isEmpty()) return current;
    if (!oldFile.isEmpty() && current == oldFile) {
        return newFile;
    }
    if (!oldBase.isEmpty() && current == oldBase) {
        return newBase;
    }
    return current;
}

static QString RenameFullName(const QString& current,
    const QString& oldBase,
    const QString& newBase,
    const QString& oldFile,
    const QString& newFile) {
    if (current.isEmpty()) return current;
    if (!oldFile.isEmpty() && current == oldFile) {
        return newFile;
    }
    if (!oldBase.isEmpty() && current == oldBase) {
        return newBase;
    }
    const int dot = current.indexOf(QLatin1Char('.'));
    if (dot < 0) {
        return current;
    }
    QString containerPart = current.left(dot);
    QString objectPart = current.mid(dot + 1);
    bool changed = false;
    if (!oldBase.isEmpty() && containerPart == oldBase) {
        containerPart = newBase;
        changed = true;
    }
    if (!oldBase.isEmpty() && objectPart == oldBase) {
        objectPart = newBase;
        changed = true;
    }
    if (!changed) {
        return current;
    }
    return containerPart + QLatin1Char('.') + objectPart;
}

static QString RenameAnimationTokenForHierarchyChange(
    const QString& current,
    const QString& oldHierarchyName,
    const QString& newHierarchyName)
{
    if (current.isEmpty()
        || oldHierarchyName.isEmpty()
        || oldHierarchyName == newHierarchyName)
    {
        return current;
    }

    const auto normalizeName = [](QString value) -> QString {
        value = value.trimmed().toLower();
        if (value.endsWith(QStringLiteral(".w3d"))) {
            value.chop(4);
        }
        return value;
    };
    const auto sameName = [&](const QString& lhs, const QString& rhs) -> bool {
        return normalizeName(lhs) == normalizeName(rhs);
    };

    const QString updatedFullName = RenameFullName(
        current,
        oldHierarchyName,
        newHierarchyName,
        QString(),
        QString());
    if (updatedFullName != current) {
        return updatedFullName;
    }

    if (sameName(current, oldHierarchyName)) {
        return newHierarchyName;
    }

    if (current.startsWith(oldHierarchyName, Qt::CaseInsensitive)
        && current.size() > oldHierarchyName.size()) {
        const QChar boundary = current.at(oldHierarchyName.size());
        if (boundary == QLatin1Char('_')
            || boundary == QLatin1Char('.')
            || boundary == QLatin1Char('-'))
        {
            return newHierarchyName + current.mid(oldHierarchyName.size());
        }
    }

    return current;
}

constexpr int kW3DNameMax = static_cast<int>(W3D_NAME_LEN) - 1;
constexpr int kMeshNameMax = kW3DNameMax;
constexpr int kPivotNameMax = kW3DNameMax;

static W3dVectorStruct EulerFromQuaternion(
    float x,
    float y,
    float z,
    float w)
{
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;

    const float sinr_cosp = 2.0f * (w * x + y * z);
    const float cosr_cosp = 1.0f - 2.0f * (xx + yy);
    const float roll = std::atan2(sinr_cosp, cosr_cosp);

    const float sinp = 2.0f * (w * y - z * x);
    const float pitch = (std::fabs(sinp) >= 1.0f)
        ? std::copysign(1.57079632679f, sinp)
        : std::asin(sinp);

    const float siny_cosp = 2.0f * (w * z + x * y);
    const float cosy_cosp = 1.0f - 2.0f * (yy + zz);
    const float yaw = std::atan2(siny_cosp, cosy_cosp);

    W3dVectorStruct euler{};
    euler.X = roll;
    euler.Y = pitch;
    euler.Z = yaw;
    return euler;
}

static OW3D::Render::Vec4 QuaternionFromEulerRadians(
    float roll,
    float pitch,
    float yaw)
{
    const float cy = std::cos(yaw * 0.5f);
    const float sy = std::sin(yaw * 0.5f);
    const float cp = std::cos(pitch * 0.5f);
    const float sp = std::sin(pitch * 0.5f);
    const float cr = std::cos(roll * 0.5f);
    const float sr = std::sin(roll * 0.5f);

    return OW3D::Render::AnimationNormalizeQuat({
        sr * cp * cy - cr * sp * sy,
        cr * sp * cy + sr * cp * sy,
        cr * cp * sy - sr * sp * cy,
        cr * cp * cy + sr * sp * sy
    });
}

static float BlendWrappedAngleRadians(float baseAngle, float targetAngle, float weight)
{
    constexpr float kTwoPi = 6.2831853071795864769f;
    const float shortestDelta = std::remainder(targetAngle - baseAngle, kTwoPi);
    return baseAngle + shortestDelta * std::clamp(weight, 0.0f, 1.0f);
}

template <typename Enum>
static void PopulateEnumCombo(QComboBox* combo) {
    combo->clear();
    using Under = std::underlying_type_t<Enum>;
    for (Under i = 0; i < static_cast<Under>(Enum::MAX); ++i) {
        const auto name = ToString(static_cast<Enum>(i));
        combo->addItem(QString::fromUtf8(name), static_cast<int>(i));
    }
}

static void SetComboValue(QComboBox* combo, int value) {
    int idx = combo->findData(value);
    if (idx == -1) {
        combo->addItem(QString::fromLatin1("Unknown (%1)").arg(value), value);
        idx = combo->count() - 1;
    }
    combo->setCurrentIndex(idx);
}

static uint8_t ComboValue(const QComboBox* combo) {
    const QVariant data = combo->currentData();
    if (!data.isValid()) {
        return static_cast<uint8_t>(combo->currentIndex());
    }
    return static_cast<uint8_t>(data.toInt());
}

Q_DECLARE_METATYPE(void*)

namespace {
struct MixEntryInfo {
    uint32_t id = 0;      // CRC/hash in the mix directory.
    uint32_t offset = 0;  // Absolute offset in the archive.
    uint32_t size = 0;
    QString name;
};

struct MixArchiveInfo {
    std::vector<MixEntryInfo> entries;
    bool isMix1 = false;
    bool hasNames = false;
    uint32_t flags = 0;
    bool hasFlags = false;
};

struct LoadedArchiveRenderContext {
    QString archivePath;
    uint32_t selectedEntryId = 0;
    QString selectedEntryPath;
    std::vector<ArchiveRenderEntryInfo> entries;
    std::vector<std::string> textureEntryNames;
    std::vector<uint32_t> textureEntryIds;
    std::vector<ArchiveTextureSourceInfo> textureSources;
};

static bool LoadChunkDataFromBytes(
    const QByteArray& bytes,
    ChunkData& outChunkData,
    QString& outError);

static QString NormalizeArchiveEntryPath(QString entryName, uint32_t entryId);

static bool ReadUInt16LE(const QByteArray& bytes, qsizetype offset, uint16_t& out) {
    if (offset < 0 || (offset + 2) > bytes.size()) {
        return false;
    }
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.constData() + offset);
    out = static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
    return true;
}

static bool ReadUInt32LE(const QByteArray& bytes, qsizetype offset, uint32_t& out) {
    if (offset < 0 || (offset + 4) > bytes.size()) {
        return false;
    }
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.constData() + offset);
    out = static_cast<uint32_t>(p[0])
        | (static_cast<uint32_t>(p[1]) << 8)
        | (static_cast<uint32_t>(p[2]) << 16)
        | (static_cast<uint32_t>(p[3]) << 24);
    return true;
}

static bool ParseMix1Archive(const QByteArray& bytes, MixArchiveInfo& outArchive, QString* outError) {
    outArchive = {};
    if (bytes.size() < 16) {
        if (outError) {
            *outError = "File is too small to be a valid MIX1 archive.";
        }
        return false;
    }
    if (std::memcmp(bytes.constData(), "MIX1", 4) != 0) {
        if (outError) {
            *outError = "Missing MIX1 signature.";
        }
        return false;
    }

    uint32_t headerOffset = 0;
    uint32_t namesOffset = 0;
    if (!ReadUInt32LE(bytes, 4, headerOffset) || !ReadUInt32LE(bytes, 8, namesOffset)) {
        if (outError) {
            *outError = "MIX1 header is truncated.";
        }
        return false;
    }
    if (headerOffset > static_cast<uint32_t>(bytes.size() - 4)
        || namesOffset > static_cast<uint32_t>(bytes.size() - 4)) {
        if (outError) {
            *outError = "MIX1 header offsets are invalid.";
        }
        return false;
    }

    uint32_t fileCount32 = 0;
    if (!ReadUInt32LE(bytes, static_cast<qsizetype>(headerOffset), fileCount32)) {
        if (outError) {
            *outError = "Failed to read MIX1 file count.";
        }
        return false;
    }
    if (fileCount32 == 0) {
        if (outError) {
            *outError = "MIX1 archive has no entries.";
        }
        return false;
    }

    const qsizetype fileCount = static_cast<qsizetype>(fileCount32);
    const qsizetype indexStart = static_cast<qsizetype>(headerOffset) + 4;
    const qsizetype indexBytes = fileCount * 12;
    if (indexStart + indexBytes > bytes.size()) {
        if (outError) {
            *outError = "MIX1 directory is truncated.";
        }
        return false;
    }

    outArchive.entries.reserve(static_cast<std::size_t>(fileCount));
    qsizetype pos = indexStart;
    for (qsizetype i = 0; i < fileCount; ++i) {
        MixEntryInfo entry;
        if (!ReadUInt32LE(bytes, pos, entry.id)
            || !ReadUInt32LE(bytes, pos + 4, entry.offset)
            || !ReadUInt32LE(bytes, pos + 8, entry.size)) {
            if (outError) {
                *outError = "MIX1 directory is truncated.";
            }
            return false;
        }
        pos += 12;

        if (entry.offset > static_cast<uint32_t>(bytes.size())
            || entry.size > static_cast<uint32_t>(bytes.size() - static_cast<qsizetype>(entry.offset))) {
            if (outError) {
                *outError = "MIX1 entry has an invalid offset/size.";
            }
            return false;
        }
        outArchive.entries.push_back(entry);
    }

    uint32_t namesCount32 = 0;
    if (ReadUInt32LE(bytes, static_cast<qsizetype>(namesOffset), namesCount32)
        && namesCount32 == fileCount32) {
        qsizetype namePos = static_cast<qsizetype>(namesOffset) + 4;
        bool namesOk = true;
        for (qsizetype i = 0; i < fileCount; ++i) {
            if (namePos >= bytes.size()) {
                namesOk = false;
                break;
            }

            const uint8_t nameLen = static_cast<uint8_t>(bytes.at(namePos));
            ++namePos;
            if (nameLen == 0 || (namePos + nameLen) > bytes.size()) {
                namesOk = false;
                break;
            }

            QByteArray rawName = bytes.mid(namePos, nameLen);
            namePos += nameLen;

            const int nulIndex = rawName.indexOf('\0');
            if (nulIndex >= 0) {
                rawName.truncate(nulIndex);
            }
            outArchive.entries[static_cast<std::size_t>(i)].name = QString::fromLatin1(rawName);
        }

        if (namesOk) {
            outArchive.hasNames = true;
        }
    }

    outArchive.isMix1 = true;
    return true;
}

static bool ParseClassicMixArchive(const QByteArray& bytes, MixArchiveInfo& outArchive, QString* outError) {
    outArchive = {};
    if (bytes.size() < 6) {
        if (outError) {
            *outError = "File is too small to be a valid MIX archive.";
        }
        return false;
    }

    qsizetype cursor = 0;
    uint32_t firstWord = 0;
    if (!ReadUInt32LE(bytes, 0, firstWord)) {
        if (outError) {
            *outError = "Failed to read MIX header.";
        }
        return false;
    }

    constexpr uint32_t kMixFlagChecksum = 0x00010000u;
    constexpr uint32_t kMixFlagEncrypted = 0x00020000u;
    constexpr uint32_t kKnownMixFlags = kMixFlagChecksum | kMixFlagEncrypted;

    if ((firstWord & kKnownMixFlags) != 0u && (firstWord & ~kKnownMixFlags) == 0u) {
        outArchive.hasFlags = true;
        outArchive.flags = firstWord;
        cursor = 4;
        if ((outArchive.flags & kMixFlagEncrypted) != 0u) {
            if (outError) {
                *outError = "Encrypted MIX archives are not supported.";
            }
            return false;
        }
    }

    uint16_t fileCount = 0;
    uint32_t dataSize = 0;
    if (!ReadUInt16LE(bytes, cursor, fileCount) || !ReadUInt32LE(bytes, cursor + 2, dataSize)) {
        if (outError) {
            *outError = "MIX header is truncated.";
        }
        return false;
    }
    if (fileCount == 0) {
        if (outError) {
            *outError = "MIX archive has no entries.";
        }
        return false;
    }

    const qsizetype indexStart = cursor + 6;
    const qsizetype entryBytes = static_cast<qsizetype>(fileCount) * 12;
    if (indexStart + entryBytes > bytes.size()) {
        if (outError) {
            *outError = "MIX entry index is truncated.";
        }
        return false;
    }

    if (dataSize > static_cast<uint32_t>(bytes.size())) {
        if (outError) {
            *outError = "MIX data size is invalid.";
        }
        return false;
    }
    const qsizetype dataStart = bytes.size() - static_cast<qsizetype>(dataSize);
    if (dataStart < indexStart + entryBytes) {
        if (outError) {
            *outError = "MIX header/index overlaps file data.";
        }
        return false;
    }

    outArchive.entries.reserve(fileCount);

    qsizetype pos = indexStart;
    for (uint16_t i = 0; i < fileCount; ++i) {
        MixEntryInfo entry;
        if (!ReadUInt32LE(bytes, pos, entry.id)
            || !ReadUInt32LE(bytes, pos + 4, entry.offset)
            || !ReadUInt32LE(bytes, pos + 8, entry.size)) {
            if (outError) {
                *outError = "MIX entry index is truncated.";
            }
            return false;
        }
        pos += 12;

        if (entry.offset > dataSize || entry.size > (dataSize - entry.offset)) {
            if (outError) {
                *outError = "MIX entry has an invalid offset/size.";
            }
            return false;
        }
        entry.offset = static_cast<uint32_t>(dataStart + static_cast<qsizetype>(entry.offset));
        outArchive.entries.push_back(entry);
    }

    return true;
}

static bool ParseMixArchive(
    const QByteArray& bytes,
    bool allowClassicFallback,
    MixArchiveInfo& outArchive,
    QString* outError) {
    if (bytes.size() >= 4 && std::memcmp(bytes.constData(), "MIX1", 4) == 0) {
        return ParseMix1Archive(bytes, outArchive, outError);
    }

    if (!allowClassicFallback) {
        if (outError) {
            *outError = "Archive is not in MIX1 format.";
        }
        return false;
    }

    return ParseClassicMixArchive(bytes, outArchive, outError);
}

static bool IsMixArchivePath(const QString& path) {
    const QString normalized = QDir::fromNativeSeparators(path).trimmed();
    return normalized.endsWith(QStringLiteral(".mix"), Qt::CaseInsensitive)
        || normalized.endsWith(QStringLiteral(".dat"), Qt::CaseInsensitive)
        || normalized.endsWith(QStringLiteral(".dbs"), Qt::CaseInsensitive);
}

static bool IsTextureFileName(const QString& path) {
    const QString lowerPath = QDir::fromNativeSeparators(path).trimmed().toLower();
    return lowerPath.endsWith(QStringLiteral(".dds"))
        || lowerPath.endsWith(QStringLiteral(".tga"))
        || lowerPath.endsWith(QStringLiteral(".png"))
        || lowerPath.endsWith(QStringLiteral(".jpg"))
        || lowerPath.endsWith(QStringLiteral(".jpeg"))
        || lowerPath.endsWith(QStringLiteral(".bmp"));
}

static bool ReadArchiveEntryBytes(
    const QString& archivePath,
    uint32_t offset,
    uint32_t size,
    QByteArray& outBytes,
    QString* outError)
{
    outBytes.clear();
    QFile file(archivePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outError) {
            *outError = QObject::tr("Failed to open archive file: %1").arg(file.errorString());
        }
        return false;
    }

    const qint64 archiveSize = file.size();
    const qint64 qOffset = static_cast<qint64>(offset);
    const qint64 qSize = static_cast<qint64>(size);
    if (qOffset < 0 || qSize < 0 || qOffset > archiveSize || qSize > (archiveSize - qOffset)) {
        if (outError) {
            *outError = QObject::tr("Archive entry has invalid offset/size.");
        }
        return false;
    }

    if (!file.seek(qOffset)) {
        if (outError) {
            *outError = QObject::tr("Failed to seek archive entry.");
        }
        return false;
    }

    outBytes = file.read(qSize);
    if (outBytes.size() != qSize) {
        if (outError) {
            *outError = QObject::tr("Failed to read archive entry bytes.");
        }
        return false;
    }
    return true;
}

static std::string NormalizeTexturePathKey(std::string input) {
    for (char& c : input) {
        if (c == '\\') {
            c = '/';
        }
        else {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return input;
}

static std::string TextureBaseNameKey(const std::string& input) {
    const std::string normalized = NormalizeTexturePathKey(input);
    const std::size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos) {
        return normalized;
    }
    return normalized.substr(slash + 1);
}

static std::string ToUpperAscii(std::string input) {
    for (char& c : input) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return input;
}

static uint32_t ComputeCRC32Stringi(const std::string& value) {
    static uint32_t table[256];
    static bool initialized = false;
    if (!initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                if (crc & 1U) {
                    crc = (crc >> 1) ^ 0xEDB88320U;
                }
                else {
                    crc >>= 1;
                }
            }
            table[i] = crc;
        }
        initialized = true;
    }

    uint32_t crc = 0xFFFFFFFFU;
    const std::string upper = ToUpperAscii(value);
    for (const unsigned char c : upper) {
        crc = (crc >> 8) ^ table[(crc ^ c) & 0xFFU];
    }
    return crc ^ 0xFFFFFFFFU;
}

static void AddTextureHashCandidate(
    const std::string& candidate,
    std::vector<uint32_t>& outCandidates,
    std::unordered_set<uint32_t>& seen)
{
    if (candidate.empty()) {
        return;
    }

    auto addHash = [&](const std::string& text) {
        if (text.empty()) {
            return;
        }
        const uint32_t hash = ComputeCRC32Stringi(text);
        if (seen.insert(hash).second) {
            outCandidates.push_back(hash);
        }
    };

    addHash(candidate);

    std::string slashVariant = candidate;
    std::replace(slashVariant.begin(), slashVariant.end(), '\\', '/');
    if (slashVariant != candidate) {
        addHash(slashVariant);
    }

    std::replace(slashVariant.begin(), slashVariant.end(), '/', '\\');
    if (slashVariant != candidate) {
        addHash(slashVariant);
    }
}

static std::vector<uint32_t> BuildTextureHashCandidates(const std::string& textureName) {
    static const std::array<const char*, 6> kTextureExtensions = {
        ".dds", ".tga", ".png", ".jpg", ".jpeg", ".bmp"
    };

    std::vector<uint32_t> out;
    std::unordered_set<uint32_t> seen;

    const std::string pathKey = NormalizeTexturePathKey(textureName);
    const std::string baseKey = TextureBaseNameKey(pathKey);
    AddTextureHashCandidate(pathKey, out, seen);
    AddTextureHashCandidate(baseKey, out, seen);

    const std::filesystem::path normalizedPath(pathKey);
    const std::filesystem::path basePath(baseKey);
    const std::string stem = basePath.stem().string();
    const std::string pathStem = normalizedPath.stem().string();
    for (const char* ext : kTextureExtensions) {
        if (!stem.empty()) {
            AddTextureHashCandidate(stem + ext, out, seen);
        }
        if (!pathStem.empty() && normalizedPath.has_parent_path()) {
            const std::filesystem::path siblingPath =
                normalizedPath.parent_path() / (pathStem + ext);
            AddTextureHashCandidate(
                NormalizeTexturePathKey(siblingPath.generic_string()),
                out,
                seen);
        }
    }

    return out;
}

static bool LooksLikeTgaPayload(const QByteArray& bytes) {
    if (bytes.size() < 18) {
        return false;
    }

    const uint8_t colorMapType = static_cast<uint8_t>(bytes.at(1));
    const uint8_t imageType = static_cast<uint8_t>(bytes.at(2));
    const uint8_t pixelDepth = static_cast<uint8_t>(bytes.at(16));

    if (colorMapType > 1) {
        return false;
    }
    if (imageType != 1 && imageType != 2 && imageType != 3
        && imageType != 9 && imageType != 10 && imageType != 11) {
        return false;
    }
    if (pixelDepth != 8 && pixelDepth != 16 && pixelDepth != 24 && pixelDepth != 32) {
        return false;
    }
    return true;
}

static QString InferTextureExtension(const QByteArray& bytes, const QString& sourceName) {
    const QString sourceExt = QFileInfo(sourceName).suffix().toLower();
    if (sourceExt == QStringLiteral("dds")
        || sourceExt == QStringLiteral("tga")
        || sourceExt == QStringLiteral("png")
        || sourceExt == QStringLiteral("jpg")
        || sourceExt == QStringLiteral("jpeg")
        || sourceExt == QStringLiteral("bmp")) {
        return QStringLiteral(".") + sourceExt;
    }

    if (bytes.size() >= 4 && std::memcmp(bytes.constData(), "DDS ", 4) == 0) {
        return QStringLiteral(".dds");
    }
    if (bytes.size() >= 8
        && static_cast<uint8_t>(bytes[0]) == 0x89
        && std::memcmp(bytes.constData() + 1, "PNG", 3) == 0) {
        return QStringLiteral(".png");
    }
    if (bytes.size() >= 2
        && static_cast<uint8_t>(bytes[0]) == 0xFF
        && static_cast<uint8_t>(bytes[1]) == 0xD8) {
        return QStringLiteral(".jpg");
    }
    if (bytes.size() >= 2
        && static_cast<uint8_t>(bytes[0]) == 'B'
        && static_cast<uint8_t>(bytes[1]) == 'M') {
        return QStringLiteral(".bmp");
    }
    if (LooksLikeTgaPayload(bytes)) {
        return QStringLiteral(".tga");
    }

    return {};
}

static bool MaterializeArchiveTextureToCache(
    const ArchiveTextureSourceInfo& source,
    const QString& cacheDir,
    QString& outPath)
{
    outPath.clear();
    if (source.size == 0) {
        return false;
    }

    QByteArray bytes;
    QString readError;
    if (!ReadArchiveEntryBytes(source.archivePath, source.offset, source.size, bytes, &readError)) {
        return false;
    }

    const QString extension = InferTextureExtension(bytes, source.name);
    if (extension.isEmpty()) {
        return false;
    }

    const QString archiveHash = QString::number(qHash(QDir::cleanPath(source.archivePath)), 16);
    const QString fileName = QStringLiteral("%1_%2_%3%4")
        .arg(source.id, 8, 16, QLatin1Char('0'))
        .arg(source.size)
        .arg(archiveHash)
        .arg(extension)
        .toLower();
    const QString filePath = QDir(cacheDir).absoluteFilePath(fileName);

    QFile existing(filePath);
    if (existing.exists() && existing.size() == static_cast<qint64>(bytes.size())) {
        outPath = filePath;
        return true;
    }

    if (!QDir().mkpath(cacheDir)) {
        return false;
    }

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    if (outFile.write(bytes) != bytes.size()) {
        outFile.close();
        return false;
    }
    outFile.close();

    outPath = filePath;
    return true;
}

static bool LooksLikeW3DStream(
    const QByteArray& bytes,
    qsizetype absoluteOffset,
    uint32_t size,
    uint32_t* outTopChunkId = nullptr,
    QString* outTopChunkName = nullptr) {
    if (size < 8 || absoluteOffset < 0 || absoluteOffset + 8 > bytes.size()) {
        return false;
    }

    uint32_t topId = 0;
    uint32_t rawLength = 0;
    if (!ReadUInt32LE(bytes, absoluteOffset, topId)
        || !ReadUInt32LE(bytes, absoluteOffset + 4, rawLength)) {
        return false;
    }

    const uint32_t payloadLength = rawLength & 0x7FFFFFFFu;
    if (payloadLength > (size - 8)) {
        return false;
    }

    const std::string chunkName = GetChunkName(topId);
    const bool knownChunk = (chunkName != "UNKNOWN");

    if (outTopChunkId) {
        *outTopChunkId = topId;
    }
    if (outTopChunkName) {
        *outTopChunkName = QString::fromStdString(chunkName);
    }

    return knownChunk;
}

static QString BuildMixEntryLabel(
    const MixEntryInfo& entry,
    bool likelyW3d,
    bool hasTopChunkInfo,
    uint32_t topChunkId,
    const QString& topChunkName) {
    QString label;
    if (!entry.name.isEmpty()) {
        label = QStringLiteral("%1 (%2 bytes)")
            .arg(entry.name)
            .arg(entry.size);
        label += QStringLiteral(" [0x%1]")
            .arg(entry.id, 8, 16, QLatin1Char('0'))
            .toUpper();
    }
    else {
        label = QStringLiteral("0x%1 (%2 bytes)")
            .arg(entry.id, 8, 16, QLatin1Char('0'))
            .arg(entry.size)
            .toUpper();
    }
    if (likelyW3d && hasTopChunkInfo) {
        label += QStringLiteral("  -> 0x%1 (%2)")
            .arg(topChunkId, 8, 16, QLatin1Char('0'))
            .arg(topChunkName);
    }
    return label;
}

static bool LoadW3DFromMixArchive(
    QWidget* parent,
    const QString& mixPath,
    ChunkData& chunkData,
    QString* outError,
    LoadedArchiveRenderContext* outRenderContext) {
    if (outRenderContext) {
        *outRenderContext = {};
    }

    const bool allowClassicFallback = IsMixArchivePath(mixPath);

    QFile file(mixPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outError) {
            *outError = QStringLiteral("Failed to open archive file:\n%1").arg(file.errorString());
        }
        return false;
    }
    const QByteArray mixBytes = file.readAll();
    file.close();

    MixArchiveInfo archive;
    QString parseError;
    if (!ParseMixArchive(mixBytes, allowClassicFallback, archive, &parseError)) {
        if (outError) {
            *outError = parseError;
        }
        return false;
    }

    struct Candidate {
        int entryIndex = -1;
        QString label;
        bool likelyByName = false;
        bool likelyW3d = false;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(archive.entries.size());

    for (int i = 0; i < static_cast<int>(archive.entries.size()); ++i) {
        const auto& entry = archive.entries[static_cast<std::size_t>(i)];
        const qsizetype absoluteOffset = static_cast<qsizetype>(entry.offset);
        uint32_t topChunkId = 0;
        QString topChunkName;
        const bool isLikelyByName =
            entry.name.endsWith(QStringLiteral(".w3d"), Qt::CaseInsensitive)
            || entry.name.endsWith(QStringLiteral(".wlt"), Qt::CaseInsensitive);
        const bool isLikelyByContent = LooksLikeW3DStream(
            mixBytes,
            absoluteOffset,
            entry.size,
            &topChunkId,
            &topChunkName);
        const bool likelyW3d = isLikelyByName || isLikelyByContent;
        candidates.push_back(Candidate{
            i,
            BuildMixEntryLabel(entry, likelyW3d, isLikelyByContent, topChunkId, topChunkName),
            isLikelyByName,
            likelyW3d
            });
    }

    std::vector<int> candidateIndexes;
    for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
        if (candidates[static_cast<std::size_t>(i)].likelyW3d) {
            candidateIndexes.push_back(i);
        }
    }

    std::vector<bool> entryLikelyW3d(archive.entries.size(), false);
    for (const Candidate& candidate : candidates) {
        if (candidate.entryIndex >= 0
            && candidate.entryIndex < static_cast<int>(entryLikelyW3d.size())) {
            entryLikelyW3d[static_cast<std::size_t>(candidate.entryIndex)] = candidate.likelyW3d;
        }
    }

    int chosenCandidateIndex = candidateIndexes.empty() ? -1 : candidateIndexes.front();
    const bool needsExplicitSelection =
        candidateIndexes.empty()
        || (candidateIndexes.size() > 1)
        || !candidates[static_cast<std::size_t>(candidateIndexes.front())].likelyByName;
    if (needsExplicitSelection) {
        std::vector<int> sortedCandidateIndexes;
        sortedCandidateIndexes.reserve(candidates.size());
        for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
            sortedCandidateIndexes.push_back(i);
        }
        std::sort(
            sortedCandidateIndexes.begin(),
            sortedCandidateIndexes.end(),
            [&](int lhs, int rhs) {
                const auto& lhsEntry = archive.entries[static_cast<std::size_t>(candidates[static_cast<std::size_t>(lhs)].entryIndex)];
                const auto& rhsEntry = archive.entries[static_cast<std::size_t>(candidates[static_cast<std::size_t>(rhs)].entryIndex)];

                const QString lhsName = lhsEntry.name.isEmpty()
                    ? QStringLiteral("entry_%1.bin").arg(lhsEntry.id, 8, 16, QLatin1Char('0')).toUpper()
                    : QDir::fromNativeSeparators(lhsEntry.name);
                const QString rhsName = rhsEntry.name.isEmpty()
                    ? QStringLiteral("entry_%1.bin").arg(rhsEntry.id, 8, 16, QLatin1Char('0')).toUpper()
                    : QDir::fromNativeSeparators(rhsEntry.name);

                const int nameCompare = QString::compare(lhsName, rhsName, Qt::CaseInsensitive);
                if (nameCompare != 0) return nameCompare < 0;
                if (lhsEntry.size != rhsEntry.size) return lhsEntry.size < rhsEntry.size;
                return lhsEntry.id < rhsEntry.id;
            });

        QDialog picker(parent);
        picker.setWindowTitle(QStringLiteral("Open Archive Entry"));
        picker.resize(920, 560);

        auto* layout = new QVBoxLayout(&picker);
        const QString promptText = candidateIndexes.empty()
            ? QStringLiteral("Browsing entries in %1. No W3D/WLT entries are currently openable (use \"Show all file types\" to inspect unsupported entries).")
            : QStringLiteral("Select a W3D/WLT entry from %1 (unsupported types are hidden by default):");
        auto* promptLabel = new QLabel(
            promptText.arg(QFileInfo(mixPath).fileName()),
            &picker);
        promptLabel->setWordWrap(true);
        layout->addWidget(promptLabel);

        auto* filterEdit = new QLineEdit(&picker);
        filterEdit->setPlaceholderText(QStringLiteral("Filter by name or CRC..."));
        layout->addWidget(filterEdit);

        auto* showAllCheck = new QCheckBox(QStringLiteral("Show all file types"), &picker);
        showAllCheck->setChecked(false);
        layout->addWidget(showAllCheck);

        auto* entryTree = new QTreeWidget(&picker);
        entryTree->setColumnCount(4);
        entryTree->setHeaderLabels(QStringList{
            QStringLiteral("Name"),
            QStringLiteral("Size"),
            QStringLiteral("CRC"),
            QStringLiteral("Hint")
            });
        entryTree->setRootIsDecorated(true);
        entryTree->setAlternatingRowColors(true);
        entryTree->setSelectionMode(QAbstractItemView::SingleSelection);
        entryTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        entryTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        entryTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        entryTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        layout->addWidget(entryTree, 1);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &picker);
        auto* openButton = buttons->button(QDialogButtonBox::Ok);
        if (openButton) {
            openButton->setText(QStringLiteral("Open"));
            openButton->setEnabled(false);
        }
        layout->addWidget(buttons);

        constexpr int kRoleCandidateIndex = Qt::UserRole;
        constexpr int kRoleIsLeaf = Qt::UserRole + 1;
        constexpr int kRoleOpenable = Qt::UserRole + 2;

        const auto findFolderChild = [&](QTreeWidgetItem* parentItem, const QString& name) -> QTreeWidgetItem* {
            if (parentItem) {
                for (int i = 0; i < parentItem->childCount(); ++i) {
                    QTreeWidgetItem* child = parentItem->child(i);
                    if (!child->data(0, kRoleIsLeaf).toBool() && child->text(0) == name) {
                        return child;
                    }
                }
                return nullptr;
            }

            for (int i = 0; i < entryTree->topLevelItemCount(); ++i) {
                QTreeWidgetItem* child = entryTree->topLevelItem(i);
                if (!child->data(0, kRoleIsLeaf).toBool() && child->text(0) == name) {
                    return child;
                }
            }
            return nullptr;
        };

        const auto ensureFolder = [&](QTreeWidgetItem* parentItem, const QString& name) -> QTreeWidgetItem* {
            if (QTreeWidgetItem* existing = findFolderChild(parentItem, name)) {
                return existing;
            }
            QTreeWidgetItem* folder = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(entryTree);
            folder->setText(0, name);
            folder->setText(3, QStringLiteral("Folder"));
            folder->setData(0, kRoleIsLeaf, false);
            return folder;
        };

        for (int idx : sortedCandidateIndexes) {
            const auto& candidate = candidates[static_cast<std::size_t>(idx)];
            const auto& entry = archive.entries[static_cast<std::size_t>(candidate.entryIndex)];

            QString entryPath = entry.name.isEmpty()
                ? QStringLiteral("entry_%1.bin").arg(entry.id, 8, 16, QLatin1Char('0')).toUpper()
                : QDir::fromNativeSeparators(entry.name);
            QStringList parts = entryPath.split('/', Qt::SkipEmptyParts);
            if (parts.isEmpty()) {
                parts << entryPath;
            }

            QTreeWidgetItem* parentItem = nullptr;
            for (int p = 0; p + 1 < parts.size(); ++p) {
                parentItem = ensureFolder(parentItem, parts[p]);
            }

            QTreeWidgetItem* item = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(entryTree);
            item->setText(0, parts.last());
            item->setText(1, QString::number(entry.size));
            item->setText(2, QStringLiteral("0x%1").arg(entry.id, 8, 16, QLatin1Char('0')).toUpper());
            if (candidate.likelyW3d) {
                item->setText(3, candidate.likelyByName ? QStringLiteral("W3D by name") : QStringLiteral("W3D by content"));
            }
            else {
                item->setText(3, QStringLiteral("Unsupported"));
            }
            item->setToolTip(0, candidate.label);
            item->setToolTip(3, candidate.label);
            item->setData(0, kRoleCandidateIndex, idx);
            item->setData(0, kRoleIsLeaf, true);
            item->setData(0, kRoleOpenable, candidate.likelyW3d);
        }

        std::function<bool(QTreeWidgetItem*, const QString&)> applyFilterRecursive =
            [&](QTreeWidgetItem* item, const QString& term) -> bool {
            bool visibleChild = false;
            for (int i = 0; i < item->childCount(); ++i) {
                visibleChild |= applyFilterRecursive(item->child(i), term);
            }

            const bool isLeaf = item->data(0, kRoleIsLeaf).toBool();
            bool selfMatch = term.isEmpty();
            if (!selfMatch) {
                selfMatch = item->text(0).contains(term, Qt::CaseInsensitive)
                    || item->text(2).contains(term, Qt::CaseInsensitive)
                    || item->toolTip(0).contains(term, Qt::CaseInsensitive);
            }

            const bool visible = selfMatch || visibleChild;
            item->setHidden(!visible);
            if (!term.isEmpty() && !isLeaf && visibleChild) {
                item->setExpanded(true);
            }
            return visible;
            };

        const auto refreshOpenButton = [&]() {
            if (!openButton) return;
            QTreeWidgetItem* current = entryTree->currentItem();
            const bool canOpen = current
                && current->data(0, kRoleIsLeaf).toBool()
                && current->data(0, kRoleOpenable).toBool()
                && !current->isHidden();
            openButton->setEnabled(canOpen);
        };

        const auto applyFilter = [&]() {
            const QString term = filterEdit->text().trimmed();
            const bool showAll = showAllCheck->isChecked();
            for (int i = 0; i < entryTree->topLevelItemCount(); ++i) {
                applyFilterRecursive(entryTree->topLevelItem(i), term);
            }

            // Default view: show only openable (W3D/WLT) entries unless requested otherwise.
            std::function<void(QTreeWidgetItem*)> applyTypeVisibility =
                [&](QTreeWidgetItem* item) {
                if (!item) return;

                const bool isLeaf = item->data(0, kRoleIsLeaf).toBool();
                if (isLeaf) {
                    const bool openable = item->data(0, kRoleOpenable).toBool();
                    if (!showAll && !openable) {
                        item->setHidden(true);
                    }
                    return;
                }

                bool anyVisibleChild = false;
                for (int i = 0; i < item->childCount(); ++i) {
                    QTreeWidgetItem* child = item->child(i);
                    applyTypeVisibility(child);
                    if (!child->isHidden()) {
                        anyVisibleChild = true;
                    }
                }
                if (!anyVisibleChild) {
                    item->setHidden(true);
                }
                };

            for (int i = 0; i < entryTree->topLevelItemCount(); ++i) {
                applyTypeVisibility(entryTree->topLevelItem(i));
            }

            QTreeWidgetItem* current = entryTree->currentItem();
            if (current && current->isHidden()) {
                entryTree->clearSelection();
            }
            refreshOpenButton();
        };

        std::function<QTreeWidgetItem*(QTreeWidgetItem*)> firstVisibleOpenableLeaf =
            [&](QTreeWidgetItem* item) -> QTreeWidgetItem* {
            if (!item || item->isHidden()) return nullptr;
            if (item->data(0, kRoleIsLeaf).toBool()) {
                if (item->data(0, kRoleOpenable).toBool()) {
                    return item;
                }
                return nullptr;
            }
            for (int i = 0; i < item->childCount(); ++i) {
                if (QTreeWidgetItem* found = firstVisibleOpenableLeaf(item->child(i))) {
                    return found;
                }
            }
            return nullptr;
            };

        QObject::connect(filterEdit, &QLineEdit::textChanged, &picker, [&](const QString&) {
            applyFilter();
            QTreeWidgetItem* current = entryTree->currentItem();
            if (current) return;
            for (int i = 0; i < entryTree->topLevelItemCount(); ++i) {
                if (QTreeWidgetItem* found = firstVisibleOpenableLeaf(entryTree->topLevelItem(i))) {
                    entryTree->setCurrentItem(found);
                    break;
                }
            }
        });
        QObject::connect(showAllCheck, &QCheckBox::toggled, &picker, [&](bool) {
            applyFilter();
            QTreeWidgetItem* current = entryTree->currentItem();
            if (current) return;
            for (int i = 0; i < entryTree->topLevelItemCount(); ++i) {
                if (QTreeWidgetItem* found = firstVisibleOpenableLeaf(entryTree->topLevelItem(i))) {
                    entryTree->setCurrentItem(found);
                    break;
                }
            }
        });
        QObject::connect(entryTree, &QTreeWidget::itemSelectionChanged, &picker, [&]() {
            refreshOpenButton();
        });
        QObject::connect(entryTree, &QTreeWidget::itemDoubleClicked, &picker,
            [&](QTreeWidgetItem* item, int) {
                if (!item
                    || !item->data(0, kRoleIsLeaf).toBool()
                    || !item->data(0, kRoleOpenable).toBool()
                    || item->isHidden()) return;
                picker.accept();
            });
        QObject::connect(buttons, &QDialogButtonBox::accepted, &picker, [&]() {
            QTreeWidgetItem* current = entryTree->currentItem();
            if (!current
                || !current->data(0, kRoleIsLeaf).toBool()
                || !current->data(0, kRoleOpenable).toBool()
                || current->isHidden()) return;
            picker.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &picker, &QDialog::reject);

        applyFilter();
        for (int i = 0; i < entryTree->topLevelItemCount(); ++i) {
            if (QTreeWidgetItem* found = firstVisibleOpenableLeaf(entryTree->topLevelItem(i))) {
                entryTree->setCurrentItem(found);
                break;
            }
        }

        if (picker.exec() != QDialog::Accepted) {
            if (outError) {
                outError->clear();
            }
            return false;
        }

        QTreeWidgetItem* selectedItem = entryTree->currentItem();
        if (!selectedItem
            || !selectedItem->data(0, kRoleIsLeaf).toBool()
            || !selectedItem->data(0, kRoleOpenable).toBool()) {
            if (outError) {
                *outError = "Selected entry type is not supported yet.";
            }
            return false;
        }

        chosenCandidateIndex = selectedItem->data(0, kRoleCandidateIndex).toInt();
        if (chosenCandidateIndex < 0
            || chosenCandidateIndex >= static_cast<int>(candidates.size())) {
            if (outError) {
                *outError = "Selected archive entry is invalid.";
            }
            return false;
        }
    }

    const auto& chosen = candidates[static_cast<std::size_t>(chosenCandidateIndex)];
    const auto& entry = archive.entries[static_cast<std::size_t>(chosen.entryIndex)];
    const qsizetype absoluteOffset = static_cast<qsizetype>(entry.offset);

    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        if (outError) {
            *outError = QStringLiteral("Failed to create a temporary file: %1")
                .arg(tempFile.errorString());
        }
        return false;
    }

    const qint64 expectedSize = static_cast<qint64>(entry.size);
    const qint64 written = tempFile.write(mixBytes.constData() + absoluteOffset, expectedSize);
    if (written != expectedSize) {
        if (outError) {
            *outError = QStringLiteral("Failed to extract the selected MIX entry.");
        }
        return false;
    }
    tempFile.flush();
    tempFile.close();

    if (!chunkData.loadFromFile(tempFile.fileName().toStdString()) || chunkData.getChunks().empty()) {
        if (outError) {
            *outError = QStringLiteral("Failed to parse selected MIX entry as W3D data.");
        }
        return false;
    }

    if (outRenderContext) {
        outRenderContext->archivePath = QFileInfo(mixPath).absoluteFilePath();
        outRenderContext->selectedEntryId = entry.id;
        outRenderContext->selectedEntryPath = NormalizeArchiveEntryPath(entry.name, entry.id);
        outRenderContext->entries.reserve(archive.entries.size());

        for (std::size_t i = 0; i < archive.entries.size(); ++i) {
            const MixEntryInfo& src = archive.entries[i];
            ArchiveRenderEntryInfo dst{};
            dst.name = QDir::fromNativeSeparators(src.name).trimmed();
            dst.id = src.id;
            dst.offset = src.offset;
            dst.size = src.size;
            dst.likelyW3d = entryLikelyW3d[i];
            outRenderContext->entries.push_back(dst);
        }

        std::unordered_set<std::string> textureNameSet;
        std::unordered_set<uint32_t> textureIdSet;
        std::unordered_map<uint32_t, ArchiveTextureSourceInfo> textureSourceMap;

        const QString selectedArchiveAbsPath = QDir::cleanPath(QFileInfo(mixPath).absoluteFilePath());
        auto collectTextureReferences = [&](const MixArchiveInfo& archiveInfo, const QString& sourceArchivePath) {
            for (const MixEntryInfo& entryInfo : archiveInfo.entries) {
                textureIdSet.insert(entryInfo.id);
                if (!textureSourceMap.contains(entryInfo.id)) {
                    ArchiveTextureSourceInfo source{};
                    source.id = entryInfo.id;
                    source.archivePath = sourceArchivePath;
                    source.offset = entryInfo.offset;
                    source.size = entryInfo.size;
                    source.name = QDir::fromNativeSeparators(entryInfo.name).trimmed();
                    textureSourceMap.emplace(source.id, std::move(source));
                }

                const QString normalizedName = QDir::fromNativeSeparators(entryInfo.name).trimmed();
                if (IsTextureFileName(normalizedName)) {
                    textureNameSet.insert(normalizedName.toStdString());
                }
            }
        };

        collectTextureReferences(archive, selectedArchiveAbsPath);

        const QString selectedArchiveDir = QFileInfo(selectedArchiveAbsPath).absolutePath();
        QDir archiveDir(selectedArchiveDir);
        const QStringList siblingArchives = archiveDir.entryList(
            QStringList{ "*.mix", "*.MIX", "*.dat", "*.DAT", "*.dbs", "*.DBS" },
            QDir::Files | QDir::NoSymLinks);
        for (const QString& siblingName : siblingArchives) {
            const QString siblingAbsPath = QDir::cleanPath(archiveDir.absoluteFilePath(siblingName));
            if (siblingAbsPath.compare(selectedArchiveAbsPath, Qt::CaseInsensitive) == 0) {
                continue;
            }

            QFile siblingFile(siblingAbsPath);
            if (!siblingFile.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QByteArray siblingBytes = siblingFile.readAll();
            siblingFile.close();

            MixArchiveInfo siblingArchive;
            if (!ParseMixArchive(
                siblingBytes,
                IsMixArchivePath(siblingAbsPath),
                siblingArchive,
                nullptr))
            {
                continue;
            }
            collectTextureReferences(siblingArchive, siblingAbsPath);
        }

        outRenderContext->textureEntryNames.assign(textureNameSet.begin(), textureNameSet.end());
        std::sort(
            outRenderContext->textureEntryNames.begin(),
            outRenderContext->textureEntryNames.end());
        outRenderContext->textureEntryIds.assign(textureIdSet.begin(), textureIdSet.end());
        std::sort(
            outRenderContext->textureEntryIds.begin(),
            outRenderContext->textureEntryIds.end());
        outRenderContext->textureEntryIds.erase(
            std::unique(outRenderContext->textureEntryIds.begin(), outRenderContext->textureEntryIds.end()),
            outRenderContext->textureEntryIds.end());
        outRenderContext->textureSources.clear();
        outRenderContext->textureSources.reserve(textureSourceMap.size());
        for (const auto& pair : textureSourceMap) {
            outRenderContext->textureSources.push_back(pair.second);
        }
        std::sort(
            outRenderContext->textureSources.begin(),
            outRenderContext->textureSources.end(),
            [](const ArchiveTextureSourceInfo& lhs, const ArchiveTextureSourceInfo& rhs) {
                return lhs.id < rhs.id;
            });
    }

    return true;
}

struct PivotInfo {
    QString name;
    int parent = -1;
};

struct MeshBinding {
    QString displayName;
    QString typeLabel;
    int pivotIndex = -1;
    QString pivotName;
    std::shared_ptr<ChunkItem> chunk;
    QStringList lookupNames;
};

struct HierarchyInfo {
    QString name;
    std::shared_ptr<ChunkItem> pivotChunk;
    std::vector<PivotInfo> pivots;
    std::vector<MeshBinding> meshes;
};

static std::string ToLower(const std::string& in) {
    std::string out = in;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
        });
    return out;
}

static std::string NormalizeName(const std::string& in) {
    std::string lowered = ToLower(in);
    auto strip = [&](std::string& s) {
        const std::string ext = ".w3d";
        if (s.size() >= ext.size() && s.compare(s.size() - ext.size(), ext.size(), ext) == 0) {
            s.erase(s.size() - ext.size());
        }
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
            s.pop_back();
        }
    };
    strip(lowered);
    return lowered;
}

static void AppendUniqueLookupName(QStringList& names, const QString& candidate) {
    const QString trimmed = candidate.trimmed();
    if (trimmed.isEmpty() || names.contains(trimmed, Qt::CaseInsensitive)) {
        return;
    }
    names.append(trimmed);
}

struct ChunkLocation {
    std::vector<std::shared_ptr<ChunkItem>>* siblings = nullptr;
    std::size_t index = 0;
    ChunkItem* parent = nullptr;
};

static std::shared_ptr<ChunkItem> FindChunkByPtr(
    const std::vector<std::shared_ptr<ChunkItem>>& roots,
    const void* targetPtr) {
    if (!targetPtr) return nullptr;

    std::function<std::shared_ptr<ChunkItem>(const std::shared_ptr<ChunkItem>&)> dfs =
        [&](const std::shared_ptr<ChunkItem>& node) -> std::shared_ptr<ChunkItem> {
        if (!node) return nullptr;
        if (node.get() == targetPtr) return node;
        for (const auto& child : node->children) {
            if (auto found = dfs(child)) return found;
        }
        return nullptr;
        };

    for (const auto& root : roots) {
        if (auto found = dfs(root)) return found;
    }
    return nullptr;
}

constexpr uint32_t kChunkAnimation = 0x0200;
constexpr uint32_t kChunkAnimationHeader = 0x0201;
constexpr uint32_t kChunkAnimationChannel = 0x0202;
constexpr uint32_t kChunkCompressedAnimation = 0x0280;
constexpr uint32_t kChunkCompressedAnimationHeader = 0x0281;

struct RawAnimationChannelHeader {
    uint16_t firstFrame = 0;
    uint16_t lastFrame = 0;
    uint16_t vectorLen = 0;
    uint16_t flags = 0;
    uint16_t pivot = 0;
};

using DensePivotAnimationSamples = OW3D::Render::RenderDensePivotAnimationSamples;

static std::shared_ptr<ChunkItem> CloneChunkTree(const std::shared_ptr<ChunkItem>& source) {
    if (!source) {
        return nullptr;
    }

    auto clone = std::make_shared<ChunkItem>();
    clone->id = source->id;
    clone->length = source->length;
    clone->typeName = source->typeName;
    clone->hasSubChunks = source->hasSubChunks;
    clone->isMicro = source->isMicro;
    clone->data = source->data;
    clone->children.reserve(source->children.size());
    for (const auto& child : source->children) {
        auto childClone = CloneChunkTree(child);
        if (childClone) {
            childClone->parent = clone.get();
            clone->children.push_back(std::move(childClone));
        }
    }
    return clone;
}

static void OverwriteChunkTree(
    const std::shared_ptr<ChunkItem>& target,
    const std::shared_ptr<ChunkItem>& snapshot)
{
    if (!target || !snapshot) {
        return;
    }

    ChunkItem* existingParent = target->parent;
    target->id = snapshot->id;
    target->length = snapshot->length;
    target->typeName = snapshot->typeName;
    target->hasSubChunks = snapshot->hasSubChunks;
    target->isMicro = snapshot->isMicro;
    target->data = snapshot->data;
    target->children.clear();
    target->children.reserve(snapshot->children.size());
    target->parent = existingParent;
    for (const auto& child : snapshot->children) {
        auto childClone = CloneChunkTree(child);
        if (childClone) {
            childClone->parent = target.get();
            target->children.push_back(std::move(childClone));
        }
    }
}

static bool ParseRawAnimationChannelHeader(
    const std::shared_ptr<ChunkItem>& chunk,
    RawAnimationChannelHeader& outHeader,
    QString* outError = nullptr)
{
    outHeader = {};
    if (!chunk || chunk->id != kChunkAnimationChannel || chunk->data.size() < 12u) {
        if (outError) {
            *outError = QObject::tr("Animation channel payload is truncated.");
        }
        return false;
    }

    std::memcpy(&outHeader.firstFrame, chunk->data.data() + 0u, sizeof(uint16_t));
    std::memcpy(&outHeader.lastFrame, chunk->data.data() + 2u, sizeof(uint16_t));
    std::memcpy(&outHeader.vectorLen, chunk->data.data() + 4u, sizeof(uint16_t));
    std::memcpy(&outHeader.flags, chunk->data.data() + 6u, sizeof(uint16_t));
    std::memcpy(&outHeader.pivot, chunk->data.data() + 8u, sizeof(uint16_t));
    return true;
}

static bool IsEditablePoseChannelFlag(uint16_t flags) {
    return flags == 0u || flags == 1u || flags == 2u || flags == 6u;
}

static void ResizeDensePivotAnimationSamples(
    DensePivotAnimationSamples& samples,
    uint32_t numFrames)
{
    samples.translationX.resize(numFrames, 0.0f);
    samples.translationY.resize(numFrames, 0.0f);
    samples.translationZ.resize(numFrames, 0.0f);
    samples.rotation.resize(numFrames, OW3D::Render::Vec4{ 0.0f, 0.0f, 0.0f, 1.0f });
}

static void ResizeAnimationEditDraftFrameCount(
    OW3D::Render::RenderAnimationEditDraft& draft,
    uint32_t numFrames)
{
    draft.numFrames = numFrames;
    for (auto& [pivotIndex, samples] : draft.pivotSamples) {
        Q_UNUSED(pivotIndex);
        ResizeDensePivotAnimationSamples(samples, numFrames);
    }
}

static std::vector<std::vector<OW3D::Render::Mat4>> BuildRenderAnimationWorldTransformsForFrame(
    const OW3D::Render::RenderScene& scene,
    int activeAnimationIndex,
    int frameIndex,
    float frameRate,
    const std::optional<OW3D::Render::RenderAnimationEditDraft>& draft,
    const std::function<std::optional<OW3D::Render::Mat4>(int, int, const OW3D::Render::Mat4&)>& localOverride = {})
{
    const OW3D::Render::AnimationPlaybackState playback =
        BuildRenderAnimationPlaybackForFrame(
            activeAnimationIndex,
            frameIndex,
            frameRate);
    return OW3D::Render::BuildAnimatedHierarchyWorldTransforms(
        scene,
        playback,
        playback.timeSeconds,
        draft,
        localOverride);
}

static OW3D::Render::Mat4 ResolveHandPinParentWorldTransform(
    const OW3D::Render::RenderScene& scene,
    const OW3D::Render::RenderHierarchy& hierarchy,
    int hierarchyIndex,
    int pivotIndex,
    const std::vector<std::vector<OW3D::Render::Mat4>>& worlds)
{
    const int parentIndex = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].parentIndex;
    if (parentIndex >= 0
        && hierarchyIndex >= 0
        && hierarchyIndex < static_cast<int>(worlds.size())
        && parentIndex < static_cast<int>(worlds[static_cast<std::size_t>(hierarchyIndex)].size())) {
        return worlds[static_cast<std::size_t>(hierarchyIndex)][static_cast<std::size_t>(parentIndex)];
    }
    if (hierarchy.attachedHierarchyIndex >= 0
        && hierarchy.attachedHierarchyIndex < static_cast<int>(worlds.size())
        && hierarchy.attachedPivotIndex >= 0
        && hierarchy.attachedPivotIndex < static_cast<int>(
            worlds[static_cast<std::size_t>(hierarchy.attachedHierarchyIndex)].size())) {
        return worlds[static_cast<std::size_t>(hierarchy.attachedHierarchyIndex)]
            [static_cast<std::size_t>(hierarchy.attachedPivotIndex)];
    }
    Q_UNUSED(scene);
    return OW3D::Render::Mat4::Identity();
}

static std::optional<OW3D::Render::Vec3> SampleRenderHandPinTargetWorldPosition(
    const OW3D::Render::RenderScene& scene,
    int activeAnimationIndex,
    int hierarchyIndex,
    int effectorPivotIndex,
    int frameIndex,
    float frameRate,
    const std::optional<OW3D::Render::RenderAnimationEditDraft>& draft)
{
    const auto worlds = BuildRenderAnimationWorldTransformsForFrame(
        scene,
        activeAnimationIndex,
        frameIndex,
        frameRate,
        draft);
    if (hierarchyIndex < 0
        || hierarchyIndex >= static_cast<int>(worlds.size())
        || effectorPivotIndex < 0
        || effectorPivotIndex >= static_cast<int>(worlds[static_cast<std::size_t>(hierarchyIndex)].size())) {
        return std::nullopt;
    }

    return OW3D::Render::TransformPoint(
        worlds[static_cast<std::size_t>(hierarchyIndex)][static_cast<std::size_t>(effectorPivotIndex)],
        { 0.0f, 0.0f, 0.0f });
}

static void SyncRenderHandPinSamplesIntoDraft(
    const std::unordered_map<int, OW3D::Render::RenderDensePivotAnimationSamples>& samplesByPivot,
    const std::vector<int>& pivotIndices,
    std::optional<OW3D::Render::RenderAnimationEditDraft>& draft)
{
    if (!draft.has_value()) {
        return;
    }

    for (const int pivotIndex : pivotIndices) {
        const auto samplesIt = samplesByPivot.find(pivotIndex);
        if (samplesIt == samplesByPivot.end()) {
            continue;
        }
        draft->pivotSamples[pivotIndex] = samplesIt->second;
    }
}

static void ApplyRenderHandPinToFrame(
    const OW3D::Render::RenderScene& scene,
    const OW3D::Render::RenderHierarchy& hierarchy,
    int hierarchyIndex,
    const OW3D::Render::RenderAnimationClip& clip,
    int activeAnimationIndex,
    int frameIndex,
    float frameRate,
    const RenderHandPinTarget& handPinTarget,
    const std::optional<OW3D::Render::RenderAnimationEditDraft>& draft,
    std::unordered_map<int, OW3D::Render::Mat4>& localOverrides,
    std::unordered_map<int, OW3D::Render::RenderDensePivotAnimationSamples>& samplesByPivot)
{
    constexpr int kMaxIterations = 12;
    constexpr float kTargetTolerance = 0.01f;
    constexpr float kMinVectorLength = 1.0e-4f;

    localOverrides.clear();

    const auto buildWorlds = [&]() {
        return BuildRenderAnimationWorldTransformsForFrame(
            scene,
            activeAnimationIndex,
            frameIndex,
            frameRate,
            draft,
            [&](int candidateHierarchyIndex, int candidatePivotIndex, const OW3D::Render::Mat4&) -> std::optional<OW3D::Render::Mat4> {
                if (candidateHierarchyIndex != hierarchyIndex) {
                    return std::nullopt;
                }
                const auto it = localOverrides.find(candidatePivotIndex);
                if (it == localOverrides.end()) {
                    return std::nullopt;
                }
                return it->second;
            });
    };

    auto worlds = buildWorlds();
    if (hierarchyIndex < 0
        || hierarchyIndex >= static_cast<int>(worlds.size())
        || handPinTarget.chain.effectorPivotIndex < 0
        || handPinTarget.chain.effectorPivotIndex >= static_cast<int>(worlds[static_cast<std::size_t>(hierarchyIndex)].size())) {
        return;
    }

    for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
        worlds = buildWorlds();
        const auto& hierarchyWorlds = worlds[static_cast<std::size_t>(hierarchyIndex)];
        const OW3D::Render::Vec3 effectorPosition =
            OW3D::Render::TransformPoint(
                hierarchyWorlds[static_cast<std::size_t>(handPinTarget.chain.effectorPivotIndex)],
                { 0.0f, 0.0f, 0.0f });
        if (OW3D::Render::Length(handPinTarget.worldPosition - effectorPosition) <= kTargetTolerance) {
            break;
        }

        bool adjustedAnyJoint = false;
        for (auto itJoint = handPinTarget.chain.jointPivotIndices.rbegin();
            itJoint != handPinTarget.chain.jointPivotIndices.rend();
            ++itJoint) {
            const int jointPivotIndex = *itJoint;
            worlds = buildWorlds();
            if (hierarchyIndex < 0
                || hierarchyIndex >= static_cast<int>(worlds.size())
                || jointPivotIndex < 0
                || jointPivotIndex >= static_cast<int>(worlds[static_cast<std::size_t>(hierarchyIndex)].size())) {
                continue;
            }

            const auto& currentHierarchyWorlds = worlds[static_cast<std::size_t>(hierarchyIndex)];
            const OW3D::Render::Vec3 jointPosition =
                OW3D::Render::TransformPoint(
                    currentHierarchyWorlds[static_cast<std::size_t>(jointPivotIndex)],
                    { 0.0f, 0.0f, 0.0f });
            const OW3D::Render::Vec3 currentEffectorPosition =
                OW3D::Render::TransformPoint(
                    currentHierarchyWorlds[static_cast<std::size_t>(handPinTarget.chain.effectorPivotIndex)],
                    { 0.0f, 0.0f, 0.0f });

            const OW3D::Render::Vec3 toEffector = currentEffectorPosition - jointPosition;
            const OW3D::Render::Vec3 toTarget = handPinTarget.worldPosition - jointPosition;
            const float toEffectorLength = OW3D::Render::Length(toEffector);
            const float toTargetLength = OW3D::Render::Length(toTarget);
            if (toEffectorLength <= kMinVectorLength || toTargetLength <= kMinVectorLength) {
                continue;
            }

            const OW3D::Render::Vec3 effectorDirection = toEffector * (1.0f / toEffectorLength);
            const OW3D::Render::Vec3 targetDirection = toTarget * (1.0f / toTargetLength);
            const float cosTheta = std::clamp(
                OW3D::Render::Dot(effectorDirection, targetDirection),
                -1.0f,
                1.0f);
            if (cosTheta >= 0.9999f) {
                continue;
            }

            OW3D::Render::Vec3 axis =
                OW3D::Render::Cross(effectorDirection, targetDirection);
            if (OW3D::Render::Length(axis) <= 1.0e-6f) {
                if (cosTheta <= -0.9999f) {
                    axis = FallbackRotationAxis(effectorDirection);
                }
                else {
                    continue;
                }
            }
            const float angleRadians = std::acos(cosTheta);
            const OW3D::Render::Vec4 deltaRotation =
                QuaternionFromAxisAngle(axis, angleRadians);

            const OW3D::Render::Mat4 currentJointWorld =
                currentHierarchyWorlds[static_cast<std::size_t>(jointPivotIndex)];
            const OW3D::Render::Mat4 rotatedJointWorld =
                RotateRigidTransformInWorldSpace(
                    currentJointWorld,
                    deltaRotation);
            const OW3D::Render::Mat4 parentWorld =
                ResolveHandPinParentWorldTransform(
                    scene,
                    hierarchy,
                    hierarchyIndex,
                    jointPivotIndex,
                    currentHierarchyWorlds.empty()
                        ? std::vector<std::vector<OW3D::Render::Mat4>>{}
                        : worlds);
            OW3D::Render::Mat4 newLocal =
                OW3D::Render::Multiply(
                    OW3D::Render::Inverse(parentWorld),
                    rotatedJointWorld);
            newLocal =
                OW3D::Render::AnimationOrthonormalizeRigidTransform(newLocal);

            const OW3D::Render::Mat4 currentLocal =
                localOverrides.contains(jointPivotIndex)
                ? localOverrides.at(jointPivotIndex)
                : OW3D::Render::ComposeAnimatedPivotLocalTransform(
                    hierarchy,
                    jointPivotIndex,
                    &clip,
                    static_cast<float>(frameIndex),
                    draft ? &*draft : nullptr);
            const OW3D::Render::Vec3 currentTranslation =
                ExtractRenderTransformTranslation(currentLocal);
            const OW3D::Render::Vec4 newLocalRotation =
                OW3D::Render::AnimationQuaternionFromMatrix(newLocal);
            localOverrides[jointPivotIndex] =
                OW3D::Render::TransformFromTranslationRotation(
                    currentTranslation,
                    newLocalRotation.x,
                    newLocalRotation.y,
                    newLocalRotation.z,
                    newLocalRotation.w);
            adjustedAnyJoint = true;
        }

        if (!adjustedAnyJoint) {
            break;
        }
    }

    for (const int jointPivotIndex : handPinTarget.chain.jointPivotIndices) {
        const auto localIt = localOverrides.find(jointPivotIndex);
        if (localIt == localOverrides.end()) {
            continue;
        }
        const OW3D::Render::Mat4 animationLocal =
            OW3D::Render::AnimationOrthonormalizeRigidTransform(
                OW3D::Render::Multiply(
                    OW3D::Render::Inverse(
                        hierarchy.pivots[static_cast<std::size_t>(jointPivotIndex)].localTransform),
                    localIt->second));
        const OW3D::Render::Vec4 animationRotation =
            OW3D::Render::AnimationQuaternionFromMatrix(animationLocal);
        auto samplesIt = samplesByPivot.find(jointPivotIndex);
        if (samplesIt == samplesByPivot.end()) {
            continue;
        }
        auto& samples = samplesIt->second;
        if (frameIndex < 0
            || frameIndex >= static_cast<int>(samples.translationX.size())
            || frameIndex >= static_cast<int>(samples.translationY.size())
            || frameIndex >= static_cast<int>(samples.translationZ.size())
            || frameIndex >= static_cast<int>(samples.rotation.size())) {
            continue;
        }
        samples.translationX[static_cast<std::size_t>(frameIndex)] = animationLocal.m[12];
        samples.translationY[static_cast<std::size_t>(frameIndex)] = animationLocal.m[13];
        samples.translationZ[static_cast<std::size_t>(frameIndex)] = animationLocal.m[14];
        samples.rotation[static_cast<std::size_t>(frameIndex)] = animationRotation;
    }
}

static float DeleteDenseFloatFrameValue(
    const std::vector<float>& samples,
    int frameIndex)
{
    if (samples.empty()
        || frameIndex < 0
        || frameIndex >= static_cast<int>(samples.size())) {
        return 0.0f;
    }

    const int previousIndex = frameIndex - 1;
    const int nextIndex = frameIndex + 1;
    if (previousIndex >= 0 && nextIndex < static_cast<int>(samples.size())) {
        return samples[static_cast<std::size_t>(previousIndex)]
            + (samples[static_cast<std::size_t>(nextIndex)]
                - samples[static_cast<std::size_t>(previousIndex)]) * 0.5f;
    }
    if (previousIndex >= 0) {
        return samples[static_cast<std::size_t>(previousIndex)];
    }
    if (nextIndex < static_cast<int>(samples.size())) {
        return samples[static_cast<std::size_t>(nextIndex)];
    }
    return 0.0f;
}

static OW3D::Render::Vec4 DeleteDenseQuatFrameValue(
    const std::vector<OW3D::Render::Vec4>& samples,
    int frameIndex)
{
    if (samples.empty()
        || frameIndex < 0
        || frameIndex >= static_cast<int>(samples.size())) {
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    }

    const int previousIndex = frameIndex - 1;
    const int nextIndex = frameIndex + 1;
    if (previousIndex >= 0 && nextIndex < static_cast<int>(samples.size())) {
        return OW3D::Render::AnimationNormalizeQuat(
            OW3D::Render::AnimationSlerpQuat(
                samples[static_cast<std::size_t>(previousIndex)],
                samples[static_cast<std::size_t>(nextIndex)],
                0.5f));
    }
    if (previousIndex >= 0) {
        return OW3D::Render::AnimationNormalizeQuat(
            samples[static_cast<std::size_t>(previousIndex)]);
    }
    if (nextIndex < static_cast<int>(samples.size())) {
        return OW3D::Render::AnimationNormalizeQuat(
            samples[static_cast<std::size_t>(nextIndex)]);
    }
    return { 0.0f, 0.0f, 0.0f, 1.0f };
}

static std::shared_ptr<ChunkItem> BuildRawAnimationChannelChunk(
    uint16_t firstFrame,
    uint16_t pivotIndex,
    uint16_t flags,
    uint16_t vectorLen,
    const std::vector<float>& samples)
{
    auto chunk = std::make_shared<ChunkItem>();
    chunk->id = kChunkAnimationChannel;
    chunk->typeName = GetChunkName(kChunkAnimationChannel);
    chunk->hasSubChunks = false;
    chunk->isMicro = false;

    const uint32_t frameCount =
        vectorLen == 0u ? 0u : static_cast<uint32_t>(samples.size() / vectorLen);
    const uint16_t lastFrame =
        frameCount > 0u
            ? static_cast<uint16_t>(
                std::min<uint32_t>(
                    static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()),
                    static_cast<uint32_t>(firstFrame) + frameCount - 1u))
            : firstFrame;

    chunk->data.resize(12u + (samples.size() * sizeof(float)));
    std::memcpy(chunk->data.data() + 0u, &firstFrame, sizeof(uint16_t));
    std::memcpy(chunk->data.data() + 2u, &lastFrame, sizeof(uint16_t));
    std::memcpy(chunk->data.data() + 4u, &vectorLen, sizeof(uint16_t));
    std::memcpy(chunk->data.data() + 6u, &flags, sizeof(uint16_t));
    std::memcpy(chunk->data.data() + 8u, &pivotIndex, sizeof(uint16_t));
    const uint16_t pad = 0u;
    std::memcpy(chunk->data.data() + 10u, &pad, sizeof(uint16_t));
    if (!samples.empty()) {
        std::memcpy(
            chunk->data.data() + 12u,
            samples.data(),
            samples.size() * sizeof(float));
    }
    chunk->length = static_cast<uint32_t>(chunk->data.size());
    return chunk;
}

static std::shared_ptr<ChunkItem> BuildRawAnimationChannelChunk(
    uint16_t pivotIndex,
    uint16_t flags,
    uint16_t vectorLen,
    const std::vector<float>& samples)
{
    return BuildRawAnimationChannelChunk(0u, pivotIndex, flags, vectorLen, samples);
}

static bool ReadRawAnimationChannelSamples(
    const std::shared_ptr<ChunkItem>& chunk,
    const RawAnimationChannelHeader& header,
    std::vector<float>& outSamples,
    QString* outError = nullptr)
{
    outSamples.clear();
    if (!chunk || chunk->id != kChunkAnimationChannel) {
        if (outError) {
            *outError = QObject::tr("Animation channel payload is missing.");
        }
        return false;
    }
    if (header.vectorLen == 0u) {
        if (outError) {
            *outError = QObject::tr("Animation channel vector length is invalid.");
        }
        return false;
    }

    const uint32_t frameCount =
        (header.lastFrame >= header.firstFrame)
        ? (static_cast<uint32_t>(header.lastFrame) - static_cast<uint32_t>(header.firstFrame) + 1u)
        : 0u;
    const std::size_t sampleCount =
        static_cast<std::size_t>(frameCount) * static_cast<std::size_t>(header.vectorLen);
    const std::size_t payloadBytes = 12u + (sampleCount * sizeof(float));
    if (chunk->data.size() < payloadBytes) {
        if (outError) {
            *outError = QObject::tr("Animation channel sample payload is truncated.");
        }
        return false;
    }

    outSamples.resize(sampleCount);
    if (!outSamples.empty()) {
        std::memcpy(
            outSamples.data(),
            chunk->data.data() + 12u,
            outSamples.size() * sizeof(float));
    }
    return true;
}

static bool UpdateRawAnimationHeaderTiming(
    const std::shared_ptr<ChunkItem>& animationChunk,
    uint32_t numFrames,
    float frameRate,
    QString* outError = nullptr)
{
    if (!animationChunk || animationChunk->id != kChunkAnimation) {
        if (outError) {
            *outError = QObject::tr("Writable raw animation chunk was not found.");
        }
        return false;
    }

    const auto headerChunk = FindFirstChildById(animationChunk, kChunkAnimationHeader);
    if (!headerChunk || headerChunk->data.size() < sizeof(W3dAnimHeaderStruct)) {
        if (outError) {
            *outError = QObject::tr("Animation header chunk is missing or truncated.");
        }
        return false;
    }

    const uint32_t storedFrameRate = frameRate > 0.0f
        ? std::max<uint32_t>(1u, static_cast<uint32_t>(std::llround(frameRate)))
        : 0u;
    std::memcpy(
        headerChunk->data.data() + offsetof(W3dAnimHeaderStruct, NumFrames),
        &numFrames,
        sizeof(uint32_t));
    std::memcpy(
        headerChunk->data.data() + offsetof(W3dAnimHeaderStruct, FrameRate),
        &storedFrameRate,
        sizeof(uint32_t));
    headerChunk->length = static_cast<uint32_t>(headerChunk->data.size());
    return true;
}

static bool TruncateRawAnimationToFrameCount(
    const std::shared_ptr<ChunkItem>& animationChunk,
    uint32_t numFrames,
    float frameRate,
    QString* outError = nullptr)
{
    if (!UpdateRawAnimationHeaderTiming(animationChunk, numFrames, frameRate, outError)) {
        return false;
    }

    std::vector<std::shared_ptr<ChunkItem>> newChildren;
    newChildren.reserve(animationChunk->children.size());
    for (const auto& child : animationChunk->children) {
        if (!child) {
            continue;
        }
        if (child->id != kChunkAnimationChannel) {
            child->parent = animationChunk.get();
            newChildren.push_back(child);
            continue;
        }

        RawAnimationChannelHeader header{};
        QString parseError;
        if (!ParseRawAnimationChannelHeader(child, header, &parseError)) {
            if (outError) {
                *outError = parseError.isEmpty()
                    ? QObject::tr("Failed to parse an animation channel while truncating the clip.")
                    : parseError;
            }
            return false;
        }

        if (static_cast<uint32_t>(header.firstFrame) >= numFrames) {
            continue;
        }
        if (static_cast<uint32_t>(header.lastFrame) < numFrames) {
            child->parent = animationChunk.get();
            newChildren.push_back(child);
            continue;
        }

        std::vector<float> samples;
        QString sampleError;
        if (!ReadRawAnimationChannelSamples(child, header, samples, &sampleError)) {
            if (outError) {
                *outError = sampleError.isEmpty()
                    ? QObject::tr("Failed to read animation channel samples while truncating the clip.")
                    : sampleError;
            }
            return false;
        }

        const uint32_t keptFrameCount = numFrames - static_cast<uint32_t>(header.firstFrame);
        samples.resize(
            static_cast<std::size_t>(keptFrameCount)
            * static_cast<std::size_t>(header.vectorLen));
        auto replacement = BuildRawAnimationChannelChunk(
            header.firstFrame,
            header.pivot,
            header.flags,
            header.vectorLen,
            samples);
        replacement->parent = animationChunk.get();
        newChildren.push_back(std::move(replacement));
    }

    animationChunk->children = std::move(newChildren);
    animationChunk->hasSubChunks = true;
    animationChunk->data.clear();
    animationChunk->length = 0u;
    return true;
}

static bool RewriteRawAnimationPivotPoseChannels(
    const std::shared_ptr<ChunkItem>& animationChunk,
    int pivotIndex,
    const DensePivotAnimationSamples& samples,
    QString* outError = nullptr)
{
    if (!animationChunk || animationChunk->id != kChunkAnimation) {
        if (outError) {
            *outError = QObject::tr("Writable raw animation chunk was not found.");
        }
        return false;
    }
    if (samples.translationX.empty()
        || samples.translationY.size() != samples.translationX.size()
        || samples.translationZ.size() != samples.translationX.size()
        || samples.rotation.size() != samples.translationX.size()) {
        if (outError) {
            *outError = QObject::tr("Animation sample buffers are inconsistent.");
        }
        return false;
    }

    const auto headerChunk = FindFirstChildById(animationChunk, kChunkAnimationHeader);
    if (!headerChunk) {
        if (outError) {
            *outError = QObject::tr("Animation header chunk is missing.");
        }
        return false;
    }

    std::vector<float> rotationSamples;
    rotationSamples.reserve(samples.rotation.size() * 4u);
    for (const auto& q : samples.rotation) {
        const OW3D::Render::Vec4 normalized = OW3D::Render::AnimationNormalizeQuat(q);
        rotationSamples.push_back(normalized.x);
        rotationSamples.push_back(normalized.y);
        rotationSamples.push_back(normalized.z);
        rotationSamples.push_back(normalized.w);
    }

    std::vector<std::shared_ptr<ChunkItem>> replacementChannels;
    replacementChannels.reserve(4u);
    replacementChannels.push_back(BuildRawAnimationChannelChunk(
        static_cast<uint16_t>(pivotIndex),
        0u,
        1u,
        samples.translationX));
    replacementChannels.push_back(BuildRawAnimationChannelChunk(
        static_cast<uint16_t>(pivotIndex),
        1u,
        1u,
        samples.translationY));
    replacementChannels.push_back(BuildRawAnimationChannelChunk(
        static_cast<uint16_t>(pivotIndex),
        2u,
        1u,
        samples.translationZ));
    replacementChannels.push_back(BuildRawAnimationChannelChunk(
        static_cast<uint16_t>(pivotIndex),
        6u,
        4u,
        rotationSamples));

    std::vector<std::shared_ptr<ChunkItem>> newChildren;
    newChildren.reserve(animationChunk->children.size() + replacementChannels.size());
    bool insertedReplacementChannels = false;
    for (const auto& child : animationChunk->children) {
        if (!child) {
            continue;
        }

        if (child == headerChunk) {
            newChildren.push_back(child);
            if (!insertedReplacementChannels) {
                for (const auto& replacement : replacementChannels) {
                    replacement->parent = animationChunk.get();
                    newChildren.push_back(replacement);
                }
                insertedReplacementChannels = true;
            }
            continue;
        }

        if (child->id == kChunkAnimationChannel) {
            RawAnimationChannelHeader header{};
            if (ParseRawAnimationChannelHeader(child, header, nullptr)
                && static_cast<int>(header.pivot) == pivotIndex
                && IsEditablePoseChannelFlag(header.flags)) {
                continue;
            }
        }

        child->parent = animationChunk.get();
        newChildren.push_back(child);
    }

    if (!insertedReplacementChannels) {
        for (auto it = replacementChannels.rbegin(); it != replacementChannels.rend(); ++it) {
            (*it)->parent = animationChunk.get();
            newChildren.insert(newChildren.begin(), *it);
        }
    }

    animationChunk->children = std::move(newChildren);
    animationChunk->hasSubChunks = true;
    animationChunk->data.clear();
    animationChunk->length = 0u;
    return true;
}

static bool ResolveEditablePrimaryRenderAnimation(
    const ChunkData* chunkData,
    const OW3D::Render::SceneBuildResult& sceneResult,
    const OW3D::Render::AnimationPlaybackState& playback,
    int* outAnimationIndex,
    const OW3D::Render::RenderAnimationClip** outClip,
    std::shared_ptr<ChunkItem>* outAnimationChunk,
    QString* outReason = nullptr)
{
    if (outAnimationIndex) {
        *outAnimationIndex = -1;
    }
    if (outClip) {
        *outClip = nullptr;
    }
    if (outAnimationChunk) {
        outAnimationChunk->reset();
    }
    if (outReason) {
        outReason->clear();
    }

    if (!chunkData) {
        if (outReason) {
            *outReason = QObject::tr("No primary file is loaded.");
        }
        return false;
    }

    const int activeIndex = playback.activeAnimationIndex;
    const auto& animations = sceneResult.scene.animations;
    if (activeIndex < 0 || activeIndex >= static_cast<int>(animations.size())) {
        if (outReason) {
            *outReason = QObject::tr("No animation clip is selected.");
        }
        return false;
    }

    const auto& clip = animations[static_cast<std::size_t>(activeIndex)];
    if (!clip.supportedForPlayback) {
        if (outReason) {
            *outReason = QObject::tr("This animation clip is not supported for playback.");
        }
        return false;
    }
    if (clip.compressed) {
        if (outReason) {
            *outReason = QObject::tr("Compressed animation clips are read-only in v1.");
        }
        return false;
    }
    if (clip.sourceFromAnimationLibrary) {
        if (outReason) {
            *outReason = QObject::tr("External animation-library clips are read-only in v1.");
        }
        return false;
    }
    if (!clip.sourceAnimationChunk) {
        if (outReason) {
            *outReason = QObject::tr("This clip is not backed by a writable raw animation chunk.");
        }
        return false;
    }
    if (clip.numFrames == 0u
        || clip.numFrames > (static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()) + 1u)) {
        if (outReason) {
            *outReason = QObject::tr("This clip has invalid frame metadata for raw key editing.");
        }
        return false;
    }

    const auto animationChunk = FindChunkByPtr(chunkData->getChunks(), clip.sourceAnimationChunk);
    if (!animationChunk || animationChunk->id != kChunkAnimation) {
        if (outReason) {
            *outReason = QObject::tr("Only raw clips from the current primary file are editable.");
        }
        return false;
    }

    for (const auto& child : animationChunk->children) {
        if (!child || child->id != kChunkAnimationChannel) {
            continue;
        }

        RawAnimationChannelHeader header{};
        QString parseError;
        if (!ParseRawAnimationChannelHeader(child, header, &parseError)) {
            if (outReason) {
                *outReason = parseError;
            }
            return false;
        }
        if (header.flags == 3u || header.flags == 4u || header.flags == 5u) {
            if (outReason) {
                *outReason = QObject::tr("Euler rotation channels are read-only in v1.");
            }
            return false;
        }
    }

    if (outAnimationIndex) {
        *outAnimationIndex = activeIndex;
    }
    if (outClip) {
        *outClip = &clip;
    }
    if (outAnimationChunk) {
        *outAnimationChunk = animationChunk;
    }
    return true;
}

static bool FindChunkLocationRecursive(
    std::vector<std::shared_ptr<ChunkItem>>& siblings,
    ChunkItem* parent,
    const void* targetPtr,
    ChunkLocation& out) {
    for (std::size_t i = 0; i < siblings.size(); ++i) {
        auto& node = siblings[i];
        if (!node) continue;
        if (node.get() == targetPtr) {
            out.siblings = &siblings;
            out.index = i;
            out.parent = parent;
            return true;
        }
        if (FindChunkLocationRecursive(node->children, node.get(), targetPtr, out)) {
            return true;
        }
    }
    return false;
}

static bool FindChunkLocation(
    std::vector<std::shared_ptr<ChunkItem>>& roots,
    const void* targetPtr,
    ChunkLocation& out) {
    return FindChunkLocationRecursive(roots, nullptr, targetPtr, out);
}

static bool SyncHLodArrayHeaderModelCounts(const std::shared_ptr<ChunkItem>& arrayChunk) {
    if (!arrayChunk) return false;
    if (arrayChunk->id != 0x0702 && arrayChunk->id != 0x0706 && arrayChunk->id != 0x0707) {
        return false;
    }

    uint32_t modelCount = 0;
    for (const auto& child : arrayChunk->children) {
        if (child && child->id == 0x0704) {
            ++modelCount;
        }
    }

    bool changed = false;
    for (const auto& child : arrayChunk->children) {
        if (!child || child->id != 0x0703) {
            continue;
        }
        auto parsed = ParseChunkStruct<W3dHLodArrayHeaderStruct>(child);
        auto* header = std::get_if<W3dHLodArrayHeaderStruct>(&parsed);
        if (!header || header->ModelCount == modelCount) {
            continue;
        }

        if (W3DEdit::MutateStructChunk<W3dHLodArrayHeaderStruct>(
            child,
            [&](W3dHLodArrayHeaderStruct& h) {
                h.ModelCount = modelCount;
            }))
        {
            changed = true;
        }
    }

    return changed;
}

static bool SyncHLodWrapperCounts(const std::shared_ptr<ChunkItem>& hlodChunk) {
    if (!hlodChunk || hlodChunk->id != 0x0700) {
        return false;
    }

    uint32_t lodCount = 0;
    for (const auto& child : hlodChunk->children) {
        if (child && child->id == 0x0702) {
            ++lodCount;
        }
    }

    bool changed = false;

    for (const auto& child : hlodChunk->children) {
        if (!child) {
            continue;
        }

        if (child->id == 0x0701) {
            auto parsed = ParseChunkStruct<W3dHLodHeaderStruct>(child);
            auto* header = std::get_if<W3dHLodHeaderStruct>(&parsed);
            if (header && header->LodCount != lodCount) {
                if (W3DEdit::MutateStructChunk<W3dHLodHeaderStruct>(
                    child,
                    [&](W3dHLodHeaderStruct& h) {
                        h.LodCount = lodCount;
                    }))
                {
                    changed = true;
                }
            }
        }

        if (SyncHLodArrayHeaderModelCounts(child)) {
            changed = true;
        }
    }

    return changed;
}

static void SyncHLodCountsForSave(ChunkData* chunkData) {
    if (!chunkData) return;

    auto& roots = chunkData->getChunksMutable();
    std::function<void(const std::shared_ptr<ChunkItem>&)> dfs =
        [&](const std::shared_ptr<ChunkItem>& node) {
        if (!node) return;
        if (node->id == 0x0700) {
            (void)SyncHLodWrapperCounts(node);
        }
        else if (node->id == 0x0702 || node->id == 0x0706 || node->id == 0x0707) {
            (void)SyncHLodArrayHeaderModelCounts(node);
        }

        for (const auto& child : node->children) {
            dfs(child);
        }
        };

    for (const auto& root : roots) {
        dfs(root);
    }
}

static void CollectDeletedMeshNames(
    const std::shared_ptr<ChunkItem>& node,
    std::unordered_set<std::string>& names)
{
    if (!node) {
        return;
    }

    if (node->id == 0x001F) { // W3D_CHUNK_MESH_HEADER3
        auto parsed = ParseChunkStruct<W3dMeshHeader3Struct>(node);
        if (auto* header = std::get_if<W3dMeshHeader3Struct>(&parsed)) {
            const QString meshName = ReadFixedString(header->MeshName, W3D_NAME_LEN);
            const QString containerName = ReadFixedString(header->ContainerName, W3D_NAME_LEN);

            const std::string meshNorm = NormalizeName(meshName.toStdString());
            if (!meshNorm.empty()) {
                names.insert(meshNorm);
            }

            if (!containerName.isEmpty() && !meshName.isEmpty()) {
                const QString combined = containerName + QLatin1Char('.') + meshName;
                const std::string combinedNorm = NormalizeName(combined.toStdString());
                if (!combinedNorm.empty()) {
                    names.insert(combinedNorm);
                }
            }
        }
    }

    for (const auto& child : node->children) {
        CollectDeletedMeshNames(child, names);
    }
}

static int RemoveMatchingHLodSubObjects(
    const std::vector<std::shared_ptr<ChunkItem>>& roots,
    const std::unordered_set<std::string>& deletedMeshNames)
{
    if (deletedMeshNames.empty()) {
        return 0;
    }

    int removed = 0;
    std::function<void(const std::shared_ptr<ChunkItem>&)> dfs =
        [&](const std::shared_ptr<ChunkItem>& node) {
        if (!node) {
            return;
        }

        if (node->id == 0x0702 || node->id == 0x0706) {
            auto& children = node->children;
            auto it = std::remove_if(children.begin(), children.end(),
                [&](const std::shared_ptr<ChunkItem>& child) {
                    if (!child || child->id != 0x0704) {
                        return false;
                    }

                    auto parsed = ParseChunkStruct<W3dHLodSubObjectStruct>(child);
                    auto* sub = std::get_if<W3dHLodSubObjectStruct>(&parsed);
                    if (!sub) {
                        return false;
                    }

                    const QString name = ReadFixedString(sub->Name, 2 * W3D_NAME_LEN);
                    const std::string normalized = NormalizeName(name.toStdString());
                    if (normalized.empty() || deletedMeshNames.count(normalized) == 0) {
                        return false;
                    }

                    ++removed;
                    return true;
                });
            children.erase(it, children.end());
        }

        for (const auto& child : node->children) {
            dfs(child);
        }
    };

    for (const auto& root : roots) {
        dfs(root);
    }

    return removed;
}

static QString FindHierarchyNameForPivotChunk(const std::shared_ptr<ChunkItem>& pivotChunk) {
    if (!pivotChunk || !pivotChunk->parent || pivotChunk->parent->id != 0x0100) {
        return {};
    }

    ChunkItem* hierarchy = pivotChunk->parent;
    for (const auto& child : hierarchy->children) {
        if (!child || child->id != 0x0101) {
            continue;
        }

        auto parsed = ParseChunkStruct<W3dHierarchyStruct>(child);
        if (auto* header = std::get_if<W3dHierarchyStruct>(&parsed)) {
            return ReadFixedString(header->Name, W3D_NAME_LEN);
        }
    }

    return {};
}

static int RenameHLodProxyNamesForHierarchy(
    const std::vector<std::shared_ptr<ChunkItem>>& roots,
    const QString& hierarchyName,
    int pivotIndex,
    const QString& oldPivotName,
    const QString& newPivotName)
{
    if (newPivotName.isEmpty()) {
        return 0;
    }

    const std::string targetHierarchyNorm = NormalizeName(hierarchyName.toStdString());
    if (targetHierarchyNorm.empty()) {
        return 0;
    }
    const auto sameName = [&](const QString& lhs, const QString& rhs) -> bool {
        return NormalizeName(lhs.toStdString()) == NormalizeName(rhs.toStdString());
        };

    int renameCount = 0;

    std::function<void(const std::shared_ptr<ChunkItem>&)> dfs =
        [&](const std::shared_ptr<ChunkItem>& node) {
        if (!node) return;

        if (node->id == 0x0700) {
            bool hierarchyMatches = targetHierarchyNorm.empty();

            for (const auto& child : node->children) {
                if (!child || child->id != 0x0701) {
                    continue;
                }
                auto parsed = ParseChunkStruct<W3dHLodHeaderStruct>(child);
                if (auto* header = std::get_if<W3dHLodHeaderStruct>(&parsed)) {
                    const QString headerName = ReadFixedString(header->Name, W3D_NAME_LEN);
                    const QString hierarchyRef = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
                    const std::string headerNorm = NormalizeName(headerName.toStdString());
                    const std::string hierarchyNorm = NormalizeName(hierarchyRef.toStdString());
                    hierarchyMatches =
                        targetHierarchyNorm.empty()
                        || (!headerNorm.empty() && headerNorm == targetHierarchyNorm)
                        || (!hierarchyNorm.empty() && hierarchyNorm == targetHierarchyNorm);
                }
                break;
            }

            if (hierarchyMatches) {
                std::function<void(const std::shared_ptr<ChunkItem>&, bool)> scanProxyEntries =
                    [&](const std::shared_ptr<ChunkItem>& cur, bool inProxyArray) {
                    if (!cur) return;
                    const bool isProxyContext = inProxyArray || (cur->id == 0x0706);

                        if (isProxyContext && cur->id == 0x0704) {
                        auto parsed = ParseChunkStruct<W3dHLodSubObjectStruct>(cur);
                        if (auto* sub = std::get_if<W3dHLodSubObjectStruct>(&parsed)) {
                            const QString currentName = ReadFixedString(sub->Name, 2 * W3D_NAME_LEN);
                            const bool indexMatches = pivotIndex >= 0
                                && static_cast<int>(sub->BoneIndex) == pivotIndex;
                            const bool fallbackNameMatches = pivotIndex < 0
                                && !oldPivotName.isEmpty()
                                && sameName(currentName, oldPivotName);

                            if (indexMatches || fallbackNameMatches) {
                                if (W3DEdit::MutateStructChunk<W3dHLodSubObjectStruct>(
                                    cur,
                                    [&](W3dHLodSubObjectStruct& target) {
                                        W3DEdit::WriteFixedString(
                                            target.Name,
                                            2 * W3D_NAME_LEN,
                                            newPivotName.toStdString());
                                    }))
                                {
                                    ++renameCount;
                                }
                            }
                        }
                    }

                    for (const auto& child : cur->children) {
                        scanProxyEntries(child, isProxyContext);
                    }
                    };

                for (const auto& child : node->children) {
                    scanProxyEntries(child, false);
                }
            }
        }

        for (const auto& child : node->children) {
            dfs(child);
        }
        };

    for (const auto& root : roots) {
        dfs(root);
    }

    return renameCount;
}

static int RenamePivotBoundMeshNamesForHierarchy(
    const std::vector<std::shared_ptr<ChunkItem>>& roots,
    const QString& hierarchyName,
    const QString& oldMeshName,
    const QString& newMeshName)
{
    if (hierarchyName.isEmpty() || oldMeshName.isEmpty() || newMeshName.isEmpty()
        || NormalizeName(oldMeshName.toStdString()) == NormalizeName(newMeshName.toStdString()))
    {
        return 0;
    }

    const std::string targetHierarchyNorm = NormalizeName(hierarchyName.toStdString());
    if (targetHierarchyNorm.empty()) {
        return 0;
    }

    const auto sameName = [](const QString& lhs, const QString& rhs) -> bool {
        return NormalizeName(lhs.toStdString()) == NormalizeName(rhs.toStdString());
    };

    const QString oldFullName = hierarchyName + QLatin1Char('.') + oldMeshName;
    const QString newFullName = hierarchyName + QLatin1Char('.') + newMeshName;
    int renameCount = 0;

    std::function<void(const std::shared_ptr<ChunkItem>&)> dfs =
        [&](const std::shared_ptr<ChunkItem>& node) {
        if (!node) return;

        if (node->id == 0x001F) { // W3D_CHUNK_MESH_HEADER3
            auto parsed = ParseChunkStruct<W3dMeshHeader3Struct>(node);
            if (auto* header = std::get_if<W3dMeshHeader3Struct>(&parsed)) {
                const QString currentContainer =
                    ReadFixedString(header->ContainerName, W3D_NAME_LEN);
                const QString currentMeshName =
                    ReadFixedString(header->MeshName, W3D_NAME_LEN);
                if (sameName(currentContainer, hierarchyName)
                    && sameName(currentMeshName, oldMeshName))
                {
                    if (W3DEdit::MutateStructChunk<W3dMeshHeader3Struct>(
                        node,
                        [&](W3dMeshHeader3Struct& target) {
                            W3DEdit::WriteFixedString(
                                target.MeshName,
                                W3D_NAME_LEN,
                                newMeshName.toStdString());
                        }))
                    {
                        ++renameCount;
                    }
                }
            }
        }

        if (node->id == 0x0700) { // W3D_CHUNK_HLOD
            bool hierarchyMatches = false;
            for (const auto& child : node->children) {
                if (!child || child->id != 0x0701) {
                    continue;
                }
                auto parsed = ParseChunkStruct<W3dHLodHeaderStruct>(child);
                if (auto* header = std::get_if<W3dHLodHeaderStruct>(&parsed)) {
                    const QString headerName = ReadFixedString(header->Name, W3D_NAME_LEN);
                    const QString hierarchyRef = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
                    hierarchyMatches =
                        sameName(headerName, hierarchyName)
                        || sameName(hierarchyRef, hierarchyName);
                }
                break;
            }

            if (hierarchyMatches) {
                std::function<void(const std::shared_ptr<ChunkItem>&)> scanSubObjects =
                    [&](const std::shared_ptr<ChunkItem>& cur) {
                    if (!cur) return;
                    if (cur->id == 0x0704) {
                        auto parsed = ParseChunkStruct<W3dHLodSubObjectStruct>(cur);
                        if (auto* sub = std::get_if<W3dHLodSubObjectStruct>(&parsed)) {
                            const QString currentName = ReadFixedString(
                                sub->Name,
                                2 * W3D_NAME_LEN);
                            QString updatedName = currentName;
                            if (sameName(currentName, oldMeshName)) {
                                updatedName = newMeshName;
                            }
                            else if (sameName(currentName, oldFullName)) {
                                updatedName = newFullName;
                            }

                            if (updatedName != currentName) {
                                if (W3DEdit::MutateStructChunk<W3dHLodSubObjectStruct>(
                                    cur,
                                    [&](W3dHLodSubObjectStruct& target) {
                                        W3DEdit::WriteFixedString(
                                            target.Name,
                                            2 * W3D_NAME_LEN,
                                            updatedName.toStdString());
                                    }))
                                {
                                    ++renameCount;
                                }
                            }
                        }
                    }

                    for (const auto& child : cur->children) {
                        scanSubObjects(child);
                    }
                };

                for (const auto& child : node->children) {
                    scanSubObjects(child);
                }
            }
        }

        for (const auto& child : node->children) {
            dfs(child);
        }
    };

    for (const auto& root : roots) {
        dfs(root);
    }

    return renameCount;
}

static bool ParseChunkIdText(const QString& text, uint32_t& outId) {
    QString normalized = text.trimmed();
    if (normalized.isEmpty()) return false;

    int base = 10;
    if (normalized.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        normalized = normalized.mid(2);
        base = 16;
    }

    bool ok = false;
    const uint32_t parsed = normalized.toUInt(&ok, base);
    if (!ok) return false;

    outId = parsed;
    return true;
}

static void* SelectedChunkPtr(const QTreeWidget* tree) {
    if (!tree) return nullptr;
    const auto items = tree->selectedItems();
    if (items.isEmpty()) return nullptr;
    return items.first()->data(0, Qt::UserRole).value<void*>();
}

static bool PromptChunkId(QWidget* parent, const QString& title, uint32_t suggestedId, uint32_t& outId) {
    bool accepted = false;
    const QString defaultText = QStringLiteral("0x%1").arg(suggestedId, 0, 16).toUpper();
    const QString chunkIdText = QInputDialog::getText(
        parent,
        title,
        QObject::tr("Chunk ID (hex or decimal)"),
        QLineEdit::Normal,
        defaultText,
        &accepted).trimmed();

    if (!accepted) return false;
    if (!ParseChunkIdText(chunkIdText, outId)) {
        QMessageBox::warning(
            parent,
            QObject::tr("Invalid Chunk ID"),
            QObject::tr("Enter a valid chunk ID using decimal or 0x-prefixed hex."));
        return false;
    }

    return true;
}

static bool MoveBoneToEndInPivots(
    std::vector<W3dPivotStruct>& pivots,
    std::vector<W3dPivotFixupStruct>* pivotFixups,
    int boneIndex,
    QString* error) {
    if (boneIndex < 0 || boneIndex >= static_cast<int>(pivots.size())) {
        if (error) *error = QObject::tr("Pivot index is out of range.");
        return false;
    }

    const auto moveSingle = [&](uint32_t sourceIdx) {
        const uint32_t count = static_cast<uint32_t>(pivots.size());
        if (sourceIdx >= count) return;

        W3dPivotStruct movedPivot = pivots[sourceIdx];
        std::optional<W3dPivotFixupStruct> movedFixup;
        if (pivotFixups) {
            movedFixup = (*pivotFixups)[sourceIdx];
        }

        for (uint32_t i = sourceIdx; i + 1 < count; ++i) {
            pivots[i] = pivots[i + 1];
            if (pivotFixups) {
                (*pivotFixups)[i] = (*pivotFixups)[i + 1];
            }
        }

        pivots[count - 1] = movedPivot;
        if (pivotFixups) {
            (*pivotFixups)[count - 1] = *movedFixup;
        }

        for (uint32_t i = sourceIdx; i + 1 < count; ++i) {
            uint32_t& parentIdx = pivots[i].ParentIdx;
            if (parentIdx == 0xFFFFFFFFu) continue;
            if (parentIdx > sourceIdx) {
                --parentIdx;
            }
            else if (parentIdx == sourceIdx) {
                parentIdx = count - 1;
            }
        }
        };

    const auto findFirstBad = [&]() -> int {
        for (uint32_t i = 1; i < static_cast<uint32_t>(pivots.size()); ++i) {
            const uint32_t parentIdx = pivots[i].ParentIdx;
            if (parentIdx != 0xFFFFFFFFu && parentIdx > i) {
                return static_cast<int>(i);
            }
        }
        return -1;
        };

    moveSingle(static_cast<uint32_t>(boneIndex));
    for (int bad = findFirstBad(); bad != -1; bad = findFirstBad()) {
        moveSingle(static_cast<uint32_t>(bad));
    }

    return true;
}
} // namespace

class HierarchyBrowserDialog : public QDialog {
public:
    using PivotRenameHandler =
        std::function<bool(const std::shared_ptr<ChunkItem>&, int, const QString&, QString*)>;

    HierarchyBrowserDialog(const std::vector<HierarchyInfo>& data,
        std::function<void(void*)> meshHandler,
        std::function<void*(const QStringList&)> resolver,
        PivotRenameHandler renameHandler,
        QWidget* parent = nullptr)
        : QDialog(parent)
        , hierarchies(data)
        , onMeshActivated(std::move(meshHandler))
        , resolveChunkCandidates(std::move(resolver))
        , onPivotRenamed(std::move(renameHandler)) {
        setWindowTitle(tr("Hierarchy Browser"));

        auto* layout = new QVBoxLayout(this);
        auto* hint = new QLabel(
            tr("Select a mesh and click \"Select Mesh\" to jump to its chunk. "
                "Double-click a pivot name to rename it."),
            this);
        layout->addWidget(hint);

        tree = new QTreeWidget(this);
        tree->setHeaderLabels({ tr("Item"), tr("Details") });
        tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        layout->addWidget(tree, 1);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
        selectButton = buttons->addButton(tr("Select Mesh"), QDialogButtonBox::ActionRole);
        selectButton->setEnabled(false);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);

        populate();
        connect(tree, &QTreeWidget::itemSelectionChanged,
            this, &HierarchyBrowserDialog::onSelectionChanged);
        connect(selectButton, &QPushButton::clicked,
            this, &HierarchyBrowserDialog::activateSelection);
        connect(tree, &QTreeWidget::itemDoubleClicked,
            this, &HierarchyBrowserDialog::onItemDoubleClicked);
    }

private:
    static constexpr int RoleIsMesh = Qt::UserRole + 1;
    static constexpr int RoleName = Qt::UserRole + 2;
    static constexpr int RoleIsPivot = Qt::UserRole + 3;
    static constexpr int RoleHierarchyIndex = Qt::UserRole + 4;
    static constexpr int RolePivotIndex = Qt::UserRole + 5;
    static constexpr int RoleLookupNames = Qt::UserRole + 6;

    void populate() {
        QSignalBlocker blocker(tree);
        tree->clear();
        for (int hierarchyIndex = 0; hierarchyIndex < static_cast<int>(hierarchies.size()); ++hierarchyIndex) {
            const auto& h = hierarchies[static_cast<std::size_t>(hierarchyIndex)];
            auto* root = new QTreeWidgetItem(tree, { h.name, tr("Hierarchy") });
            root->setData(0, RoleIsMesh, false);
            root->setData(0, RoleIsPivot, false);
            root->setData(0, RoleHierarchyIndex, hierarchyIndex);
            root->setData(0, RolePivotIndex, -1);

            // Build pivot items recursively so parent ordering does not matter
            std::vector<QTreeWidgetItem*> pivotItems(h.pivots.size(), nullptr);
            std::function<QTreeWidgetItem * (int)> ensurePivot = [&](int idx) -> QTreeWidgetItem* {
                if (idx < 0 || idx >= static_cast<int>(h.pivots.size())) return root;
                if (pivotItems[idx]) return pivotItems[idx];
                const auto& p = h.pivots[static_cast<std::size_t>(idx)];
                QTreeWidgetItem* parentItem = root;
                if (p.parent >= 0 && p.parent < static_cast<int>(h.pivots.size())) {
                    parentItem = ensurePivot(p.parent);
                }
                const QString label = p.name.isEmpty()
                    ? tr("Pivot %1").arg(idx)
                    : p.name;
                QString parentName;
                if (p.parent >= 0 && p.parent < static_cast<int>(h.pivots.size())) {
                    parentName = h.pivots[static_cast<std::size_t>(p.parent)].name;
                }
                QString detail = p.parent >= 0
                    ? tr("Parent: %1").arg(parentName.isEmpty()
                        ? tr("#%1").arg(p.parent)
                        : parentName)
                    : tr("Root");
                auto* item = new QTreeWidgetItem(parentItem, { label, detail });
                item->setData(0, RoleIsMesh, false);
                item->setData(0, RoleIsPivot, true);
                item->setData(0, RoleHierarchyIndex, hierarchyIndex);
                item->setData(0, RolePivotIndex, idx);
                pivotItems[static_cast<std::size_t>(idx)] = item;
                return item;
                };

            for (int i = 0; i < static_cast<int>(h.pivots.size()); ++i) {
                (void)ensurePivot(i);
            }

            for (auto mesh : h.meshes) {
                QTreeWidgetItem* parent = root;
                int pivotIdx = mesh.pivotIndex;
                if ((pivotIdx < 0 || pivotIdx >= static_cast<int>(pivotItems.size())) && !mesh.pivotName.isEmpty()) {
                    const QString target = mesh.pivotName.toLower();
                    for (int i = 0; i < static_cast<int>(h.pivots.size()); ++i) {
                        if (h.pivots[static_cast<std::size_t>(i)].name.toLower() == target) {
                            pivotIdx = i;
                            break;
                        }
                    }
                }
                if (pivotIdx >= 0
                    && pivotIdx < static_cast<int>(pivotItems.size())
                    && pivotItems[static_cast<std::size_t>(pivotIdx)]) {
                    parent = pivotItems[static_cast<std::size_t>(pivotIdx)];
                    if (mesh.pivotName.isEmpty()) {
                        mesh.pivotName = h.pivots[static_cast<std::size_t>(pivotIdx)].name;
                    }
                }

                QString detail = mesh.typeLabel;
                if (!mesh.pivotName.isEmpty()) {
                    detail += tr(" @ %1").arg(mesh.pivotName);
                }
                auto* meshItem = new QTreeWidgetItem(parent, { mesh.displayName, detail });
                meshItem->setData(0, Qt::UserRole,
                    QVariant::fromValue<void*>(mesh.chunk ? mesh.chunk.get() : nullptr));
                meshItem->setData(0, RoleIsMesh, true);
                meshItem->setData(0, RoleIsPivot, false);
                meshItem->setData(0, RoleHierarchyIndex, hierarchyIndex);
                meshItem->setData(0, RolePivotIndex, -1);
                meshItem->setData(0, RoleName, mesh.displayName);
                meshItem->setData(0, RoleLookupNames, mesh.lookupNames);
            }

            tree->expandItem(root);
        }
    }

    void onSelectionChanged() {
        const auto items = tree->selectedItems();
        if (items.isEmpty()) {
            selectButton->setEnabled(false);
            return;
        }
        QTreeWidgetItem* item = items.first();
        const void* ptr = item->data(0, Qt::UserRole).value<void*>();
        const bool isMesh = item->data(0, RoleIsMesh).toBool();
        selectButton->setEnabled(isMesh || ptr);
    }

    void activateSelection() {
        const auto items = tree->selectedItems();
        if (items.isEmpty()) return;
        QTreeWidgetItem* item = items.first();
        void* ptr = item->data(0, Qt::UserRole).value<void*>();
        if (!ptr && resolveChunkCandidates) {
            QStringList lookupNames = item->data(0, RoleLookupNames).toStringList();
            AppendUniqueLookupName(lookupNames, item->data(0, RoleName).toString());
            ptr = resolveChunkCandidates(lookupNames);
            if (ptr) {
                item->setData(0, Qt::UserRole, QVariant::fromValue<void*>(ptr));
            }
        }
        if (ptr && onMeshActivated) {
            onMeshActivated(ptr);
        }
    }

    void onItemDoubleClicked(QTreeWidgetItem* item, int column) {
        if (!item || column != 0) return;
        if (!item->data(0, RoleIsPivot).toBool()) return;

        const int hierarchyIndex = item->data(0, RoleHierarchyIndex).toInt();
        const int pivotIndex = item->data(0, RolePivotIndex).toInt();
        if (hierarchyIndex < 0 || hierarchyIndex >= static_cast<int>(hierarchies.size())) return;

        auto& hierarchy = hierarchies[static_cast<std::size_t>(hierarchyIndex)];
        if (pivotIndex < 0 || pivotIndex >= static_cast<int>(hierarchy.pivots.size())) return;

        const QString currentName = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].name;
        if (NormalizeName(currentName.toStdString()) == "roottransform") {
            return;
        }
        bool accepted = false;
        QString newName = QInputDialog::getText(
            this,
            tr("Rename Pivot"),
            tr("Pivot Name"),
            QLineEdit::Normal,
            currentName,
            &accepted).trimmed();

        if (!accepted || newName == currentName) return;
        if (newName.size() > kPivotNameMax) {
            QMessageBox::warning(
                this,
                tr("Invalid Name"),
                tr("Pivot names can be at most %1 characters.").arg(kPivotNameMax));
            return;
        }
        if (!onPivotRenamed) return;

        QString error;
        if (!onPivotRenamed(hierarchy.pivotChunk, pivotIndex, newName, &error)) {
            QMessageBox::warning(
                this,
                tr("Rename Failed"),
                error.isEmpty()
                    ? tr("Failed to rename pivot.")
                    : tr("Failed to rename pivot: %1").arg(error));
            return;
        }

        hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].name = newName;
        for (auto& mesh : hierarchy.meshes) {
            if (mesh.pivotIndex == pivotIndex) {
                mesh.pivotName = newName;
            }
        }

        populate();
        selectPivotItem(hierarchyIndex, pivotIndex);
    }

    void selectPivotItem(int hierarchyIndex, int pivotIndex) {
        std::function<QTreeWidgetItem * (QTreeWidgetItem*)> dfs =
            [&](QTreeWidgetItem* node) -> QTreeWidgetItem* {
            if (!node) return nullptr;
            if (node->data(0, RoleIsPivot).toBool()
                && node->data(0, RoleHierarchyIndex).toInt() == hierarchyIndex
                && node->data(0, RolePivotIndex).toInt() == pivotIndex) {
                return node;
            }
            for (int i = 0; i < node->childCount(); ++i) {
                if (auto* found = dfs(node->child(i))) {
                    return found;
                }
            }
            return nullptr;
            };

        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            if (auto* found = dfs(tree->topLevelItem(i))) {
                tree->setCurrentItem(found);
                tree->scrollToItem(found);
                break;
            }
        }
    }

    QTreeWidget* tree = nullptr;
    QPushButton* selectButton = nullptr;
    std::vector<HierarchyInfo> hierarchies;
    std::function<void(void*)> onMeshActivated;
    std::function<void*(const QStringList&)> resolveChunkCandidates;
    PivotRenameHandler onPivotRenamed;
};

static std::unordered_multimap<std::string, std::shared_ptr<ChunkItem>> BuildMeshIndex(
    const std::vector<std::shared_ptr<ChunkItem>>& roots) {
    std::unordered_multimap<std::string, std::shared_ptr<ChunkItem>> index;

    std::function<void(const std::shared_ptr<ChunkItem>&)> dfs = [&](const std::shared_ptr<ChunkItem>& node) {
        if (!node) return;

        if (node->id == 0x001F) { // W3D_CHUNK_MESH_HEADER3
            auto parsed = ParseChunkStruct<W3dMeshHeader3Struct>(node);
            if (auto header = std::get_if<W3dMeshHeader3Struct>(&parsed)) {
                const QString meshName = ReadFixedString(header->MeshName, W3D_NAME_LEN);
                const QString containerName = ReadFixedString(header->ContainerName, W3D_NAME_LEN);
                const QString combined = containerName.isEmpty()
                    ? meshName
                    : containerName + QLatin1Char('.') + meshName;

                const auto addName = [&](const QString& name) {
                    const std::string key = ToLower(name.toStdString());
                    if (!key.empty()) {
                        index.emplace(key, node);
                    }
                    const std::string norm = NormalizeName(name.toStdString());
                    if (!norm.empty()) {
                        index.emplace(norm, node);
                    }
                    };

                addName(meshName);
                addName(containerName);
                addName(combined);
            }
        }

        for (const auto& child : node->children) {
            dfs(child);
        }
        };

    for (const auto& root : roots) {
        dfs(root);
    }

    return index;
}

namespace {
struct HModelNodeData {
    std::string renderName;
    uint16_t pivotIdx = 0;
    uint32_t chunkId = 0;
};

struct HModelData {
    std::string name;
    std::string hierarchyName;
    std::vector<HModelNodeData> nodes;
};
} // namespace

static std::unordered_map<std::string, std::vector<MeshBinding>> CollectHlodBindings(
    const std::vector<std::shared_ptr<ChunkItem>>& roots,
    const std::unordered_multimap<std::string, std::shared_ptr<ChunkItem>>& meshIndex) {

    std::unordered_map<std::string, std::vector<MeshBinding>> result;

    auto findMeshChunk = [&](const std::string& name) -> std::shared_ptr<ChunkItem> {
        const std::string key = NormalizeName(name);
        auto range = meshIndex.equal_range(key);
        if (range.first != range.second) {
            return range.first->second;
        }
        return nullptr;
        };

    std::function<void(const std::shared_ptr<ChunkItem>&)> dfs =
        [&](const std::shared_ptr<ChunkItem>& node) {
        if (!node) return;

        if (node->id == 0x0700) { // HLOD wrapper
            std::string hlodName;
            std::string hierarchyName;

            for (const auto& child : node->children) {
                if (child->id == 0x0701) { // header
                    auto parsed = ParseChunkStruct<W3dHLodHeaderStruct>(child);
                    if (auto h = std::get_if<W3dHLodHeaderStruct>(&parsed)) {
                        hlodName = ReadFixedString(h->Name, W3D_NAME_LEN).toStdString();
                        hierarchyName = ReadFixedString(h->HierarchyName, W3D_NAME_LEN).toStdString();
                    }
                }
            }

            const std::string primaryKey = NormalizeName(!hierarchyName.empty() ? hierarchyName : hlodName);
            const std::string secondaryKey = NormalizeName(hlodName);

            auto addBinding = [&](const MeshBinding& b) {
                if (!primaryKey.empty()) {
                    result[primaryKey].push_back(b);
                }
                if (!secondaryKey.empty() && secondaryKey != primaryKey) {
                    result[secondaryKey].push_back(b);
                }
                };

            std::function<void(const std::shared_ptr<ChunkItem>&)> scanSub =
                [&](const std::shared_ptr<ChunkItem>& n) {
                if (!n) return;
                if (n->id == 0x0704) { // subobject
                    auto parsed = ParseChunkStruct<W3dHLodSubObjectStruct>(n);
                    if (auto s = std::get_if<W3dHLodSubObjectStruct>(&parsed)) {
                        MeshBinding b;
                        b.displayName = ReadFixedString(s->Name, W3D_NAME_LEN * 2);
                        b.typeLabel = QStringLiteral("HLOD");
                        b.pivotIndex = static_cast<int>(s->BoneIndex);
                        // Attempt several name variants
                        const std::string base = b.displayName.toStdString();
                        const std::string hdot = hlodName.empty() ? base : (hlodName + "." + base);
                        const std::string hhdot = hierarchyName.empty() ? base : (hierarchyName + "." + base);

                        AppendUniqueLookupName(b.lookupNames, QString::fromStdString(base));
                        AppendUniqueLookupName(b.lookupNames, QString::fromStdString(hdot));
                        AppendUniqueLookupName(b.lookupNames, QString::fromStdString(hhdot));

                        b.chunk = findMeshChunk(base);
                        if (!b.chunk && !hdot.empty()) b.chunk = findMeshChunk(hdot);
                        if (!b.chunk && !hhdot.empty()) b.chunk = findMeshChunk(hhdot);

                        addBinding(b);
                    }
                }
                for (const auto& c : n->children) scanSub(c);
                };

            scanSub(node);
        }

        for (const auto& child : node->children) {
            dfs(child);
        }
        };

    for (const auto& root : roots) {
        dfs(root);
    }

    return result;
}

static std::vector<HModelData> CollectHModels(const std::vector<std::shared_ptr<ChunkItem>>& roots) {
    std::vector<HModelData> hmodels;

    std::function<void(const std::shared_ptr<ChunkItem>&)> dfs = [&](const std::shared_ptr<ChunkItem>& node) {
        if (!node) return;

        if (node->id == 0x0300) { // W3D_CHUNK_HMODEL wrapper
            HModelData current;
            bool hasHeader = false;

            for (const auto& child : node->children) {
                if (child->id == 0x0301) { // header
                    auto parsed = ParseChunkStruct<W3dHModelHeaderStruct>(child);
                    if (auto header = std::get_if<W3dHModelHeaderStruct>(&parsed)) {
                        current.name = ReadFixedString(header->Name, W3D_NAME_LEN).toStdString();
                        current.hierarchyName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN).toStdString();
                        hasHeader = true;
                    }
                }
                else if (child->id == 0x0302 || child->id == 0x0303 ||
                    child->id == 0x0304 || child->id == 0x0306) {
                    auto parsed = ParseChunkStruct<W3dHModelNodeStruct>(child);
                    if (auto nodeStruct = std::get_if<W3dHModelNodeStruct>(&parsed)) {
                        HModelNodeData nd;
                        nd.chunkId = child->id;
                        nd.renderName = ReadFixedString(nodeStruct->RenderObjName, W3D_NAME_LEN).toStdString();
                        nd.pivotIdx = nodeStruct->PivotIdx;
                        current.nodes.push_back(std::move(nd));
                    }
                }
            }

            if (hasHeader) {
                hmodels.push_back(std::move(current));
            }
        }

        for (const auto& child : node->children) {
            dfs(child);
        }
        };

    for (const auto& root : roots) {
        dfs(root);
    }

    return hmodels;
}

static std::vector<HierarchyInfo> CollectHierarchies(
    const std::vector<std::shared_ptr<ChunkItem>>& roots,
    const std::vector<HModelData>& hmodels,
    const std::unordered_multimap<std::string, std::shared_ptr<ChunkItem>>& meshIndex,
    const std::unordered_map<std::string, std::vector<MeshBinding>>& hlodBindings) {

    std::vector<HierarchyInfo> hierarchies;

    auto findMatches = [&](const std::string& hierarchyName) -> std::vector<size_t> {
        std::vector<size_t> matches;
        const std::string key = NormalizeName(hierarchyName);
        for (size_t i = 0; i < hmodels.size(); ++i) {
            const auto& h = hmodels[i];
            const std::string hn = NormalizeName(h.hierarchyName);
            const std::string nameNorm = NormalizeName(h.name);
            if ((!hn.empty() && hn == key) || (!nameNorm.empty() && nameNorm == key)) {
                matches.push_back(i);
            }
        }
        return matches;
        };

    auto findMeshChunk = [&](const std::string& name) -> std::shared_ptr<ChunkItem> {
        const std::string key = NormalizeName(name);
        auto range = meshIndex.equal_range(key);
        if (range.first != range.second) {
            return range.first->second;
        }
        return nullptr;
        };

    std::function<void(const std::shared_ptr<ChunkItem>&)> dfs = [&](const std::shared_ptr<ChunkItem>& node) {
        if (!node) return;

        if (node->id == 0x0100) { // W3D_CHUNK_HIERARCHY
            HierarchyInfo info;
            bool hasHeader = false;

            for (const auto& child : node->children) {
                if (child->id == 0x0101) { // header
                    auto parsed = ParseChunkStruct<W3dHierarchyStruct>(child);
                    if (auto header = std::get_if<W3dHierarchyStruct>(&parsed)) {
                        info.name = ReadFixedString(header->Name, W3D_NAME_LEN);
                        hasHeader = true;
                    }
                }
                else if (child->id == 0x0102) { // pivots
                    info.pivotChunk = child;
                    auto parsed = ParseChunkArray<W3dPivotStruct>(child);
                    if (auto pivots = std::get_if<std::vector<W3dPivotStruct>>(&parsed)) {
                        info.pivots.reserve(pivots->size());
                        for (const auto& p : *pivots) {
                            PivotInfo pi;
                            pi.name = ReadFixedString(p.Name, W3D_NAME_LEN);
                            pi.parent = (p.ParentIdx == 0xFFFFFFFFu)
                                ? -1
                                : static_cast<int>(p.ParentIdx);
                            info.pivots.push_back(std::move(pi));
                        }
                    }
                }
            }

            if (hasHeader) {
                // Attach meshes that use this hierarchy
                auto hMatches = findMatches(info.name.toStdString());
                if (hMatches.empty() && !hmodels.empty()) {
                    for (size_t i = 0; i < hmodels.size(); ++i) hMatches.push_back(i); // fallback: show all
                }

                for (size_t idx : hMatches) {
                    if (idx >= hmodels.size()) continue;
                    const auto& hModel = hmodels[idx];
                    for (const auto& nodeData : hModel.nodes) {
                        MeshBinding binding;

                        const QString renderName = QString::fromStdString(nodeData.renderName);
                        if (!hModel.name.empty()) {
                            binding.displayName = QString::fromStdString(hModel.name) + QLatin1Char('.') + renderName;
                        }
                        else {
                            binding.displayName = renderName;
                        }
                        AppendUniqueLookupName(binding.lookupNames, binding.displayName);
                        AppendUniqueLookupName(binding.lookupNames, renderName);
                        if (!hModel.hierarchyName.empty()) {
                            AppendUniqueLookupName(
                                binding.lookupNames,
                                QString::fromStdString(hModel.hierarchyName) + QLatin1Char('.') + renderName);
                        }

                        binding.typeLabel = [id = nodeData.chunkId]() {
                            switch (id) {
                            case 0x0302: return QStringLiteral("Mesh");
                            case 0x0303: return QStringLiteral("Collision");
                            case 0x0304: return QStringLiteral("Skin");
                            case 0x0306: return QStringLiteral("Shadow");
                            default: return QStringLiteral("Mesh");
                            }
                            }();

                        binding.pivotIndex = static_cast<int>(nodeData.pivotIdx);
                        if (binding.pivotIndex >= 0 && binding.pivotIndex < static_cast<int>(info.pivots.size())) {
                            binding.pivotName = info.pivots[static_cast<std::size_t>(binding.pivotIndex)].name;
                        }

                        // Try to locate the mesh chunk by full name first, then by render name
                        std::shared_ptr<ChunkItem> meshChunk = findMeshChunk(binding.displayName.toStdString());
                        if (!meshChunk) {
                            meshChunk = findMeshChunk(renderName.toStdString());
                        }
                        binding.chunk = meshChunk;

                        info.meshes.push_back(std::move(binding));
                    }
                }

                hierarchies.push_back(std::move(info));
            }
        }

        for (const auto& child : node->children) {
            dfs(child);
        }
        };

    for (const auto& root : roots) {
        dfs(root);
    }

    // Merge HLOD-only bindings into matching hierarchies (if any)
    for (auto& info : hierarchies) {
        const std::string norm = NormalizeName(info.name.toStdString());
        auto it = hlodBindings.find(norm);
        if (it != hlodBindings.end()) {
            for (auto b : it->second) {
                if (b.pivotIndex >= 0 && b.pivotIndex < static_cast<int>(info.pivots.size())) {
                    b.pivotName = info.pivots[static_cast<std::size_t>(b.pivotIndex)].name;
                }
                info.meshes.push_back(std::move(b));
            }
        }
    }

    return hierarchies;
}

MeshEditorWidget::MeshEditorWidget(QWidget* parent) : QWidget(parent) {
    Ui::MeshEditorWidget form;
    form.setupUi(this);
    meshNameEdit = form.meshNameEdit;
    containerNameEdit = form.containerNameEdit;
    applyButton = form.applyButton;
    flagControls = {
        { MeshAttrValue(MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_PHYSICAL), form.physicalCheck },
        { MeshAttrValue(MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_PROJECTILE), form.projectileCheck },
        { MeshAttrValue(MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_VIS), form.visibilityCheck },
        { MeshAttrValue(MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_CAMERA), form.cameraCheck },
        { MeshAttrValue(MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_VEHICLE), form.vehicleCheck },
        { MeshAttrValue(MeshAttr::W3D_MESH_FLAG_HIDDEN), form.hiddenCheck },
        { MeshAttrValue(MeshAttr::W3D_MESH_FLAG_TWO_SIDED), form.twoSidedCheck },
    };
    connect(applyButton, &QPushButton::clicked,
        this, &MeshEditorWidget::applyChanges);
}

void MeshEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    meshNameEdit->clear();
    containerNameEdit->clear();
    for (auto& ctrl : flagControls) {
        if (ctrl.box) ctrl.box->setChecked(false);
    }
    if (!chunkPtr) {
        setEnabled(false);
        return;
    }

    auto parsed = ParseChunkStruct<W3dMeshHeader3Struct>(chunkPtr);
    if (auto err = std::get_if<std::string>(&parsed)) {
        Q_UNUSED(err);
        setEnabled(false);
        return;
    }
    const auto& header = std::get<W3dMeshHeader3Struct>(parsed);
    meshNameEdit->setText(ReadFixedString(header.MeshName, W3D_NAME_LEN));
    containerNameEdit->setText(ReadFixedString(header.ContainerName, W3D_NAME_LEN));

    const uint32_t attr = header.Attributes;
    for (auto& ctrl : flagControls) {
        ctrl.box->setChecked((attr & ctrl.mask) != 0);
    }

    setEnabled(true);
}

void MeshEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    QString oldMeshName;
    QString oldContainerName;
    {
        auto parsed = ParseChunkStruct<W3dMeshHeader3Struct>(chunkPtr);
        if (auto header = std::get_if<W3dMeshHeader3Struct>(&parsed)) {
            oldMeshName = ReadFixedString(header->MeshName, W3D_NAME_LEN);
            oldContainerName = ReadFixedString(header->ContainerName, W3D_NAME_LEN);
        }
    }

    const QString newMeshName = meshNameEdit->text();
    const QString newContainerName = containerNameEdit->text();
    const std::string meshName = newMeshName.toStdString();
    const std::string containerName = newContainerName.toStdString();
    uint32_t clearMask = 0;
    for (const auto& ctrl : flagControls) {
        clearMask |= ctrl.mask;
    }

    std::string error;
    const bool ok = W3DEdit::MutateStructChunk<W3dMeshHeader3Struct>(
        chunkPtr,
        [&](W3dMeshHeader3Struct& header) {
            W3DEdit::WriteFixedString(header.MeshName, W3D_NAME_LEN, meshName);
            W3DEdit::WriteFixedString(header.ContainerName, W3D_NAME_LEN, containerName);
            uint32_t attr = header.Attributes & ~clearMask;
            for (const auto& ctrl : flagControls) {
                if (ctrl.box->isChecked()) {
                    attr |= ctrl.mask;
                }
            }
            header.Attributes = attr;
        },
        &error);

    if (!ok) {
        QMessageBox::warning(this, tr("Error"),
            QString::fromStdString(error.empty() ? "Failed to update mesh header." : error));
        return;
    }

    if (oldMeshName != newMeshName || oldContainerName != newContainerName) {
        emit meshRenamed(oldMeshName, newMeshName, oldContainerName, newContainerName);
    }
    emit chunkEdited();
}

ShaderEditorWidget::ShaderEditorWidget(QWidget* parent)
    : QWidget(parent) {
    Ui::ShaderEditorWidget form;
    form.setupUi(this);
    shaderIndexCombo = form.shaderIndexCombo;
    depthCompareCombo = form.depthCompareCombo;
    depthMaskCombo = form.depthMaskCombo;
    destBlendCombo = form.destBlendCombo;
    priGradientCombo = form.priGradientCombo;
    secGradientCombo = form.secGradientCombo;
    srcBlendCombo = form.srcBlendCombo;
    texturingCombo = form.texturingCombo;
    detailColorCombo = form.detailColorCombo;
    detailAlphaCombo = form.detailAlphaCombo;
    alphaTestCombo = form.alphaTestCombo;
    postDetailColorCombo = form.postDetailColorCombo;
    postDetailAlphaCombo = form.postDetailAlphaCombo;
    colorMaskSpin = form.colorMaskSpin;
    fogFuncSpin = form.fogFuncSpin;
    shaderPresetSpin = form.shaderPresetSpin;
    applyButton = form.applyButton;

    PopulateEnumCombo<DepthCompare>(depthCompareCombo);
    PopulateEnumCombo<DepthMask>(depthMaskCombo);
    PopulateEnumCombo<DestBlend>(destBlendCombo);
    PopulateEnumCombo<PriGradient>(priGradientCombo);
    PopulateEnumCombo<SecGradient>(secGradientCombo);
    PopulateEnumCombo<SrcBlend>(srcBlendCombo);
    PopulateEnumCombo<Texturing>(texturingCombo);
    PopulateEnumCombo<DetailColorFunc>(detailColorCombo);
    PopulateEnumCombo<DetailAlphaFunc>(detailAlphaCombo);
    PopulateEnumCombo<AlphaTest>(alphaTestCombo);
    PopulateEnumCombo<DetailColorFunc>(postDetailColorCombo);
    PopulateEnumCombo<DetailAlphaFunc>(postDetailAlphaCombo);

    connect(shaderIndexCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int idx) { loadShader(idx); });
    connect(applyButton, &QPushButton::clicked,
        this, &ShaderEditorWidget::applyChanges);
}

void ShaderEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    shaders.clear();

    const QSignalBlocker blocker(shaderIndexCombo);
    shaderIndexCombo->clear();

    if (!chunkPtr) {
        setEnabled(false);
        return;
    }

    auto parsed = ParseChunkArray<W3dShaderStruct>(chunkPtr);
    if (auto err = std::get_if<std::string>(&parsed)) {
        Q_UNUSED(err);
        setEnabled(false);
        return;
    }

    shaders = std::get<std::vector<W3dShaderStruct>>(parsed);
    if (shaders.empty()) {
        setEnabled(false);
        return;
    }

    for (int i = 0; i < static_cast<int>(shaders.size()); ++i) {
        shaderIndexCombo->addItem(tr("Shader %1").arg(i), i);
    }

    setEnabled(true);
    shaderIndexCombo->setCurrentIndex(0);
    loadShader(0);
}

void ShaderEditorWidget::loadShader(int index) {
    if (index < 0 || index >= static_cast<int>(shaders.size())) {
        return;
    }
    updateControls(shaders[static_cast<std::size_t>(index)]);
}

void ShaderEditorWidget::updateControls(const W3dShaderStruct& shader) {
    const QSignalBlocker block0(depthCompareCombo);
    const QSignalBlocker block1(depthMaskCombo);
    const QSignalBlocker block2(destBlendCombo);
    const QSignalBlocker block3(priGradientCombo);
    const QSignalBlocker block4(secGradientCombo);
    const QSignalBlocker block5(srcBlendCombo);
    const QSignalBlocker block6(texturingCombo);
    const QSignalBlocker block7(detailColorCombo);
    const QSignalBlocker block8(detailAlphaCombo);
    const QSignalBlocker block9(alphaTestCombo);
    const QSignalBlocker block10(postDetailColorCombo);
    const QSignalBlocker block11(postDetailAlphaCombo);
    const QSignalBlocker block12(colorMaskSpin);
    const QSignalBlocker block13(fogFuncSpin);
    const QSignalBlocker block14(shaderPresetSpin);

    SetComboValue(depthCompareCombo, shader.DepthCompare);
    SetComboValue(depthMaskCombo, shader.DepthMask);
    SetComboValue(destBlendCombo, shader.DestBlend);
    SetComboValue(priGradientCombo, shader.PriGradient);
    SetComboValue(secGradientCombo, shader.SecGradient);
    SetComboValue(srcBlendCombo, shader.SrcBlend);
    SetComboValue(texturingCombo, shader.Texturing);
    SetComboValue(detailColorCombo, shader.DetailColorFunc);
    SetComboValue(detailAlphaCombo, shader.DetailAlphaFunc);
    SetComboValue(alphaTestCombo, shader.AlphaTest);
    SetComboValue(postDetailColorCombo, shader.PostDetailColorFunc);
    SetComboValue(postDetailAlphaCombo, shader.PostDetailAlphaFunc);

    colorMaskSpin->setValue(shader.ColorMask);
    fogFuncSpin->setValue(shader.FogFunc);
    shaderPresetSpin->setValue(shader.ShaderPreset);
}

W3dShaderStruct ShaderEditorWidget::captureCurrent() const {
    W3dShaderStruct shader{};
    const int idx = shaderIndexCombo->currentIndex();
    if (idx >= 0 && idx < static_cast<int>(shaders.size())) {
        shader = shaders[static_cast<std::size_t>(idx)];
    }

    shader.DepthCompare = ComboValue(depthCompareCombo);
    shader.DepthMask = ComboValue(depthMaskCombo);
    shader.DestBlend = ComboValue(destBlendCombo);
    shader.PriGradient = ComboValue(priGradientCombo);
    shader.SecGradient = ComboValue(secGradientCombo);
    shader.SrcBlend = ComboValue(srcBlendCombo);
    shader.Texturing = ComboValue(texturingCombo);
    shader.DetailColorFunc = ComboValue(detailColorCombo);
    shader.DetailAlphaFunc = ComboValue(detailAlphaCombo);
    shader.AlphaTest = ComboValue(alphaTestCombo);
    shader.PostDetailColorFunc = ComboValue(postDetailColorCombo);
    shader.PostDetailAlphaFunc = ComboValue(postDetailAlphaCombo);

    shader.ColorMask = static_cast<uint8_t>(colorMaskSpin->value());
    shader.FogFunc = static_cast<uint8_t>(fogFuncSpin->value());
    shader.ShaderPreset = static_cast<uint8_t>(shaderPresetSpin->value());

    return shader;
}

void ShaderEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    const int idx = shaderIndexCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(shaders.size())) return;

    W3dShaderStruct updated = captureCurrent();

    std::string error;
    const bool ok = W3DEdit::MutateStructAtIndex<W3dShaderStruct>(
        chunkPtr,
        static_cast<std::size_t>(idx),
        [&](W3dShaderStruct& target) { target = updated; },
        &error);

    if (!ok) {
        QMessageBox::warning(this, tr("Error"),
            QString::fromStdString(error.empty() ? "Failed to update shader." : error));
        return;
    }

    shaders[static_cast<std::size_t>(idx)] = updated;
    emit chunkEdited();
}

StringEditorWidget::StringEditorWidget(const QString& label, QWidget* parent, int maxLength)
    : QWidget(parent) {
    setEnabled(false);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* form = new QFormLayout();
    lineEdit = new QLineEdit(this);
    if (maxLength > 0) {
        lineEdit->setMaxLength(maxLength);
    }
    form->addRow(label, lineEdit);
    layout->addLayout(form);

    applyButton = new QPushButton(tr("Apply"), this);
    connect(applyButton, &QPushButton::clicked,
        this, &StringEditorWidget::applyChanges);

    layout->addWidget(applyButton, 0, Qt::AlignRight);
    layout->addStretch();
}

void StringEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    if (!chunkPtr) {
        lineEdit->clear();
        setEnabled(false);
        return;
    }

    const char* raw = reinterpret_cast<const char*>(chunkPtr->data.data());
    const auto len = chunkPtr->data.empty()
        ? 0
        : TruncatedLength(raw, chunkPtr->data.size());
    lineEdit->setText(QString::fromLatin1(raw, static_cast<int>(len)));
    setEnabled(true);
}

void StringEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    if (!W3DEdit::UpdateNullTermStringChunk(chunkPtr, lineEdit->text().toStdString())) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to update string chunk."));
        return;
    }
    emit chunkEdited();
}

RawTextEditorWidget::RawTextEditorWidget(const QString& label, QWidget* parent)
    : QWidget(parent) {
    setEnabled(false);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* form = new QFormLayout();
    textEdit = new QPlainTextEdit(this);
    textEdit->setMinimumHeight(96);
    form->addRow(label, textEdit);
    layout->addLayout(form);

    applyButton = new QPushButton(tr("Apply"), this);
    connect(applyButton, &QPushButton::clicked,
        this, &RawTextEditorWidget::applyChanges);

    layout->addWidget(applyButton, 0, Qt::AlignRight);
    layout->addStretch();
}

void RawTextEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    if (!chunkPtr) {
        textEdit->clear();
        setEnabled(false);
        return;
    }

    textEdit->setPlainText(QString::fromLatin1(
        reinterpret_cast<const char*>(chunkPtr->data.data()),
        static_cast<int>(TruncatedLength(
            reinterpret_cast<const char*>(chunkPtr->data.data()),
            chunkPtr->data.size()))));
    setEnabled(true);
}

void RawTextEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    QString normalized = textEdit->toPlainText();
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    normalized.replace('\n', "\r\n");
    const QByteArray text = normalized.toLatin1();
    const std::string value(text.constData(), static_cast<std::size_t>(text.size()));
    if (!W3DEdit::UpdateNullTermStringChunk(chunkPtr, value)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to update text chunk."));
        return;
    }
    emit chunkEdited();
}

HierarchyHeaderEditorWidget::HierarchyHeaderEditorWidget(QWidget* parent)
    : QWidget(parent) {
    Ui::HierarchyHeaderEditorWidget form;
    form.setupUi(this);
    nameEdit = form.nameEdit;
    applyButton = form.applyButton;
    connect(applyButton, &QPushButton::clicked,
        this, &HierarchyHeaderEditorWidget::applyChanges);
}

void HierarchyHeaderEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    nameEdit->clear();

    if (!chunkPtr) {
        setEnabled(false);
        return;
    }

    auto parsed = ParseChunkStruct<W3dHierarchyStruct>(chunkPtr);
    if (auto err = std::get_if<std::string>(&parsed)) {
        Q_UNUSED(err);
        setEnabled(false);
        return;
    }

    const auto& header = std::get<W3dHierarchyStruct>(parsed);
    nameEdit->setText(ReadFixedString(header.Name, W3D_NAME_LEN));
    setEnabled(true);
}

void HierarchyHeaderEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    std::string error;
    const bool ok = W3DEdit::MutateStructChunk<W3dHierarchyStruct>(
        chunkPtr,
        [&](W3dHierarchyStruct& header) {
            W3DEdit::WriteFixedString(header.Name, W3D_NAME_LEN, nameEdit->text().toStdString());
        },
        &error);

    if (!ok) {
        QMessageBox::warning(this, tr("Error"),
            QString::fromStdString(error.empty() ? "Failed to update hierarchy name." : error));
        return;
    }

    emit chunkEdited();
}

AnimationHierarchyEditorWidget::AnimationHierarchyEditorWidget(QWidget* parent)
    : QWidget(parent) {
    Ui::AnimationHierarchyEditorWidget form;
    form.setupUi(this);
    animationNameEdit = form.animationNameEdit;
    hierarchyNameEdit = form.hierarchyNameEdit;
    applyButton = form.applyButton;
    connect(applyButton, &QPushButton::clicked,
        this, &AnimationHierarchyEditorWidget::applyChanges);
}

void AnimationHierarchyEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    loadedAnimationName.clear();
    loadedHierarchyName.clear();
    animationNameEdit->clear();
    hierarchyNameEdit->clear();

    if (!chunkPtr) {
        setEnabled(false);
        return;
    }

    switch (chunkPtr->id) {
    case 0x0201: {
        auto parsed = ParseChunkStruct<W3dAnimHeaderStruct>(chunkPtr);
        if (auto err = std::get_if<std::string>(&parsed)) {
            Q_UNUSED(err);
            setEnabled(false);
            return;
        }
        const auto& header = std::get<W3dAnimHeaderStruct>(parsed);
        loadedAnimationName = ReadFixedString(header.Name, W3D_NAME_LEN);
        loadedHierarchyName = ReadFixedString(header.HierarchyName, W3D_NAME_LEN);
        animationNameEdit->setText(loadedAnimationName);
        hierarchyNameEdit->setText(ReadFixedString(header.HierarchyName, W3D_NAME_LEN));
        break;
    }
    case 0x0281: {
        auto parsed = ParseChunkStruct<W3dCompressedAnimHeaderStruct>(chunkPtr);
        if (auto err = std::get_if<std::string>(&parsed)) {
            Q_UNUSED(err);
            setEnabled(false);
            return;
        }
        const auto& header = std::get<W3dCompressedAnimHeaderStruct>(parsed);
        loadedAnimationName = ReadFixedString(header.Name, W3D_NAME_LEN);
        loadedHierarchyName = ReadFixedString(header.HierarchyName, W3D_NAME_LEN);
        animationNameEdit->setText(loadedAnimationName);
        hierarchyNameEdit->setText(ReadFixedString(header.HierarchyName, W3D_NAME_LEN));
        break;
    }
    case 0x02C1: {
        auto parsed = ParseChunkStruct<W3dMorphAnimHeaderStruct>(chunkPtr);
        if (auto err = std::get_if<std::string>(&parsed)) {
            Q_UNUSED(err);
            setEnabled(false);
            return;
        }
        const auto& header = std::get<W3dMorphAnimHeaderStruct>(parsed);
        loadedAnimationName = ReadFixedString(header.Name, W3D_NAME_LEN);
        loadedHierarchyName = ReadFixedString(header.HierarchyName, W3D_NAME_LEN);
        animationNameEdit->setText(loadedAnimationName);
        hierarchyNameEdit->setText(ReadFixedString(header.HierarchyName, W3D_NAME_LEN));
        break;
    }
    default:
        setEnabled(false);
        return;
    }

    setEnabled(true);
}

void AnimationHierarchyEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    const QString oldAnimationName = loadedAnimationName;
    const QString oldHierarchyName = loadedHierarchyName;
    const QString newHierarchyName = hierarchyNameEdit->text();
    QString newAnimationName = animationNameEdit->text();
    if (newAnimationName == oldAnimationName && newHierarchyName != oldHierarchyName) {
        newAnimationName = RenameAnimationTokenForHierarchyChange(
            oldAnimationName,
            oldHierarchyName,
            newHierarchyName);
    }

    const std::string animationName = newAnimationName.toStdString();
    const std::string hierarchyName = newHierarchyName.toStdString();
    std::string error;
    bool ok = false;

    switch (chunkPtr->id) {
    case 0x0201:
        ok = W3DEdit::MutateStructChunk<W3dAnimHeaderStruct>(
            chunkPtr,
            [&](W3dAnimHeaderStruct& header) {
                W3DEdit::WriteFixedString(header.Name, W3D_NAME_LEN, animationName);
                W3DEdit::WriteFixedString(header.HierarchyName, W3D_NAME_LEN, hierarchyName);
            },
            &error);
        break;
    case 0x0281:
        ok = W3DEdit::MutateStructChunk<W3dCompressedAnimHeaderStruct>(
            chunkPtr,
            [&](W3dCompressedAnimHeaderStruct& header) {
                W3DEdit::WriteFixedString(header.Name, W3D_NAME_LEN, animationName);
                W3DEdit::WriteFixedString(header.HierarchyName, W3D_NAME_LEN, hierarchyName);
            },
            &error);
        break;
    case 0x02C1:
        ok = W3DEdit::MutateStructChunk<W3dMorphAnimHeaderStruct>(
            chunkPtr,
            [&](W3dMorphAnimHeaderStruct& header) {
                W3DEdit::WriteFixedString(header.Name, W3D_NAME_LEN, animationName);
                W3DEdit::WriteFixedString(header.HierarchyName, W3D_NAME_LEN, hierarchyName);
            },
            &error);
        break;
    default:
        return;
    }

    if (!ok) {
        QMessageBox::warning(this, tr("Error"),
            QString::fromStdString(error.empty() ? "Failed to update animation header fields." : error));
        return;
    }

    loadedAnimationName = newAnimationName;
    loadedHierarchyName = newHierarchyName;
    if (animationNameEdit->text() != newAnimationName) {
        animationNameEdit->setText(newAnimationName);
    }

    if (oldAnimationName != newAnimationName || oldHierarchyName != newHierarchyName) {
        emit headerRenamed(oldAnimationName, newAnimationName, oldHierarchyName, newHierarchyName);
    }
    emit chunkEdited();
}

TransformNodeEditorWidget::TransformNodeEditorWidget(QWidget* parent)
    : QWidget(parent) {
    Ui::TransformNodeEditorWidget form;
    form.setupUi(this);
    fileNameEdit = form.fileNameEdit;
    applyButton = form.applyButton;
    connect(applyButton, &QPushButton::clicked,
        this, &TransformNodeEditorWidget::applyChanges);
}

void TransformNodeEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    fileNameEdit->clear();
    if (!chunkPtr) {
        setEnabled(false);
        return;
    }

    auto parsed = ParseChunkStruct<W3dTransformNodeStruct>(chunkPtr);
    if (auto err = std::get_if<std::string>(&parsed)) {
        Q_UNUSED(err);
        setEnabled(false);
        return;
    }

    const auto& header = std::get<W3dTransformNodeStruct>(parsed);
    const std::size_t headerBytes = sizeof(W3dTransformNodeStruct);
    const std::size_t available = chunkPtr->data.size() > headerBytes
        ? (chunkPtr->data.size() - headerBytes)
        : 0;
    const std::size_t nameLen = std::min<std::size_t>(header.name_len, available);
    if (nameLen > 0) {
        const char* raw = reinterpret_cast<const char*>(chunkPtr->data.data() + headerBytes);
        const auto len = TruncatedLength(raw, nameLen);
        fileNameEdit->setText(QString::fromLatin1(raw, static_cast<int>(len)));
    }

    setEnabled(true);
}

void TransformNodeEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    auto parsed = ParseChunkStruct<W3dTransformNodeStruct>(chunkPtr);
    if (auto err = std::get_if<std::string>(&parsed)) {
        QMessageBox::warning(this, tr("Error"),
            QString::fromStdString("Failed to update transform node: " + *err));
        return;
    }

    auto header = std::get<W3dTransformNodeStruct>(parsed);
    const std::size_t headerBytes = sizeof(W3dTransformNodeStruct);
    const std::size_t oldAvailable = chunkPtr->data.size() > headerBytes
        ? (chunkPtr->data.size() - headerBytes)
        : 0;
    const std::size_t oldNameLen = std::min<std::size_t>(header.name_len, oldAvailable);

    bool oldHadTerminator = false;
    if (oldNameLen > 0) {
        const char* oldName = reinterpret_cast<const char*>(chunkPtr->data.data() + headerBytes);
        for (std::size_t i = 0; i < oldNameLen; ++i) {
            if (oldName[i] == '\0') {
                oldHadTerminator = true;
                break;
            }
        }
    }

    QByteArray newNameBytes = fileNameEdit->text().toLatin1();
    if (oldHadTerminator) {
        newNameBytes.push_back('\0');
    }

    header.name_len = static_cast<uint32_t>(newNameBytes.size());
    std::vector<uint8_t> updatedData(headerBytes + static_cast<std::size_t>(newNameBytes.size()));
    std::memcpy(updatedData.data(), &header, sizeof(header));
    if (!newNameBytes.isEmpty()) {
        std::memcpy(updatedData.data() + headerBytes, newNameBytes.constData(), static_cast<std::size_t>(newNameBytes.size()));
    }

    chunkPtr->data = std::move(updatedData);
    chunkPtr->length = static_cast<uint32_t>(chunkPtr->data.size());
    emit chunkEdited();
}

MapperArgsEditorWidget::MapperArgsEditorWidget(const QString& label, QWidget* parent)
    : QWidget(parent) {
    setEnabled(false);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* title = new QLabel(label, this);
    layout->addWidget(title);

    argsEdit = new QPlainTextEdit(this);
    argsEdit->setPlaceholderText(tr("Enter mapper args..."));
    layout->addWidget(argsEdit, 1);

    auto* refGroup = new QGroupBox(tr("Reference"), this);
    auto* refLayout = new QVBoxLayout(refGroup);
    referenceEdit = new QPlainTextEdit(refGroup);
    referenceEdit->setReadOnly(true);
    referenceEdit->setPlainText(BuildMapperArgsReference());
    refLayout->addWidget(referenceEdit);
    layout->addWidget(refGroup);

    applyButton = new QPushButton(tr("Apply Mapper Args"), this);
    connect(applyButton, &QPushButton::clicked,
        this, &MapperArgsEditorWidget::applyChanges);

    layout->addWidget(applyButton, 0, Qt::AlignRight);
    layout->addStretch();
}

void MapperArgsEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    if (!chunkPtr) {
        argsEdit->clear();
        setEnabled(false);
        return;
    }

    const char* raw = reinterpret_cast<const char*>(chunkPtr->data.data());
    const auto len = chunkPtr->data.empty()
        ? 0
        : TruncatedLength(raw, chunkPtr->data.size());
    argsEdit->setPlainText(QString::fromLatin1(raw, static_cast<int>(len)));
    setEnabled(true);
}

void MapperArgsEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    if (!W3DEdit::UpdateNullTermStringChunk(chunkPtr, argsEdit->toPlainText().toStdString())) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to update mapper args."));
        return;
    }
    emit chunkEdited();
}

SurfaceTypeEditorWidget::SurfaceTypeEditorWidget(QWidget* parent)
    : QWidget(parent) {
    Ui::SurfaceTypeEditorWidget form;
    form.setupUi(this);
    surfaceTypeCombo = form.surfaceTypeCombo;
    applyButton = form.applyButton;

    for (uint32_t i = 0; i <= 255; ++i) {
        const char* name = SurfaceTypeName(i);
        if (!name) continue;
        surfaceTypeCombo->addItem(QString::fromLatin1("%1 (%2)").arg(QString::fromLatin1(name)).arg(i),
            static_cast<int>(i));
    }

    connect(applyButton, &QPushButton::clicked,
        this, &SurfaceTypeEditorWidget::applyChanges);
}

void SurfaceTypeEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    if (!chunkPtr) {
        surfaceTypeCombo->setCurrentIndex(-1);
        setEnabled(false);
        return;
    }

    uint32_t surfaceType = 0;
    bool found = false;

    const auto& buf = chunkPtr->data;
    size_t off = 0;
    while (off + 2 <= buf.size()) {
        const uint8_t id = buf[off + 0];
        const uint8_t size = buf[off + 1];
        off += 2;
        if (off + size > buf.size()) break;

        if (id == 0x01 && size >= 4) { // VARID_SURFACETYPE
            std::memcpy(&surfaceType, buf.data() + off, 4);
            found = true;
            break;
        }
        off += size;
    }

    if (found) {
        SetComboValue(surfaceTypeCombo, static_cast<int>(surfaceType));
    }
    else {
        SetComboValue(surfaceTypeCombo, 13); // Default
    }

    setEnabled(true);
}

void SurfaceTypeEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    const QVariant data = surfaceTypeCombo->currentData();
    if (!data.isValid()) return;
    const uint32_t surfaceType = static_cast<uint32_t>(data.toInt());

    auto& buf = chunkPtr->data;
    size_t off = 0;
    while (off + 2 <= buf.size()) {
        const uint8_t id = buf[off + 0];
        const uint8_t size = buf[off + 1];
        off += 2;
        if (off + size > buf.size()) {
            QMessageBox::warning(this, tr("Error"),
                tr("Surface type chunk is truncated."));
            return;
        }

        if (id == 0x01) { // VARID_SURFACETYPE
            if (size < 4) {
                QMessageBox::warning(this, tr("Error"),
                    tr("Surface type value is invalid."));
                return;
            }
            std::memcpy(buf.data() + off, &surfaceType, 4);
            chunkPtr->length = static_cast<uint32_t>(buf.size());
            emit chunkEdited();
            return;
        }
        off += size;
    }

    // Not found: append a new micro-chunk
    buf.push_back(0x01);
    buf.push_back(4);
    buf.push_back(static_cast<uint8_t>(surfaceType & 0xFF));
    buf.push_back(static_cast<uint8_t>((surfaceType >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((surfaceType >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((surfaceType >> 24) & 0xFF));
    chunkPtr->length = static_cast<uint32_t>(buf.size());
    emit chunkEdited();
}

TriangleSurfaceTypeEditorWidget::TriangleSurfaceTypeEditorWidget(QWidget* parent)
    : QWidget(parent) {
    Ui::TriangleSurfaceTypeEditorWidget form;
    form.setupUi(this);
    fromCombo = form.fromCombo;
    toCombo = form.toCombo;
    statsLabel = form.statsLabel;
    applyButton = form.applyButton;

    fromCombo->addItem(tr("Any"), -1);
    for (uint32_t i = 0; i <= 255; ++i) {
        const char* name = SurfaceTypeName(i);
        if (!name) continue;
        const auto label = QString::fromLatin1("%1 (%2)")
            .arg(QString::fromLatin1(name))
            .arg(i);
        fromCombo->addItem(label, static_cast<int>(i));
        toCombo->addItem(label, static_cast<int>(i));
    }

    connect(applyButton, &QPushButton::clicked,
        this, &TriangleSurfaceTypeEditorWidget::applyChanges);
    connect(fromCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &TriangleSurfaceTypeEditorWidget::updateStats);
}

void TriangleSurfaceTypeEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    if (!chunkPtr) {
        statsLabel->clear();
        fromCombo->setCurrentIndex(0);
        toCombo->setCurrentIndex(-1);
        setEnabled(false);
        return;
    }

    if (chunkPtr->data.size() % sizeof(W3dTriStruct) != 0) {
        statsLabel->setText(tr("Malformed TRIANGLES chunk."));
        setEnabled(false);
        return;
    }

    // Default selections based on the first triangle (when available).
    const std::size_t triCount = chunkPtr->data.size() / sizeof(W3dTriStruct);
    if (triCount > 0) {
        uint32_t firstType{};
        std::memcpy(&firstType,
            chunkPtr->data.data() + offsetof(W3dTriStruct, Attributes),
            sizeof(firstType));
        SetComboValue(toCombo, static_cast<int>(firstType));
    }
    else {
        SetComboValue(toCombo, 13); // Default
    }

    fromCombo->setCurrentIndex(0); // Any
    setEnabled(true);
    updateStats();
}

void TriangleSurfaceTypeEditorWidget::updateStats() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) {
        statsLabel->clear();
        return;
    }

    const auto& buf = chunkPtr->data;
    if (buf.size() % sizeof(W3dTriStruct) != 0) {
        statsLabel->setText(tr("Malformed TRIANGLES chunk."));
        return;
    }

    const int fromType = fromCombo->currentData().toInt();
    const std::size_t triCount = buf.size() / sizeof(W3dTriStruct);
    std::size_t matches = 0;

    if (fromType < 0) {
        matches = triCount;
    }
    else {
        for (std::size_t i = 0; i < triCount; ++i) {
            uint32_t surfaceType{};
            const std::size_t off = i * sizeof(W3dTriStruct) + offsetof(W3dTriStruct, Attributes);
            std::memcpy(&surfaceType, buf.data() + off, sizeof(surfaceType));
            if (surfaceType == static_cast<uint32_t>(fromType)) {
                ++matches;
            }
        }
    }

    statsLabel->setText(tr("Triangles: %1 • Matches: %2").arg(triCount).arg(matches));
}

void TriangleSurfaceTypeEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    if (chunkPtr->data.size() % sizeof(W3dTriStruct) != 0) {
        QMessageBox::warning(this, tr("Error"), tr("Malformed TRIANGLES chunk."));
        return;
    }

    const QVariant fromData = fromCombo->currentData();
    const QVariant toData = toCombo->currentData();
    if (!fromData.isValid() || !toData.isValid()) return;

    const int fromType = fromData.toInt();
    const uint32_t toType = static_cast<uint32_t>(toData.toInt());

    auto& buf = chunkPtr->data;
    const std::size_t triCount = buf.size() / sizeof(W3dTriStruct);
    std::size_t modified = 0;

    for (std::size_t i = 0; i < triCount; ++i) {
        const std::size_t off = i * sizeof(W3dTriStruct) + offsetof(W3dTriStruct, Attributes);
        uint32_t currentType{};
        std::memcpy(&currentType, buf.data() + off, sizeof(currentType));

        if (fromType >= 0 && currentType != static_cast<uint32_t>(fromType)) {
            continue;
        }

        if (currentType == toType) {
            continue;
        }

        std::memcpy(buf.data() + off, &toType, sizeof(toType));
        ++modified;
    }

    if (modified == 0) {
        updateStats();
        return;
    }

    chunkPtr->length = static_cast<uint32_t>(buf.size());
    updateStats();
    emit chunkEdited();
}

TextureInfoEditorWidget::TextureInfoEditorWidget(QWidget* parent)
    : QWidget(parent) {
    Ui::TextureInfoEditorWidget form;
    form.setupUi(this);
    mipCombo = form.mipCombo;
    hintCombo = form.hintCombo;
    typeCombo = form.typeCombo;
    animCombo = form.animCombo;
    frameCountSpin = form.frameCountSpin;
    frameRateSpin = form.frameRateSpin;
    applyButton = form.applyButton;
    flagControls = {
        { TextureAttrValue(TextureAttr::PUBLISH), form.publishCheck },
        { TextureAttrValue(TextureAttr::RESIZE_OBSOLETE), form.resizeCheck },
        { TextureAttrValue(TextureAttr::NO_LOD), form.noLodCheck },
        { TextureAttrValue(TextureAttr::CLAMP_U), form.clampUCheck },
        { TextureAttrValue(TextureAttr::CLAMP_V), form.clampVCheck },
        { TextureAttrValue(TextureAttr::ALPHA_BITMAP), form.alphaBitmapCheck },
    };

    mipCombo->addItem(tr("All"), static_cast<int>(TextureAttrValue(TextureAttr::MIP_ALL)));
    mipCombo->addItem(tr("2"), static_cast<int>(TextureAttrValue(TextureAttr::MIP_2)));
    mipCombo->addItem(tr("3"), static_cast<int>(TextureAttrValue(TextureAttr::MIP_3)));
    mipCombo->addItem(tr("4"), static_cast<int>(TextureAttrValue(TextureAttr::MIP_4)));

    hintCombo->addItem(tr("Base"), static_cast<int>(TextureAttrValue(TextureAttr::HINT_BASE)));
    hintCombo->addItem(tr("Emissive"), static_cast<int>(TextureAttrValue(TextureAttr::HINT_EMISSIVE)));
    hintCombo->addItem(tr("Environment"), static_cast<int>(TextureAttrValue(TextureAttr::HINT_ENVIRONMENT)));
    hintCombo->addItem(tr("Shiny Mask"), static_cast<int>(TextureAttrValue(TextureAttr::HINT_SHINY_MASK)));

    typeCombo->addItem(tr("Color Map"), static_cast<int>(TextureAttrValue(TextureAttr::TYPE_COLORMAP)));
    typeCombo->addItem(tr("Bump Map"), static_cast<int>(TextureAttrValue(TextureAttr::TYPE_BUMPMAP)));

    animCombo->addItem(tr("Loop"), static_cast<int>(TextureAttrValue(TextureAttr::ANIM_LOOP)));
    animCombo->addItem(tr("Ping Pong"), static_cast<int>(TextureAttrValue(TextureAttr::ANIM_PINGPONG)));
    animCombo->addItem(tr("Once"), static_cast<int>(TextureAttrValue(TextureAttr::ANIM_ONCE)));
    animCombo->addItem(tr("Manual"), static_cast<int>(TextureAttrValue(TextureAttr::ANIM_MANUAL)));
    connect(applyButton, &QPushButton::clicked,
        this, &TextureInfoEditorWidget::applyChanges);
}

void TextureInfoEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    for (auto& ctrl : flagControls) {
        if (ctrl.box) ctrl.box->setChecked(false);
    }
    if (mipCombo) mipCombo->setCurrentIndex(0);
    if (hintCombo) hintCombo->setCurrentIndex(0);
    if (typeCombo) typeCombo->setCurrentIndex(0);
    if (animCombo) animCombo->setCurrentIndex(0);
    if (frameCountSpin) frameCountSpin->setValue(0);
    if (frameRateSpin) frameRateSpin->setValue(0.0);

    if (!chunkPtr) {
        setEnabled(false);
        return;
    }

    auto parsed = ParseChunkStruct<W3dTextureInfoStruct>(chunkPtr);
    if (auto err = std::get_if<std::string>(&parsed)) {
        Q_UNUSED(err);
        setEnabled(false);
        return;
    }

    const auto& data = std::get<W3dTextureInfoStruct>(parsed);
    const uint16_t attr = data.Attributes;
    for (auto& ctrl : flagControls) {
        ctrl.box->setChecked((attr & ctrl.mask) != 0);
    }
    const uint16_t mip = static_cast<uint16_t>(attr & TextureAttrValue(TextureAttr::MIP_MASK));
    const uint16_t hint = static_cast<uint16_t>(attr & TextureAttrValue(TextureAttr::HINT_MASK));
    const uint16_t type = static_cast<uint16_t>(attr & TextureAttrValue(TextureAttr::TYPE_MASK));
    SetComboValue(mipCombo, static_cast<int>(mip));
    SetComboValue(hintCombo, static_cast<int>(hint));
    SetComboValue(typeCombo, static_cast<int>(type));
    SetComboValue(animCombo, static_cast<int>(data.AnimType));
    frameCountSpin->setValue(ClampToInt(data.FrameCount));
    frameRateSpin->setValue(data.FrameRate);

    setEnabled(true);
}

void TextureInfoEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    std::string error;
    const bool ok = W3DEdit::MutateStructChunk<W3dTextureInfoStruct>(
        chunkPtr,
        [&](W3dTextureInfoStruct& data) {
            uint16_t clearMask = 0;
            for (const auto& ctrl : flagControls) {
                clearMask = static_cast<uint16_t>(clearMask | ctrl.mask);
            }
            clearMask = static_cast<uint16_t>(clearMask | TextureAttrValue(TextureAttr::MIP_MASK));
            clearMask = static_cast<uint16_t>(clearMask | TextureAttrValue(TextureAttr::HINT_MASK));
            clearMask = static_cast<uint16_t>(clearMask | TextureAttrValue(TextureAttr::TYPE_MASK));

            uint16_t attr = static_cast<uint16_t>(data.Attributes & ~clearMask);
            for (const auto& ctrl : flagControls) {
                if (ctrl.box->isChecked()) {
                    attr = static_cast<uint16_t>(attr | ctrl.mask);
                }
            }
            const QVariant mipData = mipCombo->currentData();
            const QVariant hintData = hintCombo->currentData();
            const QVariant typeData = typeCombo->currentData();
            if (mipData.isValid()) {
                attr = static_cast<uint16_t>(attr | static_cast<uint16_t>(mipData.toInt()));
            }
            if (hintData.isValid()) {
                attr = static_cast<uint16_t>(attr | static_cast<uint16_t>(hintData.toInt()));
            }
            if (typeData.isValid()) {
                attr = static_cast<uint16_t>(attr | static_cast<uint16_t>(typeData.toInt()));
            }
            data.Attributes = attr;
            const QVariant animData = animCombo->currentData();
            if (animData.isValid()) {
                data.AnimType = static_cast<uint16_t>(animData.toInt());
            }
            data.FrameCount = static_cast<uint32_t>(frameCountSpin->value());
            data.FrameRate = static_cast<float>(frameRateSpin->value());
        },
        &error);

    if (!ok) {
        QMessageBox::warning(this, tr("Error"),
            QString::fromStdString(error.empty() ? "Failed to update texture info." : error));
        return;
    }

    emit chunkEdited();
}

MaterialEditorWidget::MaterialEditorWidget(QWidget* parent)
    : QWidget(parent) {
    Ui::MaterialEditorWidget form;
    form.setupUi(this);
    stage0Combo = form.stage0Combo;
    stage1Combo = form.stage1Combo;
    stage0CodeSpin = form.stage0CodeSpin;
    stage1CodeSpin = form.stage1CodeSpin;
    ambient = { form.ambientRSpin, form.ambientGSpin, form.ambientBSpin };
    diffuse = { form.diffuseRSpin, form.diffuseGSpin, form.diffuseBSpin };
    specular = { form.specularRSpin, form.specularGSpin, form.specularBSpin };
    emissive = { form.emissiveRSpin, form.emissiveGSpin, form.emissiveBSpin };
    shininessSpin = form.shininessSpin;
    opacitySpin = form.opacitySpin;
    translucencySpin = form.translucencySpin;
    applyButton = form.applyButton;

    for (const auto& [mask, name] : VERTMAT_BASIC_FLAGS) {
        auto* box = new QCheckBox(
            QString::fromLatin1(name.data(), static_cast<int>(name.size())),
            form.basicFlagsGroup);
        form.basicFlagsLayout->addWidget(box);
        basicFlagControls.push_back({ static_cast<uint32_t>(mask), box });
    }

    populateStageCombo(stage0Combo, 0);
    populateStageCombo(stage1Combo, 1);

    auto hookStage = [&](QComboBox* combo, QSpinBox* spin, int stage) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, combo, spin, stage](int) {
                const QVariant data = combo->currentData();
                if (!data.isValid()) return;
                int& unknownIndex = (stage == 0) ? stage0UnknownIndex : stage1UnknownIndex;
                setStageMapping(combo, spin, stage, unknownIndex,
                    static_cast<uint8_t>(data.toInt() & 0xFF));
            });
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this, combo, spin, stage](int value) {
                int& unknownIndex = (stage == 0) ? stage0UnknownIndex : stage1UnknownIndex;
                setStageMapping(combo, spin, stage, unknownIndex,
                    static_cast<uint8_t>(value & 0xFF));
            });
        };
    hookStage(stage0Combo, stage0CodeSpin, 0);
    hookStage(stage1Combo, stage1CodeSpin, 1);

    connect(applyButton, &QPushButton::clicked,
        this, &MaterialEditorWidget::applyChanges);
}

void MaterialEditorWidget::populateStageCombo(QComboBox* combo, int stage) {
    combo->clear();
    for (const auto& entry : VERTMAT_STAGE_MAPPING_CODES) {
        combo->addItem(QString::fromStdString(StageMappingName(entry.first, stage)),
            static_cast<int>(entry.first));
    }
}

void MaterialEditorWidget::setStageMapping(QComboBox* combo, QSpinBox* spin, int stage, int& unknownIndex, uint8_t code) {
    const QSignalBlocker blockCombo(combo);
    const QSignalBlocker blockSpin(spin);

    spin->setValue(static_cast<int>(code));

    int idx = combo->findData(static_cast<int>(code));
    if (idx >= 0) {
        if (unknownIndex >= 0 && unknownIndex != idx && unknownIndex < combo->count()) {
            if (unknownIndex < idx) {
                combo->removeItem(unknownIndex);
                --idx;
            }
            else {
                combo->removeItem(unknownIndex);
            }
            unknownIndex = -1;
        }
        combo->setCurrentIndex(idx);
        return;
    }

    const QString label = QString::fromStdString(StageMappingName(code, stage));
    if (unknownIndex >= 0 && unknownIndex < combo->count()) {
        combo->setItemText(unknownIndex, label);
        combo->setItemData(unknownIndex, static_cast<int>(code));
        combo->setCurrentIndex(unknownIndex);
    }
    else {
        unknownIndex = combo->count();
        combo->addItem(label, static_cast<int>(code));
        combo->setCurrentIndex(unknownIndex);
    }
}

void MaterialEditorWidget::setColor(const W3dRGBStruct& src, ColorControls& dest) {
    dest.r->setValue(static_cast<int>(src.R));
    dest.g->setValue(static_cast<int>(src.G));
    dest.b->setValue(static_cast<int>(src.B));
}

void MaterialEditorWidget::readColor(W3dRGBStruct& dest, const ColorControls& src) const {
    dest.R = static_cast<uint8_t>(src.r->value());
    dest.G = static_cast<uint8_t>(src.g->value());
    dest.B = static_cast<uint8_t>(src.b->value());
}

void MaterialEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    if (!chunkPtr) {
        setEnabled(false);
        return;
    }

    auto parsed = ParseChunkStruct<W3dVertexMaterialStruct>(chunkPtr);
    if (auto err = std::get_if<std::string>(&parsed)) {
        Q_UNUSED(err);
        setEnabled(false);
        return;
    }

    const auto& data = std::get<W3dVertexMaterialStruct>(parsed);
    for (auto& ctrl : basicFlagControls) {
        ctrl.box->setChecked((data.Attributes & ctrl.mask) != 0);
    }

    auto applyStage = [&](QComboBox* combo, QSpinBox* spin, int stage, int& unknownIndex) {
        const uint8_t code = ExtractStageMapping(data.Attributes, stage);
        setStageMapping(combo, spin, stage, unknownIndex, code);
    };
    applyStage(stage0Combo, stage0CodeSpin, 0, stage0UnknownIndex);
    applyStage(stage1Combo, stage1CodeSpin, 1, stage1UnknownIndex);

    setColor(data.Ambient, ambient);
    setColor(data.Diffuse, diffuse);
    setColor(data.Specular, specular);
    setColor(data.Emissive, emissive);
    shininessSpin->setValue(data.Shininess);
    opacitySpin->setValue(data.Opacity);
    translucencySpin->setValue(data.Translucency);

    setEnabled(true);
}

void MaterialEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    std::string error;
    const bool ok = W3DEdit::MutateStructChunk<W3dVertexMaterialStruct>(
        chunkPtr,
        [&](W3dVertexMaterialStruct& data) {
            uint32_t basicMask = 0;
            for (const auto& ctrl : basicFlagControls) {
                basicMask |= ctrl.mask;
            }
            uint32_t attr = data.Attributes & ~basicMask;
            attr &= ~VERTMAT_STAGE0_MAPPING_MASK;
            attr &= ~VERTMAT_STAGE1_MAPPING_MASK;

            for (const auto& ctrl : basicFlagControls) {
                if (ctrl.box->isChecked()) {
                    attr |= ctrl.mask;
                }
            }

            const uint32_t stage0 = static_cast<uint32_t>(stage0CodeSpin->value() & 0xFF);
            const uint32_t stage1 = static_cast<uint32_t>(stage1CodeSpin->value() & 0xFF);
            attr |= (stage0 << VERTMAT_STAGE0_MAPPING_SHIFT);
            attr |= (stage1 << VERTMAT_STAGE1_MAPPING_SHIFT);
            data.Attributes = attr;

            readColor(data.Ambient, ambient);
            readColor(data.Diffuse, diffuse);
            readColor(data.Specular, specular);
            readColor(data.Emissive, emissive);
            data.Shininess = static_cast<float>(shininessSpin->value());
            data.Opacity = static_cast<float>(opacitySpin->value());
            data.Translucency = static_cast<float>(translucencySpin->value());
        },
        &error);

    if (!ok) {
        QMessageBox::warning(this, tr("Error"),
            QString::fromStdString(error.empty() ? "Failed to update material." : error));
        return;
    }

    emit chunkEdited();
}


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      ui(std::make_unique<Ui::MainWindow>()) {
    ui->setupUi(this);
    bindDesignerWidgets();

    detailSplitter->setChildrenCollapsible(true);
    detailSplitter->setCollapsible(0, false);
    detailSplitter->setCollapsible(1, true);
    detailSplitter->setStretchFactor(0, 3);
    detailSplitter->setStretchFactor(1, 1);
    createDynamicEditorPages();
    detailSplitter->setSizes({ 800, 180 });
    detailSplitterStateCache = detailSplitter->saveState();

    renderUi = std::make_unique<Ui::RenderPanel>();
    auto* renderPanel = new QWidget(renderPane);
    renderUi->setupUi(renderPanel);
    ui->renderHostLayout->addWidget(renderPanel);

    renderSplitter = renderUi->renderSplitter;
    renderFogToggle = renderUi->renderFogToggle;
    renderLodToggle = renderUi->renderLodToggle;
    renderUvDebugToggle = renderUi->renderUvDebugToggle;
    renderLodLockToggle = renderUi->renderLodLockToggle;
    renderLodLevelSpin = renderUi->renderLodLevelSpin;
    renderCameraGizmoToggle = renderUi->renderCameraGizmoToggle;
    renderPivotMarkersToggle = renderUi->renderPivotMarkersToggle;
    renderLodBiasSpin = renderUi->renderLodBiasSpin;
    renderStatsLabel = renderUi->renderStatsLabel;
    renderSelectionLabel = renderUi->renderSelectionLabel;
    renderTabs = renderUi->renderTabs;
    renderWarningsEdit = renderUi->renderWarningsEdit;

    renderViewport = new OW3D::Render::RenderViewportWidget(renderUi->renderViewportHost);
    renderViewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    renderViewport->setMinimumHeight(180);
    renderUi->renderViewportHostLayout->addWidget(renderViewport);

    auto* lodLevelLabel = renderUi->lodLevelLabel;
    auto* renderAnimationTab = renderUi->renderAnimationTab;
    auto* renderAnimationLayout = renderUi->renderAnimationLayout;

    auto* renderAssetButtons = new QWidget(renderAnimationTab);
    auto* renderAssetButtonsLayout = new QHBoxLayout(renderAssetButtons);
    renderAssetButtonsLayout->setContentsMargins(0, 0, 0, 0);
    renderAssetButtonsLayout->setSpacing(6);
    renderAddSkeletonButton = new QPushButton(tr("Add Model/Skeleton..."), renderAssetButtons);
    renderAddAnimationsButton = new QPushButton(tr("Add Animations..."), renderAssetButtons);
    renderRemoveAssetButton = new QPushButton(tr("Remove Selected Asset"), renderAssetButtons);
    renderClearAnimationsButton = new QPushButton(tr("Clear Animation Libraries"), renderAssetButtons);
    renderAssetButtonsLayout->addWidget(renderAddSkeletonButton);
    renderAssetButtonsLayout->addWidget(renderAddAnimationsButton);
    renderAssetButtonsLayout->addWidget(renderRemoveAssetButton);
    renderAssetButtonsLayout->addWidget(renderClearAnimationsButton);
    renderAssetButtonsLayout->addStretch(1);
    renderAnimationLayout->addWidget(renderAssetButtons);

    auto* renderAssetsLabel = new QLabel(tr("Loaded Render Assets"), renderAnimationTab);
    renderAnimationLayout->addWidget(renderAssetsLabel);

    renderAssetsTree = new QTreeWidget(renderAnimationTab);
    renderAssetsTree->setColumnCount(4);
    renderAssetsTree->setHeaderLabels({ tr("Role"), tr("File"), tr("Hierarchies"), tr("Clips") });
    renderAssetsTree->setRootIsDecorated(false);
    renderAssetsTree->setAlternatingRowColors(true);
    renderAssetsTree->setSelectionMode(QAbstractItemView::SingleSelection);
    renderAssetsTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    renderAssetsTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    renderAssetsTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    renderAssetsTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    renderAnimationLayout->addWidget(renderAssetsTree, 1);

    auto* renderAnimationsLabel = new QLabel(tr("Animations"), renderAnimationTab);
    renderAnimationLayout->addWidget(renderAnimationsLabel);

    renderAnimationsTree = new QTreeWidget(renderAnimationTab);
    renderAnimationsTree->setColumnCount(3);
    renderAnimationsTree->setHeaderLabels({ tr("Clip"), tr("Source"), tr("Status") });
    renderAnimationsTree->setAlternatingRowColors(true);
    renderAnimationsTree->setSelectionMode(QAbstractItemView::SingleSelection);
    renderAnimationsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    renderAnimationsTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    renderAnimationsTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    renderAnimationLayout->addWidget(renderAnimationsTree, 2);

    auto* renderTransportRow = new QWidget(renderAnimationTab);
    auto* renderTransportLayout = new QHBoxLayout(renderTransportRow);
    renderTransportLayout->setContentsMargins(0, 0, 0, 0);
    renderTransportLayout->setSpacing(6);
    renderPlayPauseButton = new QPushButton(tr("Play"), renderTransportRow);
    renderStopButton = new QPushButton(tr("Stop"), renderTransportRow);
    renderAnimationExportGifButton = new QPushButton(tr("Export GIF..."), renderTransportRow);
    renderAnimationResetDraftButton = new QPushButton(tr("Reset Draft"), renderTransportRow);
    renderAnimationLoopToggle = new QCheckBox(tr("Loop"), renderTransportRow);
    renderAnimationLoopToggle->setChecked(true);
    renderAnimationEditKeysToggle = new QCheckBox(tr("Edit Keys"), renderTransportRow);
    auto* renderSpeedLabel = new QLabel(tr("Speed"), renderTransportRow);
    renderAnimationSpeedSpin = new QDoubleSpinBox(renderTransportRow);
    renderAnimationSpeedSpin->setDecimals(2);
    renderAnimationSpeedSpin->setRange(0.10, 8.00);
    renderAnimationSpeedSpin->setSingleStep(0.10);
    renderAnimationSpeedSpin->setValue(1.0);
    renderTransportLayout->addWidget(renderPlayPauseButton);
    renderTransportLayout->addWidget(renderStopButton);
    renderTransportLayout->addWidget(renderAnimationExportGifButton);
    renderTransportLayout->addWidget(renderAnimationResetDraftButton);
    renderTransportLayout->addWidget(renderAnimationLoopToggle);
    renderTransportLayout->addWidget(renderAnimationEditKeysToggle);
    renderTransportLayout->addWidget(renderSpeedLabel);
    renderTransportLayout->addWidget(renderAnimationSpeedSpin);
    renderTransportLayout->addStretch(1);
    renderAnimationLayout->addWidget(renderTransportRow);

    renderAnimationClipLabel = new QLabel(tr("Clip: none"), renderAnimationTab);
    renderAnimationMetadataLabel = new QLabel(tr("No animation selected."), renderAnimationTab);
    renderAnimationEditStatusLabel = new QLabel(tr("Edit Keys: no clip selected."), renderAnimationTab);
    renderAnimationMetadataLabel->setWordWrap(true);
    renderAnimationEditStatusLabel->setWordWrap(true);
    renderAnimationLayout->addWidget(renderAnimationClipLabel);
    renderAnimationLayout->addWidget(renderAnimationMetadataLabel);
    renderAnimationLayout->addWidget(renderAnimationEditStatusLabel);

    auto* renderFrameRow = new QWidget(renderAnimationTab);
    auto* renderFrameLayout = new QHBoxLayout(renderFrameRow);
    renderFrameLayout->setContentsMargins(0, 0, 0, 0);
    renderFrameLayout->setSpacing(8);

    renderAnimationFrameSlider = new QSlider(Qt::Horizontal, renderFrameRow);
    renderAnimationFrameSlider->setRange(0, 0);
    renderAnimationFrameSlider->setEnabled(false);
    renderFrameLayout->addWidget(renderAnimationFrameSlider, 1);

    renderAnimationFrameLabel = new QLabel(tr("Frame 0 / 0"), renderFrameRow);
    renderAnimationFrameLabel->setMinimumWidth(92);
    renderAnimationFrameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    renderFrameLayout->addWidget(renderAnimationFrameLabel);

    renderAnimationLayout->addWidget(renderFrameRow);

    auto* renderAnimationEditorTabs = new QTabWidget(renderAnimationTab);
    renderAnimationEditorTabs->setDocumentMode(true);
    renderAnimationLayout->addWidget(renderAnimationEditorTabs, 1);

    auto* renderPrepTab = new QWidget(renderAnimationEditorTabs);
    auto* renderPrepTabLayout = new QVBoxLayout(renderPrepTab);
    renderPrepTabLayout->setContentsMargins(0, 0, 0, 0);
    renderPrepTabLayout->setSpacing(0);
    renderAnimationPrepGroup = new QGroupBox(tr("Clip Prep"), renderPrepTab);
    auto* renderPrepLayout = new QVBoxLayout(renderAnimationPrepGroup);
    renderPrepLayout->setContentsMargins(6, 6, 6, 6);
    renderPrepLayout->setSpacing(6);

    auto* renderPrepSourceRow = new QWidget(renderAnimationPrepGroup);
    auto* renderPrepSourceLayout = new QHBoxLayout(renderPrepSourceRow);
    renderPrepSourceLayout->setContentsMargins(0, 0, 0, 0);
    renderPrepSourceLayout->setSpacing(6);
    auto* renderPrepSourceLabel = new QLabel(tr("Match Timing To"), renderPrepSourceRow);
    renderAnimationPrepSourceCombo = new QComboBox(renderPrepSourceRow);
    renderAnimationPrepSourceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    renderPrepSourceLayout->addWidget(renderPrepSourceLabel);
    renderPrepSourceLayout->addWidget(renderAnimationPrepSourceCombo, 1);
    renderPrepLayout->addWidget(renderPrepSourceRow);

    auto* renderPrepPoseRow = new QWidget(renderAnimationPrepGroup);
    auto* renderPrepPoseLayout = new QHBoxLayout(renderPrepPoseRow);
    renderPrepPoseLayout->setContentsMargins(0, 0, 0, 0);
    renderPrepPoseLayout->setSpacing(6);
    auto* renderPrepPoseLabel = new QLabel(tr("Static Pose Frame"), renderPrepPoseRow);
    renderAnimationPrepStaticPoseFrameSpin = new QSpinBox(renderPrepPoseRow);
    renderAnimationPrepStaticPoseFrameSpin->setRange(0, 0);
    renderPrepPoseLayout->addWidget(renderPrepPoseLabel);
    renderPrepPoseLayout->addWidget(renderAnimationPrepStaticPoseFrameSpin);
    renderPrepPoseLayout->addStretch(1);
    renderPrepLayout->addWidget(renderPrepPoseRow);

    auto* renderPrepActionRow = new QWidget(renderAnimationPrepGroup);
    auto* renderPrepActionLayout = new QHBoxLayout(renderPrepActionRow);
    renderPrepActionLayout->setContentsMargins(0, 0, 0, 0);
    renderPrepActionLayout->setSpacing(6);
    renderAnimationPrepApplyButton = new QPushButton(tr("Apply To Draft"), renderPrepActionRow);
    renderAnimationPrepFreezePoseButton = new QPushButton(tr("Apply Pose To All Frames"), renderPrepActionRow);
    renderAnimationPrepFitSourceButton = new QPushButton(tr("Fit Source To Clip"), renderPrepActionRow);
    renderPrepActionLayout->addWidget(renderAnimationPrepApplyButton);
    renderPrepActionLayout->addWidget(renderAnimationPrepFreezePoseButton);
    renderPrepActionLayout->addWidget(renderAnimationPrepFitSourceButton);
    renderPrepActionLayout->addStretch(1);
    renderPrepLayout->addWidget(renderPrepActionRow);

    renderAnimationPrepStatusLabel = new QLabel(
        tr("Select a writable base clip and a compatible source clip to conform the base timing."),
        renderAnimationPrepGroup);
    renderAnimationPrepStatusLabel->setWordWrap(true);
    renderPrepLayout->addWidget(renderAnimationPrepStatusLabel);
    renderPrepTabLayout->addWidget(renderAnimationPrepGroup);
    renderPrepTabLayout->addStretch(1);
    renderAnimationEditorTabs->addTab(renderPrepTab, tr("Clip Prep"));

    auto* renderBlendTab = new QWidget(renderAnimationEditorTabs);
    auto* renderBlendTabLayout = new QVBoxLayout(renderBlendTab);
    renderBlendTabLayout->setContentsMargins(0, 0, 0, 0);
    renderBlendTabLayout->setSpacing(0);
    renderAnimationBlendGroup = new QGroupBox(tr("Pivot Override"), renderBlendTab);
    auto* renderBlendLayout = new QVBoxLayout(renderAnimationBlendGroup);
    renderBlendLayout->setContentsMargins(6, 6, 6, 6);
    renderBlendLayout->setSpacing(6);

    auto* renderBlendSourceRow = new QWidget(renderAnimationBlendGroup);
    auto* renderBlendSourceLayout = new QHBoxLayout(renderBlendSourceRow);
    renderBlendSourceLayout->setContentsMargins(0, 0, 0, 0);
    renderBlendSourceLayout->setSpacing(6);
    auto* renderBlendSourceLabel = new QLabel(tr("Blend From"), renderBlendSourceRow);
    renderAnimationBlendSourceCombo = new QComboBox(renderBlendSourceRow);
    renderAnimationBlendSourceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    renderBlendSourceLayout->addWidget(renderBlendSourceLabel);
    renderBlendSourceLayout->addWidget(renderAnimationBlendSourceCombo, 1);
    renderBlendLayout->addWidget(renderBlendSourceRow);

    auto* renderBlendTimingRow = new QWidget(renderAnimationBlendGroup);
    auto* renderBlendTimingLayout = new QHBoxLayout(renderBlendTimingRow);
    renderBlendTimingLayout->setContentsMargins(0, 0, 0, 0);
    renderBlendTimingLayout->setSpacing(6);
    auto* renderBlendTimingLabel = new QLabel(tr("Timing"), renderBlendTimingRow);
    renderAnimationBlendTimingCombo = new QComboBox(renderBlendTimingRow);
    renderAnimationBlendTimingCombo->addItem(tr("Preserve Source Rate"), 0);
    renderAnimationBlendTimingCombo->addItem(tr("Fit Full Source Clip"), 1);
    renderBlendTimingLayout->addWidget(renderBlendTimingLabel);
    renderBlendTimingLayout->addWidget(renderAnimationBlendTimingCombo, 1);
    renderBlendLayout->addWidget(renderBlendTimingRow);

    renderAnimationBlendPivotTree = new QTreeWidget(renderAnimationBlendGroup);
    renderAnimationBlendPivotTree->setColumnCount(1);
    renderAnimationBlendPivotTree->setHeaderHidden(true);
    renderAnimationBlendPivotTree->setAlternatingRowColors(false);
    renderAnimationBlendPivotTree->setSelectionMode(QAbstractItemView::SingleSelection);
    renderAnimationBlendPivotTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    renderAnimationBlendPivotTree->setRootIsDecorated(true);
    renderAnimationBlendPivotTree->setUniformRowHeights(true);
    renderAnimationBlendPivotTree->setMouseTracking(false);
    renderAnimationBlendPivotTree->viewport()->setAttribute(Qt::WA_Hover, false);
    renderAnimationBlendPivotTree->setStyleSheet(
        QStringLiteral("QTreeView::item:hover { background: transparent; }"));
    renderAnimationBlendPivotTree->setMaximumHeight(180);
    renderBlendLayout->addWidget(renderAnimationBlendPivotTree);

    renderAnimationBlendPivotWeightsGroup =
        new QGroupBox(tr("Selected Pivot Axis Weights"), renderAnimationBlendGroup);
    auto* renderBlendPivotWeightsLayout =
        new QVBoxLayout(renderAnimationBlendPivotWeightsGroup);
    renderBlendPivotWeightsLayout->setContentsMargins(6, 6, 6, 6);
    renderBlendPivotWeightsLayout->setSpacing(6);
    renderAnimationBlendPivotWeightsLabel = new QLabel(
        tr("Select a pivot row above to override its per-axis weights. Uncustomized pivots use the default percentages below."),
        renderAnimationBlendPivotWeightsGroup);
    renderAnimationBlendPivotWeightsLabel->setWordWrap(true);
    renderBlendPivotWeightsLayout->addWidget(renderAnimationBlendPivotWeightsLabel);
    auto* renderBlendPivotWeightsGrid = new QGridLayout();
    renderBlendPivotWeightsGrid->setContentsMargins(0, 0, 0, 0);
    renderBlendPivotWeightsGrid->setHorizontalSpacing(6);
    renderBlendPivotWeightsGrid->setVerticalSpacing(4);
    const QStringList renderBlendAxisLabels{
        tr("Pos X"),
        tr("Pos Y"),
        tr("Pos Z"),
        tr("Rot X"),
        tr("Rot Y"),
        tr("Rot Z")
    };
    for (int axisIndex = 0; axisIndex < kRenderBlendAxisCount; ++axisIndex) {
        auto* axisLabel =
            new QLabel(renderBlendAxisLabels[static_cast<qsizetype>(axisIndex)], renderAnimationBlendPivotWeightsGroup);
        auto* axisSpin = new QSpinBox(renderAnimationBlendPivotWeightsGroup);
        axisSpin->setRange(0, 100);
        axisSpin->setSingleStep(5);
        axisSpin->setSuffix(QStringLiteral("%"));
        axisSpin->setValue(100);
        renderAnimationBlendPivotAxisPercentSpins[static_cast<std::size_t>(axisIndex)] = axisSpin;
        renderBlendPivotWeightsGrid->addWidget(axisLabel, axisIndex / 3, (axisIndex % 3) * 2);
        renderBlendPivotWeightsGrid->addWidget(axisSpin, axisIndex / 3, (axisIndex % 3) * 2 + 1);
    }
    renderBlendPivotWeightsLayout->addLayout(renderBlendPivotWeightsGrid);
    auto* renderBlendPivotWeightsActionRow = new QWidget(renderAnimationBlendPivotWeightsGroup);
    auto* renderBlendPivotWeightsActionLayout = new QHBoxLayout(renderBlendPivotWeightsActionRow);
    renderBlendPivotWeightsActionLayout->setContentsMargins(0, 0, 0, 0);
    renderBlendPivotWeightsActionLayout->setSpacing(6);
    renderAnimationBlendResetPivotWeightsButton =
        new QPushButton(tr("Use Defaults For Selected Pivot"), renderBlendPivotWeightsActionRow);
    renderBlendPivotWeightsActionLayout->addWidget(renderAnimationBlendResetPivotWeightsButton);
    renderBlendPivotWeightsActionLayout->addStretch(1);
    renderBlendPivotWeightsLayout->addWidget(renderBlendPivotWeightsActionRow);
    renderBlendLayout->addWidget(renderAnimationBlendPivotWeightsGroup);

    auto* renderBlendAmountRow = new QWidget(renderAnimationBlendGroup);
    auto* renderBlendAmountLayout = new QHBoxLayout(renderBlendAmountRow);
    renderBlendAmountLayout->setContentsMargins(0, 0, 0, 0);
    renderBlendAmountLayout->setSpacing(6);
    auto* renderBlendPositionPercentLabel = new QLabel(tr("Default Position %"), renderBlendAmountRow);
    renderAnimationBlendTranslationPercentSpin = new QSpinBox(renderBlendAmountRow);
    renderAnimationBlendTranslationPercentSpin->setRange(0, 100);
    renderAnimationBlendTranslationPercentSpin->setSingleStep(5);
    renderAnimationBlendTranslationPercentSpin->setSuffix(QStringLiteral("%"));
    renderAnimationBlendTranslationPercentSpin->setValue(100);
    auto* renderBlendRotationPercentLabel = new QLabel(tr("Default Rotation %"), renderBlendAmountRow);
    renderAnimationBlendRotationPercentSpin = new QSpinBox(renderBlendAmountRow);
    renderAnimationBlendRotationPercentSpin->setRange(0, 100);
    renderAnimationBlendRotationPercentSpin->setSingleStep(5);
    renderAnimationBlendRotationPercentSpin->setSuffix(QStringLiteral("%"));
    renderAnimationBlendRotationPercentSpin->setValue(100);
    renderBlendAmountLayout->addWidget(renderBlendPositionPercentLabel);
    renderBlendAmountLayout->addWidget(renderAnimationBlendTranslationPercentSpin);
    renderBlendAmountLayout->addWidget(renderBlendRotationPercentLabel);
    renderBlendAmountLayout->addWidget(renderAnimationBlendRotationPercentSpin);
    renderBlendAmountLayout->addStretch(1);
    renderBlendLayout->addWidget(renderBlendAmountRow);

    auto* renderBlendOptionsRow = new QWidget(renderAnimationBlendGroup);
    auto* renderBlendOptionsLayout = new QHBoxLayout(renderBlendOptionsRow);
    renderBlendOptionsLayout->setContentsMargins(0, 0, 0, 0);
    renderBlendOptionsLayout->setSpacing(6);
    renderAnimationBlendIncludeDescendantsToggle =
        new QCheckBox(tr("Include Descendants"), renderBlendOptionsRow);
    renderAnimationBlendIncludeDescendantsToggle->setChecked(true);
    renderBlendOptionsLayout->addWidget(renderAnimationBlendIncludeDescendantsToggle);
    renderAnimationBlendTruncateToggle =
        new QCheckBox(tr("Truncate To Shorter Clip"), renderBlendOptionsRow);
    renderAnimationBlendTruncateToggle->setChecked(false);
    renderBlendOptionsLayout->addWidget(renderAnimationBlendTruncateToggle);
    renderAnimationBlendNormalizeFrameRateToggle =
        new QCheckBox(tr("Normalize Base To Source FPS"), renderBlendOptionsRow);
    renderAnimationBlendNormalizeFrameRateToggle->setChecked(false);
    renderBlendOptionsLayout->addWidget(renderAnimationBlendNormalizeFrameRateToggle);
    renderBlendOptionsLayout->addStretch(1);
    renderBlendLayout->addWidget(renderBlendOptionsRow);

    auto* renderBlendHandPinRow = new QWidget(renderAnimationBlendGroup);
    auto* renderBlendHandPinLayout = new QHBoxLayout(renderBlendHandPinRow);
    renderBlendHandPinLayout->setContentsMargins(0, 0, 0, 0);
    renderBlendHandPinLayout->setSpacing(6);
    renderAnimationBlendPinLeftHandToggle =
        new QCheckBox(tr("Pin Left Hand"), renderBlendHandPinRow);
    renderAnimationBlendPinLeftHandToggle->setChecked(false);
    renderBlendHandPinLayout->addWidget(renderAnimationBlendPinLeftHandToggle);
    renderAnimationBlendPinRightHandToggle =
        new QCheckBox(tr("Pin Right Hand"), renderBlendHandPinRow);
    renderAnimationBlendPinRightHandToggle->setChecked(false);
    renderBlendHandPinLayout->addWidget(renderAnimationBlendPinRightHandToggle);
    auto* renderBlendHandPinReferenceLabel =
        new QLabel(tr("Reference Frame"), renderBlendHandPinRow);
    renderAnimationBlendHandPinReferenceFrameSpin =
        new QSpinBox(renderBlendHandPinRow);
    renderAnimationBlendHandPinReferenceFrameSpin->setRange(0, 0);
    renderBlendHandPinLayout->addWidget(renderBlendHandPinReferenceLabel);
    renderBlendHandPinLayout->addWidget(renderAnimationBlendHandPinReferenceFrameSpin);
    renderBlendHandPinLayout->addStretch(1);
    renderBlendLayout->addWidget(renderBlendHandPinRow);

    auto* renderBlendRangeRow = new QWidget(renderAnimationBlendGroup);
    auto* renderBlendRangeLayout = new QHBoxLayout(renderBlendRangeRow);
    renderBlendRangeLayout->setContentsMargins(0, 0, 0, 0);
    renderBlendRangeLayout->setSpacing(6);
    auto* renderBlendStartLabel = new QLabel(tr("Start Frame"), renderBlendRangeRow);
    renderAnimationBlendStartFrameSpin = new QSpinBox(renderBlendRangeRow);
    renderAnimationBlendStartFrameSpin->setRange(0, 0);
    auto* renderBlendEndLabel = new QLabel(tr("End Frame"), renderBlendRangeRow);
    renderAnimationBlendEndFrameSpin = new QSpinBox(renderBlendRangeRow);
    renderAnimationBlendEndFrameSpin->setRange(0, 0);
    renderBlendRangeLayout->addWidget(renderBlendStartLabel);
    renderBlendRangeLayout->addWidget(renderAnimationBlendStartFrameSpin);
    renderBlendRangeLayout->addWidget(renderBlendEndLabel);
    renderBlendRangeLayout->addWidget(renderAnimationBlendEndFrameSpin);
    renderBlendRangeLayout->addStretch(1);
    renderBlendLayout->addWidget(renderBlendRangeRow);

    auto* renderBlendActionRow = new QWidget(renderAnimationBlendGroup);
    auto* renderBlendActionLayout = new QHBoxLayout(renderBlendActionRow);
    renderBlendActionLayout->setContentsMargins(0, 0, 0, 0);
    renderBlendActionLayout->setSpacing(6);
    renderAnimationBlendApplyButton = new QPushButton(tr("Apply To Draft"), renderBlendActionRow);
    renderBlendActionLayout->addWidget(renderAnimationBlendApplyButton);
    renderBlendActionLayout->addStretch(1);
    renderBlendLayout->addWidget(renderBlendActionRow);

    renderAnimationBlendStatusLabel = new QLabel(
        tr("Select a writable base clip, a source clip, and one or more pivots."),
        renderAnimationBlendGroup);
    renderAnimationBlendStatusLabel->setWordWrap(true);
    renderBlendLayout->addWidget(renderAnimationBlendStatusLabel);
    renderBlendTabLayout->addWidget(renderAnimationBlendGroup);
    renderBlendTabLayout->addStretch(1);
    renderAnimationEditorTabs->addTab(renderBlendTab, tr("Pivot Override"));

    auto* renderPivotSheetTab = new QWidget(renderAnimationEditorTabs);
    auto* renderPivotSheetTabLayout = new QVBoxLayout(renderPivotSheetTab);
    renderPivotSheetTabLayout->setContentsMargins(0, 0, 0, 0);
    renderPivotSheetTabLayout->setSpacing(0);
    renderAnimationPivotSheetGroup = new QGroupBox(tr("Selected Pivot Sheet"), renderPivotSheetTab);
    auto* renderPivotSheetLayout = new QVBoxLayout(renderAnimationPivotSheetGroup);
    renderPivotSheetLayout->setContentsMargins(6, 6, 6, 6);
    renderPivotSheetLayout->setSpacing(6);

    renderAnimationPivotSheetLabel = new QLabel(
        tr("Select a pivot in the viewport or Scene Browser to inspect sampled transform values."),
        renderAnimationPivotSheetGroup);
    renderAnimationPivotSheetLabel->setWordWrap(true);
    renderPivotSheetLayout->addWidget(renderAnimationPivotSheetLabel);

    renderAnimationPivotSheetStatusLabel = new QLabel(
        tr("The sheet shows per-frame local translation and Euler rotation for the selected pivot. Euler angles can wrap around at +/-180 degrees."),
        renderAnimationPivotSheetGroup);
    renderAnimationPivotSheetStatusLabel->setWordWrap(true);
    renderPivotSheetLayout->addWidget(renderAnimationPivotSheetStatusLabel);

    renderAnimationPivotSheetTable = new QTableWidget(renderAnimationPivotSheetGroup);
    renderAnimationPivotSheetTable->setColumnCount(8);
    renderAnimationPivotSheetTable->setHorizontalHeaderLabels({
        tr("Frame"),
        tr("Src Key"),
        tr("Pos X"),
        tr("Pos Y"),
        tr("Pos Z"),
        tr("Rot X"),
        tr("Rot Y"),
        tr("Rot Z")
    });
    renderAnimationPivotSheetTable->setAlternatingRowColors(true);
    renderAnimationPivotSheetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    renderAnimationPivotSheetTable->setSelectionMode(QAbstractItemView::SingleSelection);
    renderAnimationPivotSheetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    renderAnimationPivotSheetTable->setWordWrap(false);
    renderAnimationPivotSheetTable->verticalHeader()->setVisible(false);
    renderAnimationPivotSheetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    renderAnimationPivotSheetTable->horizontalHeader()->setStretchLastSection(true);
    renderAnimationPivotSheetTable->setMinimumHeight(220);
    renderPivotSheetLayout->addWidget(renderAnimationPivotSheetTable);

    renderPivotSheetTabLayout->addWidget(renderAnimationPivotSheetGroup);
    renderPivotSheetTabLayout->addStretch(1);
    renderAnimationEditorTabs->addTab(renderPivotSheetTab, tr("Pivot Sheet"));

    renderTabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    renderTabs->setMinimumHeight(0);
    renderSplitter->setStretchFactor(0, 5);
    renderSplitter->setStretchFactor(1, 2);
    renderSplitter->setSizes({ 700, 240 });

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 2);
    splitter->setChildrenCollapsible(false);
    splitter->setSizes({ 900, 340, 560 });

    chunkData = std::make_unique<ChunkData>();

    connect(meshEditor, &MeshEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(meshEditor, &MeshEditorWidget::meshRenamed, this, &MainWindow::onMeshRenamed);
    connect(meshUserTextEditor, &RawTextEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(textureNameEditor, &StringEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(textureInfoEditor, &TextureInfoEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(hierarchyHeaderEditor, &HierarchyHeaderEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(animationHierarchyEditor, &AnimationHierarchyEditorWidget::headerRenamed, this, &MainWindow::onAnimationHeaderRenamed);
    connect(animationHierarchyEditor, &AnimationHierarchyEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(materialNameEditor, &StringEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(transformNodeEditor, &TransformNodeEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(stage0ArgsEditor, &MapperArgsEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(stage1ArgsEditor, &MapperArgsEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(materialEditor, &MaterialEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(shaderEditor, &ShaderEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(surfaceTypeEditor, &SurfaceTypeEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(triangleSurfaceTypeEditor, &TriangleSurfaceTypeEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(renderFogToggle, &QCheckBox::toggled, this, [this](bool) {
        applyRenderSettingsToViewport();
        });
    connect(renderLodToggle, &QCheckBox::toggled, this, [this](bool) {
        applyRenderSettingsToViewport();
        });
    connect(renderUvDebugToggle, &QCheckBox::toggled, this, [this](bool) {
        applyRenderSettingsToViewport();
        });
    connect(renderLodLockToggle, &QCheckBox::toggled, this, [this, lodLevelLabel](bool on) {
        if (renderLodLevelSpin) {
            renderLodLevelSpin->setEnabled(on);
        }
        if (lodLevelLabel) {
            lodLevelLabel->setEnabled(on);
        }
        applyRenderSettingsToViewport();
        });
    connect(renderLodLevelSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        applyRenderSettingsToViewport();
        });
    connect(renderCameraGizmoToggle, &QCheckBox::toggled, this, [this](bool) {
        applyRenderSettingsToViewport();
        });
    connect(renderPivotMarkersToggle, &QCheckBox::toggled, this, [this](bool) {
        applyRenderSettingsToViewport();
        });
    connect(renderLodBiasSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        applyRenderSettingsToViewport();
        });
    connect(renderViewport, &OW3D::Render::RenderViewportWidget::frameStatsChanged, this, [this](const QString& text) {
        if (renderStatsLabel) {
            renderStatsLabel->setText(text);
        }
        });
    connect(renderViewport, &OW3D::Render::RenderViewportWidget::sceneWarningsChanged, this, [this](const QStringList& lines) {
        if (!renderWarningsEdit) {
            return;
        }
        if (lines.isEmpty()) {
            renderWarningsEdit->setPlainText(tr("No scene warnings."));
            return;
        }
        renderWarningsEdit->setPlainText(lines.join('\n'));
        });
    connect(renderViewport, &OW3D::Render::RenderViewportWidget::sceneChunkActivated, this, &MainWindow::handleViewportChunkActivated);
    connect(
        renderViewport,
        &OW3D::Render::RenderViewportWidget::selectionStatusChanged,
        this,
        [this](const QString& text) {
            if (renderSelectionLabel) {
                renderSelectionLabel->setText(text);
            }
        });
    connect(
        renderViewport,
        &OW3D::Render::RenderViewportWidget::pivotSelectionChanged,
        this,
        &MainWindow::handleViewportPivotSelectionChanged);
    connect(
        renderViewport,
        &OW3D::Render::RenderViewportWidget::pivotTransformCommitRequested,
        this,
        &MainWindow::handleViewportPivotTransformCommit);
    connect(
        renderViewport,
        &OW3D::Render::RenderViewportWidget::animationKeyframeCommitRequested,
        this,
        &MainWindow::handleViewportAnimationKeyframeCommit);
    connect(
        renderViewport,
        &OW3D::Render::RenderViewportWidget::animationKeyframeDeleteRequested,
        this,
        &MainWindow::handleViewportAnimationKeyframeDelete);
    connect(
        renderViewport,
        &OW3D::Render::RenderViewportWidget::animationPlaybackPauseRequested,
        this,
        &MainWindow::handleViewportAnimationPlaybackPauseRequested);
    connect(renderAddSkeletonButton, &QPushButton::clicked, this, &MainWindow::addRenderSkeletons);
    connect(renderAddAnimationsButton, &QPushButton::clicked, this, &MainWindow::addRenderAnimations);
    connect(renderRemoveAssetButton, &QPushButton::clicked, this, &MainWindow::removeSelectedRenderSessionAsset);
    connect(renderClearAnimationsButton, &QPushButton::clicked, this, &MainWindow::clearRenderAnimationLibraries);
    connect(renderAnimationsTree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::handleRenderAnimationSelectionChanged);
    connect(renderAssetsTree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        if (!renderRemoveAssetButton || !renderAssetsTree) {
            return;
        }
        const QTreeWidgetItem* item = renderAssetsTree->currentItem();
        const bool removable = item && item->data(0, Qt::UserRole).isValid();
        renderRemoveAssetButton->setEnabled(removable);
    });
    connect(renderPlayPauseButton, &QPushButton::clicked, this, &MainWindow::toggleRenderAnimationPlayback);
    connect(renderStopButton, &QPushButton::clicked, this, &MainWindow::stopRenderAnimationPlayback);
    connect(renderAnimationExportGifButton, &QPushButton::clicked, this, &MainWindow::exportActiveRenderAnimationGif);
    connect(renderAnimationResetDraftButton, &QPushButton::clicked, this, &MainWindow::resetActiveRenderAnimationDraft);
    connect(renderAnimationLoopToggle, &QCheckBox::toggled, this, &MainWindow::handleRenderAnimationLoopChanged);
    connect(renderAnimationEditKeysToggle, &QCheckBox::toggled, this, &MainWindow::handleRenderAnimationEditKeysChanged);
    connect(
        renderAnimationPivotSheetTable,
        &QTableWidget::cellClicked,
        this,
        &MainWindow::handleRenderAnimationPivotSheetCellClicked);
    connect(renderAnimationPrepSourceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::handleRenderPrepSourceClipChanged);
    connect(renderAnimationPrepStaticPoseFrameSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::handleRenderPrepStaticPoseFrameChanged);
    connect(renderAnimationPrepApplyButton, &QPushButton::clicked, this, &MainWindow::applyRenderAnimationClipPrep);
    connect(renderAnimationPrepFreezePoseButton, &QPushButton::clicked, this, &MainWindow::applyRenderAnimationStaticPoseToClip);
    connect(renderAnimationPrepFitSourceButton, &QPushButton::clicked, this, &MainWindow::applyRenderAnimationFitSourceToClip);
    connect(renderAnimationBlendSourceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::handleRenderBlendSourceClipChanged);
    connect(renderAnimationBlendTimingCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::handleRenderBlendTimingModeChanged);
    connect(renderAnimationBlendPivotTree, &QTreeWidget::itemChanged, this, &MainWindow::handleRenderBlendPivotItemChanged);
    connect(
        renderAnimationBlendPivotTree,
        &QTreeWidget::currentItemChanged,
        this,
        &MainWindow::handleRenderBlendPivotCurrentItemChanged);
    connect(
        renderAnimationBlendTranslationPercentSpin,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        &MainWindow::handleRenderBlendTranslationPercentChanged);
    connect(
        renderAnimationBlendRotationPercentSpin,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        &MainWindow::handleRenderBlendRotationPercentChanged);
    for (QSpinBox* axisSpin : renderAnimationBlendPivotAxisPercentSpins) {
        connect(
            axisSpin,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            &MainWindow::handleRenderBlendPivotAxisWeightChanged);
    }
    connect(
        renderAnimationBlendResetPivotWeightsButton,
        &QPushButton::clicked,
        this,
        &MainWindow::resetRenderBlendSelectedPivotWeights);
    connect(renderAnimationBlendIncludeDescendantsToggle, &QCheckBox::toggled, this, &MainWindow::handleRenderBlendIncludeDescendantsChanged);
    connect(renderAnimationBlendTruncateToggle, &QCheckBox::toggled, this, &MainWindow::handleRenderBlendTruncateChanged);
    connect(renderAnimationBlendNormalizeFrameRateToggle, &QCheckBox::toggled, this, &MainWindow::handleRenderBlendNormalizeFrameRateChanged);
    connect(renderAnimationBlendPinLeftHandToggle, &QCheckBox::toggled, this, &MainWindow::handleRenderBlendPinLeftHandChanged);
    connect(renderAnimationBlendPinRightHandToggle, &QCheckBox::toggled, this, &MainWindow::handleRenderBlendPinRightHandChanged);
    connect(renderAnimationBlendHandPinReferenceFrameSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::handleRenderBlendHandPinReferenceFrameChanged);
    connect(renderAnimationBlendStartFrameSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::handleRenderBlendStartFrameChanged);
    connect(renderAnimationBlendEndFrameSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::handleRenderBlendEndFrameChanged);
    connect(renderAnimationBlendApplyButton, &QPushButton::clicked, this, &MainWindow::applyRenderAnimationPivotOverride);
    connect(renderAnimationSpeedSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::handleRenderAnimationSpeedChanged);
    connect(renderAnimationFrameSlider, &QSlider::valueChanged, this, &MainWindow::handleRenderAnimationFrameSliderChanged);
    connect(rawHexToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (rawHexContainer) rawHexContainer->setVisible(on);
        updateRawHex(currentChunk);
        });
    renderAnimationPlaybackTimer = new QTimer(this);
    renderAnimationPlaybackTimer->setInterval(16);
    connect(renderAnimationPlaybackTimer, &QTimer::timeout, this, &MainWindow::handleRenderAnimationPlaybackTimerTick);
    applyRenderSettingsToViewport();
    resetRenderAnimationPlayback();
    syncRenderAnimationUi();

    updateWindowTitle();
    recentFilesPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/recent_files.txt";
    recentFilesMenu = ui->menuOpenRecent;
    LoadRecentFiles();
    if (lastDirectory.isEmpty())
        lastDirectory = QDir::homePath();
    UpdateRecentFilesMenu();
    connectDesignerActions();
    restoreWindowLayout();
}

MainWindow::~MainWindow() = default;

void MainWindow::bindDesignerWidgets()
{
    splitter = ui->mainSplitter;
    chunkTreeTabs = ui->chunkTreeTabs;
    detailSourceLabel = ui->detailSourceLabel;
    detailSplitter = ui->detailSplitter;
    rawHexToggle = ui->rawHexToggle;
    tableWidget = ui->tableWidget;
    rawHexContainer = ui->rawHexContainer;
    rawHexEdit = ui->rawHexEdit;
    editorScrollArea = ui->editorScrollArea;
    editorStack = ui->editorStack;
    editorPlaceholder = ui->editorPlaceholder;
    editorPlaceholderLabel = ui->editorPlaceholderLabel;
    renderPane = ui->renderPane;

    connect(
        chunkTreeTabs,
        &QTabWidget::currentChanged,
        this,
        &MainWindow::handleChunkSourceTabChanged);
}

void MainWindow::createDynamicEditorPages()
{
    meshEditor = new MeshEditorWidget(editorStack);
    editorStack->addWidget(meshEditor);
    meshUserTextEditor = new RawTextEditorWidget(tr("User Text"), editorStack);
    editorStack->addWidget(meshUserTextEditor);
    textureNameEditor = new StringEditorWidget(tr("Texture Name"), editorStack);
    editorStack->addWidget(textureNameEditor);
    textureInfoEditor = new TextureInfoEditorWidget(editorStack);
    editorStack->addWidget(textureInfoEditor);
    hierarchyHeaderEditor = new HierarchyHeaderEditorWidget(editorStack);
    editorStack->addWidget(hierarchyHeaderEditor);
    animationHierarchyEditor = new AnimationHierarchyEditorWidget(editorStack);
    editorStack->addWidget(animationHierarchyEditor);
    materialNameEditor = new StringEditorWidget(tr("Material Name"), editorStack);
    editorStack->addWidget(materialNameEditor);
    transformNodeEditor = new TransformNodeEditorWidget(editorStack);
    editorStack->addWidget(transformNodeEditor);
    stage0ArgsEditor = new MapperArgsEditorWidget(tr("Stage 0 Mapper Args"), editorStack);
    editorStack->addWidget(stage0ArgsEditor);
    stage1ArgsEditor = new MapperArgsEditorWidget(tr("Stage 1 Mapper Args"), editorStack);
    editorStack->addWidget(stage1ArgsEditor);
    materialEditor = new MaterialEditorWidget(editorStack);
    editorStack->addWidget(materialEditor);
    shaderEditor = new ShaderEditorWidget(editorStack);
    editorStack->addWidget(shaderEditor);
    surfaceTypeEditor = new SurfaceTypeEditorWidget(editorStack);
    editorStack->addWidget(surfaceTypeEditor);
    triangleSurfaceTypeEditor = new TriangleSurfaceTypeEditorWidget(editorStack);
    editorStack->addWidget(triangleSurfaceTypeEditor);
    editorStack->setCurrentWidget(editorPlaceholder);
}

void MainWindow::connectDesignerActions()
{
    connect(ui->actionNew, &QAction::triggered, this, &MainWindow::newFile);
    connect(ui->actionOpen, &QAction::triggered, this, [this]() { openFile(QString()); });
    connect(ui->actionExportJson, &QAction::triggered, this, &MainWindow::exportJson);
    connect(ui->actionImportJson, &QAction::triggered, this, &MainWindow::importJson);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::saveFile);
    connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::saveFileAs);

    connect(ui->actionAddTopLevelChunk, &QAction::triggered, this, &MainWindow::addTopLevelChunk);
    connect(ui->actionInsertChunkBefore, &QAction::triggered, this, &MainWindow::insertChunkBefore);
    connect(ui->actionInsertChunkAfter, &QAction::triggered, this, &MainWindow::insertChunkAfter);
    connect(ui->actionAddChildChunk, &QAction::triggered, this, &MainWindow::addChildChunk);
    connect(ui->actionUndoRenderEdit, &QAction::triggered, this, &MainWindow::undoRenderTransform);
    connect(ui->actionRedoRenderEdit, &QAction::triggered, this, &MainWindow::redoRenderTransform);
    connect(ui->actionSetSelectedRenderRotation, &QAction::triggered, this, [this]() {
        if (renderViewport) {
            renderViewport->OpenManualPivotRotationDialog();
        }
    });
    connect(ui->actionMoveChunkUp, &QAction::triggered, this, &MainWindow::moveChunkUp);
    connect(ui->actionMoveChunkDown, &QAction::triggered, this, &MainWindow::moveChunkDown);
    connect(ui->actionDeleteSelectedChunk, &QAction::triggered, this, &MainWindow::deleteSelectedChunk);
    connect(ui->actionMoveHierarchyBoneToEnd, &QAction::triggered, this, &MainWindow::moveHierarchyBoneToEnd);

    connect(ui->actionExpandAll, &QAction::triggered, this, [this]() {
        if (treeWidget) {
            treeWidget->expandAll();
        }
    });
    connect(ui->actionCollapseAll, &QAction::triggered, this, [this]() {
        if (treeWidget) {
            treeWidget->collapseAll();
        }
    });
    connect(ui->actionHierarchyBrowser, &QAction::triggered, this, &MainWindow::showHierarchyBrowser);
    connect(ui->actionShowChunkTree, &QAction::toggled, chunkTreeTabs, &QWidget::setVisible);
    connect(ui->actionShowDetails, &QAction::toggled, ui->detailContainer, &QWidget::setVisible);
    connect(ui->actionShowRenderPane, &QAction::toggled, renderPane, &QWidget::setVisible);
    connect(ui->actionShowRenderTabs, &QAction::toggled, this, [this](bool visible) {
        if (renderTabs) {
            renderTabs->setVisible(visible);
        }
    });

    connect(ui->actionAddRenderSkeletons, &QAction::triggered, this, &MainWindow::addRenderSkeletons);
    connect(ui->actionAddRenderAnimations, &QAction::triggered, this, &MainWindow::addRenderAnimations);
    connect(ui->actionSetRenderTextureFolder, &QAction::triggered, this, &MainWindow::selectRenderTextureFolder);
    connect(ui->actionClearRenderTextureFolder, &QAction::triggered, this, &MainWindow::clearRenderTextureFolder);
    connect(ui->actionRemoveSelectedRenderAsset, &QAction::triggered, this, &MainWindow::removeSelectedRenderSessionAsset);
    connect(ui->actionClearAnimationLibraries, &QAction::triggered, this, &MainWindow::clearRenderAnimationLibraries);
    connect(ui->actionToggleRenderPlayback, &QAction::triggered, this, &MainWindow::toggleRenderAnimationPlayback);
    connect(ui->actionStopRenderPlayback, &QAction::triggered, this, &MainWindow::stopRenderAnimationPlayback);
    connect(ui->actionExportCurrentAnimationGif, &QAction::triggered, this, &MainWindow::exportActiveRenderAnimationGif);

    connect(ui->batchExportChunkListAction, &QAction::triggered, this, &MainWindow::on_actionExportChunkList_triggered);
    connect(ui->batchExportJsonAction, &QAction::triggered, this, &MainWindow::on_actionExportJsonBatch_triggered);
    connect(ui->batchValidateRoundTripAction, &QAction::triggered, this, &MainWindow::on_actionValidateRoundTripBatch_triggered);
    connect(ui->actionCopyPureHumanAnimations, &QAction::triggered, this, &MainWindow::on_actionCopyPureHumanAnimationsBySkeleton_triggered);
    connect(ui->actionExportAnimationGifs, &QAction::triggered, this, &MainWindow::on_actionExportSkeletonAAnimationGifs_triggered);
}

void MainWindow::restoreWindowLayout()
{
    QSettings settings;
    const QByteArray geometry = settings.value("MainWindow/geometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const QByteArray splitterState = settings.value("MainWindow/splitter").toByteArray();
    if (splitter && !splitterState.isEmpty()) {
        splitter->restoreState(splitterState);
    }
    const QByteArray detailSplitterState = settings.value("MainWindow/detailSplitter").toByteArray();
    if (detailSplitter && !detailSplitterState.isEmpty()) {
        detailSplitter->restoreState(detailSplitterState);
        detailSplitterStateCache = detailSplitterState;
    }
    const QByteArray renderSplitterState = settings.value("MainWindow/renderSplitter").toByteArray();
    if (renderSplitter) {
        bool restored = false;
        if (!renderSplitterState.isEmpty()) {
            restored = renderSplitter->restoreState(renderSplitterState);
        }
        const QList<int> sizes = renderSplitter->sizes();
        if (!restored
            || sizes.size() != 2
            || sizes[0] < 140
            || sizes[1] < 80) {
            renderSplitter->setSizes({ 700, 240 });
        }
    }
}

void MainWindow::newFile() {
    if (!confirmDiscardChanges()) {
        return;
    }

    renderTransformUndoStack.clear();
    renderTransformRedoStack.clear();
    applyingRenderTransformUndoRedo = false;

    chunkData = std::make_unique<ChunkData>();
    clearArchiveRenderContext();
    clearExternalRenderContext();
    currentFilePath.clear();
    currentRenderAnimationEditKeysEnabled = false;
    ClearChunkTree();
    setDirty(false);
    updateWindowTitle();
}


void MainWindow::openFile(const QString& path) {
    if (!confirmDiscardChanges()) return;

    renderTransformUndoStack.clear();
    renderTransformRedoStack.clear();
    applyingRenderTransformUndoRedo = false;

    QString filePath = path;
    if (filePath.isEmpty()) {
        QString startDir = lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory;
        filePath = QFileDialog::getOpenFileName(
            this,
            "Open W3D/Archive File",
            startDir,
            "W3D and Archive Files (*.w3d *.W3D *.wlt *.WLT *.mix *.MIX *.dat *.DAT *.dbs *.DBS);;W3D Files (*.w3d *.W3D *.wlt *.WLT);;Archive Files (*.mix *.MIX *.dat *.DAT *.dbs *.DBS);;All Files (*)");
        if (filePath.isEmpty()) return;
    }

    QString loadError;
    const bool isArchiveFile = IsMixArchivePath(filePath);
    LoadedArchiveRenderContext archiveRenderContext{};
    if (isArchiveFile) {
        if (!LoadW3DFromMixArchive(this, filePath, *chunkData, &loadError, &archiveRenderContext)) {
            if (!loadError.isEmpty()) {
                QMessageBox::warning(this, "Error", loadError);
            }
            return;
        }
    }
    else if (!chunkData->loadFromFile(filePath.toStdString()) || chunkData->getChunks().empty()) {
        QMessageBox::warning(this, "Error", "Failed to open file.");
        return;
    }

    if (isArchiveFile) {
        currentArchiveRenderPath = archiveRenderContext.archivePath;
        currentArchiveRenderEntryId = archiveRenderContext.selectedEntryId;
        currentArchiveRenderEntryPath = archiveRenderContext.selectedEntryPath;
        currentArchiveRenderEntries = std::move(archiveRenderContext.entries);
        currentArchiveTextureEntries = std::move(archiveRenderContext.textureEntryNames);
        currentArchiveTextureEntryIds = std::move(archiveRenderContext.textureEntryIds);
        currentArchiveTextureSourcesById.clear();
        for (const ArchiveTextureSourceInfo& source : archiveRenderContext.textureSources) {
            currentArchiveTextureSourcesById[source.id] = source;
        }
        currentArchiveSupplementalRoots.clear();
        currentArchiveLoadedSupplementalEntryIds.clear();
    }
    else {
        clearArchiveRenderContext();
    }
    clearExternalRenderContext();
    currentFilePath = filePath;
    ClearChunkTree();
    setDirty(false);
    updateWindowTitle();
    clearDetails();
    populateTree();
    rebuildRenderScene();
    if (renderViewport) {
        renderViewport->FocusScene();
    }
    AddRecentFile(filePath);
    lastDirectory = QFileInfo(filePath).absolutePath();
     
}



const std::vector<std::shared_ptr<ChunkItem>>* MainWindow::chunkRootsForSourceKey(
    const QString& sourceKey) const
{
    if (sourceKey == PrimaryChunkSourceKey()) {
        return chunkData ? &chunkData->getChunks() : nullptr;
    }

    for (const auto& asset : currentExternalRenderAssets) {
        if (RenderSessionAssetSourceKey(asset) == sourceKey) {
            return &asset.roots;
        }
    }
    return nullptr;
}

QString MainWindow::chunkSourceKeyForTree(const QTreeWidget* tree) const
{
    if (!tree) {
        return {};
    }
    for (const auto& tab : chunkSourceTabs) {
        if (tab.treeWidget == tree) {
            return tab.sourceKey;
        }
    }
    return {};
}

QString MainWindow::activeChunkSourceLabel() const
{
    for (const auto& tab : chunkSourceTabs) {
        if (tab.treeWidget == treeWidget) {
            return tab.label;
        }
    }
    return tr("(No Source)");
}

bool MainWindow::activeChunkSourceEditable() const
{
    for (const auto& tab : chunkSourceTabs) {
        if (tab.treeWidget == treeWidget) {
            return tab.editable;
        }
    }
    return false;
}

std::shared_ptr<ChunkItem> MainWindow::findChunkInActiveSource(const void* targetPtr) const
{
    if (!targetPtr) {
        return nullptr;
    }

    const auto* roots = chunkRootsForSourceKey(chunkSourceKeyForTree(treeWidget));
    if (!roots) {
        return nullptr;
    }
    return FindChunkByPtr(*roots, targetPtr);
}

void MainWindow::syncActiveChunkSourceTree()
{
    treeWidget = nullptr;
    if (chunkTreeTabs
        && chunkTreeTabs->currentIndex() >= 0
        && chunkTreeTabs->currentIndex() < static_cast<int>(chunkSourceTabs.size())) {
        treeWidget = chunkSourceTabs[static_cast<std::size_t>(chunkTreeTabs->currentIndex())].treeWidget;
    }

    if (detailSourceLabel) {
        if (!treeWidget) {
            detailSourceLabel->setText(tr("Source: none"));
        }
        else if (activeChunkSourceEditable()) {
            detailSourceLabel->setText(tr("Source: %1").arg(activeChunkSourceLabel()));
        }
        else {
            detailSourceLabel->setText(tr("Source: %1 (read-only)").arg(activeChunkSourceLabel()));
        }
    }
}

void MainWindow::handleChunkSourceTabChanged(int) {
    syncActiveChunkSourceTree();
    handleTreeSelection();
}

void MainWindow::rebuildChunkSourceTabs() {
    if (!chunkTreeTabs) {
        return;
    }

    const QString currentSourceKey = chunkSourceKeyForTree(treeWidget);
    QSignalBlocker blocker(chunkTreeTabs);
    chunkTreeTabs->clear();
    chunkSourceTabs.clear();

    auto addSourceTab = [&](const QString& sourceKey, const QString& label, bool editable) {
        auto* sourceTree = new QTreeWidget(chunkTreeTabs);
        sourceTree->setHeaderLabel(tr("Chunk Tree"));
        sourceTree->setSelectionMode(QAbstractItemView::SingleSelection);
        connect(sourceTree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::handleTreeSelection);
        chunkTreeTabs->addTab(sourceTree, label);
        chunkSourceTabs.push_back({ sourceKey, label, editable, sourceTree });
    };

    if (chunkData && (!currentFilePath.isEmpty() || !chunkData->getChunks().empty())) {
        const QString primaryLabel = BuildPrimaryRenderSourceLabel(
            currentFilePath,
            currentArchiveRenderPath,
            currentArchiveRenderEntryPath,
            chunkData.get());
        addSourceTab(PrimaryChunkSourceKey(), primaryLabel, true);
    }
    for (const auto& asset : currentExternalRenderAssets) {
        addSourceTab(
            RenderSessionAssetSourceKey(asset),
            tr("%1 | %2").arg(asset.displayLabel, RenderSessionAssetRoleLabel(asset)),
            false);
    }

    int desiredIndex = 0;
    if (!currentSourceKey.isEmpty()) {
        for (int i = 0; i < static_cast<int>(chunkSourceTabs.size()); ++i) {
            if (chunkSourceTabs[static_cast<std::size_t>(i)].sourceKey == currentSourceKey) {
                desiredIndex = i;
                break;
            }
        }
    }

    if (!chunkSourceTabs.empty()) {
        chunkTreeTabs->setCurrentIndex(std::clamp(desiredIndex, 0, static_cast<int>(chunkSourceTabs.size()) - 1));
    }
    syncActiveChunkSourceTree();
}

void MainWindow::populateTree() {
    void* selectedPtr = SelectedChunkPtr(treeWidget);
    rebuildChunkSourceTabs();

    for (const auto& tab : chunkSourceTabs) {
        tab.treeWidget->clear();

        const auto* chunks = chunkRootsForSourceKey(tab.sourceKey);
        if (!chunks) {
            continue;
        }

        for (const auto& chunk : *chunks) {
            QTreeWidgetItem* item = new QTreeWidgetItem();

            QString label = QString("0x%1 (%2)")
                .arg(chunk->id, 0, 16)
                .arg(QString::fromStdString(LabelForChunk(chunk->id, chunk.get())));

            item->setText(0, QString("%1 (size %2)").arg(label).arg(chunk->length));
            item->setData(0, Qt::UserRole, QVariant::fromValue<void*>(chunk.get()));
            tab.treeWidget->addTopLevelItem(item);

            std::function<void(QTreeWidgetItem*, const std::shared_ptr<ChunkItem>&)> addChildren;
            addChildren = [&](QTreeWidgetItem* parent, const std::shared_ptr<ChunkItem>& current) {

                constexpr uint32_t SOUND_RENDER_DEF = 0x0100;
                constexpr uint32_t SOUNDROBJ_DEFINITION = 0x0A02;
                constexpr uint32_t SOUNDROBJ_DEFINITION_EXT = 0x0200;
                if (current->id == SOUND_RENDER_DEF
                    && current->parent
                    && (current->parent->id == SOUNDROBJ_DEFINITION
                        || current->parent->id == SOUNDROBJ_DEFINITION_EXT))
                {
                    return;
                }

                if (current->parent
                    && (current->parent->id == SOUND_RENDER_DEF
                        && current->parent->parent
                        && (current->parent->parent->id == SOUNDROBJ_DEFINITION
                            || current->parent->parent->id == SOUNDROBJ_DEFINITION_EXT)))
                {
                    return;
                }

                for (auto& child : current->children) {
                    QTreeWidgetItem* childItem = new QTreeWidgetItem(parent);
                    QString lbl = QString("0x%1 (%2)")
                        .arg(child->id, 0, 16)
                        .arg(QString::fromStdString(LabelForChunk(child->id, child.get())));
                    childItem->setText(0, QString("%1 (size %2)").arg(lbl).arg(child->length));
                    childItem->setData(0, Qt::UserRole, QVariant::fromValue<void*>(child.get()));
                    addChildren(childItem, child);
                }
                };

            addChildren(item, chunk);
        }

        tab.treeWidget->collapseAll();
    }

    syncActiveChunkSourceTree();
    if (selectedPtr) {
        selectChunkInTree(selectedPtr);
    }
    else {
        handleTreeSelection();
    }
}

// Constants for clarity
constexpr uint32_t MICRO_ID = 0x01;
constexpr uint32_t SPHERE_ID = 0x0741;
constexpr uint32_t RING_ID = 0x0742;
constexpr uint32_t CHANNEL_WRAPPER = 0x03150809; 
constexpr uint32_t SOUND_RENDER_DEF = 0x0100;
constexpr uint32_t SOUND_RENDER_DEF_EXT = 0x0200;
constexpr uint32_t SOUNDROBJ_DEFINITION = 0x0A02;

void MainWindow::handleTreeSelection() {
    if (QTreeWidget* sourceTree = qobject_cast<QTreeWidget*>(sender());
        sourceTree && sourceTree != treeWidget) {
        return;
    }
    if (!treeWidget) {
        clearDetails();
        return;
    }

    const auto selectedItems = treeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        clearDetails();
        return;
    }

    QTreeWidgetItem* selected = selectedItems.first();
    const QVariant ptrVar = selected->data(0, Qt::UserRole);
    if (!ptrVar.isValid()) {
        clearDetails();
        return;
    }

    void* targetPtr = ptrVar.value<void*>();
    if (!targetPtr) {
        clearDetails();
        return;
    }

    const std::shared_ptr<ChunkItem> target = findChunkInActiveSource(targetPtr);

    tableWidget->clearContents();
    tableWidget->setRowCount(0);
    if (!target) {
        clearDetails();
        return;
    }

    std::vector<ChunkField> fields;

    // --- Special case: Sound Render Object definition microchunks ---
    if (target->id == SOUND_RENDER_DEF &&
        target->parent &&
        (target->parent->id == SOUNDROBJ_DEFINITION || target->parent->id == SOUND_RENDER_DEF_EXT)) {
        fields = InterpretSoundRObjDefinition(target);
    }

    // --- Sphere / Ring: headers and channels ---
    if (fields.empty()) {
        const uint32_t pid = target->parent ? target->parent->id : 0;

        // Headers (child id 0x0001 under sphere/ring)
        if (target->id == 0x0001 && pid == 0x0741) {
            fields = InterpretSphereHeader(target);
        }
        else if (target->id == 0x0001 && pid == 0x0742) {
            fields = InterpretRingHeader(target);
        }
        // Channel wrappers under Sphere (0x0741): wrappers show no fields
        else if (pid == 0x0741 &&
            (target->id == 0x0002 || target->id == 0x0003 ||
                target->id == 0x0004 || target->id == 0x0005)) {
            // Intentionally empty: just let the 0x03150809 child display data
        }
        // Channel wrappers under Ring (0x0742): wrappers show no fields
        else if (pid == 0x0742 &&
            (target->id == 0x0002 || target->id == 0x0003 ||
                target->id == 0x0004 || target->id == 0x0005)) {
            // Intentionally empty
        }
        // DATA node (0x03150809) - interpret based on parent wrapper and root (sphere/ring)
        else if (target->id == 0x03150809) {
            const uint32_t wrapper = target->parent ? target->parent->id : 0; // 0x0002..0x0005
            const uint32_t root = (target->parent && target->parent->parent)
                ? target->parent->parent->id
                : 0;                                       // 0x0741/0x0742

            if (root == 0x0741) { // Sphere
                switch (wrapper) {
                case 0x0002: fields = InterpretSphereColorChannel(target);  break;
                case 0x0003: fields = InterpretSphereAlphaChannel(target);  break;
                case 0x0004: fields = InterpretSphereScaleChannel(target);  break;
                case 0x0005: fields = InterpretSphereVectorChannel(target); break;
                default: break;
                }
            }
            else if (root == 0x0742) { // Ring
                switch (wrapper) {
                case 0x0002: fields = InterpretRingColorChannel(target);        break;
                case 0x0003: fields = InterpretRingAlphaChannel(target);        break;
                case 0x0004: fields = InterpretRingInnerScaleChannel(target);   break;
                case 0x0005: fields = InterpretRingOuterScaleChannel(target);   break;
                default: break;
                }
            }
        }
        // --- SHADER MESH: wrappers & VARIABLES (0x16490430) ---
        if (target->id == 0x0B41) {                      // SHDSUBMESH_SHADER_CLASSID
            fields = InterpretShdSubMeshShaderClassId(target);
        }
        else if (target->id == 0x0B42) {                 // SHDSUBMESH_SHADER_DEF (wrapper)
            /* wrapper - show nothing here; data lives in its 0x16490430 child */
        }
        else if (target->id == 0x16490430 &&             // VARIABLES under SHADER_DEF
            target->parent && target->parent->id == 0x0B42 &&
            target->parent->parent && target->parent->parent->id == 0x0B40)
        {
            fields = InterpretShdSubMeshShaderDefVariables(target);
        }
        // (Optional: some assets also use 0x16490450; treat it the same.)
        else if (target->id == 0x16490450 &&             // alternate VARIABLES id seen in the wild
            target->parent && target->parent->id == 0x0B42)
        {
            fields = InterpretShdSubMeshShaderDefVariables50(target);
        
        }
    }

    // --- Generic dispatch (only if still empty) ---
    if (fields.empty()) {
        uint16_t flavor = 0xFFFF;
        if (target->id == 0x0282) {
            const auto* activeRoots =
                chunkRootsForSourceKey(chunkSourceKeyForTree(treeWidget));
            auto tryFindFlavor = [&](const std::shared_ptr<ChunkItem>& root) -> bool {
                std::function<bool(const std::shared_ptr<ChunkItem>&)> dfs =
                    [&](const std::shared_ptr<ChunkItem>& n) -> bool {
                    if (n->id == 0x0281 && n->data.size() >= 44) {
                        flavor = *reinterpret_cast<const uint16_t*>(&n->data[42]);
                        return true;
                    }
                    for (const auto& c : n->children) if (dfs(c)) return true;
                    return false;
                    };
                return dfs(root);
                };
            if (activeRoots) {
                for (const auto& r : *activeRoots) {
                    if (tryFindFlavor(r)) break;
                }
            }
        }

        switch (target->id) {
        
        case 0x0001: fields = InterpretMeshHeader(target); break;                 // OBSOLETE header
     
            // Only hit when not sphere/ring (pid != 0x0741/0x0742) 
        case 0x0002: { // VERTICES normally, but also SPHERE/RING COLOR wrapper 
            uint32_t pid = target->parent ? target->parent->id : 0;
            if (pid == 0x0741 || pid == 0x0742) break; 
            // wrapper: let the 0x03150809 child show data
            fields = InterpretVertices(target); break; 
        }

        case 0x0003: { // NORMALS normally, but also SPHERE/RING ALPHA wrapper
            uint32_t pid = target->parent ? target->parent->id : 0;
            if (pid == 0x0741 || pid == 0x0742) break; // wrapper 
            fields = InterpretVertexNormals(target); break;
        }

        case 0x0004: { // SURRENDER NORMALS normally, but also SPHERE/RING SCALE wrapper
            uint32_t pid = target->parent ? target->parent->id : 0;
            if (pid == 0x0741 || pid == 0x0742) break; // wrapper
            fields = InterpretSurrenderNormals(target); break;         // OBSOLETE
        }

        case 0x0005: { // Texcoords normally, but also SPHERE/RING SCALE wrapper
            uint32_t pid = target->parent ? target->parent->id : 0;
            if (pid == 0x0741 || pid == 0x0742) break; // wrapper
            fields = InterpretTexcoords(target); break;                  // OBSOLETE
        }

        case 0x0006: fields = InterpretMaterials1(target); break;                 // OBSOLETE
        case 0x0007: fields = InterpretTrianglesO(target); break;                 // OBSOLETE
        case 0x0008: fields = InterpretQuadranglesO(target); break;               // OBSOLETE
        case 0x0009: fields = InterpretSurrenderTriangles(target); break;         // OBSOLETE
        case 0x000A: fields = InterpretPovTriangles(target); break;               // OBSOLETE
        case 0x000B: fields = InterpretPovQuadrangles(target); break;             // OBSOLETE
        case 0x000C: fields = InterpretMeshUserText(target); break;
        case 0x000D: fields = InterpretVertexColors(target); break;               // OBSOLETE
        case 0x000E: fields = InterpretVertexInfluences(target); break;
        case 0x000F: fields = InterpretDamage(target); break;                     // OBSOLETE wrapper
        case 0x0010: fields = InterpretDamageHeader(target); break;               // OBSOLETE
        case 0x0011: fields = InterpretDamageVertices(target); break;             // OBSOLETE
        case 0x0012: fields = InterpretDamageColors(target); break;               // OBSOLETE
        case 0x0013: fields = InterpretDamageMaterials(target); break;            // OBSOLETE
        case 0x0014: fields = InterpretMaterials2(target); break;                 // OBSOLETE
      //  case 0x0015: fields = InterpretMaterials3(target); break;                 // OBSOLETE
      //  case 0x0016: fields = InterpretMaterial3(target); break;                  // OBSOLETE
        case 0x0017: fields = InterpretMaterial3Name(target); break;              // OBSOLETE
        case 0x0018: fields = InterpretMaterial3Info(target); break;              // OBSOLETE
      // case 0x0019: fields = InterpretMaterial3DcMap(target); break;             // OBSOLETE
        case 0x001A: fields = InterpretMap3Filename(target); break;               // OBSOLETE
        case 0x001B: fields = InterpretMap3Info(target); break;                   // OBSOLETE
        case 0x001C: fields = InterpretMaterial3DiMap(target); break;             // OBSOLETE
        case 0x001D: fields = InterpretMaterial3ScMap(target); break;             // OBSOLETE
        case 0x001E: fields = InterpretMaterial3SiMap(target); break;             // OBSOLETE
        case 0x001F: fields = InterpretMeshHeader3(target); break;
        case 0x0020: fields = InterpretTriangles(target); break;
        case 0x0021: fields = InterpretPerTriMaterials(target); break;            // OBSOLETE (per-tri material ids)
        case 0x0022: fields = InterpretVertexShadeIndices(target); break;
        case 0x0028: fields = InterpretMaterialInfo(target); break;
        case 0x0029: fields = InterpretShaders(target); break;
        case 0x002C: fields = InterpretVertexMaterialName(target); break;
        case 0x002D: fields = InterpretVertexMaterialInfo(target); break;
        case 0x002E: fields = InterpretARG0(target); break;
        case 0x002F: fields = InterpretARG1(target); break;
        case 0x0032: fields = InterpretTextureName(target); break;
        case 0x0033: fields = InterpretTextureInfo(target); break;
        case 0x0039: fields = InterpretVertexMaterialIDs(target); break;
        case 0x003A: fields = InterpretShaderIDs(target); break;
        case 0x003B: fields = InterpretDCG(target); break;
        case 0x003C: fields = InterpretDIG(target); break;
        case 0x003E: fields = InterpretSCG(target); break;
        case 0x003F: fields = InterpretShaderMaterialId(target); break;           // BFME2 optional (if you have one)
        case 0x0049: fields = InterpretTextureIDs(target); break;
        case 0x004A: fields = InterpretStageTexCoords(target); break;
        case 0x004B: fields = InterpretPerFaceTexcoordIds(target); break;
        case 0x0052: fields = InterpretShaderMaterialHeader(target); break;       // BFME2
        case 0x0053: fields = InterpretShaderMaterialProperty(target); break;     // BFME2
        case 0x0058: fields = InterpretDeform(target); break;
        case 0x0059: fields = InterpretDeformSet(target); break;
        case 0x005A: fields = InterpretDeformKeyframes(target); break;
        case 0x005B: fields = InterpretDeformData(target); break;
        case 0x0060: fields = InterpretTangents(target); break;                   // if implemented
        case 0x0061: fields = InterpretBinormals(target); break;                  // if implemented
        case 0x0080: fields = InterpretPS2Shaders(target); break;
        case 0x0091: fields = InterpretAABTreeHeader(target); break;
        case 0x0092: fields = InterpretAABTreePolyIndices(target); break;
        case 0x0093: fields = InterpretAABTreeNodes(target); break;           
    //  case 0x0100: fields = InterpretSoundRObjDefinition(target); break;
        case 0x0101: fields = InterpretHierarchyHeader(target); break;
        case 0x0102: fields = InterpretPivots(target); break;
        case 0x0103: fields = InterpretPivotFixups(target); break;
        case 0x0104: fields = InterpretPivotUnknown(target); break;
        case 0x0201: fields = InterpretAnimationHeader(target); break;
        case 0x0202: fields = InterpretAnimationChannel(target); break;
        case 0x0203: fields = InterpretBitChannel(target); break;
        case 0x0281: fields = InterpretCompressedAnimationHeader(target); break;
        case 0x0282: fields = InterpretCompressedAnimationChannel(target, (flavor != 0xFFFF ? flavor : 0)); break;
        case 0x0283: fields = InterpretCompressedBitChannel(target); break;
        case 0x0284: fields = InterpretCompressedMotionChannel(target); break;
        case 0x02C1: fields = InterpretMorphAnimHeader(target); break;
        case 0x02C3: fields = InterpretMorphAnimPoseName(target); break;
        case 0x02C4: fields = InterpretMorphAnimKeyData(target); break;
        case 0x02C5: fields = InterpretMorphAnimPivotChannelData(target); break;
        case 0x0301: fields = InterpretHModelHeader(target); break;
        case 0x0302: fields = InterpretNode(target); break;
        case 0x0303: fields = InterpretCollisionNode(target); break;
        case 0x0304: fields = InterpretSkinNode(target); break;
        case 0x0305: fields = InterpretHModelAuxData(target); break;
        case 0x0306: fields = InterpretShadowNode(target); break;
        case 0x0401: fields = InterpretLODModelHeader(target); break;
        case 0x0402: fields = InterpretLOD(target); break;
        case 0x0421: fields = InterpretCollectionHeader(target); break;
        case 0x0422: fields = InterpretCollectionObjName(target); break;
        case 0x0423: fields = InterpretPlaceHolder(target); break;
        case 0x0424: fields = InterpretTransformNode(target); break;
        case 0x0440: fields = InterpretPoints(target); break;
        case 0x0461: fields = InterpretLightInfo(target); break;
        case 0x0462: fields = InterpretSpotLightInfo(target); break;
        case 0x0463: fields = InterpretNearAtten(target); break;
        case 0x0464: fields = InterpretFarAtten(target); break;
		case 0x0465: fields = InterpretSpotLightInfoTT(target); break;
        case 0x0466: fields = InterpretLightPulse(target); break;
        case 0x0501: fields = InterpretEmitterHeader(target); break;
        case 0x0502: fields = InterpretEmitterUserData(target); break;
        case 0x0503: fields = InterpretEmitterInfo(target); break;
        case 0x0504: fields = InterpretEmitterInfoV2(target); break;
        case 0x0505: fields = InterpretEmitterProps(target); break;
		case 0x0506: fields = InterpretEmitterColorKeyframe(target); break;
		case 0x0507: fields = InterpretEmitterOpacityKeyframe(target); break;
		case 0x0508: fields = InterpretEmitterSizeKeyframe(target); break;
        case 0x0509: fields = InterpretEmitterLineProperties(target); break;
        case 0x050A: fields = InterpretEmitterRotationKeys(target); break;
        case 0x050B: fields = InterpretEmitterFrameKeys(target); break;
        case 0x050C: fields = InterpretEmitterBlurTimeKeyframes(target); break;
        case 0x050D: fields = InterpretEmitterExtraInfo(target); break;
        case 0x0601: fields = InterpretAggregateHeader(target); break;
        case 0x0602: fields = InterpretAggregateInfo(target); break;
        case 0x0603: fields = InterpretTextureReplacerInfo(target); break;
        case 0x0604: fields = InterpretAggregateClassInfo(target); break;
        case 0x0701: fields = InterpretHLODHeader(target); break;
        case 0x0703: fields = InterpretHLODSubObjectArrayHeader(target); break;
        case 0x0704: fields = InterpretHLODSubObject_LodArray(target); break;
        case 0x0740: fields = InterpretBox(target); break;
        case 0x0750: fields = InterpretNullObject(target); break;
        case 0x0802: fields = InterpretLightTransform(target); break;
        case 0x0901: fields = InterpretDazzleName(target); break;
        case 0x0902: fields = InterpretDazzleTypeName(target); break;
        case 0x0A01: fields = InterpretSoundRObjHeader(target); break;
        case 0x0B01: fields = InterpretShdMeshName(target); break;
		case 0x0B02: fields = InterpretShdMeshHeader(target); break;
		case 0x0B03: fields = InterpretShdMeshUserText(target); break;
		case 0x0B21: fields = InterpretShdSubMeshHeader(target); break;
//		case 0x0B40: fields = InterpretShdSubMeshShader(target); break;
		case 0x0B41: fields = InterpretShdSubMeshShaderClassId(target); break;
//		case 0x0B42: fields = InterpretShdSubMeshShaderDef(target); break;
		case 0x0B43: fields = InterpretShdSubMeshVertices(target); break;
		case 0x0B44: fields = InterpretShdSubMeshVertexNormals(target); break;
		case 0x0B45: fields = InterpretShdSubMeshTriangles(target); break;
		case 0x0B46: fields = InterpretShdSubMeshVertexShadeIndices(target); break;
		case 0x0B47: fields = InterpretShdSubMeshUV0(target); break;
		case 0x0B48: fields = InterpretShdSubMeshUV1(target); break;
		case 0x0B49: fields = InterpretShdSubMeshTangentBasisS(target); break;
		case 0x0B4A: fields = InterpretShdSubMeshTangentBasisT(target); break;
		case 0x0B4B: fields = InterpretShdSubMeshTangentBasisSXT(target); break;
		case 0x0B4C: fields = InterpretShdSubMeshColor(target); break;
		case 0x0B4D: fields = InterpretShdSubMeshVertexInfluences(target); break;
        case 0x16490430: fields = InterpretShdSubMeshShaderDefVariables(target); break;
		case 0x0C00: fields = InterpretSecondaryVertices(target); break;
		case 0x0C01: fields = InterpretSecondaryVertexNormals(target); break;
		case 0x0C02: fields = InterpretLightMapUV(target); break;
		case 0x0C03: fields = InterpretVertexInfluencesExtended(target); break;

        default:
            break;
        }
    }

    // If still empty and there are children, show child nodes
    if (fields.empty() && !target->children.empty()) {
        for (const auto& child : target->children) {
            fields.push_back({ LabelForChunk(child->id, child.get()), "chunk", "" });
        }
    }

    // Render table
    tableWidget->setRowCount(static_cast<int>(fields.size()));
    for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
        tableWidget->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(fields[i].field)));
        tableWidget->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(fields[i].type)));
        tableWidget->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(fields[i].value)));
    }

    updateEditorForChunk(target);
    updateRawHex(target);
}

static bool IsPureAnimationTopLevelChunkId(uint32_t id) {
    switch (id) {
    case 0x0200: // W3D_CHUNK_ANIMATION
    case 0x0201: // W3D_CHUNK_ANIMATION_HEADER
    case 0x0202: // W3D_CHUNK_ANIMATION_CHANNEL
    case 0x0280: // W3D_CHUNK_COMPRESSED_ANIMATION
    case 0x0281: // W3D_CHUNK_COMPRESSED_ANIMATION_HEADER
    case 0x0282: // W3D_CHUNK_COMPRESSED_ANIMATION_CHANNEL
    case 0x02C0: // W3D_CHUNK_MORPH_ANIMATION
    case 0x02C1: // W3D_CHUNK_MORPHANIM_HEADER
    case 0x02C2: // W3D_CHUNK_MORPHANIM_CHANNEL
        return true;
    default:
        return false;
    }
}

static bool IsPureAnimationFile(const std::vector<std::shared_ptr<ChunkItem>>& roots) {
    if (roots.empty()) {
        return false;
    }

    for (const auto& root : roots) {
        if (!root || !IsPureAnimationTopLevelChunkId(root->id)) {
            return false;
        }
    }
    return true;
}

static void UpdateAnimationHeaderNameRecursive(
    const std::shared_ptr<ChunkItem>& node,
    const std::string& animationName)
{
    if (!node) {
        return;
    }

    switch (node->id) {
    case 0x0201: // W3D_CHUNK_ANIMATION_HEADER
        (void)W3DEdit::MutateStructChunk<W3dAnimHeaderStruct>(
            node,
            [&](W3dAnimHeaderStruct& header) {
                W3DEdit::WriteFixedString(header.Name, W3D_NAME_LEN, animationName);
            });
        break;
    case 0x0281: // W3D_CHUNK_COMPRESSED_ANIMATION_HEADER
        (void)W3DEdit::MutateStructChunk<W3dCompressedAnimHeaderStruct>(
            node,
            [&](W3dCompressedAnimHeaderStruct& header) {
                W3DEdit::WriteFixedString(header.Name, W3D_NAME_LEN, animationName);
            });
        break;
    case 0x02C1: // W3D_CHUNK_MORPHANIM_HEADER
        (void)W3DEdit::MutateStructChunk<W3dMorphAnimHeaderStruct>(
            node,
            [&](W3dMorphAnimHeaderStruct& header) {
                W3DEdit::WriteFixedString(header.Name, W3D_NAME_LEN, animationName);
            });
        break;
    default:
        break;
    }

    for (const auto& child : node->children) {
        UpdateAnimationHeaderNameRecursive(child, animationName);
    }
}

static void SyncPureAnimationHeaderNameForSave(ChunkData* chunkData, const QString& savePath) {
    if (!chunkData) {
        return;
    }

    auto& roots = chunkData->getChunksMutable();
    if (!IsPureAnimationFile(roots)) {
        return;
    }

    QString baseName = QFileInfo(savePath).completeBaseName().trimmed();
    if (baseName.isEmpty()) {
        baseName = QFileInfo(savePath).fileName().trimmed();
    }
    if (baseName.isEmpty()) {
        return;
    }

    const std::string animationName = baseName.toUpper().toStdString();
    for (const auto& root : roots) {
        UpdateAnimationHeaderNameRecursive(root, animationName);
    }
}

void MainWindow::saveFile() {
    if (!chunkData || chunkData->getChunks().empty()) {
        return;
    }

    if (currentFilePath.isEmpty()) {
        saveFileAs();
        return;
    }
    if (IsMixArchivePath(currentFilePath)) {
        saveFileAs();
        return;
    }

    QString draftError;
    if (!flushPendingRenderAnimationDrafts(&draftError)) {
        QMessageBox::warning(
            this,
            tr("Save Failed"),
            draftError.isEmpty()
                ? tr("Failed to apply pending animation edits before saving.")
                : draftError);
        return;
    }

    if (!createBackupFile(currentFilePath)) {
        return;
    }

    SyncHLodCountsForSave(chunkData.get());
    SyncPureAnimationHeaderNameForSave(chunkData.get(), currentFilePath);
    if (!chunkData->saveToFile(currentFilePath.toStdString())) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to save file."));
        return;
    }

    setDirty(false);
}

void MainWindow::saveFileAs() {
    if (!chunkData || chunkData->getChunks().empty()) {
        return;
    }

    QString startDir;
    if (!currentFilePath.isEmpty()) {
        startDir = QFileInfo(currentFilePath).absolutePath();
    }
    else {
        startDir = lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory;
    }

    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save W3D File"),
        startDir,
        tr("W3D Files (*.w3d);;All Files (*)"));

    if (filePath.isEmpty()) return;

    QString draftError;
    if (!flushPendingRenderAnimationDrafts(&draftError)) {
        QMessageBox::warning(
            this,
            tr("Save Failed"),
            draftError.isEmpty()
                ? tr("Failed to apply pending animation edits before saving.")
                : draftError);
        return;
    }

    const QString oldFileName = QFileInfo(currentFilePath).fileName();
    const QString oldBaseName = QFileInfo(currentFilePath).completeBaseName();
    const QString newFileName = QFileInfo(filePath).fileName();
    const QString newBaseName = QFileInfo(filePath).completeBaseName();
    if (!oldBaseName.isEmpty() && oldBaseName != newBaseName) {
        renameFileReferences(oldBaseName, newBaseName, oldFileName, newFileName);
    }

    if (!createBackupFile(filePath)) {
        return;
    }

    SyncHLodCountsForSave(chunkData.get());
    SyncPureAnimationHeaderNameForSave(chunkData.get(), filePath);
    if (!chunkData->saveToFile(filePath.toStdString())) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to save file."));
        return;
    }

    currentFilePath = filePath;
    AddRecentFile(filePath);
    lastDirectory = QFileInfo(filePath).absolutePath();
    setDirty(false);
}

bool MainWindow::createBackupFile(const QString& filePath) {
    if (filePath.isEmpty()) return true;
    QFileInfo info(filePath);
    if (!info.exists()) return true;

    const QString backupPath = filePath + QStringLiteral(".bak");
    if (QFile::exists(backupPath)) {
        if (!QFile::remove(backupPath)) {
            QMessageBox::warning(this, tr("Error"),
                tr("Failed to replace backup file:\n%1").arg(backupPath));
            return false;
        }
    }

    if (!QFile::copy(filePath, backupPath)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Failed to create backup file:\n%1").arg(backupPath));
        return false;
    }

    return true;
}

void MainWindow::renameFileReferences(const QString& oldBaseName,
    const QString& newBaseName,
    const QString& oldFileName,
    const QString& newFileName) {
    if (!chunkData) return;
    if (oldBaseName.isEmpty() || oldBaseName == newBaseName) {
        return;
    }

    bool changed = false;
    std::function<void(const std::shared_ptr<ChunkItem>&)> dfs =
        [&](const std::shared_ptr<ChunkItem>& node) {
        if (!node) return;

        switch (node->id) {
        case 0x001F: { // W3D_CHUNK_MESH_HEADER3
            auto parsed = ParseChunkStruct<W3dMeshHeader3Struct>(node);
            if (auto header = std::get_if<W3dMeshHeader3Struct>(&parsed)) {
                const QString meshName = ReadFixedString(header->MeshName, W3D_NAME_LEN);
                const QString containerName = ReadFixedString(header->ContainerName, W3D_NAME_LEN);
                const QString updatedMesh = RenameIfMatches(meshName, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                const QString updatedContainer = RenameIfMatches(containerName, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updatedMesh != meshName || updatedContainer != containerName) {
                    (void)W3DEdit::MutateStructChunk<W3dMeshHeader3Struct>(
                        node,
                        [&](W3dMeshHeader3Struct& target) {
                            if (updatedMesh != meshName) {
                                W3DEdit::WriteFixedString(target.MeshName, W3D_NAME_LEN,
                                    updatedMesh.toStdString());
                            }
                            if (updatedContainer != containerName) {
                                W3DEdit::WriteFixedString(target.ContainerName, W3D_NAME_LEN,
                                    updatedContainer.toStdString());
                            }
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0101: { // W3D_CHUNK_HIERARCHY_HEADER
            auto parsed = ParseChunkStruct<W3dHierarchyStruct>(node);
            if (auto header = std::get_if<W3dHierarchyStruct>(&parsed)) {
                const QString name = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString updated = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dHierarchyStruct>(
                        node,
                        [&](W3dHierarchyStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0201: { // W3D_CHUNK_ANIMATION_HEADER
            auto parsed = ParseChunkStruct<W3dAnimHeaderStruct>(node);
            if (auto header = std::get_if<W3dAnimHeaderStruct>(&parsed)) {
                const QString name = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString hierName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
                const QString updatedName = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                const QString updatedHier = RenameIfMatches(hierName, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updatedName != name || updatedHier != hierName) {
                    (void)W3DEdit::MutateStructChunk<W3dAnimHeaderStruct>(
                        node,
                        [&](W3dAnimHeaderStruct& target) {
                            if (updatedName != name) {
                                W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updatedName.toStdString());
                            }
                            if (updatedHier != hierName) {
                                W3DEdit::WriteFixedString(target.HierarchyName, W3D_NAME_LEN, updatedHier.toStdString());
                            }
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0281: { // W3D_CHUNK_COMPRESSED_ANIMATION_HEADER
            auto parsed = ParseChunkStruct<W3dCompressedAnimHeaderStruct>(node);
            if (auto header = std::get_if<W3dCompressedAnimHeaderStruct>(&parsed)) {
                const QString name = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString hierName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
                const QString updatedName = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                const QString updatedHier = RenameIfMatches(hierName, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updatedName != name || updatedHier != hierName) {
                    (void)W3DEdit::MutateStructChunk<W3dCompressedAnimHeaderStruct>(
                        node,
                        [&](W3dCompressedAnimHeaderStruct& target) {
                            if (updatedName != name) {
                                W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updatedName.toStdString());
                            }
                            if (updatedHier != hierName) {
                                W3DEdit::WriteFixedString(target.HierarchyName, W3D_NAME_LEN, updatedHier.toStdString());
                            }
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x02C1: { // W3D_CHUNK_MORPHANIM_HEADER
            auto parsed = ParseChunkStruct<W3dMorphAnimHeaderStruct>(node);
            if (auto header = std::get_if<W3dMorphAnimHeaderStruct>(&parsed)) {
                const QString name = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString hierName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
                const QString updatedName = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                const QString updatedHier = RenameIfMatches(hierName, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updatedName != name || updatedHier != hierName) {
                    (void)W3DEdit::MutateStructChunk<W3dMorphAnimHeaderStruct>(
                        node,
                        [&](W3dMorphAnimHeaderStruct& target) {
                            if (updatedName != name) {
                                W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updatedName.toStdString());
                            }
                            if (updatedHier != hierName) {
                                W3DEdit::WriteFixedString(target.HierarchyName, W3D_NAME_LEN, updatedHier.toStdString());
                            }
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0301: { // W3D_CHUNK_HMODEL_HEADER
            auto parsed = ParseChunkStruct<W3dHModelHeaderStruct>(node);
            if (auto header = std::get_if<W3dHModelHeaderStruct>(&parsed)) {
                const QString name = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString hierName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
                const QString updatedName = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                const QString updatedHier = RenameIfMatches(hierName, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updatedName != name || updatedHier != hierName) {
                    (void)W3DEdit::MutateStructChunk<W3dHModelHeaderStruct>(
                        node,
                        [&](W3dHModelHeaderStruct& target) {
                            if (updatedName != name) {
                                W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updatedName.toStdString());
                            }
                            if (updatedHier != hierName) {
                                W3DEdit::WriteFixedString(target.HierarchyName, W3D_NAME_LEN, updatedHier.toStdString());
                            }
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0302: // W3D_CHUNK_HMODEL_NODE
        case 0x0303: // W3D_CHUNK_HMODEL_COLLISION_NODE
        case 0x0304: // W3D_CHUNK_HMODEL_SKIN_NODE
        case 0x0306: { // W3D_CHUNK_HMODEL_SHADOW_NODE
            auto parsed = ParseChunkStruct<W3dHModelNodeStruct>(node);
            if (auto nodeStruct = std::get_if<W3dHModelNodeStruct>(&parsed)) {
                const QString name = ReadFixedString(nodeStruct->RenderObjName, W3D_NAME_LEN);
                const QString updated = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dHModelNodeStruct>(
                        node,
                        [&](W3dHModelNodeStruct& target) {
                            W3DEdit::WriteFixedString(target.RenderObjName, W3D_NAME_LEN, updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0401: { // W3D_CHUNK_LODMODEL_HEADER
            auto parsed = ParseChunkStruct<W3dLODModelHeaderStruct>(node);
            if (auto header = std::get_if<W3dLODModelHeaderStruct>(&parsed)) {
                const QString name = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString updated = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dLODModelHeaderStruct>(
                        node,
                        [&](W3dLODModelHeaderStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0402: { // W3D_CHUNK_LOD
            auto parsed = ParseChunkStruct<W3dLODStruct>(node);
            if (auto lod = std::get_if<W3dLODStruct>(&parsed)) {
                const QString name = ReadFixedString(lod->RenderObjName, 2 * W3D_NAME_LEN);
                const QString updated = RenameFullName(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dLODStruct>(
                        node,
                        [&](W3dLODStruct& target) {
                            W3DEdit::WriteFixedString(target.RenderObjName, 2 * W3D_NAME_LEN,
                                updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0421: { // W3D_CHUNK_COLLECTION_HEADER
            auto parsed = ParseChunkStruct<W3dCollectionHeaderStruct>(node);
            if (auto header = std::get_if<W3dCollectionHeaderStruct>(&parsed)) {
                const QString name = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString updated = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dCollectionHeaderStruct>(
                        node,
                        [&](W3dCollectionHeaderStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0422: { // W3D_CHUNK_COLLECTION_OBJ_NAME
            if (!node->data.empty()) {
                const char* raw = reinterpret_cast<const char*>(node->data.data());
                const auto len = TruncatedLength(raw, node->data.size());
                const QString name = QString::fromLatin1(raw, static_cast<int>(len));
                const QString updated = RenameFullName(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::UpdateNullTermStringChunk(node, updated.toStdString());
                    changed = true;
                }
            }
            break;
        }
        case 0x0501: { // W3D_CHUNK_EMITTER_HEADER
            auto parsed = ParseChunkStruct<W3dEmitterHeaderStruct>(node);
            if (auto header = std::get_if<W3dEmitterHeaderStruct>(&parsed)) {
                const QString name = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString updated = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dEmitterHeaderStruct>(
                        node,
                        [&](W3dEmitterHeaderStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0601: { // W3D_CHUNK_AGGREGATE_HEADER
            auto parsed = ParseChunkStruct<W3dAggregateHeaderStruct>(node);
            if (auto header = std::get_if<W3dAggregateHeaderStruct>(&parsed)) {
                const QString name = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString updated = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dAggregateHeaderStruct>(
                        node,
                        [&](W3dAggregateHeaderStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0602: { // W3D_CHUNK_AGGREGATE_INFO
            auto parsed = ParseChunkStruct<W3dAggregateInfoStruct>(node);
            if (auto info = std::get_if<W3dAggregateInfoStruct>(&parsed)) {
                const QString name = ReadFixedString(info->BaseModelName, 2 * W3D_NAME_LEN);
                const QString updated = RenameFullName(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dAggregateInfoStruct>(
                        node,
                        [&](W3dAggregateInfoStruct& target) {
                            W3DEdit::WriteFixedString(target.BaseModelName, 2 * W3D_NAME_LEN,
                                updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0701: { // W3D_CHUNK_HLOD_HEADER
            auto parsed = ParseChunkStruct<W3dHLodHeaderStruct>(node);
            if (auto header = std::get_if<W3dHLodHeaderStruct>(&parsed)) {
                const QString name = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString hierName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
                const QString updatedName = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                const QString updatedHier = RenameIfMatches(hierName, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updatedName != name || updatedHier != hierName) {
                    (void)W3DEdit::MutateStructChunk<W3dHLodHeaderStruct>(
                        node,
                        [&](W3dHLodHeaderStruct& target) {
                            if (updatedName != name) {
                                W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updatedName.toStdString());
                            }
                            if (updatedHier != hierName) {
                                W3DEdit::WriteFixedString(target.HierarchyName, W3D_NAME_LEN,
                                    updatedHier.toStdString());
                            }
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0704: { // W3D_CHUNK_HLOD_SUB_OBJECT
            auto parsed = ParseChunkStruct<W3dHLodSubObjectStruct>(node);
            if (auto sub = std::get_if<W3dHLodSubObjectStruct>(&parsed)) {
                const QString name = ReadFixedString(sub->Name, 2 * W3D_NAME_LEN);
                const QString updated = RenameFullName(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dHLodSubObjectStruct>(
                        node,
                        [&](W3dHLodSubObjectStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, 2 * W3D_NAME_LEN,
                                updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0740: { // W3D_CHUNK_BOX
            auto parsed = ParseChunkStruct<W3dBoxStruct>(node);
            if (auto box = std::get_if<W3dBoxStruct>(&parsed)) {
                const QString name = ReadFixedString(box->Name, 2 * W3D_NAME_LEN);
                const QString updated = RenameFullName(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dBoxStruct>(
                        node,
                        [&](W3dBoxStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, 2 * W3D_NAME_LEN,
                                updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0741: { // W3D_CHUNK_SPHERE
            auto parsed = ParseChunkStruct<W3dSphereStruct>(node);
            if (auto sphere = std::get_if<W3dSphereStruct>(&parsed)) {
                const QString name = ReadFixedString(sphere->Name, 2 * W3D_NAME_LEN);
                const QString updated = RenameFullName(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dSphereStruct>(
                        node,
                        [&](W3dSphereStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, 2 * W3D_NAME_LEN,
                                updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0742: { // W3D_CHUNK_RING
            auto parsed = ParseChunkStruct<W3dRingStruct>(node);
            if (auto ring = std::get_if<W3dRingStruct>(&parsed)) {
                const QString name = ReadFixedString(ring->Name, 2 * W3D_NAME_LEN);
                const QString updated = RenameFullName(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dRingStruct>(
                        node,
                        [&](W3dRingStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, 2 * W3D_NAME_LEN,
                                updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0750: { // W3D_CHUNK_NULL_OBJECT
            auto parsed = ParseChunkStruct<W3dNullObjectStruct>(node);
            if (auto nul = std::get_if<W3dNullObjectStruct>(&parsed)) {
                const QString name = ReadFixedString(nul->Name, 2 * W3D_NAME_LEN);
                const QString updated = RenameFullName(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dNullObjectStruct>(
                        node,
                        [&](W3dNullObjectStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, 2 * W3D_NAME_LEN,
                                updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0A01: { // W3D_CHUNK_SOUNDROBJ_HEADER
            auto parsed = ParseChunkStruct<W3dSoundRObjHeaderStruct>(node);
            if (auto header = std::get_if<W3dSoundRObjHeaderStruct>(&parsed)) {
                const QString name = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString updated = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::MutateStructChunk<W3dSoundRObjHeaderStruct>(
                        node,
                        [&](W3dSoundRObjHeaderStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updated.toStdString());
                        });
                    changed = true;
                }
            }
            break;
        }
        case 0x0B01: { // W3D_CHUNK_SHDMESH_NAME
            if (!node->data.empty()) {
                const char* raw = reinterpret_cast<const char*>(node->data.data());
                const auto len = TruncatedLength(raw, node->data.size());
                const QString name = QString::fromLatin1(raw, static_cast<int>(len));
                const QString updated = RenameIfMatches(name, oldBaseName, newBaseName,
                    oldFileName, newFileName);
                if (updated != name) {
                    (void)W3DEdit::UpdateNullTermStringChunk(node, updated.toStdString());
                    changed = true;
                }
            }
            break;
        }
        default:
            break;
        }

        for (const auto& child : node->children) {
            dfs(child);
        }
    };

    for (const auto& root : chunkData->getChunks()) {
        dfs(root);
    }

    if (changed) {
        setDirty(true);
    }
}

void MainWindow::onChunkEdited() {
    setDirty(true);
    handleTreeSelection();
    rebuildRenderScene();
}

void MainWindow::onAnimationHeaderRenamed(
    const QString& oldAnimationName,
    const QString& newAnimationName,
    const QString& oldHierarchyName,
    const QString& newHierarchyName)
{
    Q_UNUSED(oldAnimationName);
    Q_UNUSED(newAnimationName);

    if (!chunkData) {
        return;
    }

    if (NormalizeName(oldHierarchyName.toStdString())
        == NormalizeName(newHierarchyName.toStdString()))
    {
        return;
    }

    onMeshRenamed(
        QString(),
        QString(),
        oldHierarchyName,
        newHierarchyName);
}

void MainWindow::onMeshRenamed(const QString& oldMeshName,
    const QString& newMeshName,
    const QString& oldContainerName,
    const QString& newContainerName) {
    if (!chunkData) return;

    const bool meshChanged = oldMeshName != newMeshName;
    const bool containerChanged = oldContainerName != newContainerName;
    if (!meshChanged && !containerChanged) return;

    const bool hasOldContainer = !oldContainerName.isEmpty();
    auto sameName = [&](const QString& lhs, const QString& rhs) -> bool {
        return NormalizeName(lhs.toStdString()) == NormalizeName(rhs.toStdString());
        };

    auto renameFullName = [&](const QString& current) -> QString {
        const int dot = current.indexOf(QLatin1Char('.'));
        if (dot < 0) {
            if (containerChanged && hasOldContainer && sameName(current, oldContainerName)) {
                return newContainerName;
            }
            if (!hasOldContainer && meshChanged && sameName(current, oldMeshName)) {
                return newMeshName;
            }
            return current;
        }

        const QString containerPart = current.left(dot);
        const QString objectPart = current.mid(dot + 1);
        if (!hasOldContainer || !sameName(containerPart, oldContainerName)) {
            return current;
        }

        QString updatedContainer = containerPart;
        QString updatedObject = objectPart;
        if (containerChanged) {
            updatedContainer = newContainerName;
        }
        if (meshChanged && sameName(objectPart, oldMeshName)) {
            updatedObject = newMeshName;
        }
        return updatedContainer + QLatin1Char('.') + updatedObject;
        };

    auto renameAnimationToken = [&](const QString& current) -> QString {
        QString updated = renameFullName(current);
        if (updated != current) {
            return updated;
        }

        if (!containerChanged || !hasOldContainer) {
            return current;
        }

        if (current.startsWith(oldContainerName, Qt::CaseInsensitive)
            && current.size() > oldContainerName.size()) {
            const QChar boundary = current.at(oldContainerName.size());
            if (boundary == QLatin1Char('_')
                || boundary == QLatin1Char('.')
                || boundary == QLatin1Char('-')) {
                return newContainerName + current.mid(oldContainerName.size());
            }
        }

        return current;
        };

    std::function<void(const std::shared_ptr<ChunkItem>&)> dfs =
        [&](const std::shared_ptr<ChunkItem>& node) {
        if (!node) return;

        if (containerChanged && hasOldContainer && node->id == 0x001F) { // W3D_CHUNK_MESH_HEADER3
            auto parsed = ParseChunkStruct<W3dMeshHeader3Struct>(node);
            if (auto header = std::get_if<W3dMeshHeader3Struct>(&parsed)) {
                const QString containerName = ReadFixedString(header->ContainerName, W3D_NAME_LEN);
                if (sameName(containerName, oldContainerName)) {
                    (void)W3DEdit::MutateStructChunk<W3dMeshHeader3Struct>(
                        node,
                        [&](W3dMeshHeader3Struct& target) {
                            W3DEdit::WriteFixedString(target.ContainerName, W3D_NAME_LEN, newContainerName.toStdString());
                        });
                }
            }
        }

        if (hasOldContainer && node->id == 0x0300) { // W3D_CHUNK_HMODEL
            std::shared_ptr<ChunkItem> headerChunk;
            QString modelName;
            for (const auto& child : node->children) {
                if (child->id == 0x0301) { // W3D_CHUNK_HMODEL_HEADER
                    auto parsed = ParseChunkStruct<W3dHModelHeaderStruct>(child);
                    if (auto header = std::get_if<W3dHModelHeaderStruct>(&parsed)) {
                        modelName = ReadFixedString(header->Name, W3D_NAME_LEN);
                        headerChunk = child;
                    }
                    break;
                }
            }

            if (containerChanged && headerChunk && sameName(modelName, oldContainerName)) {
                (void)W3DEdit::MutateStructChunk<W3dHModelHeaderStruct>(
                    headerChunk,
                    [&](W3dHModelHeaderStruct& header) {
                        if (sameName(ReadFixedString(header.Name, W3D_NAME_LEN), oldContainerName)) {
                            W3DEdit::WriteFixedString(header.Name, W3D_NAME_LEN, newContainerName.toStdString());
                        }
                    });
            }

            if (sameName(modelName, oldContainerName)) {
                if (meshChanged) {
                    for (const auto& child : node->children) {
                        if (child->id != 0x0302 && child->id != 0x0303
                            && child->id != 0x0304 && child->id != 0x0306) {
                            continue;
                        }

                        auto parsed = ParseChunkStruct<W3dHModelNodeStruct>(child);
                        if (auto nodeStruct = std::get_if<W3dHModelNodeStruct>(&parsed)) {
                            const QString renderName = ReadFixedString(nodeStruct->RenderObjName, W3D_NAME_LEN);
                            if (sameName(renderName, oldMeshName)) {
                                (void)W3DEdit::MutateStructChunk<W3dHModelNodeStruct>(
                                    child,
                                    [&](W3dHModelNodeStruct& n) {
                                        W3DEdit::WriteFixedString(n.RenderObjName, W3D_NAME_LEN, newMeshName.toStdString());
                                    });
                            }
                        }
                    }
                }
            }
        }

        switch (node->id) {
        case 0x0101: { // W3D_CHUNK_HIERARCHY_HEADER
            if (!containerChanged || !hasOldContainer) break;
            auto parsed = ParseChunkStruct<W3dHierarchyStruct>(node);
            if (auto header = std::get_if<W3dHierarchyStruct>(&parsed)) {
                const QString currentName = ReadFixedString(header->Name, W3D_NAME_LEN);
                if (sameName(currentName, oldContainerName)) {
                    (void)W3DEdit::MutateStructChunk<W3dHierarchyStruct>(
                        node,
                        [&](W3dHierarchyStruct& target) {
                            if (sameName(ReadFixedString(target.Name, W3D_NAME_LEN), oldContainerName)) {
                                W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, newContainerName.toStdString());
                            }
                        });
                }
            }
            break;
        }
        case 0x0201: { // W3D_CHUNK_ANIMATION_HEADER
            auto parsed = ParseChunkStruct<W3dAnimHeaderStruct>(node);
            if (auto header = std::get_if<W3dAnimHeaderStruct>(&parsed)) {
                const QString curName = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString curHierarchy = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
                const QString updatedName = renameAnimationToken(curName);
                const QString updatedHierarchy = renameAnimationToken(curHierarchy);
                if (updatedName != curName || updatedHierarchy != curHierarchy) {
                    (void)W3DEdit::MutateStructChunk<W3dAnimHeaderStruct>(
                        node,
                        [&](W3dAnimHeaderStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updatedName.toStdString());
                            W3DEdit::WriteFixedString(target.HierarchyName, W3D_NAME_LEN, updatedHierarchy.toStdString());
                        });
                }
            }
            break;
        }
        case 0x0281: { // W3D_CHUNK_COMPRESSED_ANIMATION_HEADER
            auto parsed = ParseChunkStruct<W3dCompressedAnimHeaderStruct>(node);
            if (auto header = std::get_if<W3dCompressedAnimHeaderStruct>(&parsed)) {
                const QString curName = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString curHierarchy = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
                const QString updatedName = renameAnimationToken(curName);
                const QString updatedHierarchy = renameAnimationToken(curHierarchy);
                if (updatedName != curName || updatedHierarchy != curHierarchy) {
                    (void)W3DEdit::MutateStructChunk<W3dCompressedAnimHeaderStruct>(
                        node,
                        [&](W3dCompressedAnimHeaderStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updatedName.toStdString());
                            W3DEdit::WriteFixedString(target.HierarchyName, W3D_NAME_LEN, updatedHierarchy.toStdString());
                        });
                }
            }
            break;
        }
        case 0x02C1: { // W3D_CHUNK_MORPHANIM_HEADER
            auto parsed = ParseChunkStruct<W3dMorphAnimHeaderStruct>(node);
            if (auto header = std::get_if<W3dMorphAnimHeaderStruct>(&parsed)) {
                const QString curName = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString curHierarchy = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
                const QString updatedName = renameAnimationToken(curName);
                const QString updatedHierarchy = renameAnimationToken(curHierarchy);
                if (updatedName != curName || updatedHierarchy != curHierarchy) {
                    (void)W3DEdit::MutateStructChunk<W3dMorphAnimHeaderStruct>(
                        node,
                        [&](W3dMorphAnimHeaderStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, updatedName.toStdString());
                            W3DEdit::WriteFixedString(target.HierarchyName, W3D_NAME_LEN, updatedHierarchy.toStdString());
                        });
                }
            }
            break;
        }
        case 0x0701: { // W3D_CHUNK_HLOD_HEADER
            if (!containerChanged || !hasOldContainer) break;
            auto parsed = ParseChunkStruct<W3dHLodHeaderStruct>(node);
            if (auto header = std::get_if<W3dHLodHeaderStruct>(&parsed)) {
                const QString currentName = ReadFixedString(header->Name, W3D_NAME_LEN);
                const QString currentHierarchyName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN);
                const bool renameName = sameName(currentName, oldContainerName);
                const bool renameHierarchy = sameName(currentHierarchyName, oldContainerName);
                if (renameName || renameHierarchy) {
                    (void)W3DEdit::MutateStructChunk<W3dHLodHeaderStruct>(
                        node,
                        [&](W3dHLodHeaderStruct& target) {
                            if (sameName(ReadFixedString(target.Name, W3D_NAME_LEN), oldContainerName)) {
                                W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, newContainerName.toStdString());
                            }
                            if (sameName(ReadFixedString(target.HierarchyName, W3D_NAME_LEN), oldContainerName)) {
                                W3DEdit::WriteFixedString(target.HierarchyName, W3D_NAME_LEN, newContainerName.toStdString());
                            }
                        });
                }
            }
            break;
        }
        case 0x0704: { // W3D_CHUNK_HLOD_SUB_OBJECT
            auto parsed = ParseChunkStruct<W3dHLodSubObjectStruct>(node);
            if (auto sub = std::get_if<W3dHLodSubObjectStruct>(&parsed)) {
                const QString curName = ReadFixedString(sub->Name, 2 * W3D_NAME_LEN);
                const QString updated = renameFullName(curName);
                if (updated != curName) {
                    (void)W3DEdit::MutateStructChunk<W3dHLodSubObjectStruct>(
                        node,
                        [&](W3dHLodSubObjectStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, 2 * W3D_NAME_LEN, updated.toStdString());
                        });
                }
            }
            break;
        }
        case 0x0402: { // W3D_CHUNK_LOD
            auto parsed = ParseChunkStruct<W3dLODStruct>(node);
            if (auto lod = std::get_if<W3dLODStruct>(&parsed)) {
                const QString curName = ReadFixedString(lod->RenderObjName, 2 * W3D_NAME_LEN);
                const QString updated = renameFullName(curName);
                if (updated != curName) {
                    (void)W3DEdit::MutateStructChunk<W3dLODStruct>(
                        node,
                        [&](W3dLODStruct& target) {
                            W3DEdit::WriteFixedString(target.RenderObjName, 2 * W3D_NAME_LEN, updated.toStdString());
                        });
                }
            }
            break;
        }
        case 0x0740: { // W3D_CHUNK_BOX
            auto parsed = ParseChunkStruct<W3dBoxStruct>(node);
            if (auto box = std::get_if<W3dBoxStruct>(&parsed)) {
                const QString curName = ReadFixedString(box->Name, 2 * W3D_NAME_LEN);
                const QString updated = renameFullName(curName);
                if (updated != curName) {
                    (void)W3DEdit::MutateStructChunk<W3dBoxStruct>(
                        node,
                        [&](W3dBoxStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, 2 * W3D_NAME_LEN, updated.toStdString());
                        });
                }
            }
            break;
        }
        case 0x0750: { // W3D_CHUNK_NULL_OBJECT
            auto parsed = ParseChunkStruct<W3dNullObjectStruct>(node);
            if (auto nul = std::get_if<W3dNullObjectStruct>(&parsed)) {
                const QString curName = ReadFixedString(nul->Name, 2 * W3D_NAME_LEN);
                const QString updated = renameFullName(curName);
                if (updated != curName) {
                    (void)W3DEdit::MutateStructChunk<W3dNullObjectStruct>(
                        node,
                        [&](W3dNullObjectStruct& target) {
                            W3DEdit::WriteFixedString(target.Name, 2 * W3D_NAME_LEN, updated.toStdString());
                        });
                }
            }
            break;
        }
        case 0x0422: { // W3D_CHUNK_COLLECTION_OBJ_NAME
            if (node->data.empty()) break;
            const char* raw = reinterpret_cast<const char*>(node->data.data());
            const auto len = TruncatedLength(raw, node->data.size());
            const QString curName = QString::fromLatin1(raw, static_cast<int>(len));
            const QString updated = renameFullName(curName);
            if (updated != curName) {
                (void)W3DEdit::UpdateNullTermStringChunk(node, updated.toStdString());
            }
            break;
        }
        default:
            break;
        }

        for (const auto& child : node->children) {
            dfs(child);
        }
        };

    for (const auto& root : chunkData->getChunks()) {
        dfs(root);
    }
}

void MainWindow::updateEditorForChunk(const std::shared_ptr<ChunkItem>& chunk) {
    currentChunk = chunk;

    meshEditor->setChunk(nullptr);
    meshUserTextEditor->setChunk(nullptr);
    textureNameEditor->setChunk(nullptr);
    textureInfoEditor->setChunk(nullptr);
    hierarchyHeaderEditor->setChunk(nullptr);
    animationHierarchyEditor->setChunk(nullptr);
    materialNameEditor->setChunk(nullptr);
    transformNodeEditor->setChunk(nullptr);
    stage0ArgsEditor->setChunk(nullptr);
    stage1ArgsEditor->setChunk(nullptr);
    materialEditor->setChunk(nullptr);
    shaderEditor->setChunk(nullptr);
    surfaceTypeEditor->setChunk(nullptr);
    triangleSurfaceTypeEditor->setChunk(nullptr);

    if (!chunk) {
        if (editorPlaceholderLabel) {
            editorPlaceholderLabel->setText(tr("Select a supported chunk to edit."));
        }
        editorStack->setCurrentWidget(editorPlaceholder);
        if (editorScrollArea && editorScrollArea->isVisible() && detailSplitter) {
            detailSplitterStateCache = detailSplitter->saveState();
        }
        if (editorScrollArea) editorScrollArea->setVisible(false);
        return;
    }

    const auto showEditor = [&]() {
        if (!editorScrollArea) return;
        const bool wasHidden = !editorScrollArea->isVisible();
        editorScrollArea->setVisible(true);

        if (!detailSplitter) return;
        if (wasHidden && !detailSplitterStateCache.isEmpty()) {
            if (detailSplitter->restoreState(detailSplitterStateCache)) {
                return;
            }
        }
        const auto sizes = detailSplitter->sizes();
        if (sizes.size() < 2) return;
        if (!wasHidden) return;
        if (sizes[1] != 0) return;
        const int total = sizes[0] + sizes[1];
        if (total <= 0) return;
        const int editorHeight = std::min(180, std::max(0, total - 120));
        detailSplitter->setSizes({ total - editorHeight, editorHeight });
        };

    if (!activeChunkSourceEditable()) {
        if (editorPlaceholderLabel) {
            editorPlaceholderLabel->setText(
                tr("This file is loaded as supplemental render data and is read-only here."));
        }
        editorStack->setCurrentWidget(editorPlaceholder);
        showEditor();
        return;
    }

    if (editorPlaceholderLabel) {
        editorPlaceholderLabel->setText(tr("Select a supported chunk to edit."));
    }

    switch (chunk->id) {
    case 0x000C: // W3D_CHUNK_MESH_USER_TEXT
        meshUserTextEditor->setChunk(chunk);
        editorStack->setCurrentWidget(meshUserTextEditor);
        showEditor();
        break;
    case 0x001F: // W3D_CHUNK_MESH_HEADER3
        meshEditor->setChunk(chunk);
        editorStack->setCurrentWidget(meshEditor);
        showEditor();
        break;
    case 0x0101: // W3D_CHUNK_HIERARCHY_HEADER
        hierarchyHeaderEditor->setChunk(chunk);
        editorStack->setCurrentWidget(hierarchyHeaderEditor);
        showEditor();
        break;
    case 0x0201: // W3D_CHUNK_ANIMATION_HEADER
    case 0x0281: // W3D_CHUNK_COMPRESSED_ANIMATION_HEADER
    case 0x02C1: // W3D_CHUNK_MORPHANIM_HEADER
        animationHierarchyEditor->setChunk(chunk);
        editorStack->setCurrentWidget(animationHierarchyEditor);
        showEditor();
        break;
    case 0x0032: // W3D_CHUNK_TEXTURE_NAME
        textureNameEditor->setChunk(chunk);
        editorStack->setCurrentWidget(textureNameEditor);
        showEditor();
        break;
    case 0x0033: // W3D_CHUNK_TEXTURE_INFO
        textureInfoEditor->setChunk(chunk);
        editorStack->setCurrentWidget(textureInfoEditor);
        showEditor();
        break;
    case 0x002C: // W3D_CHUNK_VERTEX_MATERIAL_NAME
        materialNameEditor->setChunk(chunk);
        editorStack->setCurrentWidget(materialNameEditor);
        showEditor();
        break;
    case 0x0424: // W3D_CHUNK_TRANSFORM_NODE
        transformNodeEditor->setChunk(chunk);
        editorStack->setCurrentWidget(transformNodeEditor);
        showEditor();
        break;
    case 0x002E: // W3D_CHUNK_ARG0
        stage0ArgsEditor->setChunk(chunk);
        editorStack->setCurrentWidget(stage0ArgsEditor);
        showEditor();
        break;
    case 0x002F: // W3D_CHUNK_ARG1
        stage1ArgsEditor->setChunk(chunk);
        editorStack->setCurrentWidget(stage1ArgsEditor);
        showEditor();
        break;
    case 0x002D: // W3D_CHUNK_VERTEX_MATERIAL_INFO
        materialEditor->setChunk(chunk);
        editorStack->setCurrentWidget(materialEditor);
        showEditor();
        break;
    case 0x0029: // W3D_CHUNK_SHADERS
        shaderEditor->setChunk(chunk);
        editorStack->setCurrentWidget(shaderEditor);
        showEditor();
        break;
    case 0x0020: // W3D_CHUNK_TRIANGLES
        triangleSurfaceTypeEditor->setChunk(chunk);
        editorStack->setCurrentWidget(triangleSurfaceTypeEditor);
        showEditor();
        break;
    case 0x16490430: // W3D_CHUNK_SHDDEF_CLASS_VARS
        surfaceTypeEditor->setChunk(chunk);
        editorStack->setCurrentWidget(surfaceTypeEditor);
        showEditor();
        break;
    default:
        editorStack->setCurrentWidget(editorPlaceholder);
        if (editorScrollArea && editorScrollArea->isVisible() && detailSplitter) {
            detailSplitterStateCache = detailSplitter->saveState();
        }
        if (editorScrollArea) editorScrollArea->setVisible(false);
        break;
    }
}

void MainWindow::updateRawHex(const std::shared_ptr<ChunkItem>& chunk) {
    if (!rawHexEdit || !rawHexToggle) return;
    if (!rawHexToggle->isChecked() || !chunk) {
        rawHexEdit->clear();
        if (rawHexContainer) rawHexContainer->setTitle(tr("Raw Hex"));
        return;
    }
    rawHexEdit->setPlainText(FormatHexBlock(chunk->data));
    if (rawHexContainer) {
        rawHexContainer->setTitle(tr("Raw Hex (%1 bytes)").arg(static_cast<int>(chunk->data.size())));
    }
}

void MainWindow::clearArchiveRenderContext() {
    currentArchiveRenderPath.clear();
    currentArchiveRenderEntryId = 0;
    currentArchiveRenderEntryPath.clear();
    currentArchiveRenderEntries.clear();
    currentArchiveTextureEntries.clear();
    currentArchiveTextureEntryIds.clear();
    currentArchiveTextureSourcesById.clear();
    currentArchiveRenderEntryReferenceNamesById.clear();
    currentArchiveSupplementalRoots.clear();
    currentArchiveLoadedSupplementalEntryIds.clear();
}

void MainWindow::clearExternalRenderContext() {
    currentExternalRenderAssets.clear();
    currentExternalRenderAssetPaths.clear();
    currentAggregateRenderDependencyAssets.clear();
    currentRenderTriedSkeletonAutoload = false;
    currentRenderSuppressedMissingHierarchyKey.clear();
    currentRenderSuppressedMissingMeshKey.clear();
    currentRenderTextureDirectory.clear();
    currentRenderSuppressedMissingTextureKey.clear();
    currentRenderAggregateDependencyAttemptKey.clear();
    currentRenderHierarchyDependencyAttemptKey.clear();
    currentRenderAnimationDrafts.clear();
    resetRenderAnimationPlayback();
}

void MainWindow::resetRenderAnimationPlayback() {
    currentRenderAnimationPlayback = {};
    currentRenderAnimationPlayback.activeAnimationIndex = -1;
    currentRenderAnimationPlayback.timeSeconds = 0.0f;
    currentRenderAnimationPlayback.playing = false;
    currentRenderAnimationPlayback.loop = true;
    currentRenderAnimationPlayback.speed = 1.0f;
    currentRenderActiveClipIdentity.reset();
    currentRenderAnimationPrepState = {};
    currentRenderAnimationBlendState = {};
    if (renderAnimationPlaybackTimer) {
        renderAnimationPlaybackTimer->stop();
    }
    renderAnimationPlaybackElapsed.invalidate();
    syncRenderAnimationPlaybackToViewport();
    syncActiveRenderAnimationDraftToViewport();
}

void MainWindow::syncRenderAnimationPlaybackToViewport() {
    if (renderViewport) {
        renderViewport->SetAnimationPlayback(currentRenderAnimationPlayback);
    }
}

OW3D::Render::RenderAnimationEditDraft* MainWindow::ensureRenderAnimationEditDraft(
    const OW3D::Render::RenderAnimationClip& clip,
    const std::shared_ptr<ChunkItem>& animationChunk)
{
    if (!animationChunk || animationChunk->id != kChunkAnimation) {
        return nullptr;
    }

    auto [it, inserted] = currentRenderAnimationDrafts.try_emplace(animationChunk.get());
    auto& draft = it->second;
    draft.sourceAnimationChunk = animationChunk.get();
    if (inserted || draft.numFrames == 0u) {
        draft.numFrames = clip.numFrames;
    }
    if (inserted || draft.frameRate <= 0.0f) {
        draft.frameRate = clip.frameRate;
    }
    return &draft;
}

const OW3D::Render::RenderAnimationEditDraft* MainWindow::findRenderAnimationEditDraftForClip(
    const OW3D::Render::RenderAnimationClip& clip) const
{
    if (!clip.sourceAnimationChunk) {
        return nullptr;
    }

    const auto it = currentRenderAnimationDrafts.find(clip.sourceAnimationChunk);
    if (it == currentRenderAnimationDrafts.end()) {
        return nullptr;
    }
    return &it->second;
}

const OW3D::Render::RenderAnimationEditDraft* MainWindow::findActiveRenderAnimationEditDraft() const
{
    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    const auto& animations = currentRenderSceneResult.scene.animations;
    if (activeIndex < 0 || activeIndex >= static_cast<int>(animations.size())) {
        return nullptr;
    }

    const auto& clip = animations[static_cast<std::size_t>(activeIndex)];
    if (!clip.sourceAnimationChunk) {
        return nullptr;
    }

    return findRenderAnimationEditDraftForClip(clip);
}

void MainWindow::syncActiveRenderAnimationDraftToViewport() {
    if (!renderViewport) {
        return;
    }

    if (const auto* draft = findActiveRenderAnimationEditDraft()) {
        renderViewport->SetAnimationEditDraft(*draft);
    }
    else {
        renderViewport->SetAnimationEditDraft(std::nullopt);
    }
}

void MainWindow::syncRenderAnimationEditingStateToViewport() {
    if (!renderViewport) {
        return;
    }

    QString editKeysReason;
    const bool editableActiveClip = ResolveEditablePrimaryRenderAnimation(
        chunkData.get(),
        currentRenderSceneResult,
        currentRenderAnimationPlayback,
        nullptr,
        nullptr,
        nullptr,
        &editKeysReason);
    renderViewport->SetAnimationEditingState(
        currentRenderAnimationEditKeysEnabled && editableActiveClip,
        editableActiveClip,
        editableActiveClip ? QString() : editKeysReason);
}

int MainWindow::findRenderAnimationIndexByIdentity(
    const RenderAnimationClipIdentity& identity) const
{
    const auto& animations = currentRenderSceneResult.scene.animations;
    for (std::size_t i = 0; i < animations.size(); ++i) {
        const RenderAnimationClipIdentity candidate =
            BuildRenderAnimationClipIdentity(animations[i]);
        if (candidate == identity) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int MainWindow::findCompatibleRenderHierarchyIndexForAnimation(int animationIndex) const
{
    const auto& animations = currentRenderSceneResult.scene.animations;
    if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) {
        return -1;
    }

    const auto& clip = animations[static_cast<std::size_t>(animationIndex)];
    return FindCompatibleHierarchyIndexForAnimation(
        currentRenderSceneResult.scene,
        animationIndex,
        clip.hierarchyName.empty() ? nullptr : &clip.hierarchyName);
}

bool MainWindow::tryLoadRenderSessionAsset(
    const QString& filePath,
    RenderSessionAssetRole role,
    RenderSessionAsset& outAsset,
    QString* outError) const
{
    outAsset = {};

    const QString absolutePath =
        QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
    const QString normalizedPath = NormalizeAbsolutePathKey(absolutePath);
    if (normalizedPath.isEmpty()) {
        if (outError) {
            *outError = tr("The selected file path is invalid.");
        }
        return false;
    }
    if (currentExternalRenderAssetPaths.contains(normalizedPath)) {
        if (outError) {
            *outError = tr("This render asset is already loaded.");
        }
        return false;
    }

    std::vector<std::shared_ptr<ChunkItem>> loadedRoots;
    if (!LoadSupplementalRenderRootsFromFile(absolutePath, loadedRoots, outError)) {
        return false;
    }

    auto labelInUse = [&](const QString& candidate) {
        if (!currentFilePath.isEmpty()) {
            const QString primaryLabel = BuildPrimaryRenderSourceLabel(
                currentFilePath,
                currentArchiveRenderPath,
                currentArchiveRenderEntryPath,
                chunkData.get());
            if (primaryLabel.compare(candidate, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        for (const auto& asset : currentExternalRenderAssets) {
            if (asset.displayLabel.compare(candidate, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    };

    QString displayLabel = QFileInfo(absolutePath).fileName();
    if (displayLabel.isEmpty()) {
        displayLabel = absolutePath;
    }
    if (labelInUse(displayLabel)) {
        const QString parentDirName = QFileInfo(QFileInfo(absolutePath).absolutePath()).fileName();
        if (!parentDirName.isEmpty()) {
            const QString parentLabel =
                tr("%1 [%2]").arg(displayLabel, parentDirName);
            if (!labelInUse(parentLabel)) {
                displayLabel = parentLabel;
            }
        }
    }
    if (labelInUse(displayLabel)) {
        displayLabel = absolutePath;
    }

    outAsset.role = role;
    outAsset.filePath = absolutePath;
    outAsset.sourceKey = normalizedPath;
    outAsset.sourceDisplayPath = absolutePath;
    outAsset.displayLabel = displayLabel;
    outAsset.roots = std::move(loadedRoots);
    outAsset.hierarchyNames = CollectHierarchyNamesFromRoots(outAsset.roots);
    outAsset.meshCount = CollectMeshCountFromRoots(outAsset.roots);
    outAsset.animationCount = CollectAnimationCountFromRoots(outAsset.roots);
    return true;
}

bool MainWindow::tryLoadRenderSessionArchiveAsset(
    const QString& archivePath,
    RenderSessionAssetRole role,
    RenderSessionAsset& outAsset,
    QString* outError)
{
    outAsset = {};

    const QString absoluteArchivePath =
        QDir::cleanPath(QFileInfo(archivePath).absoluteFilePath());
    if (absoluteArchivePath.isEmpty()) {
        if (outError) {
            *outError = tr("The selected archive path is invalid.");
        }
        return false;
    }

    ChunkData archiveChunkData;
    LoadedArchiveRenderContext archiveContext{};
    if (!LoadW3DFromMixArchive(
        this,
        absoluteArchivePath,
        archiveChunkData,
        outError,
        &archiveContext))
    {
        return false;
    }

    const QString sourceKey =
        BuildArchiveEntrySourceKey(archiveContext.archivePath, archiveContext.selectedEntryId);
    if (currentExternalRenderAssetPaths.contains(sourceKey)) {
        if (outError) {
            *outError = tr("This render asset is already loaded.");
        }
        return false;
    }

    QString entryDisplayLabel = QFileInfo(archiveContext.selectedEntryPath).fileName();
    if (entryDisplayLabel.isEmpty()) {
        entryDisplayLabel = archiveContext.selectedEntryPath;
    }
    if (entryDisplayLabel.isEmpty()) {
        entryDisplayLabel = tr("Archive Entry %1")
            .arg(archiveContext.selectedEntryId, 8, 16, QLatin1Char('0'))
            .toUpper();
    }

    auto labelInUse = [&](const QString& candidate) {
        if (!currentFilePath.isEmpty()) {
            const QString primaryLabel = BuildPrimaryRenderSourceLabel(
                currentFilePath,
                currentArchiveRenderPath,
                currentArchiveRenderEntryPath,
                chunkData.get());
            if (primaryLabel.compare(candidate, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        for (const auto& asset : currentExternalRenderAssets) {
            if (asset.displayLabel.compare(candidate, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    };

    QString displayLabel = entryDisplayLabel;
    if (labelInUse(displayLabel)) {
        const QString archiveLabel = QFileInfo(archiveContext.archivePath).fileName();
        if (!archiveLabel.isEmpty()) {
            const QString qualifiedLabel =
                tr("%1 [%2]").arg(displayLabel, archiveLabel);
            if (!labelInUse(qualifiedLabel)) {
                displayLabel = qualifiedLabel;
            }
        }
    }
    if (labelInUse(displayLabel)) {
        displayLabel = BuildArchiveEntryDisplayPath(
            archiveContext.archivePath,
            archiveContext.selectedEntryPath);
    }

    const auto& parsedRoots = archiveChunkData.getChunks();
    if (parsedRoots.empty()) {
        if (outError) {
            *outError = tr("The selected archive entry does not contain any W3D/WLT roots.");
        }
        return false;
    }

    outAsset.role = role;
    outAsset.filePath = archiveContext.archivePath;
    outAsset.sourceKey = sourceKey;
    outAsset.sourceDisplayPath = BuildArchiveEntryDisplayPath(
        archiveContext.archivePath,
        archiveContext.selectedEntryPath);
    outAsset.displayLabel = displayLabel;
    outAsset.roots.assign(parsedRoots.begin(), parsedRoots.end());
    outAsset.hierarchyNames = CollectHierarchyNamesFromRoots(outAsset.roots);
    outAsset.meshCount = CollectMeshCountFromRoots(outAsset.roots);
    outAsset.animationCount = CollectAnimationCountFromRoots(outAsset.roots);
    return true;
}

void MainWindow::refreshRenderAssetList() {
    if (!renderAssetsTree) {
        return;
    }

    const QString selectedPathKey = renderAssetsTree->currentItem()
        ? renderAssetsTree->currentItem()->data(0, kRenderAssetPathRole).toString()
        : QString();

    QSignalBlocker blocker(renderAssetsTree);
    renderAssetsTree->clear();

    if (!currentFilePath.isEmpty()) {
        int hierarchyCount = 0;
        int meshCount = 0;
        int animationCount = 0;
        if (chunkData) {
            const auto& primaryRoots = chunkData->getChunks();
            hierarchyCount = CollectHierarchyNamesFromRoots(primaryRoots).size();
            meshCount = CollectMeshCountFromRoots(primaryRoots);
            animationCount = CollectAnimationCountFromRoots(primaryRoots);
        }

        auto* primaryItem = new QTreeWidgetItem(renderAssetsTree);
        QString primaryRole = tr("Primary");
        if (meshCount > 0 && hierarchyCount > 0) {
            primaryRole = tr("Model+Skeleton");
        }
        else if (meshCount > 0) {
            primaryRole = tr("Model");
        }
        else if (hierarchyCount > 0) {
            primaryRole = tr("Skeleton");
        }
        else if (animationCount > 0) {
            primaryRole = tr("Animation");
        }
        primaryItem->setText(0, primaryRole);
        primaryItem->setText(1, BuildPrimaryRenderSourceName(
            currentFilePath,
            currentArchiveRenderPath,
            currentArchiveRenderEntryPath));
        primaryItem->setText(2, QString::number(hierarchyCount));
        primaryItem->setText(3, QString::number(animationCount));
        primaryItem->setToolTip(1, !currentArchiveRenderPath.isEmpty()
            ? BuildArchiveEntryDisplayPath(
                currentArchiveRenderPath,
                currentArchiveRenderEntryPath)
            : currentFilePath);
        primaryItem->setFlags(primaryItem->flags() & ~Qt::ItemIsDropEnabled);
    }

    for (const auto& asset : currentExternalRenderAssets) {
        auto* item = new QTreeWidgetItem(renderAssetsTree);
        item->setText(0, RenderSessionAssetRoleLabel(asset));
        item->setText(1, asset.displayLabel);
        item->setText(2, QString::number(asset.hierarchyNames.size()));
        item->setText(3, QString::number(asset.animationCount));
        item->setToolTip(1, RenderSessionAssetSourceDisplayPath(asset));
        item->setData(0, kRenderAssetPathRole, RenderSessionAssetSourceKey(asset));
        item->setData(0, kRenderAssetRoleRole, static_cast<int>(asset.role));
        if (!selectedPathKey.isEmpty()
            && selectedPathKey.compare(
                RenderSessionAssetSourceKey(asset),
                Qt::CaseInsensitive) == 0)
        {
            renderAssetsTree->setCurrentItem(item);
        }
    }

    if (renderAssetsTree->topLevelItemCount() == 0) {
        auto* emptyItem = new QTreeWidgetItem(renderAssetsTree);
        emptyItem->setText(0, tr("No render assets loaded."));
        emptyItem->setFlags(emptyItem->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
    }

    const bool hasAnimationLibraries = std::any_of(
        currentExternalRenderAssets.begin(),
        currentExternalRenderAssets.end(),
        [](const RenderSessionAsset& asset) {
            return asset.role == RenderSessionAssetRole::AnimationLibrary;
        });
    if (renderClearAnimationsButton) {
        renderClearAnimationsButton->setEnabled(hasAnimationLibraries);
    }
    if (renderRemoveAssetButton) {
        const QTreeWidgetItem* item = renderAssetsTree->currentItem();
        renderRemoveAssetButton->setEnabled(item && item->data(0, kRenderAssetPathRole).isValid());
    }
}

void MainWindow::refreshRenderAnimationList() {
    if (!renderAnimationsTree) {
        return;
    }

    QSignalBlocker blocker(renderAnimationsTree);
    renderAnimationsTree->clear();

    const auto& animations = currentRenderSceneResult.scene.animations;
    if (animations.empty()) {
        auto* emptyItem = new QTreeWidgetItem(renderAnimationsTree);
        emptyItem->setText(0, tr("No animations loaded."));
        emptyItem->setFlags(emptyItem->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
        return;
    }

    std::unordered_map<std::string, QTreeWidgetItem*> groups;
    for (std::size_t i = 0; i < animations.size(); ++i) {
        const auto& clip = animations[i];
        const QString hierarchyName = clip.hierarchyName.empty()
            ? tr("(No Hierarchy)")
            : QString::fromStdString(clip.hierarchyName);
        const std::string groupKey = QString(hierarchyName).trimmed().toLower().toStdString();

        QTreeWidgetItem* groupItem = nullptr;
        const auto groupIt = groups.find(groupKey);
        if (groupIt == groups.end()) {
            groupItem = new QTreeWidgetItem(renderAnimationsTree);
            groupItem->setText(0, hierarchyName);
            groupItem->setFirstColumnSpanned(false);
            groupItem->setExpanded(true);
            groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsSelectable);
            groups.emplace(groupKey, groupItem);
        }
        else {
            groupItem = groupIt->second;
        }

        const bool compatible = SceneHasCompatibleHierarchyForAnimation(
            currentRenderSceneResult.scene,
            static_cast<int>(i));
        const bool playable = compatible && clip.supportedForPlayback;

        auto* child = new QTreeWidgetItem(groupItem);
        child->setText(0, QString::fromStdString(clip.fullName));
        child->setText(1, clip.sourceFileLabel.empty()
            ? tr("(Unknown Source)")
            : QString::fromStdString(clip.sourceFileLabel));
        child->setText(2, !clip.supportedForPlayback
            ? tr("Unsupported")
            : (compatible ? tr("Ready") : tr("No Matching Hierarchy")));
        child->setData(0, kRenderAnimationIndexRole, static_cast<int>(i));
        const QString clipTooltip = tr("Hierarchy: %1\nSource: %2\nFrames: %3\nFPS: %4")
            .arg(hierarchyName)
            .arg(child->text(1))
            .arg(QString::number(clip.numFrames))
            .arg(QString::number(clip.frameRate, 'f', 2));
        child->setToolTip(
            0,
            clipTooltip);
        if (!playable) {
            child->setFlags(child->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
        }
    }
}

void MainWindow::refreshRenderPlaybackSelection() {
    if (!renderAnimationsTree) {
        return;
    }

    QSignalBlocker blocker(renderAnimationsTree);

    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    if (activeIndex < 0) {
        renderAnimationsTree->clearSelection();
        return;
    }

    QTreeWidgetItem* selectedItem = nullptr;
    for (int i = 0; i < renderAnimationsTree->topLevelItemCount() && !selectedItem; ++i) {
        QTreeWidgetItem* groupItem = renderAnimationsTree->topLevelItem(i);
        for (int j = 0; j < groupItem->childCount(); ++j) {
            QTreeWidgetItem* child = groupItem->child(j);
            if (child->data(0, kRenderAnimationIndexRole).toInt() == activeIndex) {
                selectedItem = child;
                groupItem->setExpanded(true);
                break;
            }
        }
    }

    if (selectedItem) {
        renderAnimationsTree->setCurrentItem(selectedItem);
        renderAnimationsTree->scrollToItem(selectedItem, QAbstractItemView::PositionAtCenter);
    }
    else {
        renderAnimationsTree->clearSelection();
    }
}

void MainWindow::refreshRenderPlaybackControls() {
    const auto& animations = currentRenderSceneResult.scene.animations;
    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    const bool activeValid =
        activeIndex >= 0 && activeIndex < static_cast<int>(animations.size());

    const OW3D::Render::RenderAnimationClip* activeClip =
        activeValid ? &animations[static_cast<std::size_t>(activeIndex)] : nullptr;
    const OW3D::Render::RenderAnimationEditDraft* activeDraft =
        findActiveRenderAnimationEditDraft();
    const bool hasActiveDraft =
        HasPendingRenderAnimationDraftChanges(activeClip, activeDraft);
    const uint32_t activeFrameCount =
        activeClip ? EffectiveRenderAnimationFrameCount(*activeClip, activeDraft) : 0u;
    const float activeFrameRate =
        activeClip ? EffectiveRenderAnimationFrameRate(*activeClip, activeDraft) : 0.0f;
    QString editKeysReason;
    const bool editableActiveClip = ResolveEditablePrimaryRenderAnimation(
        chunkData.get(),
        currentRenderSceneResult,
        currentRenderAnimationPlayback,
        nullptr,
        nullptr,
        nullptr,
        &editKeysReason);
    if (!editableActiveClip && currentRenderAnimationEditKeysEnabled) {
        currentRenderAnimationEditKeysEnabled = false;
    }

    bool selectedPlayable = false;
    if (renderAnimationsTree && renderAnimationsTree->currentItem()) {
        const QTreeWidgetItem* item = renderAnimationsTree->currentItem();
        selectedPlayable = item->data(0, kRenderAnimationIndexRole).isValid()
            && (item->flags() & Qt::ItemIsEnabled);
    }

    if (renderPlayPauseButton) {
        renderPlayPauseButton->setText(
            currentRenderAnimationPlayback.playing ? tr("Pause") : tr("Play"));
        renderPlayPauseButton->setEnabled(activeClip != nullptr || selectedPlayable);
    }
    if (renderStopButton) {
        renderStopButton->setEnabled(activeClip != nullptr);
    }
    if (renderAnimationExportGifButton) {
        renderAnimationExportGifButton->setEnabled(
            activeClip != nullptr && SceneHasRenderableMeshData(currentRenderSceneResult));
    }
    if (renderAnimationResetDraftButton) {
        renderAnimationResetDraftButton->setEnabled(hasActiveDraft);
    }
    if (renderAnimationLoopToggle) {
        QSignalBlocker blocker(renderAnimationLoopToggle);
        renderAnimationLoopToggle->setChecked(currentRenderAnimationPlayback.loop);
        renderAnimationLoopToggle->setEnabled(activeClip != nullptr);
    }
    if (renderAnimationEditKeysToggle) {
        QSignalBlocker blocker(renderAnimationEditKeysToggle);
        renderAnimationEditKeysToggle->setChecked(
            currentRenderAnimationEditKeysEnabled && editableActiveClip);
        renderAnimationEditKeysToggle->setEnabled(activeClip != nullptr && editableActiveClip);
    }
    if (renderAnimationSpeedSpin) {
        QSignalBlocker blocker(renderAnimationSpeedSpin);
        renderAnimationSpeedSpin->setValue(currentRenderAnimationPlayback.speed);
        renderAnimationSpeedSpin->setEnabled(activeClip != nullptr);
    }

    suppressRenderAnimationFrameSliderChange = true;
    int displayedFrame = 0;
    int displayedMaxFrame = 0;
    if (renderAnimationFrameSlider) {
        renderAnimationFrameSlider->setEnabled(activeClip != nullptr);
        if (!activeClip || activeFrameRate <= 0.0f || activeFrameCount == 0u) {
            renderAnimationFrameSlider->setRange(0, 0);
            renderAnimationFrameSlider->setValue(0);
        }
        else {
            const int maxFrame = std::max(0, static_cast<int>(activeFrameCount) - 1);
            const float frameFloat =
                currentRenderAnimationPlayback.timeSeconds * activeFrameRate;
            const int currentFrame = std::clamp(
                static_cast<int>(std::round(frameFloat)),
                0,
                maxFrame);
            renderAnimationFrameSlider->setRange(0, maxFrame);
            renderAnimationFrameSlider->setValue(currentFrame);
            displayedFrame = currentFrame;
            displayedMaxFrame = maxFrame;
        }
    }
    suppressRenderAnimationFrameSliderChange = false;

    if (renderAnimationFrameLabel) {
        renderAnimationFrameLabel->setEnabled(activeClip != nullptr);
        renderAnimationFrameLabel->setText(
            tr("Frame %1 / %2").arg(displayedFrame).arg(displayedMaxFrame));
    }

    if (!activeClip) {
        if (renderAnimationClipLabel) {
            renderAnimationClipLabel->setText(tr("Clip: none"));
        }
        if (renderAnimationMetadataLabel) {
            renderAnimationMetadataLabel->setText(tr("No animation selected."));
        }
        if (renderAnimationEditStatusLabel) {
            renderAnimationEditStatusLabel->setText(tr("Edit Keys: no clip selected."));
        }
        return;
    }

    const QString clipName = QString::fromStdString(activeClip->fullName);
    const QString hierarchyName = QString::fromStdString(activeClip->hierarchyName);
    const QString sourceLabel = activeClip->sourceFileLabel.empty()
        ? tr("(Unknown Source)")
        : QString::fromStdString(activeClip->sourceFileLabel);
    const float duration = AnimationClipDurationSeconds(*activeClip, activeDraft);
    const float currentFrame = activeFrameRate > 0.0f
        ? currentRenderAnimationPlayback.timeSeconds * activeFrameRate
        : 0.0f;

    if (renderAnimationClipLabel) {
        renderAnimationClipLabel->setText(
            tr("Clip: %1").arg(clipName.isEmpty() ? tr("(unnamed)") : clipName));
    }
    if (renderAnimationMetadataLabel) {
        const QString metadataText =
            tr("Hierarchy: %1 | Source: %2 | Frames: %3 | FPS: %4 | Frame: %5 | Time: %6 / %7 s")
                .arg(hierarchyName.isEmpty() ? tr("(none)") : hierarchyName)
                .arg(sourceLabel)
                .arg(QString::number(activeFrameCount))
                .arg(QString::number(activeFrameRate, 'f', 2))
                .arg(QString::number(currentFrame, 'f', 2))
                .arg(QString::number(currentRenderAnimationPlayback.timeSeconds, 'f', 2))
                .arg(QString::number(duration, 'f', 2));
        renderAnimationMetadataLabel->setText(
            metadataText);
    }
    if (renderAnimationEditStatusLabel) {
        QString statusText = editableActiveClip
            ? (currentRenderAnimationEditKeysEnabled
                ? tr("Edit Keys: enabled for the active raw clip.")
                : tr("Edit Keys: available for the active raw clip."))
            : tr("Edit Keys unavailable: %1").arg(
                editKeysReason.isEmpty()
                    ? tr("Only raw clips from the current file are editable.")
                    : editKeysReason);
        if (hasActiveDraft) {
            statusText += tr(" Preview edits are pending save. Use Reset Draft to discard them.");
        }
        renderAnimationEditStatusLabel->setText(statusText);
    }

    refreshRenderAnimationPivotSheetFrameHighlight();
}

void MainWindow::refreshRenderAnimationPrepControls() {
    if (!renderAnimationPrepGroup
        || !renderAnimationPrepSourceCombo
        || !renderAnimationPrepStaticPoseFrameSpin
        || !renderAnimationPrepApplyButton
        || !renderAnimationPrepFreezePoseButton
        || !renderAnimationPrepFitSourceButton
        || !renderAnimationPrepStatusLabel) {
        return;
    }

    const auto& animations = currentRenderSceneResult.scene.animations;
    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    const bool activeValid =
        activeIndex >= 0 && activeIndex < static_cast<int>(animations.size());
    const OW3D::Render::RenderAnimationClip* activeClip =
        activeValid ? &animations[static_cast<std::size_t>(activeIndex)] : nullptr;
    const OW3D::Render::RenderAnimationEditDraft* activeDraft =
        activeClip ? findRenderAnimationEditDraftForClip(*activeClip) : nullptr;
    const uint32_t activeFrameCount =
        activeClip ? EffectiveRenderAnimationFrameCount(*activeClip, activeDraft) : 0u;
    const int activeMaxFrame = activeFrameCount > 0u
        ? std::max(0, static_cast<int>(activeFrameCount) - 1)
        : 0;

    QString editReason;
    const bool editableActiveClip = ResolveEditablePrimaryRenderAnimation(
        chunkData.get(),
        currentRenderSceneResult,
        currentRenderAnimationPlayback,
        nullptr,
        nullptr,
        nullptr,
        &editReason);
    const int hierarchyIndex =
        (editableActiveClip && activeClip) ? findCompatibleRenderHierarchyIndexForAnimation(activeIndex) : -1;
    const OW3D::Render::RenderHierarchy* hierarchy =
        hierarchyIndex >= 0
        && hierarchyIndex < static_cast<int>(currentRenderSceneResult.scene.hierarchies.size())
        ? &currentRenderSceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)]
        : nullptr;
    const bool controlsEnabled = editableActiveClip && hierarchy != nullptr;

    currentRenderAnimationPrepState.staticPoseFrame =
        std::clamp(currentRenderAnimationPrepState.staticPoseFrame, 0, activeMaxFrame);

    suppressRenderAnimationPrepUiSignals = true;
    {
        QSignalBlocker comboBlocker(renderAnimationPrepSourceCombo);
        renderAnimationPrepSourceCombo->clear();
        renderAnimationPrepSourceCombo->addItem(tr("(No Source Clip)"), -1);

        int desiredTargetIndex = -1;
        if (currentRenderAnimationPrepState.targetClipIdentity.has_value()) {
            desiredTargetIndex = findRenderAnimationIndexByIdentity(
                *currentRenderAnimationPrepState.targetClipIdentity);
        }
        if (desiredTargetIndex < 0) {
            desiredTargetIndex = currentRenderAnimationPrepState.targetAnimationIndex;
        }

        int comboSelection = 0;
        int firstCandidateSelection = -1;
        for (int animationIndex = 0; animationIndex < static_cast<int>(animations.size()); ++animationIndex) {
            if (animationIndex == activeIndex) {
                continue;
            }

            const auto& candidate = animations[static_cast<std::size_t>(animationIndex)];
            if (!candidate.supportedForPlayback) {
                continue;
            }
            if (!hierarchy || std::find(
                hierarchy->compatibleAnimationIndices.begin(),
                hierarchy->compatibleAnimationIndices.end(),
                animationIndex) == hierarchy->compatibleAnimationIndices.end()) {
                continue;
            }

            const OW3D::Render::RenderAnimationEditDraft* candidateDraft =
                findRenderAnimationEditDraftForClip(candidate);
            const uint32_t candidateFrameCount =
                EffectiveRenderAnimationFrameCount(candidate, candidateDraft);
            const float candidateFrameRate =
                EffectiveRenderAnimationFrameRate(candidate, candidateDraft);
            const QString label = tr("%1 | %2 | %3 frames @ %4 FPS")
                .arg(QString::fromStdString(candidate.fullName))
                .arg(candidate.sourceFileLabel.empty()
                    ? tr("(Unknown Source)")
                    : QString::fromStdString(candidate.sourceFileLabel))
                .arg(candidateFrameCount)
                .arg(QString::number(candidateFrameRate, 'f', 2));
            renderAnimationPrepSourceCombo->addItem(label, animationIndex);
            const int comboIndex = renderAnimationPrepSourceCombo->count() - 1;
            if (firstCandidateSelection < 0) {
                firstCandidateSelection = comboIndex;
            }
            if (animationIndex == desiredTargetIndex) {
                comboSelection = comboIndex;
            }
        }

        if (comboSelection == 0 && firstCandidateSelection > 0) {
            comboSelection = firstCandidateSelection;
        }
        renderAnimationPrepSourceCombo->setCurrentIndex(comboSelection);
        currentRenderAnimationPrepState.targetAnimationIndex =
            renderAnimationPrepSourceCombo->currentData().toInt();
        if (currentRenderAnimationPrepState.targetAnimationIndex >= 0
            && currentRenderAnimationPrepState.targetAnimationIndex < static_cast<int>(animations.size())) {
            currentRenderAnimationPrepState.targetClipIdentity =
                BuildRenderAnimationClipIdentity(
                    animations[static_cast<std::size_t>(currentRenderAnimationPrepState.targetAnimationIndex)]);
        }
        else {
            currentRenderAnimationPrepState.targetAnimationIndex = -1;
            currentRenderAnimationPrepState.targetClipIdentity.reset();
        }
    }

    {
        QSignalBlocker frameBlocker(renderAnimationPrepStaticPoseFrameSpin);
        renderAnimationPrepStaticPoseFrameSpin->setRange(0, activeMaxFrame);
        renderAnimationPrepStaticPoseFrameSpin->setValue(
            currentRenderAnimationPrepState.staticPoseFrame);
    }
    suppressRenderAnimationPrepUiSignals = false;

    renderAnimationPrepGroup->setEnabled(controlsEnabled);
    renderAnimationPrepSourceCombo->setEnabled(controlsEnabled);
    renderAnimationPrepStaticPoseFrameSpin->setEnabled(controlsEnabled);
    renderAnimationPrepApplyButton->setEnabled(
        controlsEnabled && currentRenderAnimationPrepState.targetAnimationIndex >= 0);
    renderAnimationPrepFreezePoseButton->setEnabled(
        controlsEnabled && activeFrameCount > 0u);
    renderAnimationPrepFitSourceButton->setEnabled(
        controlsEnabled
        && activeFrameCount > 0u
        && currentRenderAnimationPrepState.targetAnimationIndex >= 0);

    QString statusText;
    if (!activeClip) {
        statusText = tr("Select a writable base clip to prepare.");
    }
    else if (!editableActiveClip) {
        statusText = tr("Clip prep unavailable: %1").arg(
            editReason.isEmpty()
                ? tr("The active base clip is read-only.")
                : editReason);
    }
    else if (!hierarchy) {
        statusText = tr("Clip prep unavailable: the active clip has no compatible hierarchy.");
    }
    else if (currentRenderAnimationPrepState.targetAnimationIndex < 0) {
        statusText = tr("Select a compatible source clip to match timing or fit it into this clip. Use Apply Pose To All Frames to freeze pose frame %1 across the current clip timing.")
            .arg(currentRenderAnimationPrepState.staticPoseFrame);
    }
    else {
        const auto& targetClip =
            animations[static_cast<std::size_t>(currentRenderAnimationPrepState.targetAnimationIndex)];
        const OW3D::Render::RenderAnimationEditDraft* targetDraft =
            findRenderAnimationEditDraftForClip(targetClip);
        const uint32_t sourceFrameCount =
            EffectiveRenderAnimationFrameCount(targetClip, targetDraft);
        const float sourceFrameRate =
            EffectiveRenderAnimationFrameRate(targetClip, targetDraft);
        statusText = tr(
            "Apply To Draft freezes pose frame %1 across %2 frames at %3 FPS from %4. Fit Source To Clip retimes all %2 source frames into the active clip's %5-frame timing.")
            .arg(currentRenderAnimationPrepState.staticPoseFrame)
            .arg(sourceFrameCount)
            .arg(QString::number(
                sourceFrameRate,
                'f',
                2))
            .arg(QString::fromStdString(targetClip.fullName))
            .arg(activeFrameCount);
    }
    renderAnimationPrepStatusLabel->setText(statusText);
}

void MainWindow::refreshRenderAnimationPivotSheetFrameHighlight() {
    if (!renderAnimationPivotSheetTable) {
        return;
    }

    const auto& animations = currentRenderSceneResult.scene.animations;
    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    if (activeIndex < 0 || activeIndex >= static_cast<int>(animations.size())) {
        QSignalBlocker blocker(renderAnimationPivotSheetTable);
        renderAnimationPivotSheetTable->clearSelection();
        return;
    }

    const auto& clip = animations[static_cast<std::size_t>(activeIndex)];
    const OW3D::Render::RenderAnimationEditDraft* activeDraft =
        findRenderAnimationEditDraftForClip(clip);
    const uint32_t frameCount = EffectiveRenderAnimationFrameCount(clip, activeDraft);
    const float frameRate = EffectiveRenderAnimationFrameRate(clip, activeDraft);
    if (frameCount == 0u || renderAnimationPivotSheetTable->rowCount() <= 0) {
        QSignalBlocker blocker(renderAnimationPivotSheetTable);
        renderAnimationPivotSheetTable->clearSelection();
        return;
    }

    const int maxFrame = std::max(0, static_cast<int>(frameCount) - 1);
    const int currentFrame = frameRate > 0.0f
        ? std::clamp(
            static_cast<int>(std::round(currentRenderAnimationPlayback.timeSeconds * frameRate)),
            0,
            maxFrame)
        : 0;
    if (currentFrame >= renderAnimationPivotSheetTable->rowCount()) {
        QSignalBlocker blocker(renderAnimationPivotSheetTable);
        renderAnimationPivotSheetTable->clearSelection();
        return;
    }

    QSignalBlocker blocker(renderAnimationPivotSheetTable);
    renderAnimationPivotSheetTable->clearSelection();
    renderAnimationPivotSheetTable->selectRow(currentFrame);
}

void MainWindow::refreshRenderAnimationPivotSheet() {
    if (!renderAnimationPivotSheetGroup
        || !renderAnimationPivotSheetLabel
        || !renderAnimationPivotSheetStatusLabel
        || !renderAnimationPivotSheetTable) {
        return;
    }

    QSignalBlocker tableBlocker(renderAnimationPivotSheetTable);
    suppressRenderAnimationPivotSheetSignals = true;
    renderAnimationPivotSheetTable->setUpdatesEnabled(false);
    renderAnimationPivotSheetTable->clearContents();
    renderAnimationPivotSheetTable->setRowCount(0);
    renderAnimationPivotSheetTable->setEnabled(false);

    auto finishRefresh = [this]() {
        renderAnimationPivotSheetTable->setUpdatesEnabled(true);
        suppressRenderAnimationPivotSheetSignals = false;
        refreshRenderAnimationPivotSheetFrameHighlight();
    };

    const auto& hierarchies = currentRenderSceneResult.scene.hierarchies;
    if (currentRenderSelectedPivotHierarchyIndex < 0
        || currentRenderSelectedPivotIndex < 0
        || currentRenderSelectedPivotHierarchyIndex >= static_cast<int>(hierarchies.size())) {
        renderAnimationPivotSheetLabel->setText(
            tr("Select a pivot in the viewport or Scene Browser to inspect sampled transform values."));
        renderAnimationPivotSheetStatusLabel->setText(
            tr("The sheet shows per-frame local translation and Euler rotation for the selected pivot. Euler angles can wrap around at +/-180 degrees."));
        finishRefresh();
        return;
    }

    const auto& selectedHierarchy =
        hierarchies[static_cast<std::size_t>(currentRenderSelectedPivotHierarchyIndex)];
    if (currentRenderSelectedPivotIndex >= static_cast<int>(selectedHierarchy.pivots.size())) {
        renderAnimationPivotSheetLabel->setText(
            tr("The selected pivot is no longer valid for the current scene."));
        renderAnimationPivotSheetStatusLabel->setText(
            tr("Pick a different pivot to inspect its sampled animation values."));
        finishRefresh();
        return;
    }

    QString selectedHierarchyName = selectedHierarchy.name.empty()
        ? tr("Hierarchy %1").arg(currentRenderSelectedPivotHierarchyIndex)
        : QString::fromStdString(selectedHierarchy.name);
    QString selectedPivotName = selectedHierarchy.pivots[static_cast<std::size_t>(currentRenderSelectedPivotIndex)].name.empty()
        ? tr("Pivot %1").arg(currentRenderSelectedPivotIndex)
        : QString::fromStdString(
            selectedHierarchy.pivots[static_cast<std::size_t>(currentRenderSelectedPivotIndex)].name);

    const auto& animations = currentRenderSceneResult.scene.animations;
    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    if (activeIndex < 0 || activeIndex >= static_cast<int>(animations.size())) {
        renderAnimationPivotSheetLabel->setText(
            tr("Selected Pivot: %1 | %2").arg(selectedHierarchyName, selectedPivotName));
        renderAnimationPivotSheetStatusLabel->setText(
            tr("Select an active clip to inspect this pivot across frames."));
        finishRefresh();
        return;
    }

    const auto& activeClip = animations[static_cast<std::size_t>(activeIndex)];
    const OW3D::Render::RenderAnimationEditDraft* activeDraft =
        findRenderAnimationEditDraftForClip(activeClip);
    const uint32_t frameCount = EffectiveRenderAnimationFrameCount(activeClip, activeDraft);
    if (frameCount == 0u) {
        renderAnimationPivotSheetLabel->setText(
            tr("Selected Pivot: %1 | %2").arg(selectedHierarchyName, selectedPivotName));
        renderAnimationPivotSheetStatusLabel->setText(
            tr("The active clip reports zero frames."));
        finishRefresh();
        return;
    }

    const int clipHierarchyIndex = findCompatibleRenderHierarchyIndexForAnimation(activeIndex);
    if (clipHierarchyIndex < 0
        || clipHierarchyIndex >= static_cast<int>(hierarchies.size())) {
        renderAnimationPivotSheetLabel->setText(
            tr("Selected Pivot: %1 | %2").arg(selectedHierarchyName, selectedPivotName));
        renderAnimationPivotSheetStatusLabel->setText(
            tr("The active clip does not resolve to a compatible hierarchy for this pivot."));
        finishRefresh();
        return;
    }

    const auto& clipHierarchy = hierarchies[static_cast<std::size_t>(clipHierarchyIndex)];
    const int clipPivotIndex = ResolveMappedRenderPivotIndex(
        clipHierarchy,
        BuildRenderPivotIndexByName(clipHierarchy),
        selectedHierarchy,
        currentRenderSelectedPivotIndex);
    if (clipPivotIndex < 0) {
        renderAnimationPivotSheetLabel->setText(
            tr("Selected Pivot: %1 | %2").arg(selectedHierarchyName, selectedPivotName));
        renderAnimationPivotSheetStatusLabel->setText(
            tr("This selected pivot could not be matched into the active clip hierarchy."));
        finishRefresh();
        return;
    }

    QString clipPivotName = tr("Pivot %1").arg(clipPivotIndex);
    if (clipPivotIndex >= 0
        && clipPivotIndex < static_cast<int>(clipHierarchy.pivots.size())
        && !clipHierarchy.pivots[static_cast<std::size_t>(clipPivotIndex)].name.empty()) {
        clipPivotName =
            QString::fromStdString(clipHierarchy.pivots[static_cast<std::size_t>(clipPivotIndex)].name);
    }

    renderAnimationPivotSheetLabel->setText(
        tr("Selected Pivot: %1 | %2 | Clip Pivot: %3")
            .arg(selectedHierarchyName, selectedPivotName, clipPivotName));

    const bool pivotUsesDraftSamples =
        activeDraft
        && activeDraft->pivotSamples.find(clipPivotIndex) != activeDraft->pivotSamples.end();
    renderAnimationPivotSheetStatusLabel->setText(
        pivotUsesDraftSamples
            ? tr("Click a frame row to scrub. Values are sampled from the current dense draft for this pivot. `Src Key` still marks raw source-clip keyframes. Euler angles can wrap around at +/-180 degrees.")
            : tr("Click a frame row to scrub. Values are sampled from the active clip. `Src Key` marks raw source-clip keyframes at exact frames. Euler angles can wrap around at +/-180 degrees."));

    const OW3D::Render::RenderPivotAnimation* sourcePivotAnimation =
        clipPivotIndex >= 0
        && clipPivotIndex < static_cast<int>(activeClip.pivots.size())
        ? &activeClip.pivots[static_cast<std::size_t>(clipPivotIndex)]
        : nullptr;

    renderAnimationPivotSheetTable->setEnabled(true);
    renderAnimationPivotSheetTable->setRowCount(static_cast<int>(frameCount));
    constexpr float kRadToDeg = 57.29577951308232f;
    for (int frameIndex = 0; frameIndex < static_cast<int>(frameCount); ++frameIndex) {
        const OW3D::Render::Vec3 translation =
            OW3D::Render::SamplePivotAnimationTranslation(
                &activeClip,
                clipPivotIndex,
                static_cast<float>(frameIndex),
                activeDraft);
        const OW3D::Render::Vec4 rotation =
            OW3D::Render::SamplePivotAnimationRotation(
                &activeClip,
                clipPivotIndex,
                static_cast<float>(frameIndex),
                activeDraft);
        const W3dVectorStruct eulerRadians =
            EulerFromQuaternion(rotation.x, rotation.y, rotation.z, rotation.w);
        const QString sourceKeyFlags =
            BuildPivotAnimationSourceKeyFlags(sourcePivotAnimation, frameIndex);

        auto* frameItem = new QTableWidgetItem(QString::number(frameIndex));
        frameItem->setData(Qt::UserRole, frameIndex);
        frameItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        renderAnimationPivotSheetTable->setItem(frameIndex, 0, frameItem);

        auto* keyItem = new QTableWidgetItem(sourceKeyFlags);
        keyItem->setTextAlignment(Qt::AlignCenter);
        renderAnimationPivotSheetTable->setItem(frameIndex, 1, keyItem);

        auto makeValueItem = [](float value, int decimals) {
            auto* item = new QTableWidgetItem(QString::number(value, 'f', decimals));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return item;
        };
        renderAnimationPivotSheetTable->setItem(frameIndex, 2, makeValueItem(translation.x, 3));
        renderAnimationPivotSheetTable->setItem(frameIndex, 3, makeValueItem(translation.y, 3));
        renderAnimationPivotSheetTable->setItem(frameIndex, 4, makeValueItem(translation.z, 3));
        renderAnimationPivotSheetTable->setItem(
            frameIndex,
            5,
            makeValueItem(eulerRadians.X * kRadToDeg, 2));
        renderAnimationPivotSheetTable->setItem(
            frameIndex,
            6,
            makeValueItem(eulerRadians.Y * kRadToDeg, 2));
        renderAnimationPivotSheetTable->setItem(
            frameIndex,
            7,
            makeValueItem(eulerRadians.Z * kRadToDeg, 2));
    }
    renderAnimationPivotSheetTable->resizeColumnsToContents();
    finishRefresh();
}

void MainWindow::refreshRenderBlendControls() {
    if (!renderAnimationBlendSourceCombo
        || !renderAnimationBlendTimingCombo
        || !renderAnimationBlendPivotTree
        || !renderAnimationBlendPivotWeightsGroup
        || !renderAnimationBlendPivotWeightsLabel
        || !renderAnimationBlendIncludeDescendantsToggle
        || !renderAnimationBlendTruncateToggle
        || !renderAnimationBlendNormalizeFrameRateToggle
        || !renderAnimationBlendPinLeftHandToggle
        || !renderAnimationBlendPinRightHandToggle
        || !renderAnimationBlendHandPinReferenceFrameSpin
        || !renderAnimationBlendTranslationPercentSpin
        || !renderAnimationBlendRotationPercentSpin
        || !renderAnimationBlendResetPivotWeightsButton
        || !renderAnimationBlendStartFrameSpin
        || !renderAnimationBlendEndFrameSpin
        || !renderAnimationBlendApplyButton
        || !renderAnimationBlendStatusLabel) {
        return;
    }
    for (QSpinBox* axisSpin : renderAnimationBlendPivotAxisPercentSpins) {
        if (!axisSpin) {
            return;
        }
    }

    const auto& animations = currentRenderSceneResult.scene.animations;
    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    const bool activeValid =
        activeIndex >= 0 && activeIndex < static_cast<int>(animations.size());
    const OW3D::Render::RenderAnimationClip* activeClip =
        activeValid ? &animations[static_cast<std::size_t>(activeIndex)] : nullptr;
    const OW3D::Render::RenderAnimationEditDraft* activeDraft =
        activeClip ? findRenderAnimationEditDraftForClip(*activeClip) : nullptr;
    const uint32_t activeFrameCount =
        activeClip ? EffectiveRenderAnimationFrameCount(*activeClip, activeDraft) : 0u;

    QString editReason;
    const bool editableActiveClip = ResolveEditablePrimaryRenderAnimation(
        chunkData.get(),
        currentRenderSceneResult,
        currentRenderAnimationPlayback,
        nullptr,
        nullptr,
        nullptr,
        &editReason);

    const int hierarchyIndex =
        (editableActiveClip && activeClip) ? findCompatibleRenderHierarchyIndexForAnimation(activeIndex) : -1;
    const OW3D::Render::RenderHierarchy* hierarchy =
        hierarchyIndex >= 0
        && hierarchyIndex < static_cast<int>(currentRenderSceneResult.scene.hierarchies.size())
        ? &currentRenderSceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)]
        : nullptr;

    const QString hierarchyKey = hierarchy ? BuildRenderHierarchyIdentityKey(*hierarchy) : QString();
    if (currentRenderAnimationBlendState.hierarchyKey != hierarchyKey) {
        currentRenderAnimationBlendState.hierarchyKey = hierarchyKey;
        currentRenderAnimationBlendState.pivotIndices.clear();
        currentRenderAnimationBlendState.pivotAxisBlendPercents.clear();
        currentRenderAnimationBlendState.selectedPivotIndex = -1;
    }

    if (hierarchy) {
        QSet<int> filteredPivots;
        for (const int pivotIndex : currentRenderAnimationBlendState.pivotIndices) {
            if (pivotIndex >= 0 && pivotIndex < static_cast<int>(hierarchy->pivots.size())) {
                filteredPivots.insert(pivotIndex);
            }
        }
        currentRenderAnimationBlendState.pivotIndices = std::move(filteredPivots);

        std::unordered_map<int, std::array<int, kRenderBlendAxisCount>> filteredAxisPercents;
        for (const auto& [pivotIndex, axisPercents] : currentRenderAnimationBlendState.pivotAxisBlendPercents) {
            if (pivotIndex >= 0
                && pivotIndex < static_cast<int>(hierarchy->pivots.size())
                && !RenderBlendPivotAxisPercentsMatchDefaults(
                    axisPercents,
                    currentRenderAnimationBlendState.translationBlendPercent,
                    currentRenderAnimationBlendState.rotationBlendPercent)) {
                filteredAxisPercents.emplace(pivotIndex, axisPercents);
            }
        }
        currentRenderAnimationBlendState.pivotAxisBlendPercents = std::move(filteredAxisPercents);
        if (currentRenderAnimationBlendState.selectedPivotIndex < 0
            || currentRenderAnimationBlendState.selectedPivotIndex >= static_cast<int>(hierarchy->pivots.size())) {
            currentRenderAnimationBlendState.selectedPivotIndex = -1;
        }
    }
    else {
        currentRenderAnimationBlendState.pivotIndices.clear();
        currentRenderAnimationBlendState.pivotAxisBlendPercents.clear();
        currentRenderAnimationBlendState.selectedPivotIndex = -1;
    }

    const uint32_t selectionFrameCount =
        activeClip ? activeClip->numFrames : 0u;
    const int maxFrame = selectionFrameCount > 0u
        ? std::max(0, static_cast<int>(selectionFrameCount) - 1)
        : 0;
    if (!activeClip) {
        currentRenderAnimationBlendState.frameRangeInitialized = false;
    }
    else if (!currentRenderAnimationBlendState.frameRangeInitialized) {
        currentRenderAnimationBlendState.startFrame = 0;
        currentRenderAnimationBlendState.endFrame = maxFrame;
        currentRenderAnimationBlendState.frameRangeInitialized = true;
    }
    else {
        currentRenderAnimationBlendState.startFrame =
            std::clamp(currentRenderAnimationBlendState.startFrame, 0, maxFrame);
        currentRenderAnimationBlendState.endFrame =
            std::clamp(currentRenderAnimationBlendState.endFrame, 0, maxFrame);
        if (currentRenderAnimationBlendState.endFrame < currentRenderAnimationBlendState.startFrame) {
            currentRenderAnimationBlendState.endFrame = currentRenderAnimationBlendState.startFrame;
        }
    }

    suppressRenderAnimationBlendUiSignals = true;
    {
        QSignalBlocker comboBlocker(renderAnimationBlendSourceCombo);
        renderAnimationBlendSourceCombo->clear();
        renderAnimationBlendSourceCombo->addItem(tr("(No Source Clip)"), -1);

        int desiredOverlayIndex = -1;
        if (currentRenderAnimationBlendState.overlayClipIdentity.has_value()) {
            desiredOverlayIndex = findRenderAnimationIndexByIdentity(
                *currentRenderAnimationBlendState.overlayClipIdentity);
        }
        if (desiredOverlayIndex < 0) {
            desiredOverlayIndex = currentRenderAnimationBlendState.overlayAnimationIndex;
        }

        int comboSelection = 0;
        int firstCandidateSelection = -1;
        for (int animationIndex = 0; animationIndex < static_cast<int>(animations.size()); ++animationIndex) {
            if (animationIndex == activeIndex) {
                continue;
            }

            const auto& candidate = animations[static_cast<std::size_t>(animationIndex)];
            if (!candidate.supportedForPlayback) {
                continue;
            }
            if (!hierarchy || std::find(
                hierarchy->compatibleAnimationIndices.begin(),
                hierarchy->compatibleAnimationIndices.end(),
                animationIndex) == hierarchy->compatibleAnimationIndices.end()) {
                continue;
            }

            const QString label = tr("%1 | %2 | %3 frames")
                .arg(QString::fromStdString(candidate.fullName))
                .arg(candidate.sourceFileLabel.empty()
                    ? tr("(Unknown Source)")
                    : QString::fromStdString(candidate.sourceFileLabel))
                .arg(candidate.numFrames);
            renderAnimationBlendSourceCombo->addItem(label, animationIndex);
            const int comboIndex = renderAnimationBlendSourceCombo->count() - 1;
            if (firstCandidateSelection < 0) {
                firstCandidateSelection = comboIndex;
            }
            if (animationIndex == desiredOverlayIndex) {
                comboSelection = comboIndex;
            }
        }

        if (comboSelection == 0 && firstCandidateSelection > 0) {
            comboSelection = firstCandidateSelection;
        }
        renderAnimationBlendSourceCombo->setCurrentIndex(comboSelection);
        currentRenderAnimationBlendState.overlayAnimationIndex =
            renderAnimationBlendSourceCombo->currentData().toInt();
        if (currentRenderAnimationBlendState.overlayAnimationIndex >= 0
            && currentRenderAnimationBlendState.overlayAnimationIndex < static_cast<int>(animations.size())) {
            currentRenderAnimationBlendState.overlayClipIdentity =
                BuildRenderAnimationClipIdentity(
                    animations[static_cast<std::size_t>(currentRenderAnimationBlendState.overlayAnimationIndex)]);
        }
        else {
            currentRenderAnimationBlendState.overlayAnimationIndex = -1;
            currentRenderAnimationBlendState.overlayClipIdentity.reset();
        }
    }

    {
        QSignalBlocker timingBlocker(renderAnimationBlendTimingCombo);
        renderAnimationBlendTimingCombo->setCurrentIndex(
            currentRenderAnimationBlendState.timingMode
                == RenderAnimationBlendState::TimingMode::FitSourceToRange
                ? 1
                : 0);
    }

    {
        QSignalBlocker startBlocker(renderAnimationBlendStartFrameSpin);
        QSignalBlocker endBlocker(renderAnimationBlendEndFrameSpin);
        renderAnimationBlendStartFrameSpin->setRange(0, maxFrame);
        renderAnimationBlendEndFrameSpin->setRange(0, maxFrame);
        renderAnimationBlendStartFrameSpin->setValue(currentRenderAnimationBlendState.startFrame);
        renderAnimationBlendEndFrameSpin->setValue(currentRenderAnimationBlendState.endFrame);
    }

    currentRenderAnimationBlendState.handPinReferenceFrame =
        std::clamp(
            currentRenderAnimationBlendState.handPinReferenceFrame,
            0,
            activeFrameCount > 0u
                ? std::max(0, static_cast<int>(activeFrameCount) - 1)
                : 0);

    {
        QSignalBlocker toggleBlocker(renderAnimationBlendIncludeDescendantsToggle);
        renderAnimationBlendIncludeDescendantsToggle->setChecked(
            currentRenderAnimationBlendState.includeDescendants);
    }

    {
        QSignalBlocker toggleBlocker(renderAnimationBlendTruncateToggle);
        renderAnimationBlendTruncateToggle->setChecked(
            currentRenderAnimationBlendState.truncateToShorter);
    }

    {
        QSignalBlocker toggleBlocker(renderAnimationBlendNormalizeFrameRateToggle);
        renderAnimationBlendNormalizeFrameRateToggle->setChecked(
            currentRenderAnimationBlendState.normalizeBaseToSourceFrameRate);
    }

    {
        QSignalBlocker toggleBlocker(renderAnimationBlendPinLeftHandToggle);
        renderAnimationBlendPinLeftHandToggle->setChecked(
            currentRenderAnimationBlendState.pinLeftHand);
    }

    {
        QSignalBlocker toggleBlocker(renderAnimationBlendPinRightHandToggle);
        renderAnimationBlendPinRightHandToggle->setChecked(
            currentRenderAnimationBlendState.pinRightHand);
    }

    {
        QSignalBlocker frameBlocker(renderAnimationBlendHandPinReferenceFrameSpin);
        renderAnimationBlendHandPinReferenceFrameSpin->setRange(
            0,
            activeFrameCount > 0u
                ? std::max(0, static_cast<int>(activeFrameCount) - 1)
                : 0);
        renderAnimationBlendHandPinReferenceFrameSpin->setValue(
            currentRenderAnimationBlendState.handPinReferenceFrame);
    }

    {
        QSignalBlocker spinBlocker(renderAnimationBlendTranslationPercentSpin);
        renderAnimationBlendTranslationPercentSpin->setValue(
            currentRenderAnimationBlendState.translationBlendPercent);
    }

    {
        QSignalBlocker spinBlocker(renderAnimationBlendRotationPercentSpin);
        renderAnimationBlendRotationPercentSpin->setValue(
            currentRenderAnimationBlendState.rotationBlendPercent);
    }

    {
        QSignalBlocker treeBlocker(renderAnimationBlendPivotTree);
        renderAnimationBlendPivotTree->clear();
        QTreeWidgetItem* selectedItem = nullptr;
        if (hierarchy && !hierarchy->pivots.empty()) {
            std::vector<std::vector<int>> children(hierarchy->pivots.size());
            std::vector<int> roots;
            roots.reserve(hierarchy->pivots.size());
            for (int pivotIndex = 0; pivotIndex < static_cast<int>(hierarchy->pivots.size()); ++pivotIndex) {
                const int parentIndex = hierarchy->pivots[static_cast<std::size_t>(pivotIndex)].parentIndex;
                if (parentIndex >= 0 && parentIndex < static_cast<int>(hierarchy->pivots.size())) {
                    children[static_cast<std::size_t>(parentIndex)].push_back(pivotIndex);
                }
                else {
                    roots.push_back(pivotIndex);
                }
            }

            std::function<void(QTreeWidgetItem*, int)> addPivotItem =
                [&](QTreeWidgetItem* parentItem, int pivotIndex) {
                    const auto& pivot = hierarchy->pivots[static_cast<std::size_t>(pivotIndex)];
                    const auto resolvedAxisPercents = ResolveRenderBlendPivotAxisPercents(
                        currentRenderAnimationBlendState.translationBlendPercent,
                        currentRenderAnimationBlendState.rotationBlendPercent,
                        currentRenderAnimationBlendState.pivotAxisBlendPercents,
                        pivotIndex);
                    const bool hasCustomAxisPercents =
                        currentRenderAnimationBlendState.pivotAxisBlendPercents.find(pivotIndex)
                        != currentRenderAnimationBlendState.pivotAxisBlendPercents.end();
                    auto* item = parentItem
                        ? new QTreeWidgetItem(parentItem)
                        : new QTreeWidgetItem(renderAnimationBlendPivotTree);
                    QString itemLabel = QString::fromStdString(pivot.name);
                    if (hasCustomAxisPercents) {
                        itemLabel += tr(" [custom]");
                    }
                    item->setText(0, itemLabel);
                    item->setData(0, kRenderBlendPivotIndexRole, pivotIndex);
                    item->setFlags(
                        item->flags()
                        | Qt::ItemIsUserCheckable
                        | Qt::ItemIsEnabled
                        | Qt::ItemIsSelectable);
                    item->setCheckState(
                        0,
                        currentRenderAnimationBlendState.pivotIndices.contains(pivotIndex)
                            ? Qt::Checked
                            : Qt::Unchecked);
                    item->setToolTip(
                        0,
                        hasCustomAxisPercents
                            ? tr("Custom axis weights. %1")
                                .arg(FormatRenderBlendPivotAxisPercentsSummary(resolvedAxisPercents))
                            : tr("Using default axis weights. %1")
                                .arg(FormatRenderBlendPivotAxisPercentsSummary(resolvedAxisPercents)));
                    if (pivotIndex == currentRenderAnimationBlendState.selectedPivotIndex) {
                        selectedItem = item;
                    }
                    for (const int childIndex : children[static_cast<std::size_t>(pivotIndex)]) {
                        addPivotItem(item, childIndex);
                    }
                };

            for (const int rootIndex : roots) {
                addPivotItem(nullptr, rootIndex);
            }
            renderAnimationBlendPivotTree->expandAll();
        }
        if (selectedItem) {
            renderAnimationBlendPivotTree->setCurrentItem(selectedItem);
        }
    }
    suppressRenderAnimationBlendUiSignals = false;

    refreshRenderBlendPivotWeightEditor();
    refreshRenderBlendStatus();
}

void MainWindow::refreshRenderBlendPivotWeightEditor() {
    if (!renderAnimationBlendPivotWeightsGroup
        || !renderAnimationBlendPivotWeightsLabel
        || !renderAnimationBlendResetPivotWeightsButton
        || !renderAnimationBlendTranslationPercentSpin
        || !renderAnimationBlendRotationPercentSpin) {
        return;
    }
    for (QSpinBox* axisSpin : renderAnimationBlendPivotAxisPercentSpins) {
        if (!axisSpin) {
            return;
        }
    }

    const auto& animations = currentRenderSceneResult.scene.animations;
    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    const bool activeValid =
        activeIndex >= 0 && activeIndex < static_cast<int>(animations.size());
    const OW3D::Render::RenderAnimationClip* activeClip =
        activeValid ? &animations[static_cast<std::size_t>(activeIndex)] : nullptr;

    QString editReason;
    const bool editableActiveClip = ResolveEditablePrimaryRenderAnimation(
        chunkData.get(),
        currentRenderSceneResult,
        currentRenderAnimationPlayback,
        nullptr,
        nullptr,
        nullptr,
        &editReason);
    const int hierarchyIndex =
        (editableActiveClip && activeClip) ? findCompatibleRenderHierarchyIndexForAnimation(activeIndex) : -1;
    const OW3D::Render::RenderHierarchy* hierarchy =
        hierarchyIndex >= 0
        && hierarchyIndex < static_cast<int>(currentRenderSceneResult.scene.hierarchies.size())
        ? &currentRenderSceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)]
        : nullptr;

    const bool controlsEnabled = editableActiveClip && hierarchy != nullptr;
    const int selectedPivotIndex = currentRenderAnimationBlendState.selectedPivotIndex;
    const bool selectedPivotValid =
        controlsEnabled
        && selectedPivotIndex >= 0
        && selectedPivotIndex < static_cast<int>(hierarchy->pivots.size());
    renderAnimationBlendPivotWeightsGroup->setEnabled(controlsEnabled);

    QString labelText;
    if (!controlsEnabled) {
        labelText = tr("Selected pivot axis weights are available when an editable base clip and compatible hierarchy are active.");
    }
    else if (!selectedPivotValid) {
        labelText = tr("Select a pivot row above to override its per-axis weights. Uncustomized pivots use the default percentages below.");
    }
    else {
        const bool hasCustomAxisPercents =
            currentRenderAnimationBlendState.pivotAxisBlendPercents.find(selectedPivotIndex)
            != currentRenderAnimationBlendState.pivotAxisBlendPercents.end();
        const auto resolvedAxisPercents = ResolveRenderBlendPivotAxisPercents(
            currentRenderAnimationBlendState.translationBlendPercent,
            currentRenderAnimationBlendState.rotationBlendPercent,
            currentRenderAnimationBlendState.pivotAxisBlendPercents,
            selectedPivotIndex);
        labelText = tr("%1. %2 %3")
            .arg(QString::fromStdString(
                hierarchy->pivots[static_cast<std::size_t>(selectedPivotIndex)].name))
            .arg(
                hasCustomAxisPercents
                    ? tr("Custom axis weights are active.")
                    : tr("Using default axis weights."))
            .arg(FormatRenderBlendPivotAxisPercentsSummary(resolvedAxisPercents));
    }
    renderAnimationBlendPivotWeightsLabel->setText(labelText);

    const auto resolvedAxisPercents = selectedPivotValid
        ? ResolveRenderBlendPivotAxisPercents(
            currentRenderAnimationBlendState.translationBlendPercent,
            currentRenderAnimationBlendState.rotationBlendPercent,
            currentRenderAnimationBlendState.pivotAxisBlendPercents,
            selectedPivotIndex)
        : DefaultRenderBlendPivotAxisPercents(
            currentRenderAnimationBlendState.translationBlendPercent,
            currentRenderAnimationBlendState.rotationBlendPercent);

    for (int axisIndex = 0; axisIndex < kRenderBlendAxisCount; ++axisIndex) {
        QSignalBlocker spinBlocker(
            renderAnimationBlendPivotAxisPercentSpins[static_cast<std::size_t>(axisIndex)]);
        renderAnimationBlendPivotAxisPercentSpins[static_cast<std::size_t>(axisIndex)]->setValue(
            resolvedAxisPercents[static_cast<std::size_t>(axisIndex)]);
        renderAnimationBlendPivotAxisPercentSpins[static_cast<std::size_t>(axisIndex)]->setEnabled(
            selectedPivotValid);
    }

    renderAnimationBlendResetPivotWeightsButton->setEnabled(
        selectedPivotValid
        && currentRenderAnimationBlendState.pivotAxisBlendPercents.find(selectedPivotIndex)
        != currentRenderAnimationBlendState.pivotAxisBlendPercents.end());
}

void MainWindow::refreshRenderBlendStatus() {
    if (!renderAnimationBlendSourceCombo
        || !renderAnimationBlendTimingCombo
        || !renderAnimationBlendPivotTree
        || !renderAnimationBlendPivotWeightsGroup
        || !renderAnimationBlendPivotWeightsLabel
        || !renderAnimationBlendIncludeDescendantsToggle
        || !renderAnimationBlendTruncateToggle
        || !renderAnimationBlendNormalizeFrameRateToggle
        || !renderAnimationBlendPinLeftHandToggle
        || !renderAnimationBlendPinRightHandToggle
        || !renderAnimationBlendHandPinReferenceFrameSpin
        || !renderAnimationBlendTranslationPercentSpin
        || !renderAnimationBlendRotationPercentSpin
        || !renderAnimationBlendResetPivotWeightsButton
        || !renderAnimationBlendStartFrameSpin
        || !renderAnimationBlendEndFrameSpin
        || !renderAnimationBlendApplyButton
        || !renderAnimationBlendStatusLabel) {
        return;
    }
    for (QSpinBox* axisSpin : renderAnimationBlendPivotAxisPercentSpins) {
        if (!axisSpin) {
            return;
        }
    }

    const auto& animations = currentRenderSceneResult.scene.animations;
    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    const bool activeValid =
        activeIndex >= 0 && activeIndex < static_cast<int>(animations.size());
    const OW3D::Render::RenderAnimationClip* activeClip =
        activeValid ? &animations[static_cast<std::size_t>(activeIndex)] : nullptr;
    const OW3D::Render::RenderAnimationEditDraft* activeDraft =
        activeClip ? findRenderAnimationEditDraftForClip(*activeClip) : nullptr;

    QString editReason;
    const bool editableActiveClip = ResolveEditablePrimaryRenderAnimation(
        chunkData.get(),
        currentRenderSceneResult,
        currentRenderAnimationPlayback,
        nullptr,
        nullptr,
        nullptr,
        &editReason);

    const int hierarchyIndex =
        (editableActiveClip && activeClip) ? findCompatibleRenderHierarchyIndexForAnimation(activeIndex) : -1;
    const OW3D::Render::RenderHierarchy* hierarchy =
        hierarchyIndex >= 0
        && hierarchyIndex < static_cast<int>(currentRenderSceneResult.scene.hierarchies.size())
        ? &currentRenderSceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)]
        : nullptr;

    const bool controlsEnabled = editableActiveClip && hierarchy != nullptr;
    const bool preserveSourceRate =
        currentRenderAnimationBlendState.timingMode
        == RenderAnimationBlendState::TimingMode::PreserveSourceRate;
    const float activeFrameRate =
        activeClip ? EffectiveRenderAnimationFrameRate(*activeClip, activeDraft) : 0.0f;
    const uint32_t activeFrameCount =
        activeClip ? EffectiveRenderAnimationFrameCount(*activeClip, activeDraft) : 0u;
    const uint32_t selectedBaseFrameCount =
        activeClip ? activeClip->numFrames : 0u;
    const int selectedBaseMaxFrame = selectedBaseFrameCount > 0u
        ? std::max(0, static_cast<int>(selectedBaseFrameCount) - 1)
        : 0;
    renderAnimationBlendSourceCombo->setEnabled(controlsEnabled);
    renderAnimationBlendTimingCombo->setEnabled(controlsEnabled);
    renderAnimationBlendPivotTree->setEnabled(controlsEnabled);
    renderAnimationBlendIncludeDescendantsToggle->setEnabled(controlsEnabled);
    renderAnimationBlendTruncateToggle->setEnabled(controlsEnabled && preserveSourceRate);
    renderAnimationBlendNormalizeFrameRateToggle->setEnabled(controlsEnabled && preserveSourceRate);
    renderAnimationBlendPinLeftHandToggle->setEnabled(controlsEnabled);
    renderAnimationBlendPinRightHandToggle->setEnabled(controlsEnabled);
    renderAnimationBlendHandPinReferenceFrameSpin->setEnabled(controlsEnabled);
    renderAnimationBlendTranslationPercentSpin->setEnabled(controlsEnabled);
    renderAnimationBlendRotationPercentSpin->setEnabled(controlsEnabled);
    renderAnimationBlendStartFrameSpin->setEnabled(controlsEnabled);
    renderAnimationBlendEndFrameSpin->setEnabled(controlsEnabled);

    const bool hasOverlaySource = currentRenderAnimationBlendState.overlayAnimationIndex >= 0;
    const bool hasPivots = !currentRenderAnimationBlendState.pivotIndices.isEmpty();
    const QSet<int> affectedPivots =
        hierarchy
        ? ExpandBlendPivotSelection(
            *hierarchy,
            currentRenderAnimationBlendState.pivotIndices,
            currentRenderAnimationBlendState.includeDescendants)
        : QSet<int>{};
    bool hasBlendAmount = false;
    int customAxisWeightPivotCount = 0;
    for (const int pivotIndex : affectedPivots) {
        const auto resolvedAxisPercents = ResolveRenderBlendPivotAxisPercents(
            currentRenderAnimationBlendState.translationBlendPercent,
            currentRenderAnimationBlendState.rotationBlendPercent,
            currentRenderAnimationBlendState.pivotAxisBlendPercents,
            pivotIndex);
        hasBlendAmount |= RenderBlendPivotAxisPercentsHaveAnyBlend(resolvedAxisPercents);
        if (currentRenderAnimationBlendState.pivotAxisBlendPercents.find(pivotIndex)
            != currentRenderAnimationBlendState.pivotAxisBlendPercents.end()) {
            customAxisWeightPivotCount += 1;
        }
    }
    renderAnimationBlendApplyButton->setEnabled(
        controlsEnabled && hasOverlaySource && hasPivots && hasBlendAmount);

    QString statusText;
    if (!activeClip) {
        statusText = tr("Select a base animation clip to override.");
    }
    else if (!editableActiveClip) {
        statusText = tr("Pivot override unavailable: %1").arg(
            editReason.isEmpty()
                ? tr("The active base clip is read-only.")
                : editReason);
    }
    else if (!hierarchy) {
        statusText = tr("Pivot override unavailable: the active clip has no compatible hierarchy.");
    }
    else if (!hasOverlaySource) {
        statusText = tr("Select a compatible source clip to copy from.");
    }
    else if (!hasPivots) {
        statusText = tr("Check one or more pivots to override.");
    }
    else if (!hasBlendAmount) {
        statusText = tr("Set a default percentage above 0, or assign custom axis weights above 0 to an affected pivot, before applying the override.");
    }
    else {
        const auto& overlayClip =
            animations[static_cast<std::size_t>(currentRenderAnimationBlendState.overlayAnimationIndex)];
        const OW3D::Render::RenderAnimationEditDraft* overlayDraft =
            findRenderAnimationEditDraftForClip(overlayClip);
        const uint32_t overlayFrameCount =
            EffectiveRenderAnimationFrameCount(overlayClip, overlayDraft);
        const float overlayFrameRate =
            EffectiveRenderAnimationFrameRate(overlayClip, overlayDraft);
        const int requestedStartFrame =
            std::clamp(currentRenderAnimationBlendState.startFrame, 0, selectedBaseMaxFrame);
        const int requestedEndFrame = std::clamp(
            std::max(currentRenderAnimationBlendState.startFrame, currentRenderAnimationBlendState.endFrame),
            0,
            selectedBaseMaxFrame);
        const PivotOverrideBaseTimingPreview baseTimingPreview =
            ComputePivotOverrideBaseTimingPreview(
                activeFrameCount,
                activeFrameRate,
                requestedStartFrame,
                requestedEndFrame,
                preserveSourceRate,
                currentRenderAnimationBlendState.normalizeBaseToSourceFrameRate,
                overlayFrameRate);
        const int effectiveEndFrame = ComputePivotOverrideEffectiveEndFrame(
            baseTimingPreview.frameCount,
            baseTimingPreview.frameRate,
            overlayFrameCount,
            overlayFrameRate,
            baseTimingPreview.appliedStartFrame,
            baseTimingPreview.appliedEndFrame,
            preserveSourceRate,
            currentRenderAnimationBlendState.truncateToShorter);
        statusText = tr("Override %1 selected pivot(s) from %2 across selected base frames %3-%4.")
            .arg(currentRenderAnimationBlendState.pivotIndices.size())
            .arg(QString::fromStdString(overlayClip.fullName))
            .arg(requestedStartFrame)
            .arg(requestedEndFrame);
        statusText += tr(" Default blend: position %1%%, rotation %2%%.")
            .arg(currentRenderAnimationBlendState.translationBlendPercent)
            .arg(currentRenderAnimationBlendState.rotationBlendPercent);
        if (customAxisWeightPivotCount > 0) {
            statusText += tr(" %1 affected pivot(s) use custom per-axis weights.")
                .arg(customAxisWeightPivotCount);
        }
        if (currentRenderAnimationBlendState.pinLeftHand
            || currentRenderAnimationBlendState.pinRightHand) {
            const QStringList enabledPins = {
                currentRenderAnimationBlendState.pinLeftHand ? tr("left") : QString(),
                currentRenderAnimationBlendState.pinRightHand ? tr("right") : QString()
            };
            QStringList compactPins;
            for (const QString& pinLabel : enabledPins) {
                if (!pinLabel.isEmpty()) {
                    compactPins.push_back(pinLabel);
                }
            }
            statusText += tr(" Post-blend hand pins: %1 hand(s) fixed to base-pose frame %2.")
                .arg(compactPins.join(tr(" + ")))
                .arg(currentRenderAnimationBlendState.handPinReferenceFrame);
        }
        statusText += currentRenderAnimationBlendState.timingMode
            == RenderAnimationBlendState::TimingMode::FitSourceToRange
            ? tr(" Timing: fit the full source clip into the destination range.")
            : tr(" Timing: preserve the source clip's playback rate.");
        if (baseTimingPreview.selectedStartFrame != requestedStartFrame
            || baseTimingPreview.selectedEndFrame != requestedEndFrame)
        {
            statusText += tr(" The current draft clamps that selection to frames %1-%2 before sampling.")
                .arg(baseTimingPreview.selectedStartFrame)
                .arg(baseTimingPreview.selectedEndFrame);
        }
        if (baseTimingPreview.normalizationApplied) {
            statusText += tr(" Normalize base to source FPS is on; the base draft will be retimed from %1 FPS / %2 frames to %3 FPS / %4 frames, and the selected range maps to frames %5-%6 before sampling.")
                .arg(QString::number(activeFrameRate, 'f', 2))
                .arg(activeFrameCount)
                .arg(QString::number(baseTimingPreview.frameRate, 'f', 2))
                .arg(baseTimingPreview.frameCount)
                .arg(baseTimingPreview.appliedStartFrame)
                .arg(baseTimingPreview.appliedEndFrame);
        }
        else if (baseTimingPreview.normalizationRequested) {
            if (baseTimingPreview.normalizationRequiresValidFrameRates) {
                statusText += tr(" Normalize base to source FPS is on, but both clips need a valid FPS before the base timing can be retimed.");
            }
            else if (baseTimingPreview.normalizationAlreadyMatchesSource) {
                statusText += tr(" Normalize base to source FPS is on; the base draft already matches the source FPS.");
            }
        }
        if (preserveSourceRate && currentRenderAnimationBlendState.truncateToShorter) {
            if (effectiveEndFrame >= baseTimingPreview.appliedStartFrame) {
                statusText += tr(" Truncate to shorter clip is on; the draft clip will end at frame %1.")
                    .arg(effectiveEndFrame);
                statusText += tr(" Frames %1-%2 will be overwritten before the clip is shortened.")
                    .arg(baseTimingPreview.appliedStartFrame)
                    .arg(effectiveEndFrame);
            }
            else {
                statusText += tr(" Truncate to shorter clip is on; the selected source clip has no frames to apply.");
            }
        }
        if (currentRenderAnimationBlendState.includeDescendants) {
            statusText += tr(" Descendants will be included when applied.");
        }
    }
    renderAnimationBlendStatusLabel->setText(statusText);
}

void MainWindow::syncRenderAnimationUi() {
    refreshRenderAssetList();
    refreshRenderAnimationList();
    refreshRenderPlaybackSelection();
    refreshRenderPlaybackControls();
    refreshRenderAnimationPrepControls();
    refreshRenderAnimationPivotSheet();
    refreshRenderBlendControls();
    syncRenderAnimationPlaybackToViewport();
    syncActiveRenderAnimationDraftToViewport();
    syncRenderAnimationEditingStateToViewport();
}

void MainWindow::setRenderActiveAnimationIndex(
    int animationIndex,
    bool startPlaying,
    bool resetTime)
{
    const auto& animations = currentRenderSceneResult.scene.animations;
    const bool validIndex =
        animationIndex >= 0 && animationIndex < static_cast<int>(animations.size());

    if (!validIndex
        || !animations[static_cast<std::size_t>(animationIndex)].supportedForPlayback
        || !SceneHasCompatibleHierarchyForAnimation(currentRenderSceneResult.scene, animationIndex))
    {
        currentRenderAnimationPlayback.activeAnimationIndex = -1;
        currentRenderAnimationPlayback.timeSeconds = 0.0f;
        currentRenderAnimationPlayback.playing = false;
        currentRenderActiveClipIdentity.reset();
        if (renderAnimationPlaybackTimer) {
            renderAnimationPlaybackTimer->stop();
        }
        renderAnimationPlaybackElapsed.invalidate();
        syncRenderAnimationUi();
        return;
    }

    currentRenderAnimationPlayback.activeAnimationIndex = animationIndex;
    if (resetTime) {
        currentRenderAnimationPlayback.timeSeconds = 0.0f;
    }
    currentRenderAnimationPlayback.playing = startPlaying;
    currentRenderActiveClipIdentity =
        BuildRenderAnimationClipIdentity(animations[static_cast<std::size_t>(animationIndex)]);

    if (currentRenderAnimationPlayback.playing) {
        renderAnimationPlaybackElapsed.restart();
        if (renderAnimationPlaybackTimer) {
            renderAnimationPlaybackTimer->start();
        }
    }
    else {
        if (renderAnimationPlaybackTimer) {
            renderAnimationPlaybackTimer->stop();
        }
        renderAnimationPlaybackElapsed.invalidate();
    }

    syncRenderAnimationUi();
}

void MainWindow::handleViewportChunkActivated(void* chunkPtr) {
    if (!chunkPtr || !treeWidget) {
        return;
    }
    selectChunkInTree(chunkPtr);
}

void MainWindow::handleViewportPivotSelectionChanged(int hierarchyIndex, int pivotIndex) {
    const int normalizedHierarchyIndex = hierarchyIndex >= 0 ? hierarchyIndex : -1;
    const int normalizedPivotIndex = pivotIndex >= 0 ? pivotIndex : -1;
    if (currentRenderSelectedPivotHierarchyIndex == normalizedHierarchyIndex
        && currentRenderSelectedPivotIndex == normalizedPivotIndex) {
        return;
    }

    currentRenderSelectedPivotHierarchyIndex = normalizedHierarchyIndex;
    currentRenderSelectedPivotIndex = normalizedPivotIndex;
    refreshRenderAnimationPivotSheet();
}

void MainWindow::handleViewportPivotTransformCommit(
    void* pivotsChunkPtr,
    int pivotIndex,
    float tx,
    float ty,
    float tz,
    float qx,
    float qy,
    float qz,
    float qw)
{
    if (!chunkData || !pivotsChunkPtr || pivotIndex < 0) {
        return;
    }

    const auto pivotsChunk = FindChunkByPtr(chunkData->getChunks(), pivotsChunkPtr);
    if (!pivotsChunk) {
        QMessageBox::information(
            this,
            tr("Read-Only Selection"),
            tr("This mesh belongs to supplemental/archive data and cannot be edited."));
        return;
    }

    const auto parsedPivots = ParseChunkArray<W3dPivotStruct>(pivotsChunk);
    if (auto* err = std::get_if<std::string>(&parsedPivots)) {
        QMessageBox::warning(
            this,
            tr("Transform Commit Failed"),
            tr("Failed to parse pivots chunk: %1").arg(QString::fromStdString(*err)));
        return;
    }
    const auto& pivots = std::get<std::vector<W3dPivotStruct>>(parsedPivots);
    if (pivotIndex >= static_cast<int>(pivots.size())) {
        QMessageBox::warning(
            this,
            tr("Transform Commit Failed"),
            tr("Pivot index %1 is out of range for this hierarchy.").arg(pivotIndex));
        return;
    }
    const auto beforeSnapshot = CloneChunkTree(pivotsChunk);

    const float qLen = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
    if (qLen > 1.0e-6f) {
        const float invLen = 1.0f / qLen;
        qx *= invLen;
        qy *= invLen;
        qz *= invLen;
        qw *= invLen;
    }
    else {
        qx = 0.0f;
        qy = 0.0f;
        qz = 0.0f;
        qw = 1.0f;
    }

    const W3dVectorStruct euler = EulerFromQuaternion(qx, qy, qz, qw);
    W3dPivotStruct afterPivot = pivots[static_cast<std::size_t>(pivotIndex)];
    afterPivot.Translation.X = tx;
    afterPivot.Translation.Y = ty;
    afterPivot.Translation.Z = tz;
    afterPivot.Rotation.Q[0] = qx;
    afterPivot.Rotation.Q[1] = qy;
    afterPivot.Rotation.Q[2] = qz;
    afterPivot.Rotation.Q[3] = qw;
    afterPivot.EulerAngles = euler;

    std::string mutateError;
    const bool mutated = W3DEdit::MutateStructAtIndex<W3dPivotStruct>(
        pivotsChunk,
        static_cast<std::size_t>(pivotIndex),
        [&](W3dPivotStruct& pivot) {
            pivot = afterPivot;
        },
        &mutateError);

    if (!mutated) {
        QMessageBox::warning(
            this,
            tr("Transform Commit Failed"),
            QString::fromStdString(mutateError.empty()
                ? std::string("Failed to update pivot transform.")
                : mutateError));
        return;
    }

    if (!applyingRenderTransformUndoRedo) {
        RenderEditUndoEntry entry{};
        entry.kind = RenderEditUndoEntry::Kind::ChunkTree;
        entry.targetChunkPtr = pivotsChunkPtr;
        entry.beforeSnapshot = beforeSnapshot;
        entry.afterSnapshot = CloneChunkTree(pivotsChunk);
        renderTransformUndoStack.push_back(std::move(entry));
        renderTransformRedoStack.clear();
    }

    onChunkEdited();
}

void MainWindow::handleViewportAnimationKeyframeCommit(
    int hierarchyIndex,
    int pivotIndex,
    int frameIndex,
    float tx,
    float ty,
    float tz,
    float qx,
    float qy,
    float qz,
    float qw)
{
    int activeAnimationIndex = -1;
    const OW3D::Render::RenderAnimationClip* clip = nullptr;
    std::shared_ptr<ChunkItem> animationChunk;
    QString editReason;
    if (!ResolveEditablePrimaryRenderAnimation(
        chunkData.get(),
        currentRenderSceneResult,
        currentRenderAnimationPlayback,
        &activeAnimationIndex,
        &clip,
        &animationChunk,
        &editReason)) {
        QMessageBox::information(
            this,
            tr("Animation Edit Unavailable"),
            editReason.isEmpty() ? tr("The active animation clip is read-only.") : editReason);
        return;
    }
    if (!clip || !animationChunk) {
        return;
    }
    if (hierarchyIndex < 0
        || hierarchyIndex >= static_cast<int>(currentRenderSceneResult.scene.hierarchies.size())) {
        return;
    }

    const auto& hierarchy =
        currentRenderSceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
    if (std::find(
        hierarchy.compatibleAnimationIndices.begin(),
        hierarchy.compatibleAnimationIndices.end(),
        activeAnimationIndex) == hierarchy.compatibleAnimationIndices.end()) {
        QMessageBox::warning(
            this,
            tr("Animation Edit Failed"),
            tr("The active clip does not target the selected hierarchy."));
        return;
    }
    if (pivotIndex < 0) {
        return;
    }
    const OW3D::Render::RenderAnimationEditDraft* existingDraft =
        findRenderAnimationEditDraftForClip(*clip);
    const uint32_t clipFrameCount = EffectiveRenderAnimationFrameCount(*clip, existingDraft);
    if (frameIndex < 0 || frameIndex >= static_cast<int>(clipFrameCount)) {
        QMessageBox::warning(
            this,
            tr("Animation Edit Failed"),
            tr("Frame %1 is out of range for the active clip.").arg(frameIndex));
        return;
    }

    const int clipHierarchyIndex = findCompatibleRenderHierarchyIndexForAnimation(activeAnimationIndex);
    if (clipHierarchyIndex < 0
        || clipHierarchyIndex >= static_cast<int>(currentRenderSceneResult.scene.hierarchies.size())) {
        QMessageBox::warning(
            this,
            tr("Animation Edit Failed"),
            tr("The active clip could not resolve its writable hierarchy mapping."));
        return;
    }
    const auto& clipHierarchy =
        currentRenderSceneResult.scene.hierarchies[static_cast<std::size_t>(clipHierarchyIndex)];
    const int clipPivotIndex = ResolveMappedRenderPivotIndex(
        clipHierarchy,
        BuildRenderPivotIndexByName(clipHierarchy),
        hierarchy,
        pivotIndex);
    if (clipPivotIndex < 0) {
        QMessageBox::warning(
            this,
            tr("Animation Edit Failed"),
            tr("The selected pivot could not be mapped into the active clip hierarchy."));
        return;
    }

    const auto beforeDrafts = currentRenderAnimationDrafts;
    OW3D::Render::RenderAnimationEditDraft* draft =
        ensureRenderAnimationEditDraft(*clip, animationChunk);
    if (!draft) {
        QMessageBox::warning(
            this,
            tr("Animation Edit Failed"),
            tr("The active clip could not be prepared for draft editing."));
        return;
    }

    DensePivotAnimationSamples samples =
        OW3D::Render::BuildDensePivotAnimationSamples(*clip, clipPivotIndex, clipFrameCount, draft);

    const float qLen = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
    OW3D::Render::Vec4 rotation{
        0.0f, 0.0f, 0.0f, 1.0f
    };
    if (qLen > 1.0e-6f) {
        const float invLen = 1.0f / qLen;
        rotation.x = qx * invLen;
        rotation.y = qy * invLen;
        rotation.z = qz * invLen;
        rotation.w = qw * invLen;
    }

    samples.translationX[static_cast<std::size_t>(frameIndex)] = tx;
    samples.translationY[static_cast<std::size_t>(frameIndex)] = ty;
    samples.translationZ[static_cast<std::size_t>(frameIndex)] = tz;
    samples.rotation[static_cast<std::size_t>(frameIndex)] = rotation;

    draft->pivotSamples[clipPivotIndex] = std::move(samples);
    if (!applyingRenderTransformUndoRedo) {
        RenderEditUndoEntry entry{};
        entry.kind = RenderEditUndoEntry::Kind::AnimationDrafts;
        entry.beforeDrafts = beforeDrafts;
        entry.afterDrafts = currentRenderAnimationDrafts;
        renderTransformUndoStack.push_back(std::move(entry));
        renderTransformRedoStack.clear();
    }
    setDirty(true);
    syncRenderAnimationUi();
}

void MainWindow::handleViewportAnimationKeyframeDelete(
    int hierarchyIndex,
    int pivotIndex,
    int frameIndex)
{
    int activeAnimationIndex = -1;
    const OW3D::Render::RenderAnimationClip* clip = nullptr;
    std::shared_ptr<ChunkItem> animationChunk;
    QString editReason;
    if (!ResolveEditablePrimaryRenderAnimation(
        chunkData.get(),
        currentRenderSceneResult,
        currentRenderAnimationPlayback,
        &activeAnimationIndex,
        &clip,
        &animationChunk,
        &editReason)) {
        QMessageBox::information(
            this,
            tr("Animation Edit Unavailable"),
            editReason.isEmpty() ? tr("The active animation clip is read-only.") : editReason);
        return;
    }
    if (!clip || !animationChunk) {
        return;
    }
    if (hierarchyIndex < 0
        || hierarchyIndex >= static_cast<int>(currentRenderSceneResult.scene.hierarchies.size())) {
        return;
    }

    const auto& hierarchy =
        currentRenderSceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
    if (std::find(
        hierarchy.compatibleAnimationIndices.begin(),
        hierarchy.compatibleAnimationIndices.end(),
        activeAnimationIndex) == hierarchy.compatibleAnimationIndices.end()) {
        QMessageBox::warning(
            this,
            tr("Animation Edit Failed"),
            tr("The active clip does not target the selected hierarchy."));
        return;
    }
    if (pivotIndex < 0) {
        return;
    }
    const OW3D::Render::RenderAnimationEditDraft* existingDraft =
        findRenderAnimationEditDraftForClip(*clip);
    const uint32_t clipFrameCount = EffectiveRenderAnimationFrameCount(*clip, existingDraft);
    if (frameIndex < 0 || frameIndex >= static_cast<int>(clipFrameCount)) {
        QMessageBox::warning(
            this,
            tr("Animation Edit Failed"),
            tr("Frame %1 is out of range for the active clip.").arg(frameIndex));
        return;
    }

    const int clipHierarchyIndex = findCompatibleRenderHierarchyIndexForAnimation(activeAnimationIndex);
    if (clipHierarchyIndex < 0
        || clipHierarchyIndex >= static_cast<int>(currentRenderSceneResult.scene.hierarchies.size())) {
        QMessageBox::warning(
            this,
            tr("Animation Edit Failed"),
            tr("The active clip could not resolve its writable hierarchy mapping."));
        return;
    }
    const auto& clipHierarchy =
        currentRenderSceneResult.scene.hierarchies[static_cast<std::size_t>(clipHierarchyIndex)];
    const int clipPivotIndex = ResolveMappedRenderPivotIndex(
        clipHierarchy,
        BuildRenderPivotIndexByName(clipHierarchy),
        hierarchy,
        pivotIndex);
    if (clipPivotIndex < 0) {
        QMessageBox::warning(
            this,
            tr("Animation Edit Failed"),
            tr("The selected pivot could not be mapped into the active clip hierarchy."));
        return;
    }

    const auto beforeDrafts = currentRenderAnimationDrafts;
    OW3D::Render::RenderAnimationEditDraft* draft =
        ensureRenderAnimationEditDraft(*clip, animationChunk);
    if (!draft) {
        QMessageBox::warning(
            this,
            tr("Animation Edit Failed"),
            tr("The active clip could not be prepared for draft editing."));
        return;
    }

    DensePivotAnimationSamples samples =
        OW3D::Render::BuildDensePivotAnimationSamples(*clip, clipPivotIndex, clipFrameCount, draft);

    samples.translationX[static_cast<std::size_t>(frameIndex)] =
        DeleteDenseFloatFrameValue(samples.translationX, frameIndex);
    samples.translationY[static_cast<std::size_t>(frameIndex)] =
        DeleteDenseFloatFrameValue(samples.translationY, frameIndex);
    samples.translationZ[static_cast<std::size_t>(frameIndex)] =
        DeleteDenseFloatFrameValue(samples.translationZ, frameIndex);
    samples.rotation[static_cast<std::size_t>(frameIndex)] =
        DeleteDenseQuatFrameValue(samples.rotation, frameIndex);

    draft->pivotSamples[clipPivotIndex] = std::move(samples);
    if (!applyingRenderTransformUndoRedo) {
        RenderEditUndoEntry entry{};
        entry.kind = RenderEditUndoEntry::Kind::AnimationDrafts;
        entry.beforeDrafts = beforeDrafts;
        entry.afterDrafts = currentRenderAnimationDrafts;
        renderTransformUndoStack.push_back(std::move(entry));
        renderTransformRedoStack.clear();
    }
    setDirty(true);
    syncRenderAnimationUi();
}

bool MainWindow::flushPendingRenderAnimationDrafts(QString* outError) {
    if (outError) {
        outError->clear();
    }
    if (!chunkData || currentRenderAnimationDrafts.empty()) {
        return true;
    }

    std::vector<const void*> draftKeys;
    draftKeys.reserve(currentRenderAnimationDrafts.size());
    for (const auto& [chunkPtr, draft] : currentRenderAnimationDrafts) {
        if (draft.sourceAnimationChunk && !draft.pivotSamples.empty()) {
            draftKeys.push_back(chunkPtr);
        }
    }
    if (draftKeys.empty()) {
        currentRenderAnimationDrafts.clear();
        return true;
    }

    std::sort(
        draftKeys.begin(),
        draftKeys.end(),
        [](const void* lhs, const void* rhs) {
            return std::less<const void*>{}(lhs, rhs);
        });

    bool changed = false;
    for (const void* chunkPtr : draftKeys) {
        const auto itDraft = currentRenderAnimationDrafts.find(chunkPtr);
        if (itDraft == currentRenderAnimationDrafts.end()) {
            continue;
        }

        const auto& draft = itDraft->second;
        const auto animationChunk = FindChunkByPtr(chunkData->getChunks(), draft.sourceAnimationChunk);
        if (!animationChunk || animationChunk->id != kChunkAnimation) {
            if (outError) {
                *outError = tr("A pending animation draft no longer maps to a writable raw animation chunk.");
            }
            return false;
        }

        const auto beforeSnapshot = CloneChunkTree(animationChunk);
        QString truncateError;
        if (!TruncateRawAnimationToFrameCount(
            animationChunk,
            draft.numFrames,
            draft.frameRate,
            &truncateError))
        {
            if (outError) {
                *outError = truncateError.isEmpty()
                    ? tr("Failed to update the raw animation header while saving a draft.")
                    : truncateError;
            }
            return false;
        }
        std::vector<int> pivotIndices;
        pivotIndices.reserve(draft.pivotSamples.size());
        for (const auto& [pivotIndex, samples] : draft.pivotSamples) {
            if (!samples.translationX.empty()) {
                pivotIndices.push_back(pivotIndex);
            }
        }
        std::sort(pivotIndices.begin(), pivotIndices.end());

        for (const int pivotIndex : pivotIndices) {
            const auto samplesIt = draft.pivotSamples.find(pivotIndex);
            if (samplesIt == draft.pivotSamples.end()) {
                continue;
            }

            QString rewriteError;
            if (!RewriteRawAnimationPivotPoseChannels(
                animationChunk,
                pivotIndex,
                samplesIt->second,
                &rewriteError))
            {
                if (outError) {
                    *outError = rewriteError.isEmpty()
                        ? tr("Failed to rewrite a pending animation draft to raw channels.")
                        : rewriteError;
                }
                return false;
            }
        }

        if (!applyingRenderTransformUndoRedo) {
            RenderEditUndoEntry entry{};
            entry.kind = RenderEditUndoEntry::Kind::ChunkTree;
            entry.targetChunkPtr = animationChunk.get();
            entry.beforeSnapshot = beforeSnapshot;
            entry.afterSnapshot = CloneChunkTree(animationChunk);
            renderTransformUndoStack.push_back(std::move(entry));
            renderTransformRedoStack.clear();
        }
        changed = true;
    }

    currentRenderAnimationDrafts.clear();
    if (changed) {
        onChunkEdited();
    }
    return true;
}

void MainWindow::undoRenderTransform() {
    if (!chunkData || renderTransformUndoStack.empty()) {
        return;
    }

    RenderEditUndoEntry entry = renderTransformUndoStack.back();
    renderTransformUndoStack.pop_back();

    if (entry.kind == RenderEditUndoEntry::Kind::AnimationDrafts) {
        currentRenderAnimationDrafts = entry.beforeDrafts;
        applyingRenderTransformUndoRedo = true;
        syncRenderAnimationUi();
        applyingRenderTransformUndoRedo = false;
        setDirty(true);
        renderTransformRedoStack.push_back(std::move(entry));
        return;
    }

    const auto targetChunk = FindChunkByPtr(chunkData->getChunks(), entry.targetChunkPtr);
    if (!targetChunk || !entry.beforeSnapshot) {
        renderTransformRedoStack.clear();
        return;
    }
    OverwriteChunkTree(targetChunk, entry.beforeSnapshot);

    applyingRenderTransformUndoRedo = true;
    onChunkEdited();
    applyingRenderTransformUndoRedo = false;

    renderTransformRedoStack.push_back(std::move(entry));
}

void MainWindow::redoRenderTransform() {
    if (!chunkData || renderTransformRedoStack.empty()) {
        return;
    }

    RenderEditUndoEntry entry = renderTransformRedoStack.back();
    renderTransformRedoStack.pop_back();

    if (entry.kind == RenderEditUndoEntry::Kind::AnimationDrafts) {
        currentRenderAnimationDrafts = entry.afterDrafts;
        applyingRenderTransformUndoRedo = true;
        syncRenderAnimationUi();
        applyingRenderTransformUndoRedo = false;
        setDirty(true);
        renderTransformUndoStack.push_back(std::move(entry));
        return;
    }

    const auto targetChunk = FindChunkByPtr(chunkData->getChunks(), entry.targetChunkPtr);
    if (!targetChunk || !entry.afterSnapshot) {
        return;
    }
    OverwriteChunkTree(targetChunk, entry.afterSnapshot);

    applyingRenderTransformUndoRedo = true;
    onChunkEdited();
    applyingRenderTransformUndoRedo = false;

    renderTransformUndoStack.push_back(std::move(entry));
}

void MainWindow::applyRenderSettingsToViewport() {
    if (!renderViewport) {
        return;
    }

    OW3D::Render::RenderSettings settings{};
    settings.profile = OW3D::Render::ParityProfile::W3DViewD3D11Baseline;
    settings.enableFog = renderFogToggle ? renderFogToggle->isChecked() : true;
    settings.enableLod = renderLodToggle ? renderLodToggle->isChecked() : true;
    settings.lodBias = renderLodBiasSpin ? static_cast<float>(renderLodBiasSpin->value()) : 1.0f;
    settings.debugShowUv = renderUvDebugToggle ? renderUvDebugToggle->isChecked() : false;
    settings.lockLodLevel = renderLodLockToggle ? renderLodLockToggle->isChecked() : false;
    settings.lockedLodLevel = renderLodLevelSpin ? renderLodLevelSpin->value() : 0;
    settings.showCameraGizmo = renderCameraGizmoToggle ? renderCameraGizmoToggle->isChecked() : true;
    settings.showPivotMarkers = renderPivotMarkersToggle ? renderPivotMarkersToggle->isChecked() : true;
    renderViewport->SetRenderSettings(settings);
}

void MainWindow::addRenderSkeletons() {
    const QString startDir = !currentFilePath.isEmpty()
        ? QFileInfo(currentFilePath).absolutePath()
        : (lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory);
    const QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        tr("Add Model/Skeleton Asset"),
        startDir,
        tr("Render Assets (*.w3d *.W3D *.wlt *.WLT *.mix *.MIX *.dat *.DAT *.dbs *.DBS);;"
           "W3D Files (*.w3d *.W3D *.wlt *.WLT);;"
           "Archive Files (*.mix *.MIX *.dat *.DAT *.dbs *.DBS);;"
           "All Files (*)"));
    if (filePaths.isEmpty()) {
        return;
    }

    lastDirectory = QFileInfo(filePaths.front()).absolutePath();

    bool addedAny = false;
    QStringList errors;
    for (const QString& filePath : filePaths) {
        if (!IsMixArchivePath(filePath)
            && currentExternalRenderAssetPaths.contains(NormalizeAbsolutePathKey(filePath))) {
            continue;
        }

        RenderSessionAsset asset;
        QString loadError;
        const bool loaded = IsMixArchivePath(filePath)
            ? tryLoadRenderSessionArchiveAsset(
                filePath,
                RenderSessionAssetRole::Skeleton,
                asset,
                &loadError)
            : tryLoadRenderSessionAsset(
                filePath,
                RenderSessionAssetRole::Skeleton,
                asset,
                &loadError);
        if (!loaded) {
            if (!loadError.isEmpty()) {
                errors.push_back(tr("%1: %2").arg(QFileInfo(filePath).fileName(), loadError));
            }
            continue;
        }
        if (asset.hierarchyNames.isEmpty() && asset.meshCount <= 0) {
            errors.push_back(tr("%1 does not contain any hierarchy or mesh data.").arg(asset.displayLabel));
            continue;
        }

        currentExternalRenderAssetPaths.insert(RenderSessionAssetSourceKey(asset));
        currentExternalRenderAssets.push_back(std::move(asset));
        addedAny = true;
    }

    if (!errors.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Some Reference Assets Were Skipped"),
            errors.join(QStringLiteral("\n\n")));
    }

    if (addedAny) {
        currentRenderSuppressedMissingHierarchyKey.clear();
        currentRenderSuppressedMissingMeshKey.clear();
        populateTree();
        rebuildRenderScene();
    }
}

void MainWindow::addRenderAnimations() {
    const QString startDir = !currentFilePath.isEmpty()
        ? QFileInfo(currentFilePath).absolutePath()
        : (lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory);
    const QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        tr("Add Animation Asset"),
        startDir,
        tr("Render Assets (*.w3d *.W3D *.wlt *.WLT *.mix *.MIX *.dat *.DAT *.dbs *.DBS);;"
           "W3D Files (*.w3d *.W3D *.wlt *.WLT);;"
           "Archive Files (*.mix *.MIX *.dat *.DAT *.dbs *.DBS);;"
           "All Files (*)"));
    if (filePaths.isEmpty()) {
        return;
    }

    lastDirectory = QFileInfo(filePaths.front()).absolutePath();

    bool addedAny = false;
    QStringList errors;
    for (const QString& filePath : filePaths) {
        if (!IsMixArchivePath(filePath)
            && currentExternalRenderAssetPaths.contains(NormalizeAbsolutePathKey(filePath))) {
            continue;
        }

        RenderSessionAsset asset;
        QString loadError;
        const bool loaded = IsMixArchivePath(filePath)
            ? tryLoadRenderSessionArchiveAsset(
                filePath,
                RenderSessionAssetRole::AnimationLibrary,
                asset,
                &loadError)
            : tryLoadRenderSessionAsset(
                filePath,
                RenderSessionAssetRole::AnimationLibrary,
                asset,
                &loadError);
        if (!loaded) {
            if (!loadError.isEmpty()) {
                errors.push_back(tr("%1: %2").arg(QFileInfo(filePath).fileName(), loadError));
            }
            continue;
        }
        if (asset.animationCount <= 0) {
            errors.push_back(tr("%1 does not contain any animation chunks.").arg(asset.displayLabel));
            continue;
        }

        currentExternalRenderAssetPaths.insert(RenderSessionAssetSourceKey(asset));
        currentExternalRenderAssets.push_back(std::move(asset));
        addedAny = true;
    }

    if (!errors.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Some Animation Files Were Skipped"),
            errors.join(QStringLiteral("\n\n")));
    }

    if (addedAny) {
        populateTree();
        rebuildRenderScene();
    }
}

void MainWindow::selectRenderTextureFolder() {
    const QString startDir = !currentRenderTextureDirectory.isEmpty()
        ? currentRenderTextureDirectory
        : (!currentFilePath.isEmpty()
            ? QFileInfo(currentFilePath).absolutePath()
            : (lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory));
    const QString selectedDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Texture Folder"),
        startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (selectedDir.isEmpty()) {
        return;
    }

    currentRenderTextureDirectory = QDir::cleanPath(selectedDir);
    currentRenderSuppressedMissingTextureKey.clear();
    lastDirectory = currentRenderTextureDirectory;
    rebuildRenderScene();
}

void MainWindow::clearRenderTextureFolder() {
    if (currentRenderTextureDirectory.isEmpty()) {
        return;
    }

    currentRenderTextureDirectory.clear();
    currentRenderSuppressedMissingTextureKey.clear();
    rebuildRenderScene();
}

void MainWindow::removeSelectedRenderSessionAsset() {
    if (!renderAssetsTree || !renderAssetsTree->currentItem()) {
        return;
    }

    const QString normalizedPath =
        renderAssetsTree->currentItem()->data(0, kRenderAssetPathRole).toString();
    if (normalizedPath.isEmpty()) {
        return;
    }

    const auto newEnd = std::remove_if(
        currentExternalRenderAssets.begin(),
        currentExternalRenderAssets.end(),
        [&](const RenderSessionAsset& asset) {
            return RenderSessionAssetSourceKey(asset).compare(
                normalizedPath,
                Qt::CaseInsensitive) == 0;
        });
    if (newEnd == currentExternalRenderAssets.end()) {
        return;
    }
    currentExternalRenderAssets.erase(newEnd, currentExternalRenderAssets.end());
    currentExternalRenderAssetPaths.remove(normalizedPath);
    currentRenderSuppressedMissingHierarchyKey.clear();
    currentRenderSuppressedMissingMeshKey.clear();
    populateTree();
    rebuildRenderScene();
}

void MainWindow::clearRenderAnimationLibraries() {
    const auto newEnd = std::remove_if(
        currentExternalRenderAssets.begin(),
        currentExternalRenderAssets.end(),
        [](const RenderSessionAsset& asset) {
            return asset.role == RenderSessionAssetRole::AnimationLibrary;
        });
    if (newEnd == currentExternalRenderAssets.end()) {
        return;
    }
    currentExternalRenderAssets.erase(newEnd, currentExternalRenderAssets.end());

    currentExternalRenderAssetPaths.clear();
    for (const auto& asset : currentExternalRenderAssets) {
        currentExternalRenderAssetPaths.insert(RenderSessionAssetSourceKey(asset));
    }

    currentRenderSuppressedMissingMeshKey.clear();
    populateTree();
    rebuildRenderScene();
}

void MainWindow::handleRenderAnimationSelectionChanged() {
    if (!renderAnimationsTree || !renderAnimationsTree->currentItem()) {
        refreshRenderPlaybackControls();
        return;
    }

    const QTreeWidgetItem* item = renderAnimationsTree->currentItem();
    if (!item->data(0, kRenderAnimationIndexRole).isValid()
        || !(item->flags() & Qt::ItemIsEnabled))
    {
        refreshRenderPlaybackControls();
        return;
    }

    setRenderActiveAnimationIndex(
        item->data(0, kRenderAnimationIndexRole).toInt(),
        false,
        true);
}

void MainWindow::handleRenderAnimationEditKeysChanged(bool checked) {
    currentRenderAnimationEditKeysEnabled = checked;
    syncRenderAnimationUi();
}

void MainWindow::handleRenderPrepSourceClipChanged(int) {
    if (suppressRenderAnimationPrepUiSignals || !renderAnimationPrepSourceCombo) {
        return;
    }

    const int targetAnimationIndex =
        renderAnimationPrepSourceCombo->currentData().toInt();
    currentRenderAnimationPrepState.targetAnimationIndex = targetAnimationIndex;

    const auto& animations = currentRenderSceneResult.scene.animations;
    if (targetAnimationIndex >= 0
        && targetAnimationIndex < static_cast<int>(animations.size())) {
        currentRenderAnimationPrepState.targetClipIdentity =
            BuildRenderAnimationClipIdentity(
                animations[static_cast<std::size_t>(targetAnimationIndex)]);
    }
    else {
        currentRenderAnimationPrepState.targetAnimationIndex = -1;
        currentRenderAnimationPrepState.targetClipIdentity.reset();
    }

    refreshRenderAnimationPrepControls();
}

void MainWindow::handleRenderPrepStaticPoseFrameChanged(int value) {
    currentRenderAnimationPrepState.staticPoseFrame = value;
    refreshRenderAnimationPrepControls();
}

void MainWindow::applyRenderAnimationClipPrep() {
    int activeAnimationIndex = -1;
    const OW3D::Render::RenderAnimationClip* baseClip = nullptr;
    std::shared_ptr<ChunkItem> baseAnimationChunk;
    QString editReason;
    if (!ResolveEditablePrimaryRenderAnimation(
        chunkData.get(),
        currentRenderSceneResult,
        currentRenderAnimationPlayback,
        &activeAnimationIndex,
        &baseClip,
        &baseAnimationChunk,
        &editReason)) {
        QMessageBox::information(
            this,
            tr("Clip Prep Unavailable"),
            editReason.isEmpty() ? tr("The active clip is read-only.") : editReason);
        return;
    }
    if (!baseClip || !baseAnimationChunk) {
        return;
    }

    const int targetAnimationIndex = currentRenderAnimationPrepState.targetAnimationIndex;
    const auto& animations = currentRenderSceneResult.scene.animations;
    if (targetAnimationIndex < 0
        || targetAnimationIndex >= static_cast<int>(animations.size())) {
        QMessageBox::information(
            this,
            tr("Clip Prep Unavailable"),
            tr("Select a compatible source clip first."));
        return;
    }
    if (targetAnimationIndex == activeAnimationIndex) {
        QMessageBox::information(
            this,
            tr("Clip Prep Unavailable"),
            tr("The source clip must be different from the active base clip."));
        return;
    }

    const auto& targetClip = animations[static_cast<std::size_t>(targetAnimationIndex)];
    if (!targetClip.supportedForPlayback) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The selected source clip is not supported for playback sampling."));
        return;
    }

    const int hierarchyIndex = findCompatibleRenderHierarchyIndexForAnimation(activeAnimationIndex);
    if (hierarchyIndex < 0
        || hierarchyIndex >= static_cast<int>(currentRenderSceneResult.scene.hierarchies.size())) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The active base clip does not target a compatible hierarchy."));
        return;
    }

    const auto& hierarchy =
        currentRenderSceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
    if (std::find(
        hierarchy.compatibleAnimationIndices.begin(),
        hierarchy.compatibleAnimationIndices.end(),
        targetAnimationIndex) == hierarchy.compatibleAnimationIndices.end()) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The selected source clip is not compatible with the active hierarchy."));
        return;
    }

    const OW3D::Render::RenderAnimationEditDraft* existingDraft =
        findRenderAnimationEditDraftForClip(*baseClip);
    const uint32_t baseFrameCount =
        EffectiveRenderAnimationFrameCount(*baseClip, existingDraft);
    const int maxPoseFrame = baseFrameCount > 0u
        ? std::max(0, static_cast<int>(baseFrameCount) - 1)
        : 0;
    const int poseFrame =
        std::clamp(currentRenderAnimationPrepState.staticPoseFrame, 0, maxPoseFrame);

    const OW3D::Render::RenderAnimationEditDraft* targetDraft =
        findRenderAnimationEditDraftForClip(targetClip);
    const uint32_t targetFrameCount =
        EffectiveRenderAnimationFrameCount(targetClip, targetDraft);
    const float targetFrameRate =
        EffectiveRenderAnimationFrameRate(targetClip, targetDraft);
    if (targetFrameCount == 0u) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The selected source clip reports zero frames."));
        return;
    }
    if (targetFrameCount > 1u && targetFrameRate <= 0.0f) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The selected source clip reports an invalid frame rate."));
        return;
    }

    const auto beforeDrafts = currentRenderAnimationDrafts;
    OW3D::Render::RenderAnimationEditDraft* baseDraft =
        ensureRenderAnimationEditDraft(*baseClip, baseAnimationChunk);
    if (!baseDraft) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The active base clip could not be prepared for draft editing."));
        return;
    }

    PrepareAnimationEditDraftFromStaticPose(
        *baseClip,
        *baseDraft,
        targetFrameCount,
        targetFrameRate,
        static_cast<float>(poseFrame));

    if (!applyingRenderTransformUndoRedo) {
        RenderEditUndoEntry entry{};
        entry.kind = RenderEditUndoEntry::Kind::AnimationDrafts;
        entry.beforeDrafts = beforeDrafts;
        entry.afterDrafts = currentRenderAnimationDrafts;
        renderTransformUndoStack.push_back(std::move(entry));
        renderTransformRedoStack.clear();
    }

    currentRenderAnimationPrepState.staticPoseFrame = poseFrame;
    setDirty(true);
    currentRenderAnimationPlayback.timeSeconds = std::clamp(
        currentRenderAnimationPlayback.timeSeconds,
        0.0f,
        AnimationClipDurationSeconds(*baseClip, baseDraft));
    syncRenderAnimationUi();
}

void MainWindow::applyRenderAnimationStaticPoseToClip() {
    int activeAnimationIndex = -1;
    const OW3D::Render::RenderAnimationClip* clip = nullptr;
    std::shared_ptr<ChunkItem> animationChunk;
    QString editReason;
    if (!ResolveEditablePrimaryRenderAnimation(
        chunkData.get(),
        currentRenderSceneResult,
        currentRenderAnimationPlayback,
        &activeAnimationIndex,
        &clip,
        &animationChunk,
        &editReason)) {
        QMessageBox::information(
            this,
            tr("Clip Prep Unavailable"),
            editReason.isEmpty() ? tr("The active clip is read-only.") : editReason);
        return;
    }
    Q_UNUSED(activeAnimationIndex);
    if (!clip || !animationChunk) {
        return;
    }

    const OW3D::Render::RenderAnimationEditDraft* existingDraft =
        findRenderAnimationEditDraftForClip(*clip);
    const uint32_t clipFrameCount =
        EffectiveRenderAnimationFrameCount(*clip, existingDraft);
    const float clipFrameRate =
        EffectiveRenderAnimationFrameRate(*clip, existingDraft);
    if (clipFrameCount == 0u) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The active clip reports zero frames."));
        return;
    }

    const int maxPoseFrame = std::max(0, static_cast<int>(clipFrameCount) - 1);
    const int poseFrame =
        std::clamp(currentRenderAnimationPrepState.staticPoseFrame, 0, maxPoseFrame);

    const auto beforeDrafts = currentRenderAnimationDrafts;
    OW3D::Render::RenderAnimationEditDraft* draft =
        ensureRenderAnimationEditDraft(*clip, animationChunk);
    if (!draft) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The active clip could not be prepared for draft editing."));
        return;
    }

    PrepareAnimationEditDraftFromStaticPose(
        *clip,
        *draft,
        clipFrameCount,
        clipFrameRate,
        static_cast<float>(poseFrame));

    if (!applyingRenderTransformUndoRedo) {
        RenderEditUndoEntry entry{};
        entry.kind = RenderEditUndoEntry::Kind::AnimationDrafts;
        entry.beforeDrafts = beforeDrafts;
        entry.afterDrafts = currentRenderAnimationDrafts;
        renderTransformUndoStack.push_back(std::move(entry));
        renderTransformRedoStack.clear();
    }

    currentRenderAnimationPrepState.staticPoseFrame = poseFrame;
    setDirty(true);
    currentRenderAnimationPlayback.timeSeconds = std::clamp(
        currentRenderAnimationPlayback.timeSeconds,
        0.0f,
        AnimationClipDurationSeconds(*clip, draft));
    syncRenderAnimationUi();
}

void MainWindow::applyRenderAnimationFitSourceToClip() {
    int activeAnimationIndex = -1;
    const OW3D::Render::RenderAnimationClip* baseClip = nullptr;
    std::shared_ptr<ChunkItem> baseAnimationChunk;
    QString editReason;
    if (!ResolveEditablePrimaryRenderAnimation(
        chunkData.get(),
        currentRenderSceneResult,
        currentRenderAnimationPlayback,
        &activeAnimationIndex,
        &baseClip,
        &baseAnimationChunk,
        &editReason)) {
        QMessageBox::information(
            this,
            tr("Clip Prep Unavailable"),
            editReason.isEmpty() ? tr("The active clip is read-only.") : editReason);
        return;
    }
    if (!baseClip || !baseAnimationChunk) {
        return;
    }

    const int sourceAnimationIndex = currentRenderAnimationPrepState.targetAnimationIndex;
    const auto& animations = currentRenderSceneResult.scene.animations;
    if (sourceAnimationIndex < 0
        || sourceAnimationIndex >= static_cast<int>(animations.size())) {
        QMessageBox::information(
            this,
            tr("Clip Prep Unavailable"),
            tr("Select a compatible source clip first."));
        return;
    }
    if (sourceAnimationIndex == activeAnimationIndex) {
        QMessageBox::information(
            this,
            tr("Clip Prep Unavailable"),
            tr("The source clip must be different from the active base clip."));
        return;
    }

    const auto& sourceClip = animations[static_cast<std::size_t>(sourceAnimationIndex)];
    if (!sourceClip.supportedForPlayback) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The selected source clip is not supported for playback sampling."));
        return;
    }

    const int hierarchyIndex = findCompatibleRenderHierarchyIndexForAnimation(activeAnimationIndex);
    if (hierarchyIndex < 0
        || hierarchyIndex >= static_cast<int>(currentRenderSceneResult.scene.hierarchies.size())) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The active base clip does not target a compatible hierarchy."));
        return;
    }

    const auto& hierarchy =
        currentRenderSceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
    if (std::find(
        hierarchy.compatibleAnimationIndices.begin(),
        hierarchy.compatibleAnimationIndices.end(),
        sourceAnimationIndex) == hierarchy.compatibleAnimationIndices.end()) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The selected source clip is not compatible with the active hierarchy."));
        return;
    }

    const OW3D::Render::RenderAnimationEditDraft* existingBaseDraft =
        findRenderAnimationEditDraftForClip(*baseClip);
    const uint32_t baseFrameCount =
        EffectiveRenderAnimationFrameCount(*baseClip, existingBaseDraft);
    const float baseFrameRate =
        EffectiveRenderAnimationFrameRate(*baseClip, existingBaseDraft);
    if (baseFrameCount == 0u) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The active clip reports zero frames."));
        return;
    }
    if (baseFrameCount > 1u && baseFrameRate <= 0.0f) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The active clip reports an invalid frame rate."));
        return;
    }

    const OW3D::Render::RenderAnimationEditDraft* sourceDraft =
        findRenderAnimationEditDraftForClip(sourceClip);
    const uint32_t sourceFrameCount =
        EffectiveRenderAnimationFrameCount(sourceClip, sourceDraft);
    if (sourceFrameCount == 0u) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The selected source clip reports zero frames."));
        return;
    }

    const auto beforeDrafts = currentRenderAnimationDrafts;
    OW3D::Render::RenderAnimationEditDraft* baseDraft =
        ensureRenderAnimationEditDraft(*baseClip, baseAnimationChunk);
    if (!baseDraft) {
        QMessageBox::warning(
            this,
            tr("Clip Prep Failed"),
            tr("The active base clip could not be prepared for draft editing."));
        return;
    }
    sourceDraft = findRenderAnimationEditDraftForClip(sourceClip);

    PrepareAnimationEditDraftFromFittedSourceClip(
        *baseClip,
        *baseDraft,
        sourceClip,
        sourceDraft,
        baseFrameCount,
        baseFrameRate);

    if (!applyingRenderTransformUndoRedo) {
        RenderEditUndoEntry entry{};
        entry.kind = RenderEditUndoEntry::Kind::AnimationDrafts;
        entry.beforeDrafts = beforeDrafts;
        entry.afterDrafts = currentRenderAnimationDrafts;
        renderTransformUndoStack.push_back(std::move(entry));
        renderTransformRedoStack.clear();
    }

    setDirty(true);
    currentRenderAnimationPlayback.timeSeconds = std::clamp(
        currentRenderAnimationPlayback.timeSeconds,
        0.0f,
        AnimationClipDurationSeconds(*baseClip, baseDraft));
    syncRenderAnimationUi();
}

void MainWindow::handleRenderBlendSourceClipChanged(int) {
    if (suppressRenderAnimationBlendUiSignals || !renderAnimationBlendSourceCombo) {
        return;
    }

    const int overlayAnimationIndex =
        renderAnimationBlendSourceCombo->currentData().toInt();
    currentRenderAnimationBlendState.overlayAnimationIndex = overlayAnimationIndex;

    const auto& animations = currentRenderSceneResult.scene.animations;
    if (overlayAnimationIndex >= 0
        && overlayAnimationIndex < static_cast<int>(animations.size())) {
        currentRenderAnimationBlendState.overlayClipIdentity =
            BuildRenderAnimationClipIdentity(
                animations[static_cast<std::size_t>(overlayAnimationIndex)]);
    }
    else {
        currentRenderAnimationBlendState.overlayAnimationIndex = -1;
        currentRenderAnimationBlendState.overlayClipIdentity.reset();
    }

    refreshRenderBlendStatus();
}

void MainWindow::handleRenderBlendTimingModeChanged(int) {
    if (suppressRenderAnimationBlendUiSignals || !renderAnimationBlendTimingCombo) {
        return;
    }

    currentRenderAnimationBlendState.timingMode =
        renderAnimationBlendTimingCombo->currentData().toInt() == 1
        ? RenderAnimationBlendState::TimingMode::FitSourceToRange
        : RenderAnimationBlendState::TimingMode::PreserveSourceRate;
    refreshRenderBlendStatus();
}

void MainWindow::handleRenderBlendPivotItemChanged(QTreeWidgetItem* item, int) {
    if (suppressRenderAnimationBlendUiSignals || !item) {
        return;
    }

    const QVariant pivotData = item->data(0, kRenderBlendPivotIndexRole);
    if (!pivotData.isValid()) {
        return;
    }

    const int pivotIndex = pivotData.toInt();
    if (item->checkState(0) == Qt::Checked) {
        currentRenderAnimationBlendState.pivotIndices.insert(pivotIndex);
    }
    else {
        currentRenderAnimationBlendState.pivotIndices.remove(pivotIndex);
    }
    if (!renderAnimationBlendPivotTree->currentItem()) {
        renderAnimationBlendPivotTree->setCurrentItem(item);
    }
    QTimer::singleShot(0, this, [this]() {
        refreshRenderBlendStatus();
    });
}

void MainWindow::handleRenderBlendPivotCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem*) {
    if (suppressRenderAnimationBlendUiSignals) {
        return;
    }

    if (!current) {
        currentRenderAnimationBlendState.selectedPivotIndex = -1;
        refreshRenderBlendPivotWeightEditor();
        return;
    }

    const QVariant pivotData = current->data(0, kRenderBlendPivotIndexRole);
    currentRenderAnimationBlendState.selectedPivotIndex =
        pivotData.isValid() ? pivotData.toInt() : -1;
    refreshRenderBlendPivotWeightEditor();
}

void MainWindow::handleRenderBlendIncludeDescendantsChanged(bool checked) {
    currentRenderAnimationBlendState.includeDescendants = checked;
    refreshRenderBlendStatus();
}

void MainWindow::handleRenderBlendTruncateChanged(bool checked) {
    currentRenderAnimationBlendState.truncateToShorter = checked;
    refreshRenderBlendStatus();
}

void MainWindow::handleRenderBlendNormalizeFrameRateChanged(bool checked) {
    currentRenderAnimationBlendState.normalizeBaseToSourceFrameRate = checked;
    refreshRenderBlendStatus();
}

void MainWindow::handleRenderBlendPinLeftHandChanged(bool checked) {
    currentRenderAnimationBlendState.pinLeftHand = checked;
    refreshRenderBlendStatus();
}

void MainWindow::handleRenderBlendPinRightHandChanged(bool checked) {
    currentRenderAnimationBlendState.pinRightHand = checked;
    refreshRenderBlendStatus();
}

void MainWindow::handleRenderBlendHandPinReferenceFrameChanged(int value) {
    currentRenderAnimationBlendState.handPinReferenceFrame = std::max(value, 0);
    refreshRenderBlendStatus();
}

void MainWindow::handleRenderBlendTranslationPercentChanged(int value) {
    currentRenderAnimationBlendState.translationBlendPercent = std::clamp(value, 0, 100);
    refreshRenderBlendControls();
}

void MainWindow::handleRenderBlendRotationPercentChanged(int value) {
    currentRenderAnimationBlendState.rotationBlendPercent = std::clamp(value, 0, 100);
    refreshRenderBlendControls();
}

void MainWindow::handleRenderBlendPivotAxisWeightChanged(int) {
    if (suppressRenderAnimationBlendUiSignals) {
        return;
    }

    const int selectedPivotIndex = currentRenderAnimationBlendState.selectedPivotIndex;
    if (selectedPivotIndex < 0) {
        return;
    }

    std::array<int, kRenderBlendAxisCount> axisPercents{};
    for (int axisIndex = 0; axisIndex < kRenderBlendAxisCount; ++axisIndex) {
        axisPercents[static_cast<std::size_t>(axisIndex)] = std::clamp(
            renderAnimationBlendPivotAxisPercentSpins[static_cast<std::size_t>(axisIndex)]->value(),
            0,
            100);
    }

    if (RenderBlendPivotAxisPercentsMatchDefaults(
            axisPercents,
            currentRenderAnimationBlendState.translationBlendPercent,
            currentRenderAnimationBlendState.rotationBlendPercent)) {
        currentRenderAnimationBlendState.pivotAxisBlendPercents.erase(selectedPivotIndex);
    }
    else {
        currentRenderAnimationBlendState.pivotAxisBlendPercents[selectedPivotIndex] = axisPercents;
    }
    refreshRenderBlendControls();
}

void MainWindow::resetRenderBlendSelectedPivotWeights() {
    const int selectedPivotIndex = currentRenderAnimationBlendState.selectedPivotIndex;
    if (selectedPivotIndex < 0) {
        return;
    }

    currentRenderAnimationBlendState.pivotAxisBlendPercents.erase(selectedPivotIndex);
    refreshRenderBlendControls();
}

void MainWindow::handleRenderBlendStartFrameChanged(int value) {
    currentRenderAnimationBlendState.startFrame = value;
    if (currentRenderAnimationBlendState.endFrame < value) {
        currentRenderAnimationBlendState.endFrame = value;
    }
    refreshRenderBlendStatus();
}

void MainWindow::handleRenderBlendEndFrameChanged(int value) {
    currentRenderAnimationBlendState.endFrame = value;
    if (currentRenderAnimationBlendState.startFrame > value) {
        currentRenderAnimationBlendState.startFrame = value;
    }
    refreshRenderBlendStatus();
}

void MainWindow::applyRenderAnimationPivotOverride() {
    int activeAnimationIndex = -1;
    const OW3D::Render::RenderAnimationClip* baseClip = nullptr;
    std::shared_ptr<ChunkItem> baseAnimationChunk;
    QString editReason;
    if (!ResolveEditablePrimaryRenderAnimation(
        chunkData.get(),
        currentRenderSceneResult,
        currentRenderAnimationPlayback,
        &activeAnimationIndex,
        &baseClip,
        &baseAnimationChunk,
        &editReason)) {
        QMessageBox::information(
            this,
            tr("Pivot Override Unavailable"),
            editReason.isEmpty() ? tr("The active clip is read-only.") : editReason);
        return;
    }
    if (!baseClip || !baseAnimationChunk) {
        return;
    }

    const int overlayAnimationIndex = currentRenderAnimationBlendState.overlayAnimationIndex;
    const auto& animations = currentRenderSceneResult.scene.animations;
    if (overlayAnimationIndex < 0
        || overlayAnimationIndex >= static_cast<int>(animations.size())) {
        QMessageBox::information(
            this,
            tr("Pivot Override Unavailable"),
            tr("Select a compatible source clip first."));
        return;
    }
    if (overlayAnimationIndex == activeAnimationIndex) {
        QMessageBox::information(
            this,
            tr("Pivot Override Unavailable"),
            tr("The source clip must be different from the active base clip."));
        return;
    }

    const auto& sourceClip = animations[static_cast<std::size_t>(overlayAnimationIndex)];
    if (!sourceClip.supportedForPlayback) {
        QMessageBox::warning(
            this,
            tr("Pivot Override Failed"),
            tr("The selected source clip is not supported for playback sampling."));
        return;
    }

    const int hierarchyIndex = findCompatibleRenderHierarchyIndexForAnimation(activeAnimationIndex);
    if (hierarchyIndex < 0
        || hierarchyIndex >= static_cast<int>(currentRenderSceneResult.scene.hierarchies.size())) {
        QMessageBox::warning(
            this,
            tr("Pivot Override Failed"),
            tr("The active base clip does not target a compatible hierarchy."));
        return;
    }

    const auto& hierarchy =
        currentRenderSceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
    if (std::find(
        hierarchy.compatibleAnimationIndices.begin(),
        hierarchy.compatibleAnimationIndices.end(),
        overlayAnimationIndex) == hierarchy.compatibleAnimationIndices.end()) {
        QMessageBox::warning(
            this,
            tr("Pivot Override Failed"),
            tr("The selected source clip is not compatible with the active hierarchy."));
        return;
    }

    const int sourceHierarchyIndex = findCompatibleRenderHierarchyIndexForAnimation(overlayAnimationIndex);
    if (sourceHierarchyIndex < 0
        || sourceHierarchyIndex >= static_cast<int>(currentRenderSceneResult.scene.hierarchies.size())) {
        QMessageBox::warning(
            this,
            tr("Pivot Override Failed"),
            tr("The selected source clip does not resolve to a compatible hierarchy."));
        return;
    }
    const auto& sourceHierarchy =
        currentRenderSceneResult.scene.hierarchies[static_cast<std::size_t>(sourceHierarchyIndex)];
    const auto basePivotIndexByName = BuildRenderPivotIndexByName(hierarchy);
    const auto sourcePivotIndexByName = BuildRenderPivotIndexByName(sourceHierarchy);

    const QSet<int> affectedPivots = ExpandBlendPivotSelection(
        hierarchy,
        currentRenderAnimationBlendState.pivotIndices,
        currentRenderAnimationBlendState.includeDescendants);
    if (affectedPivots.isEmpty()) {
        QMessageBox::information(
            this,
            tr("Pivot Override Unavailable"),
            tr("Check one or more pivots to override."));
        return;
    }
    bool affectedPivotsHaveBlend = false;
    for (const int pivotIndex : affectedPivots) {
        const auto axisPercents = ResolveRenderBlendPivotAxisPercents(
            currentRenderAnimationBlendState.translationBlendPercent,
            currentRenderAnimationBlendState.rotationBlendPercent,
            currentRenderAnimationBlendState.pivotAxisBlendPercents,
            pivotIndex);
        if (RenderBlendPivotAxisPercentsHaveAnyBlend(axisPercents)) {
            affectedPivotsHaveBlend = true;
            break;
        }
    }
    if (!affectedPivotsHaveBlend) {
        QMessageBox::information(
            this,
            tr("Pivot Override Unavailable"),
            tr("Set a default percentage above 0, or assign custom axis weights above 0 to an affected pivot, before applying the override."));
        return;
    }

    OW3D::Render::RenderAnimationEditDraft* baseDraft =
        ensureRenderAnimationEditDraft(*baseClip, baseAnimationChunk);
    if (!baseDraft) {
        QMessageBox::warning(
            this,
            tr("Pivot Override Failed"),
            tr("The active base clip could not be prepared for draft editing."));
        return;
    }

    const uint32_t baseFrameCount = EffectiveRenderAnimationFrameCount(*baseClip, baseDraft);
    const float baseFrameRate = EffectiveRenderAnimationFrameRate(*baseClip, baseDraft);
    const auto* sourceDraft = findRenderAnimationEditDraftForClip(sourceClip);
    const uint32_t sourceFrameCount =
        EffectiveRenderAnimationFrameCount(sourceClip, sourceDraft);
    const float sourceFrameRate = EffectiveRenderAnimationFrameRate(sourceClip, sourceDraft);
    const int selectedBaseMaxFrame = baseClip->numFrames > 0u
        ? std::max(0, static_cast<int>(baseClip->numFrames) - 1)
        : 0;
    const int requestedStartFrame =
        std::clamp(currentRenderAnimationBlendState.startFrame, 0, selectedBaseMaxFrame);
    const int requestedEndFrame = std::clamp(
        std::max(currentRenderAnimationBlendState.startFrame, currentRenderAnimationBlendState.endFrame),
        0,
        selectedBaseMaxFrame);
    const bool preserveSourceRate =
        currentRenderAnimationBlendState.timingMode
        == RenderAnimationBlendState::TimingMode::PreserveSourceRate;
    const PivotOverrideBaseTimingPreview baseTimingPreview =
        ComputePivotOverrideBaseTimingPreview(
            baseFrameCount,
            baseFrameRate,
            requestedStartFrame,
            requestedEndFrame,
            preserveSourceRate,
            currentRenderAnimationBlendState.normalizeBaseToSourceFrameRate,
            sourceFrameRate);
    const int startFrame = baseTimingPreview.appliedStartFrame;
    const int endFrame = baseTimingPreview.appliedEndFrame;
    const int effectiveEndFrame = ComputePivotOverrideEffectiveEndFrame(
        baseTimingPreview.frameCount,
        baseTimingPreview.frameRate,
        sourceFrameCount,
        sourceFrameRate,
        startFrame,
        endFrame,
        preserveSourceRate,
        currentRenderAnimationBlendState.truncateToShorter);
    if (effectiveEndFrame < startFrame) {
        QMessageBox::warning(
            this,
            tr("Pivot Override Failed"),
            tr("The selected source clip does not contain any frames that can be applied."));
        return;
    }

    const auto beforeDrafts = currentRenderAnimationDrafts;
    if (baseTimingPreview.normalizationApplied) {
        RetimeAnimationEditDraft(
            *baseClip,
            *baseDraft,
            baseTimingPreview.frameCount,
            baseTimingPreview.frameRate);
    }

    const uint32_t appliedBaseFrameCount =
        EffectiveRenderAnimationFrameCount(*baseClip, baseDraft);
    const float appliedBaseFrameRate =
        EffectiveRenderAnimationFrameRate(*baseClip, baseDraft);
    const uint32_t newDraftFrameCount =
        (preserveSourceRate && currentRenderAnimationBlendState.truncateToShorter)
        ? static_cast<uint32_t>(effectiveEndFrame + 1)
        : appliedBaseFrameCount;
    if (baseDraft->numFrames != newDraftFrameCount) {
        ResizeAnimationEditDraftFrameCount(*baseDraft, newDraftFrameCount);
    }

    const bool handPinsRequested =
        currentRenderAnimationBlendState.pinLeftHand
        || currentRenderAnimationBlendState.pinRightHand;
    QStringList skippedHandPinReasons;
    std::vector<RenderHandPinTarget> handPinTargets;
    if (handPinsRequested) {
        const bool multiFrameDraft = newDraftFrameCount > 1u;
        if (multiFrameDraft && appliedBaseFrameRate <= 0.0f) {
            skippedHandPinReasons.push_back(
                tr("Hand pins were skipped because the prepared base draft has an invalid frame rate."));
        }
        else {
            const std::optional<OW3D::Render::RenderAnimationEditDraft> preBlendDraft = *baseDraft;
            const int maxReferenceFrame = newDraftFrameCount > 0u
                ? std::max(0, static_cast<int>(newDraftFrameCount) - 1)
                : 0;
            const int referenceFrame = std::clamp(
                currentRenderAnimationBlendState.handPinReferenceFrame,
                0,
                maxReferenceFrame);

            auto appendHandPinTarget = [&](bool leftHand) {
                const QString handLabel = leftHand ? tr("Left hand") : tr("Right hand");
                const std::optional<RenderHandPinChain> chain =
                    ResolveRenderHandPinChain(
                        hierarchy,
                        basePivotIndexByName,
                        leftHand);
                if (!chain.has_value()) {
                    skippedHandPinReasons.push_back(
                        tr("%1 pin was skipped because no usable clavicle/upperarm/forearm chain could be resolved.")
                            .arg(handLabel));
                    return;
                }

                const std::optional<OW3D::Render::Vec3> targetWorldPosition =
                    SampleRenderHandPinTargetWorldPosition(
                        currentRenderSceneResult.scene,
                        activeAnimationIndex,
                        hierarchyIndex,
                        chain->effectorPivotIndex,
                        referenceFrame,
                        appliedBaseFrameRate,
                        preBlendDraft);
                if (!targetWorldPosition.has_value()) {
                    skippedHandPinReasons.push_back(
                        tr("%1 pin was skipped because the base-pose target could not be sampled at frame %2.")
                            .arg(handLabel)
                            .arg(referenceFrame));
                    return;
                }

                RenderHandPinTarget handPinTarget{};
                handPinTarget.chain = *chain;
                handPinTarget.worldPosition = *targetWorldPosition;
                handPinTargets.push_back(std::move(handPinTarget));
            };

            if (currentRenderAnimationBlendState.pinLeftHand) {
                appendHandPinTarget(true);
            }
            if (currentRenderAnimationBlendState.pinRightHand) {
                appendHandPinTarget(false);
            }
        }
    }

    QStringList skippedPivotNames;
    int appliedPivotCount = 0;
    for (const int selectedPivotIndex : affectedPivots) {
        if (selectedPivotIndex < 0
            || selectedPivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
            continue;
        }

        const int basePivotIndex = ResolveMappedRenderPivotIndex(
            hierarchy,
            basePivotIndexByName,
            hierarchy,
            selectedPivotIndex);
        const int sourcePivotIndex = ResolveMappedRenderPivotIndex(
            sourceHierarchy,
            sourcePivotIndexByName,
            hierarchy,
            selectedPivotIndex);
        if (basePivotIndex < 0 || sourcePivotIndex < 0) {
            skippedPivotNames.push_back(QString::fromStdString(
                hierarchy.pivots[static_cast<std::size_t>(selectedPivotIndex)].name));
            continue;
        }

        const auto axisPercents = ResolveRenderBlendPivotAxisPercents(
            currentRenderAnimationBlendState.translationBlendPercent,
            currentRenderAnimationBlendState.rotationBlendPercent,
            currentRenderAnimationBlendState.pivotAxisBlendPercents,
            selectedPivotIndex);
        if (!RenderBlendPivotAxisPercentsHaveAnyBlend(axisPercents)) {
            continue;
        }
        const float translationBlendX =
            std::clamp(
                static_cast<float>(axisPercents[static_cast<std::size_t>(kRenderBlendPositionXAxis)]) / 100.0f,
                0.0f,
                1.0f);
        const float translationBlendY =
            std::clamp(
                static_cast<float>(axisPercents[static_cast<std::size_t>(kRenderBlendPositionYAxis)]) / 100.0f,
                0.0f,
                1.0f);
        const float translationBlendZ =
            std::clamp(
                static_cast<float>(axisPercents[static_cast<std::size_t>(kRenderBlendPositionZAxis)]) / 100.0f,
                0.0f,
                1.0f);
        const float rotationBlendX =
            std::clamp(
                static_cast<float>(axisPercents[static_cast<std::size_t>(kRenderBlendRotationXAxis)]) / 100.0f,
                0.0f,
                1.0f);
        const float rotationBlendY =
            std::clamp(
                static_cast<float>(axisPercents[static_cast<std::size_t>(kRenderBlendRotationYAxis)]) / 100.0f,
                0.0f,
                1.0f);
        const float rotationBlendZ =
            std::clamp(
                static_cast<float>(axisPercents[static_cast<std::size_t>(kRenderBlendRotationZAxis)]) / 100.0f,
                0.0f,
                1.0f);
        const bool uniformRotationBlend =
            std::fabs(rotationBlendX - rotationBlendY) <= 1.0e-6f
            && std::fabs(rotationBlendY - rotationBlendZ) <= 1.0e-6f;

        DensePivotAnimationSamples samples =
            OW3D::Render::BuildDensePivotAnimationSamples(
                *baseClip,
                basePivotIndex,
                newDraftFrameCount,
                baseDraft);

        for (int frameIndex = startFrame; frameIndex <= effectiveEndFrame; ++frameIndex) {
            float sourceFrame = 0.0f;
            if (currentRenderAnimationBlendState.timingMode
                == RenderAnimationBlendState::TimingMode::FitSourceToRange) {
                const float progress = startFrame == endFrame
                    ? 0.0f
                    : static_cast<float>(frameIndex - startFrame)
                        / static_cast<float>(endFrame - startFrame);
                sourceFrame = OW3D::Render::AnimationFrameFromNormalizedProgress(
                    sourceFrameCount,
                    progress);
            }
            else if (appliedBaseFrameRate > 0.0f && sourceFrameRate > 0.0f) {
                const float elapsedSeconds =
                    static_cast<float>(frameIndex - startFrame) / appliedBaseFrameRate;
                sourceFrame = elapsedSeconds * sourceFrameRate;
                if (sourceFrameCount > 0u) {
                    sourceFrame = std::clamp(
                        sourceFrame,
                        0.0f,
                        static_cast<float>(sourceFrameCount - 1u));
                }
            }
            else {
                const float progress = startFrame == endFrame
                    ? 0.0f
                    : static_cast<float>(frameIndex - startFrame)
                        / static_cast<float>(endFrame - startFrame);
                sourceFrame = OW3D::Render::AnimationFrameFromNormalizedProgress(
                    sourceFrameCount,
                    progress);
            }
            const OW3D::Render::Vec3 translation =
                OW3D::Render::SamplePivotAnimationTranslation(
                    &sourceClip,
                    sourcePivotIndex,
                    sourceFrame,
                    sourceDraft);
            const OW3D::Render::Vec4 rotation =
                OW3D::Render::SamplePivotAnimationRotation(
                    &sourceClip,
                    sourcePivotIndex,
                    sourceFrame,
                    sourceDraft);
            const OW3D::Render::Vec4 sourceRotation =
                OW3D::Render::AnimationNormalizeQuat(rotation);
            const OW3D::Render::Vec3 baseTranslation{
                samples.translationX[static_cast<std::size_t>(frameIndex)],
                samples.translationY[static_cast<std::size_t>(frameIndex)],
                samples.translationZ[static_cast<std::size_t>(frameIndex)]
            };
            const OW3D::Render::Vec4 baseRotation =
                OW3D::Render::AnimationNormalizeQuat(
                    samples.rotation[static_cast<std::size_t>(frameIndex)]);
            const OW3D::Render::Vec3 blendedTranslation{
                baseTranslation.x + (translation.x - baseTranslation.x) * translationBlendX,
                baseTranslation.y + (translation.y - baseTranslation.y) * translationBlendY,
                baseTranslation.z + (translation.z - baseTranslation.z) * translationBlendZ
            };
            OW3D::Render::Vec4 blendedRotation = baseRotation;
            if (uniformRotationBlend) {
                const float rotationBlend = rotationBlendX;
                blendedRotation =
                    rotationBlend <= 1.0e-6f
                    ? baseRotation
                    : (rotationBlend >= 1.0f - 1.0e-6f
                        ? sourceRotation
                        : OW3D::Render::AnimationNormalizeQuat(
                            OW3D::Render::AnimationSlerpQuat(
                                baseRotation,
                                sourceRotation,
                                rotationBlend)));
            }
            else {
                const W3dVectorStruct baseEuler =
                    EulerFromQuaternion(
                        baseRotation.x,
                        baseRotation.y,
                        baseRotation.z,
                        baseRotation.w);
                const W3dVectorStruct sourceEuler =
                    EulerFromQuaternion(
                        sourceRotation.x,
                        sourceRotation.y,
                        sourceRotation.z,
                        sourceRotation.w);
                blendedRotation = QuaternionFromEulerRadians(
                    BlendWrappedAngleRadians(baseEuler.X, sourceEuler.X, rotationBlendX),
                    BlendWrappedAngleRadians(baseEuler.Y, sourceEuler.Y, rotationBlendY),
                    BlendWrappedAngleRadians(baseEuler.Z, sourceEuler.Z, rotationBlendZ));
            }

            samples.translationX[static_cast<std::size_t>(frameIndex)] = blendedTranslation.x;
            samples.translationY[static_cast<std::size_t>(frameIndex)] = blendedTranslation.y;
            samples.translationZ[static_cast<std::size_t>(frameIndex)] = blendedTranslation.z;
            samples.rotation[static_cast<std::size_t>(frameIndex)] = blendedRotation;
        }

        baseDraft->pivotSamples[basePivotIndex] = std::move(samples);
        appliedPivotCount += 1;
    }

    if (appliedPivotCount <= 0) {
        currentRenderAnimationDrafts = beforeDrafts;
        QMessageBox::warning(
            this,
            tr("Pivot Override Failed"),
            tr("None of the selected pivots could be matched between the base and source hierarchies."));
        return;
    }

    if (!handPinTargets.empty()) {
        std::vector<int> handPinJointPivotIndices;
        for (const RenderHandPinTarget& handPinTarget : handPinTargets) {
            handPinJointPivotIndices.insert(
                handPinJointPivotIndices.end(),
                handPinTarget.chain.jointPivotIndices.begin(),
                handPinTarget.chain.jointPivotIndices.end());
        }
        std::sort(handPinJointPivotIndices.begin(), handPinJointPivotIndices.end());
        handPinJointPivotIndices.erase(
            std::unique(handPinJointPivotIndices.begin(), handPinJointPivotIndices.end()),
            handPinJointPivotIndices.end());

        std::unordered_map<int, OW3D::Render::RenderDensePivotAnimationSamples> handPinSamplesByPivot;
        handPinSamplesByPivot.reserve(handPinJointPivotIndices.size());
        for (const int jointPivotIndex : handPinJointPivotIndices) {
            handPinSamplesByPivot.emplace(
                jointPivotIndex,
                OW3D::Render::BuildDensePivotAnimationSamples(
                    *baseClip,
                    jointPivotIndex,
                    newDraftFrameCount,
                    baseDraft));
        }

        std::optional<OW3D::Render::RenderAnimationEditDraft> workingHandPinDraft = *baseDraft;
        SyncRenderHandPinSamplesIntoDraft(
            handPinSamplesByPivot,
            handPinJointPivotIndices,
            workingHandPinDraft);

        std::unordered_map<int, OW3D::Render::Mat4> handPinLocalOverrides;
        for (int frameIndex = startFrame;
            frameIndex <= effectiveEndFrame && frameIndex < static_cast<int>(newDraftFrameCount);
            ++frameIndex) {
            for (const RenderHandPinTarget& handPinTarget : handPinTargets) {
                ApplyRenderHandPinToFrame(
                    currentRenderSceneResult.scene,
                    hierarchy,
                    hierarchyIndex,
                    *baseClip,
                    activeAnimationIndex,
                    frameIndex,
                    appliedBaseFrameRate,
                    handPinTarget,
                    workingHandPinDraft,
                    handPinLocalOverrides,
                    handPinSamplesByPivot);
                SyncRenderHandPinSamplesIntoDraft(
                    handPinSamplesByPivot,
                    handPinTarget.chain.jointPivotIndices,
                    workingHandPinDraft);
            }
        }

        for (auto& [jointPivotIndex, jointSamples] : handPinSamplesByPivot) {
            baseDraft->pivotSamples[jointPivotIndex] = std::move(jointSamples);
        }
    }

    currentRenderAnimationBlendState.startFrame = requestedStartFrame;
    currentRenderAnimationBlendState.endFrame = requestedEndFrame;
    if (!applyingRenderTransformUndoRedo) {
        RenderEditUndoEntry entry{};
        entry.kind = RenderEditUndoEntry::Kind::AnimationDrafts;
        entry.beforeDrafts = beforeDrafts;
        entry.afterDrafts = currentRenderAnimationDrafts;
        renderTransformUndoStack.push_back(std::move(entry));
        renderTransformRedoStack.clear();
    }
    setDirty(true);
    if (appliedBaseFrameRate > 0.0f) {
        currentRenderAnimationPlayback.timeSeconds = std::clamp(
            currentRenderAnimationPlayback.timeSeconds,
            0.0f,
            AnimationClipDurationSeconds(*baseClip, baseDraft));
    }
    syncRenderAnimationUi();

    if (!skippedPivotNames.isEmpty()) {
        skippedPivotNames.removeDuplicates();
        QMessageBox::information(
            this,
            tr("Some Pivots Were Skipped"),
            tr("These pivots could not be matched by name in the source clip hierarchy:\n%1")
                .arg(skippedPivotNames.join(QStringLiteral("\n"))));
    }
    if (!skippedHandPinReasons.isEmpty()) {
        skippedHandPinReasons.removeDuplicates();
        QMessageBox::information(
            this,
            tr("Some Hand Pins Were Skipped"),
            skippedHandPinReasons.join(QStringLiteral("\n")));
    }
}

void MainWindow::handleViewportAnimationPlaybackPauseRequested() {
    if (!currentRenderAnimationPlayback.playing) {
        return;
    }

    currentRenderAnimationPlayback.playing = false;
    if (renderAnimationPlaybackTimer) {
        renderAnimationPlaybackTimer->stop();
    }
    renderAnimationPlaybackElapsed.invalidate();
    syncRenderAnimationUi();
}

void MainWindow::handleRenderAnimationPivotSheetCellClicked(int row, int) {
    if (suppressRenderAnimationPivotSheetSignals
        || !renderAnimationPivotSheetTable
        || !renderAnimationFrameSlider
        || row < 0
        || row >= renderAnimationPivotSheetTable->rowCount()) {
        return;
    }

    int frameIndex = row;
    if (const QTableWidgetItem* frameItem = renderAnimationPivotSheetTable->item(row, 0);
        frameItem != nullptr) {
        frameIndex = frameItem->data(Qt::UserRole).toInt();
    }

    frameIndex = std::max(frameIndex, 0);
    if (renderAnimationFrameSlider->value() == frameIndex) {
        refreshRenderAnimationPivotSheetFrameHighlight();
        return;
    }

    renderAnimationFrameSlider->setValue(frameIndex);
}

void MainWindow::toggleRenderAnimationPlayback() {
    if (currentRenderAnimationPlayback.activeAnimationIndex < 0) {
        if (!renderAnimationsTree || !renderAnimationsTree->currentItem()) {
            refreshRenderPlaybackControls();
            return;
        }
        const QTreeWidgetItem* item = renderAnimationsTree->currentItem();
        if (!item->data(0, kRenderAnimationIndexRole).isValid()
            || !(item->flags() & Qt::ItemIsEnabled))
        {
            refreshRenderPlaybackControls();
            return;
        }
        setRenderActiveAnimationIndex(
            item->data(0, kRenderAnimationIndexRole).toInt(),
            true,
            true);
        return;
    }

    currentRenderAnimationPlayback.playing = !currentRenderAnimationPlayback.playing;
    if (currentRenderAnimationPlayback.playing) {
        renderAnimationPlaybackElapsed.restart();
        if (renderAnimationPlaybackTimer) {
            renderAnimationPlaybackTimer->start();
        }
    }
    else {
        if (renderAnimationPlaybackTimer) {
            renderAnimationPlaybackTimer->stop();
        }
        renderAnimationPlaybackElapsed.invalidate();
    }

    syncRenderAnimationUi();
}

void MainWindow::stopRenderAnimationPlayback() {
    if (currentRenderAnimationPlayback.activeAnimationIndex < 0) {
        return;
    }

    currentRenderAnimationPlayback.playing = false;
    currentRenderAnimationPlayback.timeSeconds = 0.0f;
    if (renderAnimationPlaybackTimer) {
        renderAnimationPlaybackTimer->stop();
    }
    renderAnimationPlaybackElapsed.invalidate();
    syncRenderAnimationUi();
}

void MainWindow::exportActiveRenderAnimationGif() {
    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    const auto& animations = currentRenderSceneResult.scene.animations;
    if (activeIndex < 0 || activeIndex >= static_cast<int>(animations.size())) {
        QMessageBox::information(
            this,
            tr("Export Current Animation GIF"),
            tr("Select an active animation clip first."));
        return;
    }
    if (!SceneHasRenderableMeshData(currentRenderSceneResult)) {
        QMessageBox::warning(
            this,
            tr("Export Current Animation GIF"),
            tr("The current scene does not build a renderable model for export."));
        return;
    }

    const auto& clip = animations[static_cast<std::size_t>(activeIndex)];
    if (!clip.supportedForPlayback) {
        QMessageBox::warning(
            this,
            tr("Export Current Animation GIF"),
            tr("The active clip is not supported for playback sampling."));
        return;
    }

    const OW3D::Render::RenderAnimationEditDraft* activeDraft =
        findActiveRenderAnimationEditDraft();
    const uint32_t frameCount = EffectiveRenderAnimationFrameCount(clip, activeDraft);
    const float frameRate = EffectiveRenderAnimationFrameRate(clip, activeDraft);
    if (frameCount == 0u) {
        QMessageBox::warning(
            this,
            tr("Export Current Animation GIF"),
            tr("The active clip reports zero frames."));
        return;
    }
    if (frameCount > 1u && frameRate <= 0.0f) {
        QMessageBox::warning(
            this,
            tr("Export Current Animation GIF"),
            tr("The active clip has an invalid frame rate."));
        return;
    }

    QString clipStem = SanitizePathComponent(QString::fromStdString(clip.fullName));
    clipStem.replace(QLatin1Char('/'), QLatin1Char('_'));
    clipStem.replace(QLatin1Char('\\'), QLatin1Char('_'));
    if (clipStem.isEmpty()) {
        clipStem = QStringLiteral("current_animation");
    }
    QString defaultDirectory = lastDirectory;
    if (defaultDirectory.isEmpty()) {
        if (!currentFilePath.isEmpty()) {
            defaultDirectory = QFileInfo(currentFilePath).absolutePath();
        }
        else if (!currentArchiveRenderPath.isEmpty()) {
            defaultDirectory = QFileInfo(currentArchiveRenderPath).absolutePath();
        }
        else {
            defaultDirectory = QDir::homePath();
        }
    }
    const QString selectedPath = QFileDialog::getSaveFileName(
        this,
        tr("Export Current Animation GIF"),
        QDir(defaultDirectory).absoluteFilePath(clipStem + QStringLiteral(".gif")),
        tr("GIF Files (*.gif)"));
    if (selectedPath.isEmpty()) {
        return;
    }

    QString outputGifPath = QDir::cleanPath(QDir::fromNativeSeparators(selectedPath));
    if (QFileInfo(outputGifPath).suffix().compare(QStringLiteral("gif"), Qt::CaseInsensitive) != 0) {
        outputGifPath += QStringLiteral(".gif");
    }

    const QString outputDir = QFileInfo(outputGifPath).absolutePath();
    if (outputDir.isEmpty() || !QDir().mkpath(outputDir)) {
        QMessageBox::warning(
            this,
            tr("Export Current Animation GIF"),
            tr("Cannot create output directory: %1")
                .arg(QDir::toNativeSeparators(outputDir)));
        return;
    }

    const bool useCurrentEditorCamera = renderViewport != nullptr;
    const OW3D::Render::CameraState fixedCamera = useCurrentEditorCamera
        ? renderViewport->camera()
        : OW3D::Render::CameraState{};
    const std::optional<OW3D::Render::RenderAnimationEditDraft> exportDraft =
        activeDraft ? std::optional<OW3D::Render::RenderAnimationEditDraft>(*activeDraft) : std::nullopt;

    OW3D::Render::RenderSettings exportSettings{};
    exportSettings.profile = OW3D::Render::ParityProfile::W3DViewD3D11Baseline;
    exportSettings.enableFog = renderFogToggle ? renderFogToggle->isChecked() : true;
    exportSettings.enableLod = renderLodToggle ? renderLodToggle->isChecked() : true;
    exportSettings.lodBias = renderLodBiasSpin ? static_cast<float>(renderLodBiasSpin->value()) : 1.0f;
    exportSettings.debugShowUv = renderUvDebugToggle ? renderUvDebugToggle->isChecked() : false;
    exportSettings.lockLodLevel = renderLodLockToggle ? renderLodLockToggle->isChecked() : false;
    exportSettings.lockedLodLevel = renderLodLevelSpin ? renderLodLevelSpin->value() : 0;
    exportSettings.clearColor = { 0.86f, 0.86f, 0.86f, 1.0f };
    exportSettings.showCameraGizmo = false;
    exportSettings.showPivotMarkers = false;

    const int effectiveFrameCount = (frameCount > 1u)
        ? std::max(1, static_cast<int>(frameCount) - 1)
        : 1;
    const int frameDelayCentiseconds = GifDelayCentisecondsFromClipFrameRate(frameRate);

    QDialog exportDialog(this);
    exportDialog.setWindowTitle(tr("Export Current Animation GIF"));
    exportDialog.setModal(true);
    auto* exportLayout = new QVBoxLayout(&exportDialog);
    auto* exportViewport = new OW3D::Render::RenderViewportWidget(&exportDialog);
    exportViewport->setFixedSize(512, 512);
    exportViewport->SetContinuousRenderingEnabled(false);
    exportViewport->SetOverlayUiEnabled(false);
    exportLayout->addWidget(exportViewport, 0, Qt::AlignCenter);
    auto* exportStatusLabel = new QLabel(
        tr("Preparing `%1` for export...")
            .arg(QString::fromStdString(clip.fullName)),
        &exportDialog);
    exportStatusLabel->setWordWrap(true);
    exportLayout->addWidget(exportStatusLabel);
    auto* exportProgressBar = new QProgressBar(&exportDialog);
    exportProgressBar->setRange(0, effectiveFrameCount);
    exportProgressBar->setValue(0);
    exportLayout->addWidget(exportProgressBar);
    auto* cancelExportButton = new QPushButton(tr("Cancel"), &exportDialog);
    exportLayout->addWidget(cancelExportButton, 0, Qt::AlignRight);

    bool cancelRequested = false;
    bool exportDialogClosing = false;
    auto requestCancel = [&]() {
        if (cancelRequested || exportDialogClosing) {
            return;
        }
        cancelRequested = true;
        cancelExportButton->setEnabled(false);
        exportStatusLabel->setText(tr("Cancel requested. Finishing the current frame..."));
    };
    connect(cancelExportButton, &QPushButton::clicked, &exportDialog, requestCancel);
    connect(&exportDialog, &QDialog::rejected, &exportDialog, requestCancel);

    exportDialog.show();
    QCoreApplication::processEvents();

    exportViewport->SetRenderSettings(exportSettings);
    exportViewport->SetSceneResult(currentRenderSceneResult);
    exportViewport->SetAnimationEditDraft(exportDraft);
    exportViewport->SetAnimationEditingState(false, false, QString());
    if (useCurrentEditorCamera) {
        exportViewport->SetCameraState(fixedCamera);
    }
    else {
        exportViewport->FocusScene();
    }

    OW3D::Gif::Writer gifWriter;
    QString gifError;
    if (!gifWriter.Open(outputGifPath, 512, 512, &gifError)) {
        exportDialogClosing = true;
        exportDialog.close();
        QMessageBox::warning(
            this,
            tr("Export Current Animation GIF"),
            gifError.isEmpty() ? tr("Failed to open the output GIF for writing.") : gifError);
        return;
    }

    bool exportFailed = false;
    int framesWritten = 0;
    QString exportError;
    for (int frameIndex = 0; frameIndex < effectiveFrameCount; ++frameIndex) {
        exportProgressBar->setValue(frameIndex);
        exportStatusLabel->setText(
            tr("Exporting `%1` frame %2 / %3")
                .arg(QString::fromStdString(clip.fullName))
                .arg(frameIndex + 1)
                .arg(effectiveFrameCount));

        OW3D::Render::AnimationPlaybackState playback{};
        playback.activeAnimationIndex = activeIndex;
        playback.timeSeconds = frameRate > 0.0f
            ? static_cast<float>(frameIndex) / frameRate
            : 0.0f;
        playback.loop = true;
        playback.playing = false;
        playback.speed = 1.0f;

        exportViewport->SetAnimationPlayback(playback);
        exportViewport->RenderOnce();

        QImage frameImage;
        if (!exportViewport->CaptureCurrentFrame(frameImage)) {
            exportFailed = true;
            exportError = tr("Failed to capture a rendered frame.");
            break;
        }
        if (!gifWriter.AddFrame(frameImage, frameDelayCentiseconds, &gifError)) {
            exportFailed = true;
            exportError = gifError.isEmpty()
                ? tr("Failed to append a frame to the output GIF.")
                : gifError;
            break;
        }

        ++framesWritten;
        exportProgressBar->setValue(frameIndex + 1);
        QCoreApplication::processEvents();
        if (cancelRequested) {
            break;
        }
    }

    QString closeError;
    if (!gifWriter.Close(&closeError) && !exportFailed) {
        exportFailed = true;
        exportError = closeError.isEmpty()
            ? tr("Failed to finalize the output GIF.")
            : closeError;
    }

    exportDialogClosing = true;
    exportDialog.close();

    if (exportFailed || cancelRequested || framesWritten <= 0) {
        QFile::remove(outputGifPath);
    }

    if (exportFailed) {
        QMessageBox::warning(
            this,
            tr("Export Current Animation GIF"),
            exportError);
        return;
    }
    if (cancelRequested) {
        QMessageBox::information(
            this,
            tr("Export Current Animation GIF"),
            tr("Export canceled."));
        return;
    }
    if (framesWritten <= 0) {
        QMessageBox::warning(
            this,
            tr("Export Current Animation GIF"),
            tr("No frames were written for the active clip."));
        return;
    }

    lastDirectory = outputDir;
    QMessageBox::information(
        this,
        tr("Export Current Animation GIF"),
        tr("Exported `%1` to:\n%2")
            .arg(QString::fromStdString(clip.fullName))
            .arg(QDir::toNativeSeparators(outputGifPath)));
}

void MainWindow::resetActiveRenderAnimationDraft() {
    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    const auto& animations = currentRenderSceneResult.scene.animations;
    if (activeIndex < 0 || activeIndex >= static_cast<int>(animations.size())) {
        return;
    }

    const auto& clip = animations[static_cast<std::size_t>(activeIndex)];
    const OW3D::Render::RenderAnimationEditDraft* activeDraft =
        findRenderAnimationEditDraftForClip(clip);
    if (!HasPendingRenderAnimationDraftChanges(&clip, activeDraft)) {
        return;
    }

    const QString clipLabel = QString::fromStdString(clip.fullName);
    const auto choice = QMessageBox::question(
        this,
        tr("Reset Draft"),
        tr("Discard all pending draft edits for `%1` and restore the base clip?")
            .arg(clipLabel.isEmpty() ? tr("(unnamed)") : clipLabel),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (choice != QMessageBox::Yes) {
        return;
    }

    const auto beforeDrafts = currentRenderAnimationDrafts;
    currentRenderAnimationDrafts.erase(clip.sourceAnimationChunk);

    if (!applyingRenderTransformUndoRedo) {
        RenderEditUndoEntry entry{};
        entry.kind = RenderEditUndoEntry::Kind::AnimationDrafts;
        entry.beforeDrafts = beforeDrafts;
        entry.afterDrafts = currentRenderAnimationDrafts;
        renderTransformUndoStack.push_back(std::move(entry));
        renderTransformRedoStack.clear();
    }

    syncRenderAnimationUi();
}

void MainWindow::handleRenderAnimationLoopChanged(bool checked) {
    currentRenderAnimationPlayback.loop = checked;
    syncRenderAnimationUi();
}

void MainWindow::handleRenderAnimationSpeedChanged(double value) {
    currentRenderAnimationPlayback.speed = std::max(0.1f, static_cast<float>(value));
    if (currentRenderAnimationPlayback.playing) {
        renderAnimationPlaybackElapsed.restart();
    }
    syncRenderAnimationUi();
}

void MainWindow::handleRenderAnimationFrameSliderChanged(int value) {
    if (suppressRenderAnimationFrameSliderChange) {
        return;
    }

    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    const auto& animations = currentRenderSceneResult.scene.animations;
    if (activeIndex < 0 || activeIndex >= static_cast<int>(animations.size())) {
        return;
    }

    const auto& clip = animations[static_cast<std::size_t>(activeIndex)];
    const OW3D::Render::RenderAnimationEditDraft* activeDraft =
        findRenderAnimationEditDraftForClip(clip);
    const uint32_t frameCount = EffectiveRenderAnimationFrameCount(clip, activeDraft);
    const float frameRate = EffectiveRenderAnimationFrameRate(clip, activeDraft);
    if (frameRate <= 0.0f || frameCount == 0u) {
        currentRenderAnimationPlayback.timeSeconds = 0.0f;
    }
    else {
        currentRenderAnimationPlayback.timeSeconds =
            static_cast<float>(value) / frameRate;
    }
    if (currentRenderAnimationPlayback.playing) {
        renderAnimationPlaybackElapsed.restart();
        if (renderAnimationPlaybackTimer) {
            renderAnimationPlaybackTimer->start();
        }
    }
    else {
        if (renderAnimationPlaybackTimer) {
            renderAnimationPlaybackTimer->stop();
        }
        renderAnimationPlaybackElapsed.invalidate();
    }
    syncRenderAnimationUi();
}

void MainWindow::handleRenderAnimationPlaybackTimerTick() {
    const int activeIndex = currentRenderAnimationPlayback.activeAnimationIndex;
    const auto& animations = currentRenderSceneResult.scene.animations;
    if (!currentRenderAnimationPlayback.playing
        || activeIndex < 0
        || activeIndex >= static_cast<int>(animations.size()))
    {
        if (renderAnimationPlaybackTimer) {
            renderAnimationPlaybackTimer->stop();
        }
        refreshRenderPlaybackControls();
        return;
    }

    if (!renderAnimationPlaybackElapsed.isValid()) {
        renderAnimationPlaybackElapsed.restart();
        return;
    }

    const float deltaSeconds =
        static_cast<float>(renderAnimationPlaybackElapsed.restart()) / 1000.0f;
    if (deltaSeconds <= 0.0f) {
        return;
    }

    const auto& clip = animations[static_cast<std::size_t>(activeIndex)];
    const OW3D::Render::RenderAnimationEditDraft* activeDraft =
        findRenderAnimationEditDraftForClip(clip);
    const float duration = AnimationClipDurationSeconds(clip, activeDraft);
    currentRenderAnimationPlayback.timeSeconds +=
        deltaSeconds * currentRenderAnimationPlayback.speed;

    if (duration <= 0.0f) {
        currentRenderAnimationPlayback.timeSeconds = 0.0f;
        currentRenderAnimationPlayback.playing = false;
        if (renderAnimationPlaybackTimer) {
            renderAnimationPlaybackTimer->stop();
        }
    }
    else if (currentRenderAnimationPlayback.loop) {
        currentRenderAnimationPlayback.timeSeconds =
            std::fmod(currentRenderAnimationPlayback.timeSeconds, duration);
        if (currentRenderAnimationPlayback.timeSeconds < 0.0f) {
            currentRenderAnimationPlayback.timeSeconds += duration;
        }
    }
    else if (currentRenderAnimationPlayback.timeSeconds > duration) {
        currentRenderAnimationPlayback.timeSeconds = duration;
        currentRenderAnimationPlayback.playing = false;
        if (renderAnimationPlaybackTimer) {
            renderAnimationPlaybackTimer->stop();
        }
        renderAnimationPlaybackElapsed.invalidate();
    }

    syncRenderAnimationPlaybackToViewport();
    refreshRenderPlaybackControls();
}

void MainWindow::rebuildRenderScene() {
    if (!renderViewport) {
        return;
    }
    bool chunkSourceTabsDirty = false;

    OW3D::Render::SceneBuildOptions options{};
    options.profile = OW3D::Render::ParityProfile::W3DViewD3D11Baseline;
    options.externalTextureNames = currentArchiveTextureEntries;
    options.externalTextureHashes = currentArchiveTextureEntryIds;
    const QString primaryTextureDirectory = !currentFilePath.isEmpty()
        ? QFileInfo(currentFilePath).absolutePath()
        : QString();
    if (!primaryTextureDirectory.isEmpty()) {
        options.textureSearchDirectory = primaryTextureDirectory.toStdString();
    }
    else if (!currentRenderTextureDirectory.isEmpty()) {
        options.textureSearchDirectory = currentRenderTextureDirectory.toStdString();
    }
    else if (!lastDirectory.isEmpty()) {
        options.textureSearchDirectory = lastDirectory.toStdString();
    }

    QSet<QString> additionalTextureDirectoryKeys;
    auto addAdditionalTextureDirectory = [&](const QString& directoryPath) {
        const QString cleanPath = QDir::cleanPath(directoryPath);
        if (cleanPath.isEmpty()) {
            return;
        }

        const QString normalizedKey = NormalizeAbsolutePathKey(cleanPath);
        if (normalizedKey.isEmpty()) {
            return;
        }
        if (!options.textureSearchDirectory.empty()
            && normalizedKey.compare(
                NormalizeAbsolutePathKey(QString::fromStdString(options.textureSearchDirectory)),
                Qt::CaseInsensitive) == 0)
        {
            return;
        }
        if (additionalTextureDirectoryKeys.contains(normalizedKey)) {
            return;
        }

        additionalTextureDirectoryKeys.insert(normalizedKey);
        options.additionalTextureSearchDirectories.push_back(cleanPath.toStdString());
    };

    if (!currentRenderTextureDirectory.isEmpty()) {
        addAdditionalTextureDirectory(currentRenderTextureDirectory);
    }
    for (const auto& asset : currentExternalRenderAssets) {
        addAdditionalTextureDirectory(QFileInfo(asset.filePath).absolutePath());
    }

    const QString primarySourceLabel = BuildPrimaryRenderSourceLabel(
        currentFilePath,
        currentArchiveRenderPath,
        currentArchiveRenderEntryPath,
        chunkData.get());

    auto rebuildWithCurrentSession = [&]() -> OW3D::Render::SceneBuildResult {
        options.rootSourceLabels.clear();

        OW3D::Render::W3DChunk primaryRoots;
        if (chunkData) {
            const auto& roots = chunkData->getChunks();
            primaryRoots.assign(roots.begin(), roots.end());
        }
        for (const auto& root : primaryRoots) {
            if (root) {
                options.rootSourceLabels[root.get()] = primarySourceLabel.toStdString();
            }
        }

        if (!currentArchiveSupplementalRoots.empty()) {
            primaryRoots.insert(
                primaryRoots.end(),
                currentArchiveSupplementalRoots.begin(),
                currentArchiveSupplementalRoots.end());
            for (const auto& root : currentArchiveSupplementalRoots) {
                if (root) {
                    options.rootSourceLabels[root.get()] = primarySourceLabel.toStdString();
                }
            }
        }

        OW3D::Render::W3DChunk skeletonRoots;
        OW3D::Render::W3DChunk animationLibraryRoots;
        OW3D::Render::W3DChunk referenceOnlyRoots;
        for (const auto& asset : currentExternalRenderAssets) {
            auto& targetRoots = asset.role == RenderSessionAssetRole::Skeleton
                ? skeletonRoots
                : animationLibraryRoots;
            targetRoots.insert(targetRoots.end(), asset.roots.begin(), asset.roots.end());
            for (const auto& root : asset.roots) {
                if (root) {
                    options.rootSourceLabels[root.get()] = asset.displayLabel.toStdString();
                }
            }
        }
        for (const auto& asset : currentAggregateRenderDependencyAssets) {
            referenceOnlyRoots.insert(
                referenceOnlyRoots.end(),
                asset.roots.begin(),
                asset.roots.end());
            for (const auto& root : asset.roots) {
                if (root) {
                    options.rootSourceLabels[root.get()] = asset.sourceLabel.toStdString();
                }
            }
        }

        const OW3D::Render::W3DChunk* skeletonRootsPtr =
            skeletonRoots.empty() ? nullptr : &skeletonRoots;
        const OW3D::Render::W3DChunk* animationRootsPtr =
            animationLibraryRoots.empty() ? nullptr : &animationLibraryRoots;
        const OW3D::Render::W3DChunk* referenceRootsPtr =
            referenceOnlyRoots.empty() ? nullptr : &referenceOnlyRoots;
        return OW3D::Render::BuildRenderScene(
            primaryRoots,
            options,
            skeletonRootsPtr,
            animationRootsPtr,
            referenceRootsPtr);
    };

    OW3D::Render::SceneBuildResult result = rebuildWithCurrentSession();

    auto hasAggregateDependencySourceKey = [&](const QString& sourceKey) {
        return std::any_of(
            currentAggregateRenderDependencyAssets.begin(),
            currentAggregateRenderDependencyAssets.end(),
            [&](const AggregateRenderDependencyAsset& asset) {
                return asset.sourceKey.compare(sourceKey, Qt::CaseInsensitive) == 0;
            });
    };

    auto tryAutoloadReferenceDependencies = [&](const QStringList& missingReferences) {
        if (missingReferences.isEmpty()) {
            return false;
        }

        const QSet<QString> matchTokens = BuildAggregateDependencyMatchTokens(missingReferences);
        if (matchTokens.isEmpty()) {
            return false;
        }

        bool loadedAny = false;
        if (!currentArchiveRenderPath.isEmpty() && !currentArchiveRenderEntries.empty()) {
            const QString archiveKeyPrefix =
                NormalizeAbsolutePathKey(currentArchiveRenderPath) + QStringLiteral("::");

            for (const ArchiveRenderEntryInfo& entry : currentArchiveRenderEntries) {
                if (!entry.likelyW3d || entry.id == currentArchiveRenderEntryId) {
                    continue;
                }

                const QString sourceKey =
                    archiveKeyPrefix + QString::number(static_cast<qulonglong>(entry.id));
                if (hasAggregateDependencySourceKey(sourceKey)) {
                    continue;
                }

                const QString entryPath = entry.name.isEmpty()
                    ? QStringLiteral("entry_%1.w3d").arg(entry.id, 8, 16, QLatin1Char('0')).toUpper()
                    : QDir::fromNativeSeparators(entry.name).trimmed();

                QByteArray entryBytes;
                ChunkData dependencyData;
                bool dependencyLoaded = false;

                bool matchesDependency = MatchesAggregateDependencyPath(entryPath, matchTokens);
                if (!matchesDependency) {
                    const auto cachedIt =
                        currentArchiveRenderEntryReferenceNamesById.find(entry.id);
                    if (cachedIt != currentArchiveRenderEntryReferenceNamesById.end()) {
                        matchesDependency =
                            ReferenceNamesContainAnyMatchToken(cachedIt->second, matchTokens);
                    }
                    else {
                        QString readError;
                        if (!ReadArchiveEntryBytes(
                            currentArchiveRenderPath,
                            entry.offset,
                            entry.size,
                            entryBytes,
                            &readError))
                        {
                            continue;
                        }

                        QString parseError;
                        if (!LoadChunkDataFromBytes(entryBytes, dependencyData, parseError)) {
                            continue;
                        }

                        dependencyLoaded = true;
                        const auto& parsedRoots = dependencyData.getChunks();
                        currentArchiveRenderEntryReferenceNamesById[entry.id] =
                            CollectRenderReferenceNamesFromRoots(parsedRoots);
                        matchesDependency = ReferenceNamesContainAnyMatchToken(
                            currentArchiveRenderEntryReferenceNamesById[entry.id],
                            matchTokens);
                    }
                }

                if (!matchesDependency) {
                    continue;
                }

                if (!dependencyLoaded) {
                    QString readError;
                    if (!ReadArchiveEntryBytes(
                        currentArchiveRenderPath,
                        entry.offset,
                        entry.size,
                        entryBytes,
                        &readError))
                    {
                        continue;
                    }

                    QString parseError;
                    if (!LoadChunkDataFromBytes(entryBytes, dependencyData, parseError)) {
                        continue;
                    }
                }

                AggregateRenderDependencyAsset asset{};
                asset.sourceKey = sourceKey;
                asset.sourceLabel = QFileInfo(entryPath).fileName();
                if (asset.sourceLabel.isEmpty()) {
                    asset.sourceLabel = entryPath;
                }
                const auto& parsedRoots = dependencyData.getChunks();
                asset.roots.assign(parsedRoots.begin(), parsedRoots.end());
                if (asset.roots.empty()) {
                    continue;
                }
                currentArchiveRenderEntryReferenceNamesById[entry.id] =
                    CollectRenderReferenceNamesFromRoots(asset.roots);

                currentAggregateRenderDependencyAssets.push_back(std::move(asset));
                loadedAny = true;
            }

            return loadedAny;
        }

        if (currentFilePath.isEmpty()) {
            return false;
        }

        const QFileInfo currentInfo(currentFilePath);
        const QDir currentDir = currentInfo.absoluteDir();
        const QStringList candidates = currentDir.entryList(
            QStringList{ QStringLiteral("*.w3d"), QStringLiteral("*.W3D"), QStringLiteral("*.wlt"), QStringLiteral("*.WLT") },
            QDir::Files | QDir::Readable,
            QDir::Name | QDir::IgnoreCase);

        for (const QString& fileName : candidates) {
            const QString candidatePath =
                QDir::cleanPath(currentDir.absoluteFilePath(fileName));
            if (candidatePath.compare(
                QDir::cleanPath(currentInfo.absoluteFilePath()),
                Qt::CaseInsensitive) == 0)
            {
                continue;
            }

            const QString normalizedCandidatePath = NormalizeAbsolutePathKey(candidatePath);
            if (normalizedCandidatePath.isEmpty()
                || currentExternalRenderAssetPaths.contains(normalizedCandidatePath)
                || hasAggregateDependencySourceKey(normalizedCandidatePath))
            {
                continue;
            }
            if (!MatchesAggregateDependencyPath(candidatePath, matchTokens)) {
                continue;
            }

            std::vector<std::shared_ptr<ChunkItem>> loadedRoots;
            QString loadError;
            if (!LoadSupplementalRenderRootsFromFile(candidatePath, loadedRoots, &loadError)
                || loadedRoots.empty())
            {
                continue;
            }

            AggregateRenderDependencyAsset asset{};
            asset.sourceKey = normalizedCandidatePath;
            asset.sourceLabel = QFileInfo(candidatePath).fileName();
            asset.roots = std::move(loadedRoots);
            currentAggregateRenderDependencyAssets.push_back(std::move(asset));
            loadedAny = true;
        }

        return loadedAny;
    };

    {
        const QStringList missingAggregateReferences =
            CollectMissingAggregateRenderObjectNames(result);
        const QString aggregateAttemptKey =
            (!currentArchiveRenderPath.isEmpty()
                ? NormalizeAbsolutePathKey(currentArchiveRenderPath)
                : NormalizeAbsolutePathKey(currentFilePath))
            + QStringLiteral("|")
            + missingAggregateReferences.join(QLatin1Char('|')).toLower();

        if (missingAggregateReferences.isEmpty()) {
            currentRenderAggregateDependencyAttemptKey.clear();
        }
        else if (currentRenderAggregateDependencyAttemptKey != aggregateAttemptKey) {
            currentRenderAggregateDependencyAttemptKey = aggregateAttemptKey;
            if (tryAutoloadReferenceDependencies(missingAggregateReferences)) {
                result = rebuildWithCurrentSession();
                if (CollectMissingAggregateRenderObjectNames(result).isEmpty()) {
                    currentRenderAggregateDependencyAttemptKey.clear();
                }
            }
        }
    }

    if (!currentArchiveRenderPath.isEmpty() && !currentArchiveRenderEntries.empty()) {
        const QStringList missingHierarchyNames =
            CollectMissingRenderHierarchyNames(result);
        const QString hierarchyAttemptKey =
            NormalizeAbsolutePathKey(currentArchiveRenderPath)
            + QStringLiteral("|hier|")
            + missingHierarchyNames.join(QLatin1Char('|')).toLower();

        if (missingHierarchyNames.isEmpty()) {
            currentRenderHierarchyDependencyAttemptKey.clear();
        }
        else if (currentRenderHierarchyDependencyAttemptKey != hierarchyAttemptKey) {
            currentRenderHierarchyDependencyAttemptKey = hierarchyAttemptKey;
            if (tryAutoloadReferenceDependencies(missingHierarchyNames)) {
                result = rebuildWithCurrentSession();
                if (CollectMissingRenderHierarchyNames(result).isEmpty()) {
                    currentRenderHierarchyDependencyAttemptKey.clear();
                }
            }
        }
    }
    else {
        currentRenderHierarchyDependencyAttemptKey.clear();
    }

    if (!currentArchiveRenderPath.isEmpty() && !currentArchiveRenderEntries.empty()) {
        static const std::string kMissingSubObjectPrefix = "Referenced subobject mesh not found: ";
        static const std::string kMissingLodMeshPrefix = "Referenced LOD mesh not found: ";

        QSet<QString> missingMeshTokens;
        for (const auto& warning : result.warnings) {
            if (warning.code != OW3D::Render::SceneBuildWarningCode::InvalidIndex) {
                continue;
            }

            std::string missingName;
            if (warning.message.rfind(kMissingSubObjectPrefix, 0) == 0) {
                missingName = warning.message.substr(kMissingSubObjectPrefix.size());
            }
            else if (warning.message.rfind(kMissingLodMeshPrefix, 0) == 0) {
                missingName = warning.message.substr(kMissingLodMeshPrefix.size());
            }
            if (missingName.empty()) {
                continue;
            }

            const QString token = QString::fromStdString(missingName)
                .section(QLatin1Char('.'), 0, 0)
                .trimmed()
                .toLower();
            if (!token.isEmpty()) {
                missingMeshTokens.insert(token);
            }
        }

        bool loadedSupplementalEntries = false;
        int loadedCount = 0;
        for (const ArchiveRenderEntryInfo& entry : currentArchiveRenderEntries) {
            if (missingMeshTokens.isEmpty() || loadedCount >= 64) {
                break;
            }
            if (!entry.likelyW3d) {
                continue;
            }
            if (entry.id == currentArchiveRenderEntryId) {
                continue;
            }
            if (currentArchiveLoadedSupplementalEntryIds.contains(entry.id)) {
                continue;
            }

            const QString normalizedEntryName = QDir::fromNativeSeparators(entry.name).trimmed();
            if (normalizedEntryName.isEmpty()) {
                continue;
            }

            const QString entryPathLower = normalizedEntryName.toLower();
            const QString entryStemLower = QFileInfo(normalizedEntryName).completeBaseName().toLower();
            bool matchesMissingMesh = false;
            for (const QString& token : missingMeshTokens) {
                if (entryStemLower == token
                    || entryStemLower.contains(token)
                    || entryPathLower.contains(token))
                {
                    matchesMissingMesh = true;
                    break;
                }
            }
            if (!matchesMissingMesh) {
                continue;
            }

            QByteArray entryBytes;
            QString readError;
            if (!ReadArchiveEntryBytes(
                currentArchiveRenderPath,
                entry.offset,
                entry.size,
                entryBytes,
                &readError))
            {
                continue;
            }

            ChunkData supplementalData;
            QString parseError;
            if (!LoadChunkDataFromBytes(entryBytes, supplementalData, parseError)) {
                continue;
            }

            const auto& parsedRoots = supplementalData.getChunks();
            currentArchiveSupplementalRoots.insert(
                currentArchiveSupplementalRoots.end(),
                parsedRoots.begin(),
                parsedRoots.end());
            currentArchiveLoadedSupplementalEntryIds.insert(entry.id);
            loadedSupplementalEntries = true;
            ++loadedCount;
        }

        if (loadedSupplementalEntries) {
            result = rebuildWithCurrentSession();
        }
    }

    if (currentArchiveRenderPath.isEmpty() && !currentFilePath.isEmpty()) {
        QStringList missingHierarchyNames = CollectMissingRenderHierarchyNames(result);
        QString missingHierarchyKey = missingHierarchyNames.join(QLatin1Char('|'));

        if (!missingHierarchyNames.isEmpty() && !currentRenderTriedSkeletonAutoload) {
            currentRenderTriedSkeletonAutoload = true;

            const QFileInfo currentInfo(currentFilePath);
            const QDir currentDir = currentInfo.absoluteDir();
            const QStringList candidates = currentDir.entryList(
                QStringList{ QStringLiteral("*.w3d"), QStringLiteral("*.W3D"), QStringLiteral("*.wlt"), QStringLiteral("*.WLT") },
                QDir::Files | QDir::Readable,
                QDir::Name | QDir::IgnoreCase);

            QSet<QString> unresolved;
            for (const QString& name : missingHierarchyNames) {
                unresolved.insert(name.toLower());
            }

            bool loadedAnySkeleton = false;
            for (const QString& fileName : candidates) {
                if (unresolved.isEmpty()) {
                    break;
                }

                const QString candidatePath =
                    QDir::cleanPath(currentDir.absoluteFilePath(fileName));
                if (candidatePath.compare(
                    QDir::cleanPath(currentInfo.absoluteFilePath()),
                    Qt::CaseInsensitive) == 0)
                {
                    continue;
                }
                if (currentExternalRenderAssetPaths.contains(
                    NormalizeAbsolutePathKey(candidatePath)))
                {
                    continue;
                }

                RenderSessionAsset candidateAsset;
                QString loadError;
                if (!tryLoadRenderSessionAsset(
                    candidatePath,
                    RenderSessionAssetRole::Skeleton,
                    candidateAsset,
                    &loadError))
                {
                    continue;
                }

                bool matchesMissingHierarchy = false;
                for (const QString& missingName : unresolved) {
                    if (candidateAsset.hierarchyNames.contains(missingName)) {
                        matchesMissingHierarchy = true;
                        break;
                    }
                }
                if (!matchesMissingHierarchy) {
                    continue;
                }

                currentExternalRenderAssetPaths.insert(
                    NormalizeAbsolutePathKey(candidateAsset.filePath));
                currentExternalRenderAssets.push_back(std::move(candidateAsset));
                chunkSourceTabsDirty = true;
                loadedAnySkeleton = true;

                const auto& loadedAsset = currentExternalRenderAssets.back();
                for (const QString& hierarchyName : loadedAsset.hierarchyNames) {
                    unresolved.remove(hierarchyName.toLower());
                }
            }

            if (loadedAnySkeleton) {
                result = rebuildWithCurrentSession();
                missingHierarchyNames = CollectMissingRenderHierarchyNames(result);
                missingHierarchyKey = missingHierarchyNames.join(QLatin1Char('|'));
            }
        }

        if (missingHierarchyNames.isEmpty()) {
            currentRenderSuppressedMissingHierarchyKey.clear();
        }
        else if (currentRenderSuppressedMissingHierarchyKey != missingHierarchyKey) {
            const QString selectedSkeletonPath = QFileDialog::getOpenFileName(
                this,
                tr("Select Skeleton W3D/WLT"),
                QFileInfo(currentFilePath).absolutePath(),
                tr("W3D Files (*.w3d *.W3D *.wlt *.WLT);;All Files (*)"));

            if (selectedSkeletonPath.isEmpty()) {
                currentRenderSuppressedMissingHierarchyKey = missingHierarchyKey;
            }
            else {
                const QString normalizedSkeletonPath = NormalizeAbsolutePathKey(selectedSkeletonPath);
                if (!currentExternalRenderAssetPaths.contains(normalizedSkeletonPath)) {
                    RenderSessionAsset selectedAsset;
                    QString loadError;
                    if (!tryLoadRenderSessionAsset(
                        selectedSkeletonPath,
                        RenderSessionAssetRole::Skeleton,
                        selectedAsset,
                        &loadError))
                    {
                        QMessageBox::warning(
                            this,
                            tr("Skeleton Load Failed"),
                            loadError.isEmpty()
                                ? tr("Failed to load the selected skeleton file.")
                                : loadError);
                    }
                    else if (selectedAsset.hierarchyNames.isEmpty()) {
                        QMessageBox::warning(
                            this,
                            tr("Skeleton Load Failed"),
                            tr("The selected file does not contain any hierarchy data."));
                    }
                    else {
                        currentExternalRenderAssetPaths.insert(
                            NormalizeAbsolutePathKey(selectedAsset.filePath));
                        currentExternalRenderAssets.push_back(std::move(selectedAsset));
                        chunkSourceTabsDirty = true;
                        currentRenderSuppressedMissingHierarchyKey.clear();
                        result = rebuildWithCurrentSession();
                    }
                }
                else {
                    currentRenderSuppressedMissingHierarchyKey.clear();
                    result = rebuildWithCurrentSession();
                }
            }
        }

        const QString missingMeshKey = NormalizeAbsolutePathKey(currentFilePath);
        if (!missingHierarchyNames.isEmpty()) {
            currentRenderSuppressedMissingMeshKey.clear();
        }
        else if (SceneHasRenderableMeshData(result)) {
            currentRenderSuppressedMissingMeshKey.clear();
        }
        else if (currentRenderSuppressedMissingMeshKey != missingMeshKey) {
            const QString selectedMeshPath = QFileDialog::getOpenFileName(
                this,
                tr("Select Model/Mesh W3D/WLT"),
                QFileInfo(currentFilePath).absolutePath(),
                tr("W3D Files (*.w3d *.W3D *.wlt *.WLT);;All Files (*)"));

            if (selectedMeshPath.isEmpty()) {
                currentRenderSuppressedMissingMeshKey = missingMeshKey;
            }
            else {
                const QString normalizedMeshPath = NormalizeAbsolutePathKey(selectedMeshPath);
                if (!currentExternalRenderAssetPaths.contains(normalizedMeshPath)) {
                    RenderSessionAsset selectedAsset;
                    QString loadError;
                    if (!tryLoadRenderSessionAsset(
                        selectedMeshPath,
                        RenderSessionAssetRole::Skeleton,
                        selectedAsset,
                        &loadError))
                    {
                        QMessageBox::warning(
                            this,
                            tr("Model Load Failed"),
                            loadError.isEmpty()
                                ? tr("Failed to load the selected model file.")
                                : loadError);
                    }
                    else if (selectedAsset.meshCount <= 0) {
                        QMessageBox::warning(
                            this,
                            tr("Model Load Failed"),
                            tr("The selected file does not contain any mesh data."));
                    }
                    else {
                        currentExternalRenderAssetPaths.insert(
                            NormalizeAbsolutePathKey(selectedAsset.filePath));
                        currentExternalRenderAssets.push_back(std::move(selectedAsset));
                        chunkSourceTabsDirty = true;
                        currentRenderSuppressedMissingMeshKey.clear();
                        result = rebuildWithCurrentSession();
                    }
                }
                else {
                    currentRenderSuppressedMissingMeshKey.clear();
                    result = rebuildWithCurrentSession();
                }
            }
        }
    }

    auto resolveArchiveTextures = [&](OW3D::Render::SceneBuildResult& sceneResult) {
        if (currentArchiveTextureSourcesById.empty()) {
            return;
        }
        QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (cacheRoot.isEmpty()) {
            cacheRoot = QDir::tempPath();
        }
        cacheRoot = QDir(cacheRoot).absoluteFilePath(QStringLiteral("archive_textures"));

        std::unordered_map<uint32_t, std::string> cachedPathById;
        for (auto& texture : sceneResult.scene.textures) {
            const bool usesArchivePlaceholder =
                texture.resolvedPath.rfind("archive:", 0) == 0;
            if (!usesArchivePlaceholder) {
                continue;
            }

            const std::vector<uint32_t> hashCandidates =
                BuildTextureHashCandidates(texture.name);
            bool extracted = false;
            for (const uint32_t candidateHash : hashCandidates) {
                const auto sourceIt = currentArchiveTextureSourcesById.find(candidateHash);
                if (sourceIt == currentArchiveTextureSourcesById.end()) {
                    continue;
                }

                const auto cachedIt = cachedPathById.find(candidateHash);
                if (cachedIt != cachedPathById.end()) {
                    texture.resolvedPath = cachedIt->second;
                    texture.resolved = true;
                    extracted = true;
                    break;
                }

                QString extractedPath;
                if (!MaterializeArchiveTextureToCache(sourceIt->second, cacheRoot, extractedPath)) {
                    continue;
                }

                texture.resolvedPath = extractedPath.toStdString();
                texture.resolved = true;
                cachedPathById.emplace(candidateHash, texture.resolvedPath);
                extracted = true;
                break;
            }

            if (!extracted) {
                texture.resolved = false;
                sceneResult.warnings.push_back({
                    OW3D::Render::SceneBuildWarningCode::MissingTexture,
                    "archive",
                    "Texture hash was found in archive index but payload could not be extracted: " + texture.name
                    });
            }
        }
    };

    resolveArchiveTextures(result);

    QStringList missingTextureNames = CollectMissingRenderTextureNames(result);
    const QString missingTextureKey = missingTextureNames.join(QLatin1Char('|'));
    if (missingTextureNames.isEmpty()) {
        currentRenderSuppressedMissingTextureKey.clear();
    }
    else if (currentRenderTextureDirectory.isEmpty()
        && currentRenderSuppressedMissingTextureKey != missingTextureKey)
    {
        const QString startDir = !currentFilePath.isEmpty()
            ? QFileInfo(currentFilePath).absolutePath()
            : (lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory);
        const QString selectedDir = QFileDialog::getExistingDirectory(
            this,
            tr("Select Texture Folder"),
            startDir,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (selectedDir.isEmpty()) {
            currentRenderSuppressedMissingTextureKey = missingTextureKey;
        }
        else {
            currentRenderTextureDirectory = QDir::cleanPath(selectedDir);
            currentRenderSuppressedMissingTextureKey.clear();
            lastDirectory = currentRenderTextureDirectory;
            result = rebuildWithCurrentSession();
            resolveArchiveTextures(result);
            missingTextureNames = CollectMissingRenderTextureNames(result);
            if (!missingTextureNames.isEmpty()) {
                currentRenderSuppressedMissingTextureKey =
                    missingTextureNames.join(QLatin1Char('|'));
            }
        }
    }

    currentRenderSceneResult = result;
    if (chunkSourceTabsDirty) {
        populateTree();
    }
    auto clearActiveRenderAnimation = [&]() {
        currentRenderAnimationPlayback.activeAnimationIndex = -1;
        currentRenderAnimationPlayback.timeSeconds = 0.0f;
        currentRenderAnimationPlayback.playing = false;
        currentRenderActiveClipIdentity.reset();
        if (renderAnimationPlaybackTimer) {
            renderAnimationPlaybackTimer->stop();
        }
        renderAnimationPlaybackElapsed.invalidate();
    };
    auto activateRenderAnimationAtRest = [&](int animationIndex) {
        if (animationIndex < 0
            || animationIndex >= static_cast<int>(currentRenderSceneResult.scene.animations.size()))
        {
            clearActiveRenderAnimation();
            return;
        }

        currentRenderAnimationPlayback.activeAnimationIndex = animationIndex;
        currentRenderAnimationPlayback.timeSeconds = 0.0f;
        currentRenderAnimationPlayback.playing = false;
        currentRenderActiveClipIdentity = BuildRenderAnimationClipIdentity(
            currentRenderSceneResult.scene.animations[static_cast<std::size_t>(animationIndex)]);
        if (renderAnimationPlaybackTimer) {
            renderAnimationPlaybackTimer->stop();
        }
        renderAnimationPlaybackElapsed.invalidate();
    };

    if (currentRenderActiveClipIdentity) {
        const int remappedIndex = findRenderAnimationIndexByIdentity(*currentRenderActiveClipIdentity);
        if (remappedIndex >= 0
            && currentRenderSceneResult.scene.animations[static_cast<std::size_t>(remappedIndex)].supportedForPlayback
            && SceneHasCompatibleHierarchyForAnimation(currentRenderSceneResult.scene, remappedIndex))
        {
            currentRenderAnimationPlayback.activeAnimationIndex = remappedIndex;
            const auto& clip =
                currentRenderSceneResult.scene.animations[static_cast<std::size_t>(remappedIndex)];
            const OW3D::Render::RenderAnimationEditDraft* activeDraft =
                findRenderAnimationEditDraftForClip(clip);
            const float duration = AnimationClipDurationSeconds(clip, activeDraft);
            if (duration <= 0.0f) {
                currentRenderAnimationPlayback.timeSeconds = 0.0f;
                currentRenderAnimationPlayback.playing = false;
            }
            else if (currentRenderAnimationPlayback.loop) {
                currentRenderAnimationPlayback.timeSeconds = std::fmod(
                    std::max(0.0f, currentRenderAnimationPlayback.timeSeconds),
                    duration);
            }
            else {
                currentRenderAnimationPlayback.timeSeconds = std::clamp(
                    currentRenderAnimationPlayback.timeSeconds,
                    0.0f,
                    duration);
            }

            if (currentRenderAnimationPlayback.playing) {
                renderAnimationPlaybackElapsed.restart();
                if (renderAnimationPlaybackTimer) {
                    renderAnimationPlaybackTimer->start();
                }
            }
            else {
                if (renderAnimationPlaybackTimer) {
                    renderAnimationPlaybackTimer->stop();
                }
                renderAnimationPlaybackElapsed.invalidate();
            }
        }
        else {
            const int defaultAnimationIndex =
                FindFirstPlayableAnimationIndex(currentRenderSceneResult.scene);
            if (defaultAnimationIndex >= 0) {
                activateRenderAnimationAtRest(defaultAnimationIndex);
            }
            else {
                clearActiveRenderAnimation();
            }
        }
    }
    else {
        const int defaultAnimationIndex =
            FindFirstPlayableAnimationIndex(currentRenderSceneResult.scene);
        if (defaultAnimationIndex >= 0) {
            activateRenderAnimationAtRest(defaultAnimationIndex);
        }
        else {
            clearActiveRenderAnimation();
        }
    }

    renderViewport->SetSceneResult(currentRenderSceneResult);
    syncRenderAnimationUi();
    applyRenderSettingsToViewport();
}

void MainWindow::setDirty(bool value) {
    if (dirty == value) return;
    dirty = value;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
    QString title = tr("oW3DEdit");
    if (!currentFilePath.isEmpty()) {
        if (!currentArchiveRenderPath.isEmpty() && !currentArchiveRenderEntryPath.isEmpty()) {
            title += QStringLiteral(" - ")
                + QFileInfo(currentArchiveRenderPath).fileName()
                + QStringLiteral("::")
                + QDir::toNativeSeparators(currentArchiveRenderEntryPath);
        }
        else {
            title += QStringLiteral(" - ") + QFileInfo(currentFilePath).fileName();
        }
    }
    if (dirty) {
        title += QLatin1Char('*');
    }
    setWindowTitle(title);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    QSettings settings;
    settings.setValue("MainWindow/geometry", saveGeometry());
    if (splitter) {
        settings.setValue("MainWindow/splitter", splitter->saveState());
    }
    if (detailSplitter) {
        QByteArray state;
        if (editorScrollArea && editorScrollArea->isVisible()) {
            state = detailSplitter->saveState();
            detailSplitterStateCache = state;
        }
        else {
            state = detailSplitterStateCache.isEmpty()
                ? detailSplitter->saveState()
                : detailSplitterStateCache;
        }
        settings.setValue("MainWindow/detailSplitter", state);
    }
    if (renderSplitter) {
        settings.setValue("MainWindow/renderSplitter", renderSplitter->saveState());
    }
    QMainWindow::closeEvent(event);
}

bool MainWindow::confirmDiscardChanges() {
    if (!dirty) return true;

    const auto reply = QMessageBox::question(
        this,
        tr("Unsaved Changes"),
        tr("Discard unsaved changes?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    return reply == QMessageBox::Yes;
}

void MainWindow::clearDetails() {
    tableWidget->clearContents();
    tableWidget->setRowCount(0);
    updateEditorForChunk(nullptr);
    updateRawHex(nullptr);
}


void MainWindow::LoadRecentFiles() {
    QFile file(recentFilesPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                recentFiles.append(line);
            }
        }
        file.close();
    }
    if (!recentFiles.isEmpty())
        lastDirectory = QFileInfo(recentFiles.first()).absolutePath();
}



void MainWindow::SaveRecentFiles() {
    QDir().mkpath(QFileInfo(recentFilesPath).path());
    QFile file(recentFilesPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        for (const QString& path : recentFiles) {
            out << path << '\n';
        }
        file.close();
    }
}

void MainWindow::UpdateRecentFilesMenu() {
    recentFilesMenu->clear();

    for (int i = 0; i < recentFiles.size(); ++i) {
        const QString& path = recentFiles[i];
        QAction* action = new QAction(QFileInfo(path).fileName(), this);
        action->setData(path);
        connect(action, &QAction::triggered, this, &MainWindow::OpenRecentFile);
        recentFilesMenu->addAction(action);
    }

    if (recentFiles.isEmpty()) {
        QAction* none = new QAction("(No Recent Files)", this);
        none->setDisabled(true);
        recentFilesMenu->addAction(none);
    }
}

void MainWindow::AddRecentFile(const QString& path) {
    recentFiles.removeAll(path);
    recentFiles.prepend(path);
    while (recentFiles.size() > 10)
        recentFiles.removeLast();
    SaveRecentFiles();
    UpdateRecentFilesMenu();
}

void MainWindow::selectChunkInTree(void* chunkPtr) {
    if (!chunkPtr || chunkSourceTabs.empty()) return;

    for (int tabIndex = 0; tabIndex < static_cast<int>(chunkSourceTabs.size()); ++tabIndex) {
        QTreeWidget* candidateTree = chunkSourceTabs[static_cast<std::size_t>(tabIndex)].treeWidget;
        if (!candidateTree) {
            continue;
        }

        QTreeWidgetItem* found = nullptr;
        std::function<bool(QTreeWidgetItem*)> dfs =
            [&](QTreeWidgetItem* item) -> bool {
            if (!item) return false;
            if (item->data(0, Qt::UserRole).value<void*>() == chunkPtr) {
                found = item;
                return true;
            }
            for (int i = 0; i < item->childCount(); ++i) {
                if (dfs(item->child(i))) {
                    item->setExpanded(true);
                    return true;
                }
            }
            return false;
            };

        for (int i = 0; i < candidateTree->topLevelItemCount() && !found; ++i) {
            (void)dfs(candidateTree->topLevelItem(i));
        }

        if (found) {
            if (chunkTreeTabs) {
                chunkTreeTabs->setCurrentIndex(tabIndex);
            }
            syncActiveChunkSourceTree();
            candidateTree->setCurrentItem(found);
            candidateTree->scrollToItem(found, QAbstractItemView::PositionAtCenter);
            return;
        }
    }
}

void MainWindow::showHierarchyBrowser() {
    if (!chunkData || chunkData->getChunks().empty()) {
        QMessageBox::information(this, tr("No File Loaded"),
            tr("Load a W3D file to inspect hierarchy data."));
        return;
    }

    const auto meshIndex = BuildMeshIndex(chunkData->getChunks());
    const auto hmodels = CollectHModels(chunkData->getChunks());
    const auto hlodBindings = CollectHlodBindings(chunkData->getChunks(), meshIndex);
    auto hierarchies = CollectHierarchies(chunkData->getChunks(), hmodels, meshIndex, hlodBindings);

    if (hierarchies.empty()) {
        QMessageBox::information(this, tr("No Hierarchy Found"),
            tr("This file does not contain hierarchy/pivot data."));
        return;
    }

    HierarchyBrowserDialog dlg(
        hierarchies,
        [this](void* ptr) { selectChunkInTree(ptr); },
        [meshIndex](const QStringList& names) -> void* {
            for (const QString& name : names) {
                const std::string key = NormalizeName(name.toStdString());
                auto range = meshIndex.equal_range(key);
                if (range.first != range.second) {
                    return range.first->second.get();
                }
            }
            return nullptr;
        },
        [this](const std::shared_ptr<ChunkItem>& pivotChunk,
            int pivotIndex,
            const QString& newName,
            QString* error) -> bool {
            if (!pivotChunk) {
                if (error) {
                    *error = tr("Hierarchy pivot chunk was not found.");
                }
                return false;
            }

            if (pivotIndex < 0) {
                if (error) {
                    *error = tr("Pivot index is invalid.");
                }
                return false;
            }

            auto parsedPivots = ParseChunkArray<W3dPivotStruct>(pivotChunk);
            if (auto parseError = std::get_if<std::string>(&parsedPivots)) {
                if (error) {
                    *error = QString::fromStdString(*parseError);
                }
                return false;
            }
            const auto& pivots = std::get<std::vector<W3dPivotStruct>>(parsedPivots);
            if (pivotIndex >= static_cast<int>(pivots.size())) {
                if (error) {
                    *error = tr("Pivot index is out of range.");
                }
                return false;
            }
            const QString oldName = ReadFixedString(
                pivots[static_cast<std::size_t>(pivotIndex)].Name,
                W3D_NAME_LEN);

            std::string mutateError;
            const bool mutated = W3DEdit::MutateStructAtIndex<W3dPivotStruct>(
                pivotChunk,
                static_cast<std::size_t>(pivotIndex),
                [&](W3dPivotStruct& pivot) {
                    W3DEdit::WriteFixedString(
                        pivot.Name,
                        W3D_NAME_LEN,
                        newName.toStdString());
                },
                &mutateError);

            if (!mutated) {
                if (error) {
                    *error = QString::fromStdString(mutateError);
                }
                return false;
            }

            const QString hierarchyName = FindHierarchyNameForPivotChunk(pivotChunk);
            (void)RenamePivotBoundMeshNamesForHierarchy(
                chunkData->getChunks(),
                hierarchyName,
                oldName,
                newName);
            if (!hierarchyName.isEmpty()) {
                onMeshRenamed(
                    oldName,
                    newName,
                    hierarchyName,
                    hierarchyName);
            }
            (void)RenameHLodProxyNamesForHierarchy(
                chunkData->getChunks(),
                hierarchyName,
                pivotIndex,
                oldName,
                newName);

            onChunkEdited();
            return true;
        },
        this);
    dlg.exec();
}

void MainWindow::addTopLevelChunk() {
    if (!chunkData) return;

    uint32_t chunkId = 0;
    if (!PromptChunkId(this, tr("Add Top-Level Chunk"), 0x0100u, chunkId)) {
        return;
    }

    auto newChunk = std::make_shared<ChunkItem>();
    newChunk->id = chunkId;
    newChunk->hasSubChunks = false;
    newChunk->length = 0;
    newChunk->parent = nullptr;

    auto& roots = chunkData->getChunksMutable();
    roots.push_back(newChunk);

    SyncHLodCountsForSave(chunkData.get());
    setDirty(true);
    populateTree();
    selectChunkInTree(newChunk.get());
    rebuildRenderScene();
}

void MainWindow::insertChunkBefore() {
    if (!chunkData) return;

    void* selectedPtr = SelectedChunkPtr(treeWidget);
    if (!selectedPtr) {
        QMessageBox::information(this, tr("No Chunk Selected"),
            tr("Select a chunk to insert before."));
        return;
    }

    auto& roots = chunkData->getChunksMutable();
    ChunkLocation location;
    if (!FindChunkLocation(roots, selectedPtr, location) || !location.siblings) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to locate the selected chunk."));
        return;
    }

    uint32_t chunkId = 0;
    const uint32_t suggestedId = (*location.siblings)[location.index]
        ? (*location.siblings)[location.index]->id
        : 0x0000u;
    if (!PromptChunkId(this, tr("Insert Chunk Before"), suggestedId, chunkId)) {
        return;
    }

    auto newChunk = std::make_shared<ChunkItem>();
    newChunk->id = chunkId;
    newChunk->hasSubChunks = false;
    newChunk->length = 0;
    newChunk->parent = location.parent;

    location.siblings->insert(location.siblings->begin() + static_cast<std::ptrdiff_t>(location.index), newChunk);

    SyncHLodCountsForSave(chunkData.get());
    setDirty(true);
    populateTree();
    selectChunkInTree(newChunk.get());
    rebuildRenderScene();
}

void MainWindow::insertChunkAfter() {
    if (!chunkData) return;

    void* selectedPtr = SelectedChunkPtr(treeWidget);
    if (!selectedPtr) {
        QMessageBox::information(this, tr("No Chunk Selected"),
            tr("Select a chunk to insert after."));
        return;
    }

    auto& roots = chunkData->getChunksMutable();
    ChunkLocation location;
    if (!FindChunkLocation(roots, selectedPtr, location) || !location.siblings) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to locate the selected chunk."));
        return;
    }

    uint32_t chunkId = 0;
    const uint32_t suggestedId = (*location.siblings)[location.index]
        ? (*location.siblings)[location.index]->id
        : 0x0000u;
    if (!PromptChunkId(this, tr("Insert Chunk After"), suggestedId, chunkId)) {
        return;
    }

    auto newChunk = std::make_shared<ChunkItem>();
    newChunk->id = chunkId;
    newChunk->hasSubChunks = false;
    newChunk->length = 0;
    newChunk->parent = location.parent;

    const std::size_t insertIndex = location.index + 1;
    location.siblings->insert(location.siblings->begin() + static_cast<std::ptrdiff_t>(insertIndex), newChunk);

    SyncHLodCountsForSave(chunkData.get());
    setDirty(true);
    populateTree();
    selectChunkInTree(newChunk.get());
    rebuildRenderScene();
}

void MainWindow::addChildChunk() {
    if (!chunkData) return;

    void* selectedPtr = SelectedChunkPtr(treeWidget);
    if (!selectedPtr) {
        QMessageBox::information(this, tr("No Chunk Selected"),
            tr("Select a parent chunk to add a child chunk."));
        return;
    }

    auto parentChunk = FindChunkByPtr(chunkData->getChunks(), selectedPtr);
    if (!parentChunk) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to locate the selected chunk."));
        return;
    }

    if (parentChunk->isMicro) {
        QMessageBox::warning(this, tr("Unsupported"),
            tr("Cannot add child chunks under micro chunks."));
        return;
    }

    if (!parentChunk->data.empty() && parentChunk->children.empty()) {
        const auto choice = QMessageBox::question(
            this,
            tr("Convert To Wrapper"),
            tr("The selected chunk currently contains raw data. "
                "Adding children will replace that raw data when the file is saved. Continue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (choice != QMessageBox::Yes) return;
    }

    uint32_t chunkId = 0;
    if (!PromptChunkId(this, tr("Add Child Chunk"), 0x0001u, chunkId)) {
        return;
    }

    auto newChunk = std::make_shared<ChunkItem>();
    newChunk->id = chunkId;
    newChunk->hasSubChunks = false;
    newChunk->length = 0;
    newChunk->parent = parentChunk.get();

    parentChunk->children.push_back(newChunk);
    parentChunk->hasSubChunks = true;

    SyncHLodCountsForSave(chunkData.get());
    setDirty(true);
    populateTree();
    selectChunkInTree(newChunk.get());
    rebuildRenderScene();
}

void MainWindow::deleteSelectedChunk() {
    if (!chunkData) return;

    void* selectedPtr = SelectedChunkPtr(treeWidget);
    if (!selectedPtr) {
        QMessageBox::information(this, tr("No Chunk Selected"),
            tr("Select a chunk to delete."));
        return;
    }

    auto& roots = chunkData->getChunksMutable();
    ChunkLocation location;
    if (!FindChunkLocation(roots, selectedPtr, location) || !location.siblings) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to locate the selected chunk."));
        return;
    }

    auto& siblings = *location.siblings;
    if (location.index >= siblings.size() || !siblings[location.index]) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid selected chunk index."));
        return;
    }

    const auto& chunk = siblings[location.index];
    const QString chunkLabel = QStringLiteral("0x%1 (%2)")
        .arg(chunk->id, 0, 16)
        .arg(QString::fromStdString(LabelForChunk(chunk->id, chunk.get())));

    const auto choice = QMessageBox::question(
        this,
        tr("Delete Chunk"),
        tr("Delete %1 and all of its child chunks?").arg(chunkLabel),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    std::unordered_set<std::string> deletedMeshNames;
    CollectDeletedMeshNames(chunk, deletedMeshNames);

    void* nextSelection = nullptr;
    if (location.index + 1 < siblings.size()) {
        nextSelection = siblings[location.index + 1].get();
    }
    else if (location.index > 0) {
        nextSelection = siblings[location.index - 1].get();
    }
    else if (location.parent) {
        nextSelection = location.parent;
    }

    siblings.erase(siblings.begin() + static_cast<std::ptrdiff_t>(location.index));
    (void)RemoveMatchingHLodSubObjects(roots, deletedMeshNames);

    SyncHLodCountsForSave(chunkData.get());
    setDirty(true);
    populateTree();
    if (nextSelection) {
        selectChunkInTree(nextSelection);
    }
    else {
        clearDetails();
    }
    rebuildRenderScene();
}

void MainWindow::moveChunkUp() {
    if (!chunkData) return;

    void* selectedPtr = SelectedChunkPtr(treeWidget);
    if (!selectedPtr) {
        QMessageBox::information(this, tr("No Chunk Selected"),
            tr("Select a chunk to move."));
        return;
    }

    auto& roots = chunkData->getChunksMutable();
    ChunkLocation location;
    if (!FindChunkLocation(roots, selectedPtr, location) || !location.siblings) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to locate the selected chunk."));
        return;
    }

    if (location.index == 0) {
        return;
    }

    auto& siblings = *location.siblings;
    std::swap(siblings[location.index], siblings[location.index - 1]);

    SyncHLodCountsForSave(chunkData.get());
    setDirty(true);
    populateTree();
    selectChunkInTree(selectedPtr);
    rebuildRenderScene();
}

void MainWindow::moveChunkDown() {
    if (!chunkData) return;

    void* selectedPtr = SelectedChunkPtr(treeWidget);
    if (!selectedPtr) {
        QMessageBox::information(this, tr("No Chunk Selected"),
            tr("Select a chunk to move."));
        return;
    }

    auto& roots = chunkData->getChunksMutable();
    ChunkLocation location;
    if (!FindChunkLocation(roots, selectedPtr, location) || !location.siblings) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to locate the selected chunk."));
        return;
    }

    auto& siblings = *location.siblings;
    if (location.index + 1 >= siblings.size()) {
        return;
    }

    std::swap(siblings[location.index], siblings[location.index + 1]);

    SyncHLodCountsForSave(chunkData.get());
    setDirty(true);
    populateTree();
    selectChunkInTree(selectedPtr);
    rebuildRenderScene();
}

void MainWindow::moveHierarchyBoneToEnd() {
    if (!chunkData || chunkData->getChunks().empty()) {
        QMessageBox::information(this, tr("No File Loaded"),
            tr("Load a W3D file first."));
        return;
    }

    void* selectedPtr = SelectedChunkPtr(treeWidget);
    if (!selectedPtr) {
        QMessageBox::information(this, tr("No Chunk Selected"),
            tr("Select any chunk inside a hierarchy (0x0100) and try again."));
        return;
    }

    auto current = FindChunkByPtr(chunkData->getChunks(), selectedPtr);
    if (!current) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to locate the selected chunk."));
        return;
    }

    std::shared_ptr<ChunkItem> hierarchyChunk = current;
    while (hierarchyChunk && hierarchyChunk->id != 0x0100) {
        if (!hierarchyChunk->parent) {
            hierarchyChunk.reset();
            break;
        }
        hierarchyChunk = FindChunkByPtr(chunkData->getChunks(), hierarchyChunk->parent);
    }

    if (!hierarchyChunk) {
        QMessageBox::information(this, tr("No Hierarchy Context"),
            tr("The selected chunk is not inside a hierarchy block (0x0100)."));
        return;
    }

    std::shared_ptr<ChunkItem> pivotChunk;
    std::shared_ptr<ChunkItem> pivotFixupChunk;
    for (const auto& child : hierarchyChunk->children) {
        if (!child) continue;
        if (child->id == 0x0102 && !pivotChunk) {
            pivotChunk = child;
        }
        else if (child->id == 0x0103 && !pivotFixupChunk) {
            pivotFixupChunk = child;
        }
    }

    if (!pivotChunk) {
        QMessageBox::warning(this, tr("Missing Pivots"),
            tr("This hierarchy does not contain a pivots chunk (0x0102)."));
        return;
    }

    auto parsedPivots = ParseChunkArray<W3dPivotStruct>(pivotChunk);
    if (auto err = std::get_if<std::string>(&parsedPivots)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Failed to parse pivots chunk: %1").arg(QString::fromStdString(*err)));
        return;
    }

    auto pivots = std::get<std::vector<W3dPivotStruct>>(parsedPivots);
    if (pivots.empty()) {
        QMessageBox::information(this, tr("No Pivots"),
            tr("This hierarchy has no pivots to reorder."));
        return;
    }

    std::optional<std::vector<W3dPivotFixupStruct>> pivotFixups;
    if (pivotFixupChunk) {
        auto parsedFixups = ParseChunkArray<W3dPivotFixupStruct>(pivotFixupChunk);
        if (auto err = std::get_if<std::string>(&parsedFixups)) {
            QMessageBox::warning(this, tr("Error"),
                tr("Failed to parse pivot fixups chunk: %1").arg(QString::fromStdString(*err)));
            return;
        }
        pivotFixups = std::get<std::vector<W3dPivotFixupStruct>>(parsedFixups);
        if (pivotFixups->size() != pivots.size()) {
            QMessageBox::warning(this, tr("Pivot Size Mismatch"),
                tr("Pivot count (%1) and pivot-fixup count (%2) do not match.")
                .arg(pivots.size())
                .arg(pivotFixups->size()));
            return;
        }
    }

    bool accepted = false;
    const int pivotIndex = QInputDialog::getInt(
        this,
        tr("Move Bone To End"),
        tr("Pivot index to move"),
        0,
        0,
        static_cast<int>(pivots.size()) - 1,
        1,
        &accepted);
    if (!accepted) return;

    QString error;
    if (!MoveBoneToEndInPivots(
        pivots,
        pivotFixups ? &*pivotFixups : nullptr,
        pivotIndex,
        &error)) {
        QMessageBox::warning(this, tr("Failed"),
            error.isEmpty() ? tr("Failed to reorder pivots.") : error);
        return;
    }

    const auto* pivotBytes = reinterpret_cast<const uint8_t*>(pivots.data());
    const std::size_t pivotByteCount = pivots.size() * sizeof(W3dPivotStruct);
    pivotChunk->data.assign(pivotBytes, pivotBytes + pivotByteCount);
    pivotChunk->length = static_cast<uint32_t>(pivotChunk->data.size());

    if (pivotFixups && pivotFixupChunk) {
        const auto* fixupBytes = reinterpret_cast<const uint8_t*>(pivotFixups->data());
        const std::size_t fixupByteCount = pivotFixups->size() * sizeof(W3dPivotFixupStruct);
        pivotFixupChunk->data.assign(fixupBytes, fixupBytes + fixupByteCount);
        pivotFixupChunk->length = static_cast<uint32_t>(pivotFixupChunk->data.size());
    }

    onChunkEdited();
    QMessageBox::information(this, tr("Complete"),
        tr("Moved pivot %1 (and descendants) to the end of the hierarchy order.")
        .arg(pivotIndex));
}

void MainWindow::ClearChunkTree() {
    if (chunkTreeTabs) {
        chunkTreeTabs->clear();
    }
    chunkSourceTabs.clear();
    treeWidget = nullptr;
    syncActiveChunkSourceTree();
    clearDetails();
    rebuildRenderScene();
}

void MainWindow::OpenRecentFile() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (action) {
        QString path = action->data().toString(); // Use the stored path
        if (!path.isEmpty()) {
            openFile(path);
        }
    }
}



//extern void recursePrint(const std::shared_ptr<ChunkItem>& c, int depth, std::ostream& out);



static void recursePrint(const std::shared_ptr<ChunkItem>& c,
    int depth,
    QTextStream& out,
    std::map<uint32_t, int>& counts)
{
    // 1) Skip raw micro chunks under the channel wrapper
    if (c->id == MICRO_ID && c->parent && c->parent->id == CHANNEL_WRAPPER)
        return;

    // 2) Skip children of the SOUND_RENDER_DEF node only when it sits under
    //    a SOUNDROBJ definition or extended definition
    if (c->parent && c->parent->id == SOUND_RENDER_DEF &&
        c->parent->parent &&
        (c->parent->parent->id == SOUNDROBJ_DEFINITION ||
            c->parent->parent->id == SOUND_RENDER_DEF_EXT))
        return;

    // 3) Count this chunk
    ++counts[c->id];

    // 4) Print "0x######## NAME"
    out
        << QString(depth * 2, ' ')
        << QString("0x%1 ").arg(c->id, 8, 16, QChar('0')).toUpper()
        << QString::fromStdString(LabelForChunk(c->id, c.get()))
        << "\n";

    // Recurse
    for (auto& ch : c->children) {
        recursePrint(ch, depth + 1, out, counts);
    }
}

namespace {

constexpr const char* kJsonDefaultModeSettingKey = "Json/DefaultSerializationMode";
constexpr const char* kJsonValidatorRunModeSettingKey = "Json/ValidatorRunMode";

QString SerializationModeToken(JsonSerializationMode mode) {
    return mode == JsonSerializationMode::HexOnly
        ? QStringLiteral("HEX_ONLY")
        : QStringLiteral("STRUCTURED_PREFERRED");
}

QString SerializationModeUiLabel(JsonSerializationMode mode) {
    if (mode == JsonSerializationMode::HexOnly) {
        return QObject::tr("Hex Only (RAW_DATA_HEX for all leaf chunks)");
    }
    return QObject::tr("Structured Preferred (DATA when supported, RAW_DATA_HEX fallback)");
}

bool TryParseSerializationModeToken(const QString& token, JsonSerializationMode& outMode) {
    const QString normalized = token.trimmed().toUpper();
    if (normalized == QStringLiteral("HEX_ONLY")) {
        outMode = JsonSerializationMode::HexOnly;
        return true;
    }
    if (normalized == QStringLiteral("STRUCTURED_PREFERRED")) {
        outMode = JsonSerializationMode::StructuredPreferred;
        return true;
    }
    return false;
}

struct RoundTripFallbackMetrics {
    int nodeCount = 0;
    std::map<uint32_t, int> chunkCounts;
};

struct RoundTripReportRow {
    QString status = QStringLiteral("FAIL");
    QString mode;
    QString stage = QStringLiteral("LOAD_W3D");
    QString sourcePath;
    QString relativePath;
    qint64 originalSize = -1;
    qint64 rebuiltSize = -1;
    qint64 firstDiffOffset = -1;
    int originalByte = -1;
    int rebuiltByte = -1;
    int fallbackNodeCount = 0;
    QString fallbackChunkIds;
    QString errorMessage;
    QString jsonArtifactPath;
    QString rebuiltArtifactPath;
    qint64 durationMs = 0;
    int warningCount = 0;
    QString warnings;
};

struct PureAnimationCopyReportRow {
    QString status = QStringLiteral("SKIP");
    QString sourcePath;
    QString relativePath;
    QString detectedHierarchies;
    QString matchedSkeletons;
    int copiesWritten = 0;
    QString copyTargets;
    QString errorMessage;
};

static QString CsvEscape(const QString& value) {
    QString out = value;
    out.replace('"', "\"\"");
    const bool needsQuotes = out.contains(',') || out.contains('"') || out.contains('\n') || out.contains('\r');
    if (needsQuotes) {
        out.prepend('"');
        out.append('"');
    }
    return out;
}

static void WriteCsvLine(QTextStream& out, const QStringList& columns) {
    for (int i = 0; i < columns.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        out << CsvEscape(columns[i]);
    }
    out << '\n';
}

static QString NumberOrBlank(qint64 value) {
    return value < 0 ? QString() : QString::number(value);
}

static QString ByteOrBlank(int value) {
    if (value < 0 || value > 0xFF) {
        return QString();
    }
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

static void WriteRoundTripCsvHeader(QTextStream& out) {
    out
        << "status,mode,stage,source_path,relative_path,original_size,rebuilt_size,"
        << "first_diff_offset,original_byte_hex,rebuilt_byte_hex,fallback_node_count,"
        << "fallback_chunk_ids,error_message,json_artifact_path,rebuilt_artifact_path,duration_ms,"
        << "warning_count,warnings\n";
}

static void WriteRoundTripCsvRow(QTextStream& out, const RoundTripReportRow& row) {
    const QStringList columns = {
        row.status,
        row.mode,
        row.stage,
        row.sourcePath,
        row.relativePath,
        NumberOrBlank(row.originalSize),
        NumberOrBlank(row.rebuiltSize),
        NumberOrBlank(row.firstDiffOffset),
        ByteOrBlank(row.originalByte),
        ByteOrBlank(row.rebuiltByte),
        QString::number(row.fallbackNodeCount),
        row.fallbackChunkIds,
        row.errorMessage,
        row.jsonArtifactPath,
        row.rebuiltArtifactPath,
        NumberOrBlank(row.durationMs),
        QString::number(row.warningCount),
        row.warnings
    };

    for (int i = 0; i < columns.size(); ++i) {
        if (i > 0) out << ',';
        out << CsvEscape(columns[i]);
    }
    out << '\n';
}

static void WritePureAnimationCopyCsvHeader(QTextStream& out) {
    out
        << "status,source_path,relative_path,detected_hierarchies,matched_skeletons,"
        << "copies_written,copy_targets,error_message\n";
}

static void WritePureAnimationCopyCsvRow(QTextStream& out, const PureAnimationCopyReportRow& row) {
    const QStringList columns = {
        row.status,
        row.sourcePath,
        row.relativePath,
        row.detectedHierarchies,
        row.matchedSkeletons,
        QString::number(row.copiesWritten),
        row.copyTargets,
        row.errorMessage
    };

    for (int i = 0; i < columns.size(); ++i) {
        if (i > 0) out << ',';
        out << CsvEscape(columns[i]);
    }
    out << '\n';
}

static bool EnsureParentDirectory(const QString& filePath) {
    QFileInfo info(filePath);
    return QDir().mkpath(info.path());
}

static bool ReadAllBytes(const QString& path, QByteArray& outBytes, QString& errorMessage) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = QStringLiteral("Failed to open file for reading: %1").arg(path);
        return false;
    }
    outBytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        errorMessage = QStringLiteral("Failed to read file bytes: %1").arg(path);
        return false;
    }
    return true;
}

static bool WriteAllBytes(const QString& path, const QByteArray& bytes, QString& errorMessage) {
    if (!EnsureParentDirectory(path)) {
        errorMessage = QStringLiteral("Failed to create output directory for: %1").arg(path);
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorMessage = QStringLiteral("Failed to open file for writing: %1").arg(path);
        return false;
    }
    const qint64 written = file.write(bytes);
    if (written != bytes.size()) {
        errorMessage = QStringLiteral("Failed to write full file: %1").arg(path);
        return false;
    }
    return true;
}

static bool TryReadChunkId(const ordered_json& value, uint32_t& outId) {
    if (value.is_number_unsigned()) {
        const uint64_t raw = value.get<uint64_t>();
        if (raw <= std::numeric_limits<uint32_t>::max()) {
            outId = static_cast<uint32_t>(raw);
            return true;
        }
        return false;
    }
    if (value.is_number_integer()) {
        const int64_t raw = value.get<int64_t>();
        if (raw >= 0 && static_cast<uint64_t>(raw) <= std::numeric_limits<uint32_t>::max()) {
            outId = static_cast<uint32_t>(raw);
            return true;
        }
    }
    return false;
}

static void CollectFallbackMetrics(const ordered_json& node, RoundTripFallbackMetrics& metrics) {
    if (node.is_object()) {
        const auto rawIt = node.find("RAW_DATA_HEX");
        if (rawIt != node.end()) {
            ++metrics.nodeCount;
            const auto idIt = node.find("CHUNK_ID");
            if (idIt != node.end()) {
                uint32_t chunkId = 0;
                if (TryReadChunkId(*idIt, chunkId)) {
                    ++metrics.chunkCounts[chunkId];
                }
            }
        }

        for (auto it = node.begin(); it != node.end(); ++it) {
            CollectFallbackMetrics(it.value(), metrics);
        }
        return;
    }

    if (node.is_array()) {
        for (const auto& child : node) {
            CollectFallbackMetrics(child, metrics);
        }
    }
}

static QString FormatFallbackChunkCounts(const std::map<uint32_t, int>& chunkCounts) {
    QStringList entries;
    for (const auto& [chunkId, count] : chunkCounts) {
        const QString chunkText = QStringLiteral("0x%1").arg(chunkId, 4, 16, QLatin1Char('0')).toUpper();
        entries << QStringLiteral("%1:%2").arg(chunkText).arg(count);
    }
    return entries.join(';');
}

static QString SanitizeRelativePath(QString relativePath) {
    relativePath = QDir::fromNativeSeparators(relativePath);
    relativePath = QDir::cleanPath(relativePath);
    while (relativePath.startsWith("../")) {
        relativePath.remove(0, 3);
    }
    if (relativePath == "." || relativePath.isEmpty()) {
        return QStringLiteral("unnamed.w3d");
    }
    return relativePath;
}

static QString BuildFailureJsonRelativePath(const QString& relativePath) {
    return relativePath + QStringLiteral(".json");
}

static QString BuildFailureRebuiltRelativePath(const QString& relativePath) {
    QFileInfo relInfo(relativePath);
    const QString dir = (relInfo.path() == ".") ? QString() : relInfo.path();
    const QString base = relInfo.completeBaseName();
    const QString suffix = relInfo.completeSuffix();
    const QString rebuiltName = suffix.isEmpty()
        ? QStringLiteral("%1.rebuilt").arg(base)
        : QStringLiteral("%1.rebuilt.%2").arg(base, suffix);
    return dir.isEmpty() ? rebuiltName : QDir::cleanPath(dir + "/" + rebuiltName);
}

static bool CompareBytes(
    const QByteArray& originalBytes,
    const QByteArray& rebuiltBytes,
    qint64& outFirstOffset,
    int& outOriginalByte,
    int& outRebuiltByte)
{
    const qint64 minSize = std::min(originalBytes.size(), rebuiltBytes.size());
    for (qint64 i = 0; i < minSize; ++i) {
        const int o = static_cast<unsigned char>(originalBytes.at(i));
        const int r = static_cast<unsigned char>(rebuiltBytes.at(i));
        if (o != r) {
            outFirstOffset = i;
            outOriginalByte = o;
            outRebuiltByte = r;
            return false;
        }
    }

    if (originalBytes.size() != rebuiltBytes.size()) {
        outFirstOffset = minSize;
        outOriginalByte = (minSize < originalBytes.size())
            ? static_cast<unsigned char>(originalBytes.at(minSize))
            : -1;
        outRebuiltByte = (minSize < rebuiltBytes.size())
            ? static_cast<unsigned char>(rebuiltBytes.at(minSize))
            : -1;
        return false;
    }

    outFirstOffset = -1;
    outOriginalByte = -1;
    outRebuiltByte = -1;
    return true;
}

struct BatchInputSource {
    bool fromArchive = false;
    QString sourcePath;
    QString relativePath;
    QString standalonePath;
    QString archivePath;
    QString archiveEntryPath;
    uint32_t archiveEntryId = 0;
    uint32_t archiveEntryOffset = 0;
    uint32_t archiveEntrySize = 0;
};

struct SkeletonAllowlist {
    std::map<QString, QString> folderNameByNormalized;
};

static QString SanitizePathComponent(QString component) {
    component = component.trimmed();
    for (qsizetype i = 0; i < component.size(); ++i) {
        const QChar ch = component.at(i);
        if (ch == QLatin1Char('<')
            || ch == QLatin1Char('>')
            || ch == QLatin1Char(':')
            || ch == QLatin1Char('"')
            || ch == QLatin1Char('|')
            || ch == QLatin1Char('?')
            || ch == QLatin1Char('*'))
        {
            component[i] = QLatin1Char('_');
        }
    }
    return component;
}

static QString NormalizeArchiveEntryPath(QString entryName, uint32_t entryId) {
    entryName = QDir::fromNativeSeparators(entryName).trimmed();
    if (entryName.isEmpty()) {
        return QStringLiteral("entry_%1.w3d").arg(entryId, 8, 16, QLatin1Char('0')).toUpper();
    }

    const QStringList rawParts = QDir::cleanPath(entryName).split('/', Qt::SkipEmptyParts);
    QStringList cleanParts;
    cleanParts.reserve(rawParts.size());
    for (QString part : rawParts) {
        if (part == QStringLiteral(".") || part == QStringLiteral("..")) {
            continue;
        }
        part = SanitizePathComponent(part);
        if (!part.isEmpty()) {
            cleanParts << part;
        }
    }

    if (cleanParts.isEmpty()) {
        return QStringLiteral("entry_%1.w3d").arg(entryId, 8, 16, QLatin1Char('0')).toUpper();
    }
    return cleanParts.join('/');
}

static QString BuildArchiveEntryRelativePath(
    const QString& archiveRelativePath,
    const QString& entryPath)
{
    return SanitizeRelativePath(
        QDir::cleanPath(archiveRelativePath + QStringLiteral("/_entries/") + entryPath));
}

static QString BuildBatchSourceDisplayPath(const BatchInputSource& input) {
    if (!input.fromArchive) {
        return QDir::toNativeSeparators(QFileInfo(input.standalonePath).absoluteFilePath());
    }
    return QDir::toNativeSeparators(QFileInfo(input.archivePath).absoluteFilePath())
        + QStringLiteral("::")
        + QDir::toNativeSeparators(input.archiveEntryPath);
}

static QString BuildBatchJsonRelativePath(const QString& relativePath) {
    QFileInfo relInfo(relativePath);
    const QString dir = (relInfo.path() == ".") ? QString() : relInfo.path();

    QString base = relInfo.completeBaseName();
    if (base.isEmpty()) {
        base = relInfo.fileName();
    }
    if (base.isEmpty()) {
        base = QStringLiteral("unnamed");
    }

    const QString jsonName = base + QStringLiteral(".json");
    return dir.isEmpty() ? jsonName : QDir::cleanPath(dir + QStringLiteral("/") + jsonName);
}

static bool ReadBatchInputOriginalBytes(
    const BatchInputSource& input,
    QByteArray& outBytes,
    QString& outError,
    QString& cachedArchivePath,
    QByteArray& cachedArchiveBytes)
{
    if (!input.fromArchive) {
        return ReadAllBytes(input.standalonePath, outBytes, outError);
    }

    const QString archiveAbsPath = QDir::cleanPath(QFileInfo(input.archivePath).absoluteFilePath());
    if (cachedArchivePath.compare(archiveAbsPath, Qt::CaseInsensitive) != 0) {
        if (!ReadAllBytes(archiveAbsPath, cachedArchiveBytes, outError)) {
            return false;
        }
        cachedArchivePath = archiveAbsPath;
    }

    const qint64 offset = static_cast<qint64>(input.archiveEntryOffset);
    const qint64 size = static_cast<qint64>(input.archiveEntrySize);
    const qint64 archiveSize = static_cast<qint64>(cachedArchiveBytes.size());
    if (offset < 0 || size < 0 || offset > archiveSize || size > (archiveSize - offset)) {
        outError = QObject::tr("Archive entry has an invalid offset/size: %1")
            .arg(BuildBatchSourceDisplayPath(input));
        return false;
    }

    outBytes = cachedArchiveBytes.mid(static_cast<qsizetype>(offset), static_cast<qsizetype>(size));
    return true;
}

static bool LoadChunkDataFromBytes(
    const QByteArray& bytes,
    ChunkData& outChunkData,
    QString& outError)
{
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(true);
    if (!tempFile.open()) {
        outError = QObject::tr("Failed to create temporary file: %1").arg(tempFile.errorString());
        return false;
    }

    if (tempFile.write(bytes) != bytes.size()) {
        outError = QObject::tr("Failed to write temporary file: %1").arg(tempFile.errorString());
        return false;
    }
    if (!tempFile.flush()) {
        outError = QObject::tr("Failed to flush temporary file: %1").arg(tempFile.errorString());
        return false;
    }

    const QString tempPath = tempFile.fileName();
    tempFile.close();

    if (!outChunkData.loadFromFile(tempPath.toStdString()) || outChunkData.getChunks().empty()) {
        outError = QObject::tr("Failed to parse source data as W3D/WLT.");
        return false;
    }

    return true;
}

static bool LoadBatchInputChunkData(
    const BatchInputSource& input,
    const QByteArray& originalBytes,
    ChunkData& outChunkData,
    QString& outError)
{
    if (!input.fromArchive) {
        if (!outChunkData.loadFromFile(input.standalonePath.toStdString())
            || outChunkData.getChunks().empty())
        {
            outError = QObject::tr("Failed to load source W3D/WLT.");
            return false;
        }
        return true;
    }

    return LoadChunkDataFromBytes(originalBytes, outChunkData, outError);
}

static bool LoadSkeletonAllowlist(
    const QString& allowlistPath,
    SkeletonAllowlist& outAllowlist,
    QString& outError)
{
    outAllowlist = {};

    QFile file(allowlistPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        outError = QObject::tr("Failed to open allowlist file: %1").arg(file.errorString());
        return false;
    }

    QTextStream stream(&file);
    int lineNumber = 0;
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        ++lineNumber;
        if (lineNumber == 1 && !line.isEmpty() && line.at(0) == QChar(0xFEFF)) {
            line.remove(0, 1);
        }

        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QString normalized =
            QString::fromStdString(NormalizeName(line.toStdString())).trimmed();
        if (normalized.isEmpty()) {
            continue;
        }

        if (outAllowlist.folderNameByNormalized.find(normalized)
            == outAllowlist.folderNameByNormalized.end())
        {
            QString folderName = SanitizePathComponent(normalized);
            if (folderName.isEmpty()) {
                folderName = QStringLiteral("unnamed_skeleton");
            }
            outAllowlist.folderNameByNormalized.emplace(normalized, folderName);
        }
    }

    if (outAllowlist.folderNameByNormalized.empty()) {
        outError = QObject::tr("The allowlist file does not contain any hierarchy names.");
        return false;
    }

    return true;
}

static bool CollectPureAnimationHierarchyNames(
    const std::vector<std::shared_ptr<ChunkItem>>& roots,
    QStringList& outHierarchyNames,
    QString& outError)
{
    outHierarchyNames.clear();

    std::vector<std::shared_ptr<ChunkItem>> headerChunks;
    for (const auto& root : roots) {
        CollectChunksByIdRecursive(root, 0x0201, headerChunks);
        CollectChunksByIdRecursive(root, 0x0281, headerChunks);
        CollectChunksByIdRecursive(root, 0x02C1, headerChunks);
    }

    QSet<QString> names;
    auto addName = [&](const QString& rawName) {
        const QString normalized =
            QString::fromStdString(NormalizeName(rawName.trimmed().toStdString())).trimmed();
        if (!normalized.isEmpty()) {
            names.insert(normalized);
        }
    };

    for (const auto& headerChunk : headerChunks) {
        if (!headerChunk) {
            continue;
        }

        switch (headerChunk->id) {
        case 0x0201: {
            auto parsed = ParseChunkStruct<W3dAnimHeaderStruct>(headerChunk);
            if (const auto* err = std::get_if<std::string>(&parsed)) {
                outError = QObject::tr("Failed to parse raw animation header: %1")
                    .arg(QString::fromStdString(*err));
                return false;
            }
            addName(ReadFixedString(std::get<W3dAnimHeaderStruct>(parsed).HierarchyName, W3D_NAME_LEN));
            break;
        }
        case 0x0281: {
            auto parsed = ParseChunkStruct<W3dCompressedAnimHeaderStruct>(headerChunk);
            if (const auto* err = std::get_if<std::string>(&parsed)) {
                outError = QObject::tr("Failed to parse compressed animation header: %1")
                    .arg(QString::fromStdString(*err));
                return false;
            }
            addName(
                ReadFixedString(
                    std::get<W3dCompressedAnimHeaderStruct>(parsed).HierarchyName,
                    W3D_NAME_LEN));
            break;
        }
        case 0x02C1: {
            auto parsed = ParseChunkStruct<W3dMorphAnimHeaderStruct>(headerChunk);
            if (const auto* err = std::get_if<std::string>(&parsed)) {
                outError = QObject::tr("Failed to parse morph animation header: %1")
                    .arg(QString::fromStdString(*err));
                return false;
            }
            addName(
                ReadFixedString(
                    std::get<W3dMorphAnimHeaderStruct>(parsed).HierarchyName,
                    W3D_NAME_LEN));
            break;
        }
        default:
            break;
        }
    }

    outHierarchyNames = names.values();
    outHierarchyNames.sort(Qt::CaseInsensitive);
    return true;
}

static QStringList MatchAllowlistHierarchyNames(
    const QStringList& detectedHierarchyNames,
    const SkeletonAllowlist& allowlist)
{
    QStringList matched;
    for (const QString& hierarchyName : detectedHierarchyNames) {
        if (allowlist.folderNameByNormalized.find(hierarchyName)
            != allowlist.folderNameByNormalized.end())
        {
            matched << hierarchyName;
        }
    }
    matched.removeDuplicates();
    matched.sort(Qt::CaseInsensitive);
    return matched;
}

static void DiscoverBatchInputs(
    const QString& sourceDirectory,
    std::vector<BatchInputSource>& outInputs,
    QStringList* outWarnings = nullptr)
{
    outInputs.clear();

    QDir sourceRoot(sourceDirectory);

    QDirIterator fileIt(
        sourceDirectory,
        QStringList{ "*.w3d", "*.W3D", "*.wlt", "*.WLT" },
        QDir::Files | QDir::NoSymLinks,
        QDirIterator::Subdirectories);
    while (fileIt.hasNext()) {
        const QString absolutePath = QDir::cleanPath(fileIt.next());
        BatchInputSource input;
        input.fromArchive = false;
        input.standalonePath = absolutePath;
        input.relativePath = SanitizeRelativePath(sourceRoot.relativeFilePath(absolutePath));
        input.sourcePath = BuildBatchSourceDisplayPath(input);
        outInputs.push_back(std::move(input));
    }

    QDirIterator archiveIt(
        sourceDirectory,
        QStringList{ "*.mix", "*.MIX", "*.dat", "*.DAT" },
        QDir::Files | QDir::NoSymLinks,
        QDirIterator::Subdirectories);
    while (archiveIt.hasNext()) {
        const QString archivePath = QDir::cleanPath(archiveIt.next());
        QByteArray archiveBytes;
        QString readError;
        if (!ReadAllBytes(archivePath, archiveBytes, readError)) {
            if (outWarnings) {
                outWarnings->append(
                    QObject::tr("%1: %2")
                        .arg(QDir::toNativeSeparators(archivePath), readError));
            }
            continue;
        }

        MixArchiveInfo archiveInfo;
        QString parseError;
        const bool allowClassicFallback = IsMixArchivePath(archivePath);
        if (!ParseMixArchive(archiveBytes, allowClassicFallback, archiveInfo, &parseError)) {
            if (outWarnings) {
                outWarnings->append(
                    QObject::tr("%1: %2")
                        .arg(QDir::toNativeSeparators(archivePath), parseError));
            }
            continue;
        }

        const QString archiveRelativePath = SanitizeRelativePath(sourceRoot.relativeFilePath(archivePath));
        for (const MixEntryInfo& entry : archiveInfo.entries) {
            const bool likelyByName = entry.name.endsWith(QStringLiteral(".w3d"), Qt::CaseInsensitive)
                || entry.name.endsWith(QStringLiteral(".wlt"), Qt::CaseInsensitive);
            const bool likelyByContent = LooksLikeW3DStream(
                archiveBytes,
                static_cast<qsizetype>(entry.offset),
                entry.size);
            if (!likelyByName && !likelyByContent) {
                continue;
            }

            BatchInputSource input;
            input.fromArchive = true;
            input.archivePath = archivePath;
            input.archiveEntryPath = NormalizeArchiveEntryPath(entry.name, entry.id);
            input.archiveEntryId = entry.id;
            input.archiveEntryOffset = entry.offset;
            input.archiveEntrySize = entry.size;
            input.relativePath = BuildArchiveEntryRelativePath(archiveRelativePath, input.archiveEntryPath);
            input.sourcePath = BuildBatchSourceDisplayPath(input);
            outInputs.push_back(std::move(input));
        }
    }

    std::sort(outInputs.begin(), outInputs.end(), [](const BatchInputSource& lhs, const BatchInputSource& rhs) {
        const int relCompare = lhs.relativePath.compare(rhs.relativePath, Qt::CaseInsensitive);
        if (relCompare != 0) {
            return relCompare < 0;
        }
        return lhs.sourcePath.compare(rhs.sourcePath, Qt::CaseInsensitive) < 0;
        });

    std::map<QString, int> seenRelativePaths;
    for (BatchInputSource& input : outInputs) {
        const QString key = input.relativePath.toLower();
        int& seenCount = seenRelativePaths[key];
        if (seenCount > 0) {
            QFileInfo relInfo(input.relativePath);
            const QString dir = (relInfo.path() == ".") ? QString() : relInfo.path();

            QString base = relInfo.completeBaseName();
            if (base.isEmpty()) {
                base = relInfo.fileName();
            }
            if (base.isEmpty()) {
                base = QStringLiteral("unnamed");
            }

            const QString suffix = relInfo.completeSuffix();
            const QString dedupName = suffix.isEmpty()
                ? QStringLiteral("%1__dup%2").arg(base).arg(seenCount + 1)
                : QStringLiteral("%1__dup%2.%3").arg(base).arg(seenCount + 1).arg(suffix);
            input.relativePath = dir.isEmpty()
                ? dedupName
                : QDir::cleanPath(dir + QStringLiteral("/") + dedupName);
        }
        ++seenCount;
    }
}

struct AnimationGifFileMetadata {
    bool parsed = false;
    QString clipName;
    QString hierarchyName;
    QString skeletonCode;
    int numFrames = 0;
    double frameRate = 0.0;
    bool compressed = false;
    bool supportedForPlayback = true;
    QString errorMessage;
};

struct AnimationGifSource {
    QString clipStem;
    QString resolvedW3dPath;
    QString resolvedJsonPath;
    AnimationGifFileMetadata metadata;
};

struct AnimationGifDiscoverySummary {
    int scannedW3dCount = 0;
    int parsedMetadataCount = 0;
    int unsupportedCount = 0;
    int unreadableCount = 0;
    int unclassifiedCount = 0;
    QSet<QString> discoveredSkeletonCodes;
};

struct AnimationGifReportRow {
    QString status;
    QString selectedSkeletonCode;
    QString detectedSkeletonCode;
    QString clipStem;
    QString clipName;
    QString resolvedW3dPath;
    QString resolvedJsonPath;
    QString hierarchyName;
    QString numFrames;
    QString frameRate;
    QString outputGifPath;
    QString errorMessage;
};

static QString FindOptionalAnimationGifJsonSidecar(const QString& w3dPath)
{
    const QFileInfo info(w3dPath);
    const QString stem = info.completeBaseName();
    if (stem.isEmpty()) {
        return QString();
    }

    const QDir dir(info.absolutePath());
    const QString lowerCasePath = QDir::cleanPath(
        dir.absoluteFilePath(stem + QStringLiteral(".json")));
    if (QFileInfo::exists(lowerCasePath)) {
        return lowerCasePath;
    }

    const QString upperCasePath = QDir::cleanPath(
        dir.absoluteFilePath(stem + QStringLiteral(".JSON")));
    if (QFileInfo::exists(upperCasePath)) {
        return upperCasePath;
    }

    return QString();
}

static QString ExtractAnimationGifSkeletonCode(const QString& hierarchyName)
{
    const QString normalized = hierarchyName.trimmed().toUpper();
    if (!normalized.startsWith(QStringLiteral("S_"))) {
        return QString();
    }

    const int nextUnderscore = normalized.indexOf(QLatin1Char('_'), 2);
    if (nextUnderscore <= 2) {
        return QString();
    }

    return normalized.mid(2, nextUnderscore - 2).trimmed();
}

static bool TryExtractAnimationGifMetadataFromRoots(
    const std::vector<std::shared_ptr<ChunkItem>>& roots,
    AnimationGifFileMetadata& outMetadata,
    QString& outError)
{
    outMetadata = {};
    outError.clear();

    std::vector<std::shared_ptr<ChunkItem>> rawAnimationChunks;
    std::vector<std::shared_ptr<ChunkItem>> compressedAnimationChunks;
    for (const auto& root : roots) {
        CollectChunksByIdRecursive(root, kChunkAnimation, rawAnimationChunks);
        CollectChunksByIdRecursive(root, kChunkCompressedAnimation, compressedAnimationChunks);
    }

    const int animationChunkCount =
        static_cast<int>(rawAnimationChunks.size() + compressedAnimationChunks.size());
    if (animationChunkCount <= 0) {
        outError = QObject::tr("The W3D file does not contain raw or compressed animation chunks.");
        return false;
    }
    if (animationChunkCount > 1) {
        outError =
            QObject::tr("The W3D file contains multiple animation chunks and cannot be exported as a single clip.");
        return false;
    }

    if (!rawAnimationChunks.empty()) {
        const auto headerChunk = FindFirstChildById(rawAnimationChunks.front(), kChunkAnimationHeader);
        if (!headerChunk) {
            outError = QObject::tr("Animation header chunk is missing.");
            return false;
        }

        auto parsed = ParseChunkStruct<W3dAnimHeaderStruct>(headerChunk);
        if (const auto* header = std::get_if<W3dAnimHeaderStruct>(&parsed)) {
            outMetadata.clipName = ReadFixedString(header->Name, W3D_NAME_LEN).trimmed();
            outMetadata.hierarchyName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN).trimmed();
            outMetadata.numFrames = static_cast<int>(header->NumFrames);
            outMetadata.frameRate = static_cast<double>(header->FrameRate);
            outMetadata.compressed = false;
            outMetadata.supportedForPlayback = true;
            outMetadata.skeletonCode =
                ExtractAnimationGifSkeletonCode(outMetadata.hierarchyName);
            outMetadata.parsed = true;
            return true;
        }

        outError = QObject::tr("Failed to parse raw animation header.");
        return false;
    }

    const auto headerChunk = FindFirstChildById(
        compressedAnimationChunks.front(),
        kChunkCompressedAnimationHeader);
    if (!headerChunk) {
        outError = QObject::tr("Compressed animation header chunk is missing.");
        return false;
    }

    auto parsed = ParseChunkStruct<W3dCompressedAnimHeaderStruct>(headerChunk);
    if (const auto* header = std::get_if<W3dCompressedAnimHeaderStruct>(&parsed)) {
        outMetadata.clipName = ReadFixedString(header->Name, W3D_NAME_LEN).trimmed();
        outMetadata.hierarchyName = ReadFixedString(header->HierarchyName, W3D_NAME_LEN).trimmed();
        outMetadata.numFrames = static_cast<int>(header->NumFrames);
        outMetadata.frameRate = static_cast<double>(header->FrameRate);
        outMetadata.compressed = true;
        outMetadata.supportedForPlayback = header->Flavor != 1u;
        outMetadata.skeletonCode =
            ExtractAnimationGifSkeletonCode(outMetadata.hierarchyName);
        outMetadata.parsed = true;
        if (!outMetadata.supportedForPlayback) {
            outMetadata.errorMessage =
                QObject::tr("Adaptive-delta compressed animation is not supported for rendering yet.");
        }
        return true;
    }

    outError = QObject::tr("Failed to parse compressed animation header.");
    return false;
}

static bool DiscoverAnimationGifSources(
    const QString& animationRoot,
    std::vector<AnimationGifSource>& outSources,
    AnimationGifDiscoverySummary& outSummary,
    std::vector<AnimationGifReportRow>& outSkippedRows,
    QString& outError)
{
    outSources.clear();
    outSummary = {};
    outSkippedRows.clear();
    outError.clear();

    QDirIterator fileIt(
        animationRoot,
        QStringList{ QStringLiteral("*.w3d"), QStringLiteral("*.W3D") },
        QDir::Files | QDir::NoSymLinks,
        QDirIterator::Subdirectories);

    while (fileIt.hasNext()) {
        const QString absolutePath = QDir::cleanPath(fileIt.next());
        ++outSummary.scannedW3dCount;

        AnimationGifSource source;
        source.clipStem = QFileInfo(absolutePath).completeBaseName().trimmed();
        source.resolvedW3dPath = absolutePath;
        source.resolvedJsonPath = FindOptionalAnimationGifJsonSidecar(absolutePath);

        std::vector<std::shared_ptr<ChunkItem>> loadedRoots;
        QString loadError;
        if (!LoadSupplementalRenderRootsFromFile(absolutePath, loadedRoots, &loadError)) {
            ++outSummary.unreadableCount;

            AnimationGifReportRow skippedRow;
            skippedRow.status = QStringLiteral("SKIP");
            skippedRow.clipStem = source.clipStem;
            skippedRow.resolvedW3dPath = QDir::toNativeSeparators(source.resolvedW3dPath);
            skippedRow.resolvedJsonPath = source.resolvedJsonPath.isEmpty()
                ? QString()
                : QDir::toNativeSeparators(source.resolvedJsonPath);
            skippedRow.errorMessage = loadError.isEmpty()
                ? QObject::tr("Failed to load the animation W3D.")
                : loadError;
            outSkippedRows.push_back(std::move(skippedRow));
            continue;
        }

        QString parseError;
        if (!TryExtractAnimationGifMetadataFromRoots(loadedRoots, source.metadata, parseError)) {
            ++outSummary.unreadableCount;

            AnimationGifReportRow skippedRow;
            skippedRow.status = QStringLiteral("SKIP");
            skippedRow.clipStem = source.clipStem;
            skippedRow.resolvedW3dPath = QDir::toNativeSeparators(source.resolvedW3dPath);
            skippedRow.resolvedJsonPath = source.resolvedJsonPath.isEmpty()
                ? QString()
                : QDir::toNativeSeparators(source.resolvedJsonPath);
            skippedRow.errorMessage = parseError;
            outSkippedRows.push_back(std::move(skippedRow));
            continue;
        }

        ++outSummary.parsedMetadataCount;

        if (source.metadata.skeletonCode.isEmpty()) {
            ++outSummary.unclassifiedCount;

            AnimationGifReportRow skippedRow;
            skippedRow.status = QStringLiteral("SKIP");
            skippedRow.clipStem = source.clipStem;
            skippedRow.clipName = source.metadata.clipName;
            skippedRow.resolvedW3dPath = QDir::toNativeSeparators(source.resolvedW3dPath);
            skippedRow.resolvedJsonPath = source.resolvedJsonPath.isEmpty()
                ? QString()
                : QDir::toNativeSeparators(source.resolvedJsonPath);
            skippedRow.hierarchyName = source.metadata.hierarchyName;
            if (source.metadata.numFrames > 0) {
                skippedRow.numFrames = QString::number(source.metadata.numFrames);
            }
            if (source.metadata.frameRate > 0.0) {
                skippedRow.frameRate =
                    QString::number(source.metadata.frameRate, 'f', 2);
            }
            skippedRow.errorMessage =
                QObject::tr("Could not derive a skeleton code from hierarchy `%1`.")
                    .arg(source.metadata.hierarchyName);
            outSkippedRows.push_back(std::move(skippedRow));
            continue;
        }

        outSummary.discoveredSkeletonCodes.insert(source.metadata.skeletonCode);
        if (!source.metadata.supportedForPlayback) {
            ++outSummary.unsupportedCount;
        }

        outSources.push_back(std::move(source));
    }

    if (outSummary.scannedW3dCount <= 0) {
        outError = QObject::tr("No animation W3D files were found under %1.")
            .arg(QDir::toNativeSeparators(animationRoot));
        return false;
    }
    if (outSources.empty()) {
        outError = QObject::tr("No animation clips with detectable skeleton codes were found under %1.")
            .arg(QDir::toNativeSeparators(animationRoot));
        return false;
    }

    std::sort(outSources.begin(), outSources.end(), [](const AnimationGifSource& lhs, const AnimationGifSource& rhs) {
        const int skeletonCompare =
            lhs.metadata.skeletonCode.compare(rhs.metadata.skeletonCode, Qt::CaseInsensitive);
        if (skeletonCompare != 0) {
            return skeletonCompare < 0;
        }

        const int stemCompare = lhs.clipStem.compare(rhs.clipStem, Qt::CaseInsensitive);
        if (stemCompare != 0) {
            return stemCompare < 0;
        }

        return lhs.resolvedW3dPath.compare(rhs.resolvedW3dPath, Qt::CaseInsensitive) < 0;
    });

    return true;
}

static void WriteAnimationGifReportCsvHeader(QTextStream& out)
{
    WriteCsvLine(
        out,
        {
            QStringLiteral("status"),
            QStringLiteral("selected_skeleton_code"),
            QStringLiteral("detected_skeleton_code"),
            QStringLiteral("clip_stem"),
            QStringLiteral("clip_name"),
            QStringLiteral("resolved_w3d_path"),
            QStringLiteral("resolved_json_path"),
            QStringLiteral("hierarchy_name"),
            QStringLiteral("num_frames"),
            QStringLiteral("frame_rate"),
            QStringLiteral("output_gif_path"),
            QStringLiteral("error_message")
        });
}

static void WriteAnimationGifReportCsvRow(QTextStream& out, const AnimationGifReportRow& row)
{
    WriteCsvLine(
        out,
        {
            row.status,
            row.selectedSkeletonCode,
            row.detectedSkeletonCode,
            row.clipStem,
            row.clipName,
            row.resolvedW3dPath,
            row.resolvedJsonPath,
            row.hierarchyName,
            row.numFrames,
            row.frameRate,
            row.outputGifPath,
            row.errorMessage
        });
}

static bool FindAnimationGifAnimationIndex(
    const OW3D::Render::RenderScene& scene,
    const QString& sourceFileLabel,
    const AnimationGifSource& source,
    int& outIndex,
    QString& outError)
{
    outIndex = -1;
    outError.clear();

    std::vector<int> candidates;
    const QString sourceLabelNormalized = sourceFileLabel.trimmed().toLower();
    const QString clipStemNormalized = source.clipStem.trimmed().toLower();
    const QString clipNameNormalized = source.metadata.clipName.trimmed().toLower();
    const QString hierarchyNormalized =
        source.metadata.hierarchyName.trimmed().toLower();

    for (std::size_t i = 0; i < scene.animations.size(); ++i) {
        const auto& clip = scene.animations[i];
        if (QString::fromStdString(clip.sourceFileLabel).trimmed().toLower() != sourceLabelNormalized) {
            continue;
        }
        if (!clip.supportedForPlayback
            || !SceneHasCompatibleHierarchyForAnimation(scene, static_cast<int>(i)))
        {
            continue;
        }
        candidates.push_back(static_cast<int>(i));
    }

    if (candidates.empty()) {
        outError = QObject::tr("The resolved animation file did not produce a compatible playable clip.");
        return false;
    }

    auto nameMatches = [&](const QString& clipShortName) {
        const QString normalized = clipShortName.trimmed().toLower();
        if (!clipNameNormalized.isEmpty() && normalized == clipNameNormalized) {
            return true;
        }
        if (!clipStemNormalized.isEmpty() && normalized == clipStemNormalized) {
            return true;
        }
        return false;
    };

    auto candidateMatches = [&](int animationIndex, bool requireHierarchy, bool requireName) {
        const auto& clip = scene.animations[static_cast<std::size_t>(animationIndex)];
        const QString clipHierarchy =
            QString::fromStdString(clip.hierarchyName).trimmed().toLower();
        const QString clipFullName =
            QString::fromStdString(clip.fullName).trimmed();
        const QString clipShortName =
            clipFullName.section(QLatin1Char('.'), -1).trimmed();

        if (requireHierarchy && !hierarchyNormalized.isEmpty()
            && clipHierarchy != hierarchyNormalized)
        {
            return false;
        }
        if (requireName && !nameMatches(clipShortName)) {
            return false;
        }
        return true;
    };

    for (const int candidate : candidates) {
        if (candidateMatches(candidate, true, true)) {
            outIndex = candidate;
            return true;
        }
    }

    for (const int candidate : candidates) {
        if (candidateMatches(candidate, false, true)) {
            outIndex = candidate;
            return true;
        }
    }

    for (const int candidate : candidates) {
        if (candidateMatches(candidate, true, false)) {
            outIndex = candidate;
            return true;
        }
    }

    if (candidates.size() == 1u) {
        outIndex = candidates.front();
        return true;
    }

    outError = QObject::tr(
        "The resolved animation file contains multiple compatible clips and could not be disambiguated.");
    return false;
}

static int GifDelayCentisecondsFromClipFrameRate(float frameRate) {
    const float safeFrameRate = frameRate > 0.0f ? frameRate : 10.0f;
    return std::max(1, static_cast<int>(std::round(100.0f / safeFrameRate)));
}

} // namespace

JsonSerializationMode MainWindow::loadDefaultSerializationModeSetting() const {
    QSettings settings;
    const QString token = settings.value(
        kJsonDefaultModeSettingKey,
        SerializationModeToken(JsonSerializationMode::StructuredPreferred)).toString();

    JsonSerializationMode mode = JsonSerializationMode::StructuredPreferred;
    if (!TryParseSerializationModeToken(token, mode)) {
        mode = JsonSerializationMode::StructuredPreferred;
    }
    return mode;
}

void MainWindow::saveDefaultSerializationModeSetting(JsonSerializationMode mode) const {
    QSettings settings;
    settings.setValue(kJsonDefaultModeSettingKey, SerializationModeToken(mode));
}

bool MainWindow::promptSerializationMode(
    const QString& title,
    const QString& prompt,
    JsonSerializationMode& outMode)
{
    const JsonSerializationMode defaultMode = loadDefaultSerializationModeSetting();
    const QStringList options = {
        SerializationModeUiLabel(JsonSerializationMode::StructuredPreferred),
        SerializationModeUiLabel(JsonSerializationMode::HexOnly)
    };
    const int defaultIndex = (defaultMode == JsonSerializationMode::HexOnly) ? 1 : 0;
    bool accepted = false;
    const QString selected = QInputDialog::getItem(
        this,
        title,
        prompt,
        options,
        defaultIndex,
        false,
        &accepted);
    if (!accepted) {
        return false;
    }

    outMode = (options.indexOf(selected) == 1)
        ? JsonSerializationMode::HexOnly
        : JsonSerializationMode::StructuredPreferred;
    saveDefaultSerializationModeSetting(outMode);
    return true;
}

MainWindow::ValidatorRunMode MainWindow::loadValidatorRunModeSetting() const {
    QSettings settings;
    const QString token = settings.value(
        kJsonValidatorRunModeSettingKey,
        QStringLiteral("BOTH")).toString().trimmed().toUpper();

    if (token == QStringLiteral("HEX_ONLY")) {
        return ValidatorRunMode::HexOnly;
    }
    if (token == QStringLiteral("STRUCTURED_PREFERRED")) {
        return ValidatorRunMode::StructuredPreferred;
    }
    return ValidatorRunMode::Both;
}

void MainWindow::saveValidatorRunModeSetting(ValidatorRunMode mode) const {
    QString token = QStringLiteral("BOTH");
    switch (mode) {
    case ValidatorRunMode::HexOnly:
        token = QStringLiteral("HEX_ONLY");
        break;
    case ValidatorRunMode::StructuredPreferred:
        token = QStringLiteral("STRUCTURED_PREFERRED");
        break;
    case ValidatorRunMode::Both:
    default:
        token = QStringLiteral("BOTH");
        break;
    }

    QSettings settings;
    settings.setValue(kJsonValidatorRunModeSettingKey, token);
}

bool MainWindow::promptValidatorRunMode(ValidatorRunMode& outMode) {
    const ValidatorRunMode defaultMode = loadValidatorRunModeSetting();
    const QStringList options = {
        tr("Run Both Modes"),
        tr("Structured Preferred"),
        tr("Hex Only")
    };

    int defaultIndex = 0;
    switch (defaultMode) {
    case ValidatorRunMode::StructuredPreferred:
        defaultIndex = 1;
        break;
    case ValidatorRunMode::HexOnly:
        defaultIndex = 2;
        break;
    case ValidatorRunMode::Both:
    default:
        defaultIndex = 0;
        break;
    }

    bool accepted = false;
    const QString selected = QInputDialog::getItem(
        this,
        tr("Round-Trip Validate"),
        tr("Serialization mode run selection"),
        options,
        defaultIndex,
        false,
        &accepted);
    if (!accepted) {
        return false;
    }

    const int selectedIndex = options.indexOf(selected);
    switch (selectedIndex) {
    case 1:
        outMode = ValidatorRunMode::StructuredPreferred;
        break;
    case 2:
        outMode = ValidatorRunMode::HexOnly;
        break;
    case 0:
    default:
        outMode = ValidatorRunMode::Both;
        break;
    }

    saveValidatorRunModeSetting(outMode);
    return true;
}

void MainWindow::on_actionExportChunkList_triggered()
{
    const QString startDir = lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory;
    QString srcDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Source Directory"),
        startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (srcDir.isEmpty()) return;

    std::vector<BatchInputSource> inputs;
    QStringList discoveryWarnings;
    DiscoverBatchInputs(srcDir, inputs, &discoveryWarnings);
    if (inputs.empty()) {
        QMessageBox::information(this, tr("Export Chunk List"),
            tr("No W3D/WLT files or archive entries found in %1.").arg(srcDir));
        return;
    }

    QString outPath = QFileDialog::getSaveFileName(
        this,
        tr("Save Chunk List As..."),
        QString(),
        tr("Text Files (*.txt);;All Files (*)")
    );
    if (outPath.isEmpty()) return;

    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Cannot write to %1").arg(outPath));
        return;
    }
    QTextStream txt(&file);

    std::map<uint32_t, int> counts;
    QString cachedArchivePath;
    QByteArray cachedArchiveBytes;
    for (const BatchInputSource& input : inputs) {
        txt << "=== " << input.sourcePath << " ===\n";

        QByteArray sourceBytes;
        QString readError;
        if (!ReadBatchInputOriginalBytes(
            input,
            sourceBytes,
            readError,
            cachedArchivePath,
            cachedArchiveBytes))
        {
            txt << "[ read error ] " << readError << "\n\n";
            continue;
        }

        ChunkData cd;
        QString loadError;
        if (!LoadBatchInputChunkData(input, sourceBytes, cd, loadError)) {
            txt << "[ parse error ] " << loadError << "\n\n";
            continue;
        }

        for (auto& top : cd.getChunks()) {
            recursePrint(top, 1, txt, counts);
        }
        txt << "\n";
    }
    if (!discoveryWarnings.isEmpty()) {
        txt << "=== Archive Scan Warnings ===\n";
        for (const QString& warning : discoveryWarnings) {
            txt << warning << "\n";
        }
        txt << "\n";
    }

    txt << "=== Chunk Type Totals ===\n";
    for (const auto& [id, count] : counts) {
        txt << QString("0x%1 ").arg(id, 8, 16, QChar('0')).toUpper()
            << QString::fromStdString(LabelForChunk(id, nullptr))
            << ": " << count << "\n";
    }

    file.close();
    lastDirectory = srcDir;

    QString summary = tr("Chunk list exported to %1.\nInputs scanned: %2")
        .arg(outPath)
        .arg(static_cast<int>(inputs.size()));
    if (!discoveryWarnings.isEmpty()) {
        summary += tr("\nArchive scan warnings: %1").arg(discoveryWarnings.size());
        QMessageBox::warning(this, tr("Done"), summary);
        return;
    }

    QMessageBox::information(this, tr("Done"), summary);
}

void MainWindow::on_actionExportJsonBatch_triggered()
{
    QString startDir = lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory;
    QString srcDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Source Directory"),
        startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (srcDir.isEmpty()) return;

    QString outDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        srcDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (outDir.isEmpty()) return;

    std::vector<BatchInputSource> inputs;
    QStringList discoveryWarnings;
    DiscoverBatchInputs(srcDir, inputs, &discoveryWarnings);
    if (inputs.empty()) {
        QMessageBox::information(this, tr("Export JSON"),
            tr("No W3D/WLT files or archive entries found in %1.").arg(srcDir));
        return;
    }

    JsonSerializationMode selectedMode = JsonSerializationMode::StructuredPreferred;
    if (!promptSerializationMode(
        tr("Export JSON Batch"),
        tr("Serialization mode"),
        selectedMode))
    {
        return;
    }

    QDir outputDir(outDir);
    int successCount = 0;
    QStringList failures;
    bool canceled = false;

    QString cachedArchivePath;
    QByteArray cachedArchiveBytes;

    QProgressDialog progress(tr("Preparing export..."), tr("Cancel"), 0, static_cast<int>(inputs.size()), this);
    progress.setWindowTitle(tr("Export JSON Batch"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setValue(0);

    for (int i = 0; i < static_cast<int>(inputs.size()); ++i) {
        const BatchInputSource& input = inputs[static_cast<std::size_t>(i)];
        progress.setValue(i);
        progress.setLabelText(tr("Exporting %1 (%2/%3)")
            .arg(input.relativePath)
            .arg(i + 1)
            .arg(static_cast<int>(inputs.size())));
        QCoreApplication::processEvents();
        if (progress.wasCanceled()) {
            canceled = true;
            break;
        }

        QByteArray sourceBytes;
        QString readError;
        if (!ReadBatchInputOriginalBytes(
            input,
            sourceBytes,
            readError,
            cachedArchivePath,
            cachedArchiveBytes))
        {
            failures << tr("%1 (read failed: %2)").arg(input.sourcePath, readError);
            continue;
        }

        ChunkData cd;
        QString loadError;
        if (!LoadBatchInputChunkData(input, sourceBytes, cd, loadError)) {
            failures << tr("%1 (load failed: %2)").arg(input.sourcePath, loadError);
            continue;
        }

        ordered_json doc;
        try {
            doc = cd.toJson(selectedMode);
        }
        catch (const std::exception& e) {
            failures << tr("%1 (JSON export failed: %2)")
                .arg(input.sourcePath, QString::fromUtf8(e.what()));
            continue;
        }

        const QString outputPath = outputDir.absoluteFilePath(
            BuildBatchJsonRelativePath(input.relativePath));
        QString writeError;
        if (!WriteAllBytes(outputPath, QByteArray::fromStdString(doc.dump(4)), writeError)) {
            failures << tr("%1 (write failed: %2)").arg(outputPath, writeError);
            continue;
        }

        ++successCount;
    }
    progress.setValue(static_cast<int>(inputs.size()));

    lastDirectory = srcDir;

    QString summary = tr("%1\nExported %2 of %3 input(s) to %4.\nMode: %5")
        .arg(canceled ? tr("Export canceled.") : tr("Export completed."))
        .arg(successCount)
        .arg(static_cast<int>(inputs.size()))
        .arg(outDir)
        .arg(SerializationModeToken(selectedMode));

    if (!discoveryWarnings.isEmpty()) {
        QStringList preview = discoveryWarnings.mid(0, 10);
        if (discoveryWarnings.size() > preview.size()) {
            preview << tr("... (%1 additional warnings)")
                .arg(discoveryWarnings.size() - preview.size());
        }
        summary += tr("\n\nArchive scan warnings (%1):\n%2")
            .arg(discoveryWarnings.size())
            .arg(preview.join("\n"));
    }

    if (!failures.isEmpty()) {
        QStringList preview = failures.mid(0, 20);
        if (failures.size() > preview.size()) {
            preview << tr("... (%1 additional failures)")
                .arg(failures.size() - preview.size());
        }
        summary += tr("\n\nFailures:\n%1").arg(preview.join("\n"));
    }

    if (!canceled && failures.isEmpty() && discoveryWarnings.isEmpty()) {
        QMessageBox::information(this, tr("Export JSON"), summary);
    }
    else {
        QMessageBox::warning(this, tr("Export JSON"), summary);
    }
}

void MainWindow::on_actionValidateRoundTripBatch_triggered()
{
    const QString startDir = lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory;
    const QString srcDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Source Directory"),
        startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (srcDir.isEmpty()) return;

    const QString outDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        srcDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (outDir.isEmpty()) return;

    std::vector<BatchInputSource> inputs;
    QStringList discoveryWarnings;
    DiscoverBatchInputs(srcDir, inputs, &discoveryWarnings);

    if (inputs.empty()) {
        QString summary = tr("No W3D/WLT files or archive entries found in %1.").arg(srcDir);
        if (!discoveryWarnings.isEmpty()) {
            QStringList preview = discoveryWarnings.mid(0, 10);
            if (discoveryWarnings.size() > preview.size()) {
                preview << tr("... (%1 additional warnings)")
                    .arg(discoveryWarnings.size() - preview.size());
            }
            summary += tr("\n\nArchive scan warnings (%1):\n%2")
                .arg(discoveryWarnings.size())
                .arg(preview.join("\n"));
        }
        QMessageBox::information(this, tr("Round-Trip Validate"), summary);
        return;
    }
    const int discoveredFileCount = static_cast<int>(inputs.size());

    ValidatorRunMode selectedRunMode = ValidatorRunMode::Both;
    if (!promptValidatorRunMode(selectedRunMode)) {
        return;
    }

    std::vector<JsonSerializationMode> modesToRun;
    switch (selectedRunMode) {
    case ValidatorRunMode::StructuredPreferred:
        modesToRun = { JsonSerializationMode::StructuredPreferred };
        break;
    case ValidatorRunMode::HexOnly:
        modesToRun = { JsonSerializationMode::HexOnly };
        break;
    case ValidatorRunMode::Both:
    default:
        modesToRun = { JsonSerializationMode::StructuredPreferred, JsonSerializationMode::HexOnly };
        break;
    }
    const int totalRuns = discoveredFileCount * static_cast<int>(modesToRun.size());

    QDir outputRoot(outDir);
    const QString runBase = QStringLiteral("roundtrip-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
    QString runName = runBase;
    int runSuffix = 1;
    while (QFileInfo::exists(outputRoot.absoluteFilePath(runName))) {
        runName = QStringLiteral("%1-%2").arg(runBase).arg(runSuffix++);
    }

    const QString runDirPath = outputRoot.absoluteFilePath(runName);
    const QString failuresRootPath = QDir(runDirPath).absoluteFilePath(QStringLiteral("failures"));
    const QString workRootPath = QDir(runDirPath).absoluteFilePath(QStringLiteral("_work"));
    const QString reportPath = QDir(runDirPath).absoluteFilePath(QStringLiteral("report.csv"));

    if (!QDir().mkpath(runDirPath) || !QDir().mkpath(failuresRootPath) || !QDir().mkpath(workRootPath)) {
        QMessageBox::warning(this, tr("Round-Trip Validate"),
            tr("Failed to create output folders under %1").arg(outDir));
        return;
    }

    QFile reportFile(reportPath);
    if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Round-Trip Validate"),
            tr("Cannot write report file: %1").arg(reportPath));
        return;
    }
    QTextStream reportStream(&reportFile);
    WriteRoundTripCsvHeader(reportStream);
    reportStream.flush();

    QProgressDialog progress(tr("Preparing validation..."), tr("Cancel"), 0, totalRuns, this);
    progress.setWindowTitle(tr("Round-Trip Validate"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setValue(0);

    QDir failuresRoot(failuresRootPath);

    int processedRuns = 0;
    int passCount = 0;
    int failCount = 0;
    bool canceled = false;
    int runCounter = 0;
    QString cachedArchivePath;
    QByteArray cachedArchiveBytes;

    for (int i = 0; i < discoveredFileCount; ++i) {
        const BatchInputSource& input = inputs[static_cast<std::size_t>(i)];
        const QString relativePath = input.relativePath;

        for (const JsonSerializationMode mode : modesToRun) {
            progress.setValue(runCounter);
            progress.setLabelText(tr("Validating %1 [%2] (%3/%4)")
                .arg(relativePath)
                .arg(SerializationModeToken(mode))
                .arg(runCounter + 1)
                .arg(totalRuns));
            QCoreApplication::processEvents();
            if (progress.wasCanceled()) {
                canceled = true;
                break;
            }

            QElapsedTimer timer;
            timer.start();

            RoundTripReportRow row;
            row.mode = SerializationModeToken(mode);
            row.sourcePath = input.sourcePath;
            row.relativePath = relativePath;

            QByteArray originalBytes;
            QByteArray rebuiltBytes;
            QString jsonPayload;
            bool haveJsonPayload = false;
            bool haveRebuiltBytes = false;
            RoundTripFallbackMetrics fallbackMetrics;
            std::vector<std::string> importWarnings;

            do {
                QString ioError;
                if (!ReadBatchInputOriginalBytes(
                    input,
                    originalBytes,
                    ioError,
                    cachedArchivePath,
                    cachedArchiveBytes))
                {
                    row.stage = QStringLiteral("LOAD_W3D");
                    row.errorMessage = ioError;
                    break;
                }
                row.originalSize = originalBytes.size();

                ChunkData sourceData;
                QString loadError;
                if (!LoadBatchInputChunkData(input, originalBytes, sourceData, loadError)) {
                    row.stage = QStringLiteral("LOAD_W3D");
                    row.errorMessage = loadError;
                    break;
                }

                ordered_json exportedDoc;
                row.stage = QStringLiteral("EXPORT_JSON");
                try {
                    exportedDoc = sourceData.toJson(mode);
                    CollectFallbackMetrics(exportedDoc, fallbackMetrics);
                    jsonPayload = QString::fromStdString(exportedDoc.dump(4));
                    haveJsonPayload = true;
                }
                catch (const std::exception& e) {
                    row.errorMessage = tr("JSON export failed: %1").arg(QString::fromUtf8(e.what()));
                    break;
                }

                ordered_json reparsedDoc;
                row.stage = QStringLiteral("PARSE_JSON");
                try {
                    reparsedDoc = ordered_json::parse(jsonPayload.toStdString());
                }
                catch (const std::exception& e) {
                    row.errorMessage = tr("JSON parse failed: %1").arg(QString::fromUtf8(e.what()));
                    break;
                }

                ChunkData rebuiltData;
                row.stage = QStringLiteral("IMPORT_JSON");
                try {
                    if (!rebuiltData.fromJson(reparsedDoc, &importWarnings)) {
                        row.errorMessage = tr("ChunkData::fromJson returned false.");
                        break;
                    }
                }
                catch (const std::exception& e) {
                    row.errorMessage = tr("JSON import failed: %1").arg(QString::fromUtf8(e.what()));
                    break;
                }

                row.stage = QStringLiteral("SAVE_REBUILT");
                QTemporaryFile rebuiltTempFile(QDir(workRootPath).absoluteFilePath(QStringLiteral("rebuilt-XXXXXX.tmp")));
                rebuiltTempFile.setAutoRemove(true);
                if (!rebuiltTempFile.open()) {
                    row.errorMessage = tr("Failed to create temporary rebuilt file.");
                    break;
                }
                const QString rebuiltTempPath = rebuiltTempFile.fileName();
                rebuiltTempFile.close();

                if (!rebuiltData.saveToFile(rebuiltTempPath.toStdString())) {
                    row.errorMessage = tr("Failed to save rebuilt W3D/WLT.");
                    break;
                }

                QString rebuiltReadError;
                if (!ReadAllBytes(rebuiltTempPath, rebuiltBytes, rebuiltReadError)) {
                    row.stage = QStringLiteral("COMPARE_BYTES");
                    row.errorMessage = rebuiltReadError;
                    break;
                }
                haveRebuiltBytes = true;
                row.rebuiltSize = rebuiltBytes.size();

                qint64 firstDiffOffset = -1;
                int originalByte = -1;
                int rebuiltByte = -1;
                row.stage = QStringLiteral("COMPARE_BYTES");
                if (!CompareBytes(originalBytes, rebuiltBytes, firstDiffOffset, originalByte, rebuiltByte)) {
                    row.firstDiffOffset = firstDiffOffset;
                    row.originalByte = originalByte;
                    row.rebuiltByte = rebuiltByte;
                    row.errorMessage = tr("Byte mismatch at offset %1.").arg(firstDiffOffset);
                    break;
                }

                row.status = QStringLiteral("PASS");
            } while (false);

            row.fallbackNodeCount = fallbackMetrics.nodeCount;
            row.fallbackChunkIds = FormatFallbackChunkCounts(fallbackMetrics.chunkCounts);
            row.durationMs = timer.elapsed();
            row.warningCount = static_cast<int>(importWarnings.size());
            if (!importWarnings.empty()) {
                QStringList warningLines;
                warningLines.reserve(static_cast<int>(importWarnings.size()));
                for (const std::string& warning : importWarnings) {
                    warningLines << QString::fromStdString(warning);
                }
                row.warnings = warningLines.join(QStringLiteral(" | "));
            }

            if (row.status == QStringLiteral("PASS")) {
                ++passCount;
            }
            else {
                ++failCount;
                if (row.errorMessage.isEmpty()) {
                    row.errorMessage = tr("Validation failed at stage %1.").arg(row.stage);
                }

                if (haveJsonPayload) {
                    const QString jsonRelPath = BuildFailureJsonRelativePath(relativePath);
                    const QString jsonAbsPath = failuresRoot.absoluteFilePath(
                        QDir::cleanPath(SerializationModeToken(mode) + "/" + jsonRelPath));
                    QString writeError;
                    if (WriteAllBytes(jsonAbsPath, jsonPayload.toUtf8(), writeError)) {
                        row.jsonArtifactPath = QDir::toNativeSeparators(jsonAbsPath);
                    }
                    else {
                        row.errorMessage += QStringLiteral(" | ") + writeError;
                    }
                }

                if (haveRebuiltBytes) {
                    const QString rebuiltRelPath = BuildFailureRebuiltRelativePath(relativePath);
                    const QString rebuiltAbsPath = failuresRoot.absoluteFilePath(
                        QDir::cleanPath(SerializationModeToken(mode) + "/" + rebuiltRelPath));
                    QString writeError;
                    if (WriteAllBytes(rebuiltAbsPath, rebuiltBytes, writeError)) {
                        row.rebuiltArtifactPath = QDir::toNativeSeparators(rebuiltAbsPath);
                    }
                    else {
                        row.errorMessage += QStringLiteral(" | ") + writeError;
                    }
                }
            }

            WriteRoundTripCsvRow(reportStream, row);
            reportStream.flush();
            ++processedRuns;
            ++runCounter;
        }

        if (canceled) {
            break;
        }
    }

    progress.setValue(runCounter);
    reportFile.close();
    lastDirectory = srcDir;

    QString summary = tr("%1\n\nDiscovered inputs: %2\nTotal mode-runs: %3\nProcessed mode-runs: %4\nPass: %5\nFail: %6\nReport: %7\nFailure artifacts: %8")
        .arg(canceled ? tr("Validation canceled.") : tr("Validation completed."))
        .arg(discoveredFileCount)
        .arg(totalRuns)
        .arg(processedRuns)
        .arg(passCount)
        .arg(failCount)
        .arg(QDir::toNativeSeparators(reportPath))
        .arg(QDir::toNativeSeparators(failuresRootPath));

    if (!discoveryWarnings.isEmpty()) {
        QStringList preview = discoveryWarnings.mid(0, 10);
        if (discoveryWarnings.size() > preview.size()) {
            preview << tr("... (%1 additional warnings)")
                .arg(discoveryWarnings.size() - preview.size());
        }
        summary += tr("\n\nArchive scan warnings (%1):\n%2")
            .arg(discoveryWarnings.size())
            .arg(preview.join("\n"));
    }

    if (canceled || failCount > 0 || !discoveryWarnings.isEmpty()) {
        QMessageBox::warning(this, tr("Round-Trip Validate"), summary);
    }
    else {
        QMessageBox::information(this, tr("Round-Trip Validate"), summary);
    }
}

void MainWindow::on_actionCopyPureHumanAnimationsBySkeleton_triggered()
{
    const QString startDir = lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory;
    const QString srcDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Source Directory"),
        startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (srcDir.isEmpty()) return;

    const QString allowlistPath = QFileDialog::getOpenFileName(
        this,
        tr("Select Human Skeleton Allowlist"),
        srcDir,
        tr("Text Files (*.txt);;All Files (*)"));
    if (allowlistPath.isEmpty()) return;

    const QString outDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        srcDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (outDir.isEmpty()) return;

    SkeletonAllowlist allowlist;
    QString allowlistError;
    if (!LoadSkeletonAllowlist(allowlistPath, allowlist, allowlistError)) {
        QMessageBox::warning(
            this,
            tr("Copy Pure Human Animations by Skeleton"),
            allowlistError.isEmpty()
                ? tr("Failed to load the allowlist file.")
                : allowlistError);
        return;
    }

    std::vector<BatchInputSource> inputs;
    QStringList discoveryWarnings;
    DiscoverBatchInputs(srcDir, inputs, &discoveryWarnings);

    std::vector<BatchInputSource> archiveInputs;
    archiveInputs.reserve(inputs.size());
    for (const BatchInputSource& input : inputs) {
        if (input.fromArchive) {
            archiveInputs.push_back(input);
        }
    }

    if (archiveInputs.empty()) {
        QString summary = tr("No W3D/WLT archive entries found in %1.").arg(srcDir);
        if (!discoveryWarnings.isEmpty()) {
            QStringList preview = discoveryWarnings.mid(0, 10);
            if (discoveryWarnings.size() > preview.size()) {
                preview << tr("... (%1 additional warnings)")
                    .arg(discoveryWarnings.size() - preview.size());
            }
            summary += tr("\n\nArchive scan warnings (%1):\n%2")
                .arg(discoveryWarnings.size())
                .arg(preview.join("\n"));
        }
        QMessageBox::information(this, tr("Copy Pure Human Animations by Skeleton"), summary);
        return;
    }

    const QString reportPath = QDir(outDir).absoluteFilePath(QStringLiteral("report.csv"));
    QFile reportFile(reportPath);
    if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(
            this,
            tr("Copy Pure Human Animations by Skeleton"),
            tr("Cannot write report file: %1").arg(reportPath));
        return;
    }
    QTextStream reportStream(&reportFile);
    WritePureAnimationCopyCsvHeader(reportStream);
    reportStream.flush();

    QProgressDialog progress(
        tr("Preparing copy..."),
        tr("Cancel"),
        0,
        static_cast<int>(archiveInputs.size()),
        this);
    progress.setWindowTitle(tr("Copy Pure Human Animations by Skeleton"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setValue(0);

    QString cachedArchivePath;
    QByteArray cachedArchiveBytes;
    int pureAnimationCount = 0;
    int copiedEntryCount = 0;
    int copiesWrittenCount = 0;
    int skippedCount = 0;
    int failureCount = 0;
    bool canceled = false;

    for (int i = 0; i < static_cast<int>(archiveInputs.size()); ++i) {
        const BatchInputSource& input = archiveInputs[static_cast<std::size_t>(i)];
        progress.setValue(i);
        progress.setLabelText(tr("Checking %1 (%2/%3)")
            .arg(input.relativePath)
            .arg(i + 1)
            .arg(static_cast<int>(archiveInputs.size())));
        QCoreApplication::processEvents();
        if (progress.wasCanceled()) {
            canceled = true;
            break;
        }

        PureAnimationCopyReportRow row;
        row.sourcePath = input.sourcePath;
        row.relativePath = input.relativePath;

        QByteArray sourceBytes;
        QString readError;
        if (!ReadBatchInputOriginalBytes(
            input,
            sourceBytes,
            readError,
            cachedArchivePath,
            cachedArchiveBytes))
        {
            row.status = QStringLiteral("FAIL");
            row.errorMessage = readError;
            ++failureCount;
            WritePureAnimationCopyCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }

        ChunkData sourceData;
        QString loadError;
        if (!LoadBatchInputChunkData(input, sourceBytes, sourceData, loadError)) {
            row.status = QStringLiteral("FAIL");
            row.errorMessage = loadError;
            ++failureCount;
            WritePureAnimationCopyCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }

        if (!IsPureAnimationFile(sourceData.getChunks())) {
            row.status = QStringLiteral("SKIP");
            row.errorMessage = tr("Entry is not a pure-animation W3D/WLT.");
            ++skippedCount;
            WritePureAnimationCopyCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }

        ++pureAnimationCount;

        QStringList detectedHierarchyNames;
        QString hierarchyError;
        if (!CollectPureAnimationHierarchyNames(
            sourceData.getChunks(),
            detectedHierarchyNames,
            hierarchyError))
        {
            row.status = QStringLiteral("FAIL");
            row.errorMessage = hierarchyError;
            ++failureCount;
            WritePureAnimationCopyCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }

        row.detectedHierarchies = detectedHierarchyNames.join(QStringLiteral(" | "));

        const QStringList matchedHierarchyNames =
            MatchAllowlistHierarchyNames(detectedHierarchyNames, allowlist);
        row.matchedSkeletons = matchedHierarchyNames.join(QStringLiteral(" | "));

        if (matchedHierarchyNames.isEmpty()) {
            row.status = QStringLiteral("SKIP");
            row.errorMessage = detectedHierarchyNames.isEmpty()
                ? tr("Pure-animation entry does not declare any hierarchy names.")
                : tr("Pure-animation entry does not match the human skeleton allowlist.");
            ++skippedCount;
            WritePureAnimationCopyCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }

        QStringList copyTargets;
        QString copyError;
        for (const QString& hierarchyName : matchedHierarchyNames) {
            const auto folderIt = allowlist.folderNameByNormalized.find(hierarchyName);
            const QString folderName = folderIt != allowlist.folderNameByNormalized.end()
                ? folderIt->second
                : SanitizePathComponent(hierarchyName);
            const QString destinationPath = QDir(outDir).absoluteFilePath(
                QDir::cleanPath(folderName + QLatin1Char('/') + input.relativePath));

            QString writeError;
            if (!WriteAllBytes(destinationPath, sourceBytes, writeError)) {
                copyError = writeError;
                break;
            }

            copyTargets << QDir::toNativeSeparators(destinationPath);
        }

        row.copiesWritten = copyTargets.size();
        row.copyTargets = copyTargets.join(QStringLiteral(" | "));

        if (!copyError.isEmpty()) {
            row.status = QStringLiteral("FAIL");
            row.errorMessage = copyError;
            ++failureCount;
        }
        else {
            row.status = QStringLiteral("COPIED");
            ++copiedEntryCount;
            copiesWrittenCount += row.copiesWritten;
        }

        WritePureAnimationCopyCsvRow(reportStream, row);
        reportStream.flush();
    }

    progress.setValue(static_cast<int>(archiveInputs.size()));
    reportFile.close();
    lastDirectory = srcDir;

    QString summary =
        tr("%1\n\nArchive entries discovered: %2\nPure-animation entries: %3\nCopied entries: %4\nCopies written: %5\nSkipped: %6\nFailed: %7\nReport: %8")
            .arg(canceled ? tr("Copy canceled.") : tr("Copy completed."))
            .arg(static_cast<int>(archiveInputs.size()))
            .arg(pureAnimationCount)
            .arg(copiedEntryCount)
            .arg(copiesWrittenCount)
            .arg(skippedCount)
            .arg(failureCount)
            .arg(QDir::toNativeSeparators(reportPath));

    if (!discoveryWarnings.isEmpty()) {
        QStringList preview = discoveryWarnings.mid(0, 10);
        if (discoveryWarnings.size() > preview.size()) {
            preview << tr("... (%1 additional warnings)")
                .arg(discoveryWarnings.size() - preview.size());
        }
        summary += tr("\n\nArchive scan warnings (%1):\n%2")
            .arg(discoveryWarnings.size())
            .arg(preview.join("\n"));
    }

    if (canceled || failureCount > 0 || !discoveryWarnings.isEmpty()) {
        QMessageBox::warning(this, tr("Copy Pure Human Animations by Skeleton"), summary);
    }
    else {
        QMessageBox::information(this, tr("Copy Pure Human Animations by Skeleton"), summary);
    }
}

void MainWindow::on_actionExportSkeletonAAnimationGifs_triggered()
{
    if (!chunkData || (currentFilePath.isEmpty() && currentArchiveRenderPath.isEmpty())) {
        QMessageBox::information(
            this,
            tr("Export Animation GIFs"),
            tr("Open a renderable model or aggregate file first."));
        return;
    }

    auto findPathUpward = [](const QString& startDirectory, const QString& relativePath, bool requireDirectory) {
        QDir dir(startDirectory);
        while (dir.exists()) {
            const QString candidate = QDir::cleanPath(dir.absoluteFilePath(relativePath));
            const QFileInfo info(candidate);
            if ((requireDirectory && info.isDir()) || (!requireDirectory && info.isFile())) {
                return candidate;
            }
            if (!dir.cdUp()) {
                break;
            }
        }
        return QString();
    };

    const QString searchStart = !currentFilePath.isEmpty()
        ? QFileInfo(currentFilePath).absolutePath()
        : QDir::currentPath();
    QString animationRoot = findPathUpward(searchStart, QStringLiteral("animation"), true);
    if (animationRoot.isEmpty()) {
        animationRoot = findPathUpward(QDir::currentPath(), QStringLiteral("animation"), true);
    }
    if (animationRoot.isEmpty()) {
        animationRoot = searchStart;
    }
    if (animationRoot.isEmpty()) {
        animationRoot = lastDirectory;
    }

    QString modelDisplayPath;
    QString modelStem;
    if (!currentArchiveRenderPath.isEmpty() && !currentArchiveRenderEntryPath.isEmpty()) {
        modelDisplayPath =
            BuildArchiveEntryDisplayPath(currentArchiveRenderPath, currentArchiveRenderEntryPath);
        modelStem = QFileInfo(currentArchiveRenderEntryPath).completeBaseName();
    }
    else {
        modelDisplayPath = QDir::toNativeSeparators(QDir::cleanPath(currentFilePath));
        modelStem = QFileInfo(currentFilePath).completeBaseName();
    }
    if (modelStem.isEmpty()) {
        modelStem = QStringLiteral("current_model");
    }
    modelStem = SanitizePathComponent(modelStem);
    if (modelStem.isEmpty()) {
        modelStem = QStringLiteral("current_model");
    }

    const bool useCurrentEditorCamera = renderViewport != nullptr;
    const OW3D::Render::CameraState fixedCamera = useCurrentEditorCamera
        ? renderViewport->camera()
        : OW3D::Render::CameraState{};

    QDialog configDialog(this);
    configDialog.setWindowTitle(tr("Export Animation GIFs"));
    configDialog.setModal(true);
    auto* configLayout = new QVBoxLayout(&configDialog);
    auto* formLayout = new QFormLayout();
    configLayout->addLayout(formLayout);

    auto* modelPathEdit = new QLineEdit(modelDisplayPath, &configDialog);
    modelPathEdit->setReadOnly(true);
    formLayout->addRow(tr("Current Model"), modelPathEdit);

    auto* animationRootRow = new QWidget(&configDialog);
    auto* animationRootLayout = new QHBoxLayout(animationRootRow);
    animationRootLayout->setContentsMargins(0, 0, 0, 0);
    animationRootLayout->setSpacing(6);
    auto* animationRootEdit = new QLineEdit(QDir::toNativeSeparators(animationRoot), animationRootRow);
    auto* browseAnimationRootButton = new QPushButton(tr("Browse..."), animationRootRow);
    animationRootLayout->addWidget(animationRootEdit, 1);
    animationRootLayout->addWidget(browseAnimationRootButton);
    formLayout->addRow(tr("Animation Root"), animationRootRow);

    auto* skeletonCombo = new QComboBox(&configDialog);
    formLayout->addRow(tr("Skeleton"), skeletonCombo);

    auto* outputRow = new QWidget(&configDialog);
    auto* outputRowLayout = new QHBoxLayout(outputRow);
    outputRowLayout->setContentsMargins(0, 0, 0, 0);
    outputRowLayout->setSpacing(6);
    auto* outputPathEdit = new QLineEdit(outputRow);
    auto* browseOutputButton = new QPushButton(tr("Browse..."), outputRow);
    outputRowLayout->addWidget(outputPathEdit, 1);
    outputRowLayout->addWidget(browseOutputButton);
    formLayout->addRow(tr("Output Folder"), outputRow);

    auto* summaryLabel = new QLabel(&configDialog);
    summaryLabel->setWordWrap(true);
    configLayout->addWidget(summaryLabel);

    auto* cameraLabel = new QLabel(
        tr("This batch uses the current main render viewport camera as-is."),
        &configDialog);
    cameraLabel->setWordWrap(true);
    configLayout->addWidget(cameraLabel);

    auto* configButtons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal,
        &configDialog);
    configButtons->button(QDialogButtonBox::Ok)->setText(tr("Export"));
    configLayout->addWidget(configButtons);

    std::vector<AnimationGifSource> allDiscoveredSources;
    AnimationGifDiscoverySummary discoverySummary;
    std::vector<AnimationGifReportRow> discoverySkippedRows;
    QString discoveryError;
    bool outputPathCustomized = false;

    auto buildDefaultOutputDir = [&](const QString& rootPath, const QString& skeletonCode) {
        const QString normalizedRoot = QDir::cleanPath(QDir::fromNativeSeparators(rootPath.trimmed()));
        if (normalizedRoot.isEmpty()) {
            return QString();
        }

        const QString normalizedSkeleton = skeletonCode.trimmed().toUpper();
        const QString relativeOutputPath = normalizedSkeleton.isEmpty()
            ? QStringLiteral("gifs/%1").arg(modelStem)
            : QStringLiteral("gifs/%1/%2").arg(modelStem, normalizedSkeleton);
        return QDir(normalizedRoot).absoluteFilePath(relativeOutputPath);
    };

    auto refreshSelectionSummary = [&]() {
        auto* okButton = configButtons->button(QDialogButtonBox::Ok);
        const QString normalizedRoot =
            QDir::cleanPath(QDir::fromNativeSeparators(animationRootEdit->text().trimmed()));
        if (!outputPathCustomized) {
            outputPathEdit->setText(QDir::toNativeSeparators(
                buildDefaultOutputDir(normalizedRoot, skeletonCombo->currentText())));
        }

        if (!discoveryError.isEmpty()) {
            summaryLabel->setText(discoveryError);
            skeletonCombo->setEnabled(false);
            okButton->setEnabled(false);
            return;
        }

        QStringList detectedCodes = discoverySummary.discoveredSkeletonCodes.values();
        std::sort(
            detectedCodes.begin(),
            detectedCodes.end(),
            [](const QString& lhs, const QString& rhs) {
                return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
            });

        const QString selectedSkeletonCode = skeletonCombo->currentText().trimmed().toUpper();
        int matchingCount = 0;
        int exportableCount = 0;
        for (const AnimationGifSource& source : allDiscoveredSources) {
            if (source.metadata.skeletonCode.compare(selectedSkeletonCode, Qt::CaseInsensitive) != 0) {
                continue;
            }

            ++matchingCount;
            if (source.metadata.supportedForPlayback
                && source.metadata.numFrames > 0
                && (source.metadata.numFrames <= 1 || source.metadata.frameRate > 0.0))
            {
                ++exportableCount;
            }
        }

        const int selectedMetadataSkipCount = std::max(0, matchingCount - exportableCount);
        summaryLabel->setText(
            tr("Scanned W3D files: %1\n"
               "Detected skeleton codes: %2\n"
               "Selected skeleton files: %3\n"
               "Exportable by metadata: %4\n"
               "Selected-skeleton metadata skips: %5\n"
               "Unreadable files: %6\n"
               "Unclassified files: %7")
                .arg(discoverySummary.scannedW3dCount)
                .arg(detectedCodes.isEmpty() ? tr("(none)") : detectedCodes.join(QStringLiteral(", ")))
                .arg(matchingCount)
                .arg(exportableCount)
                .arg(selectedMetadataSkipCount)
                .arg(discoverySummary.unreadableCount)
                .arg(discoverySummary.unclassifiedCount));
        skeletonCombo->setEnabled(!detectedCodes.isEmpty());
        okButton->setEnabled(!selectedSkeletonCode.isEmpty() && exportableCount > 0);
    };

    auto rescanAnimationRoot = [&]() {
        allDiscoveredSources.clear();
        discoverySummary = {};
        discoverySkippedRows.clear();
        discoveryError.clear();

        const QString normalizedRoot =
            QDir::cleanPath(QDir::fromNativeSeparators(animationRootEdit->text().trimmed()));
        if (normalizedRoot.isEmpty()) {
            discoveryError = tr("Select an animation root folder.");
        }
        else if (!QFileInfo(normalizedRoot).isDir()) {
            discoveryError = tr("Animation root does not exist: %1")
                .arg(QDir::toNativeSeparators(normalizedRoot));
        }
        else if (!DiscoverAnimationGifSources(
            normalizedRoot,
            allDiscoveredSources,
            discoverySummary,
            discoverySkippedRows,
            discoveryError))
        {
        }

        const QString previousSelection = skeletonCombo->currentText().trimmed().toUpper();
        QStringList detectedCodes = discoverySummary.discoveredSkeletonCodes.values();
        std::sort(
            detectedCodes.begin(),
            detectedCodes.end(),
            [](const QString& lhs, const QString& rhs) {
                return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
            });

        {
            QSignalBlocker blocker(skeletonCombo);
            skeletonCombo->clear();
            skeletonCombo->addItems(detectedCodes);
            int selectedIndex = detectedCodes.indexOf(previousSelection);
            if (selectedIndex < 0 && !detectedCodes.isEmpty()) {
                selectedIndex = 0;
            }
            if (selectedIndex >= 0) {
                skeletonCombo->setCurrentIndex(selectedIndex);
            }
        }

        refreshSelectionSummary();
    };

    connect(browseAnimationRootButton, &QPushButton::clicked, &configDialog, [&]() {
        const QString startDir = animationRootEdit->text().trimmed().isEmpty()
            ? (lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory)
            : animationRootEdit->text().trimmed();
        const QString selectedDir = QFileDialog::getExistingDirectory(
            this,
            tr("Select Animation Root"),
            startDir,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!selectedDir.isEmpty()) {
            animationRootEdit->setText(QDir::toNativeSeparators(selectedDir));
            rescanAnimationRoot();
        }
    });
    connect(animationRootEdit, &QLineEdit::editingFinished, &configDialog, rescanAnimationRoot);
    connect(
        skeletonCombo,
        qOverload<const QString&>(&QComboBox::currentTextChanged),
        &configDialog,
        refreshSelectionSummary);
    connect(outputPathEdit, &QLineEdit::textEdited, &configDialog, [&]() {
        outputPathCustomized = true;
    });
    connect(browseOutputButton, &QPushButton::clicked, &configDialog, [this, outputPathEdit, &outputPathCustomized]() {
        const QString startDir = outputPathEdit->text().trimmed().isEmpty()
            ? (lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory)
            : outputPathEdit->text().trimmed();
        const QString selectedDir = QFileDialog::getExistingDirectory(
            this,
            tr("Select Output Directory"),
            startDir,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!selectedDir.isEmpty()) {
            outputPathEdit->setText(QDir::toNativeSeparators(selectedDir));
            outputPathCustomized = true;
        }
    });
    connect(configButtons, &QDialogButtonBox::accepted, &configDialog, [&]() {
        const QString normalizedAnimationRoot =
            QDir::cleanPath(QDir::fromNativeSeparators(animationRootEdit->text().trimmed()));
        if (normalizedAnimationRoot.isEmpty() || !QFileInfo(normalizedAnimationRoot).isDir()) {
            QMessageBox::warning(
                &configDialog,
                tr("Export Animation GIFs"),
                tr("Select a valid animation root folder first."));
            return;
        }
        if (skeletonCombo->currentText().trimmed().isEmpty()) {
            QMessageBox::warning(
                &configDialog,
                tr("Export Animation GIFs"),
                tr("Select a skeleton code first."));
            return;
        }
        if (outputPathEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(
                &configDialog,
                tr("Export Animation GIFs"),
                tr("Select an output folder first."));
            return;
        }
        configDialog.accept();
    });
    connect(configButtons, &QDialogButtonBox::rejected, &configDialog, &QDialog::reject);

    rescanAnimationRoot();
    if (configDialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString selectedSkeletonCode = skeletonCombo->currentText().trimmed().toUpper();
    const QString normalizedAnimationRoot =
        QDir::cleanPath(QDir::fromNativeSeparators(animationRootEdit->text().trimmed()));
    const QString outputDir = QDir::cleanPath(QDir::fromNativeSeparators(outputPathEdit->text().trimmed()));
    if (outputDir.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Export Animation GIFs"),
            tr("Select an output folder first."));
        return;
    }

    std::vector<AnimationGifSource> selectedSources;
    selectedSources.reserve(allDiscoveredSources.size());
    for (const AnimationGifSource& source : allDiscoveredSources) {
        if (source.metadata.skeletonCode.compare(selectedSkeletonCode, Qt::CaseInsensitive) == 0) {
            selectedSources.push_back(source);
        }
    }

    std::vector<AnimationGifReportRow> filteredDiscoverySkippedRows;
    filteredDiscoverySkippedRows.reserve(discoverySkippedRows.size());
    for (const AnimationGifReportRow& skippedRow : discoverySkippedRows) {
        const QString detectedSkeletonCode = skippedRow.detectedSkeletonCode.trimmed().toUpper();
        if (!detectedSkeletonCode.isEmpty()
            && detectedSkeletonCode.compare(selectedSkeletonCode, Qt::CaseInsensitive) != 0)
        {
            continue;
        }

        AnimationGifReportRow filteredRow = skippedRow;
        filteredRow.selectedSkeletonCode = selectedSkeletonCode;
        filteredDiscoverySkippedRows.push_back(std::move(filteredRow));
    }

    if (selectedSources.empty()) {
        QMessageBox::warning(
            this,
            tr("Export Animation GIFs"),
            tr("No animation clips matched skeleton `%1` under %2.")
                .arg(selectedSkeletonCode, QDir::toNativeSeparators(normalizedAnimationRoot)));
        return;
    }

    struct RenderExportStateSnapshot {
        std::vector<RenderSessionAsset> externalAssets;
        QSet<QString> externalAssetPaths;
        std::vector<AggregateRenderDependencyAsset> aggregateDependencyAssets;
        bool renderTriedSkeletonAutoload = false;
        QString suppressedMissingHierarchyKey;
        QString suppressedMissingMeshKey;
        QString textureDirectory;
        QString suppressedMissingTextureKey;
        QString aggregateDependencyAttemptKey;
        QString hierarchyDependencyAttemptKey;
        OW3D::Render::AnimationPlaybackState playback;
        std::unordered_map<const void*, OW3D::Render::RenderAnimationEditDraft> animationDrafts;
        std::optional<RenderAnimationClipIdentity> activeClipIdentity;
        bool animationEditKeysEnabled = false;
        RenderAnimationPrepState prepState;
        RenderAnimationBlendState blendState;
    };

    const RenderExportStateSnapshot savedState{
        currentExternalRenderAssets,
        currentExternalRenderAssetPaths,
        currentAggregateRenderDependencyAssets,
        currentRenderTriedSkeletonAutoload,
        currentRenderSuppressedMissingHierarchyKey,
        currentRenderSuppressedMissingMeshKey,
        currentRenderTextureDirectory,
        currentRenderSuppressedMissingTextureKey,
        currentRenderAggregateDependencyAttemptKey,
        currentRenderHierarchyDependencyAttemptKey,
        currentRenderAnimationPlayback,
        currentRenderAnimationDrafts,
        currentRenderActiveClipIdentity,
        currentRenderAnimationEditKeysEnabled,
        currentRenderAnimationPrepState,
        currentRenderAnimationBlendState
    };

    auto rebuildExternalPathSet = [](const std::vector<RenderSessionAsset>& assets) {
        QSet<QString> outPaths;
        for (const auto& asset : assets) {
            const QString sourceKey = RenderSessionAssetSourceKey(asset);
            if (!sourceKey.isEmpty()) {
                outPaths.insert(sourceKey);
            }
        }
        return outPaths;
    };

    auto restoreRenderState = [&]() {
        currentExternalRenderAssets = savedState.externalAssets;
        currentExternalRenderAssetPaths = savedState.externalAssetPaths;
        currentAggregateRenderDependencyAssets = savedState.aggregateDependencyAssets;
        currentRenderTriedSkeletonAutoload = savedState.renderTriedSkeletonAutoload;
        currentRenderSuppressedMissingHierarchyKey = savedState.suppressedMissingHierarchyKey;
        currentRenderSuppressedMissingMeshKey = savedState.suppressedMissingMeshKey;
        currentRenderTextureDirectory = savedState.textureDirectory;
        currentRenderSuppressedMissingTextureKey = savedState.suppressedMissingTextureKey;
        currentRenderAggregateDependencyAttemptKey = savedState.aggregateDependencyAttemptKey;
        currentRenderHierarchyDependencyAttemptKey = savedState.hierarchyDependencyAttemptKey;
        currentRenderAnimationPlayback = savedState.playback;
        currentRenderAnimationDrafts = savedState.animationDrafts;
        currentRenderActiveClipIdentity = savedState.activeClipIdentity;
        currentRenderAnimationEditKeysEnabled = savedState.animationEditKeysEnabled;
        currentRenderAnimationPrepState = savedState.prepState;
        currentRenderAnimationBlendState = savedState.blendState;
        rebuildRenderScene();
    };

    std::vector<RenderSessionAsset> exportBaseAssets;
    exportBaseAssets.reserve(savedState.externalAssets.size());
    for (const auto& asset : savedState.externalAssets) {
        if (asset.role != RenderSessionAssetRole::AnimationLibrary) {
            exportBaseAssets.push_back(asset);
        }
    }

    OW3D::Render::RenderSettings exportSettings{};
    exportSettings.profile = OW3D::Render::ParityProfile::W3DViewD3D11Baseline;
    exportSettings.enableFog = renderFogToggle ? renderFogToggle->isChecked() : true;
    exportSettings.enableLod = renderLodToggle ? renderLodToggle->isChecked() : true;
    exportSettings.lodBias = renderLodBiasSpin ? static_cast<float>(renderLodBiasSpin->value()) : 1.0f;
    exportSettings.debugShowUv = renderUvDebugToggle ? renderUvDebugToggle->isChecked() : false;
    exportSettings.lockLodLevel = renderLodLockToggle ? renderLodLockToggle->isChecked() : false;
    exportSettings.lockedLodLevel = renderLodLevelSpin ? renderLodLevelSpin->value() : 0;
    exportSettings.clearColor = { 0.86f, 0.86f, 0.86f, 1.0f };
    exportSettings.showCameraGizmo = false;
    exportSettings.showPivotMarkers = false;

    currentExternalRenderAssets = exportBaseAssets;
    currentExternalRenderAssetPaths = rebuildExternalPathSet(exportBaseAssets);
    currentRenderAnimationDrafts.clear();
    currentRenderAnimationEditKeysEnabled = false;
    currentRenderAnimationBlendState = {};
    resetRenderAnimationPlayback();
    rebuildRenderScene();

    if (!SceneHasRenderableMeshData(currentRenderSceneResult)) {
        restoreRenderState();
        QMessageBox::warning(
            this,
            tr("Export Animation GIFs"),
            tr("The current file does not build a renderable scene for export."));
        return;
    }

    int exportedCount = 0;
    int skippedCount = static_cast<int>(filteredDiscoverySkippedRows.size());
    int failedCount = 0;
    bool canceled = false;
    QString reportPath = QDir(outputDir).absoluteFilePath(QStringLiteral("report.csv"));

    if (!QDir().mkpath(outputDir)) {
        restoreRenderState();
        QMessageBox::warning(
            this,
            tr("Export Animation GIFs"),
            tr("Cannot create output directory: %1").arg(QDir::toNativeSeparators(outputDir)));
        return;
    }

    QFile reportFile(reportPath);
    if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        restoreRenderState();
        QMessageBox::warning(
            this,
            tr("Export Animation GIFs"),
            tr("Cannot write report file: %1").arg(QDir::toNativeSeparators(reportPath)));
        return;
    }

    QTextStream reportStream(&reportFile);
    WriteAnimationGifReportCsvHeader(reportStream);
    for (const AnimationGifReportRow& skippedRow : filteredDiscoverySkippedRows) {
        WriteAnimationGifReportCsvRow(reportStream, skippedRow);
    }
    reportStream.flush();

    QDialog exportDialog(this);
    exportDialog.setWindowTitle(tr("Export Animation GIFs"));
    exportDialog.setModal(true);
    auto* exportLayout = new QVBoxLayout(&exportDialog);
    auto* exportViewport = new OW3D::Render::RenderViewportWidget(&exportDialog);
    exportViewport->setFixedSize(512, 512);
    exportViewport->SetContinuousRenderingEnabled(false);
    exportViewport->SetOverlayUiEnabled(false);
    exportLayout->addWidget(exportViewport, 0, Qt::AlignCenter);
    auto* exportStatusLabel = new QLabel(tr("Preparing export..."), &exportDialog);
    exportStatusLabel->setWordWrap(true);
    exportLayout->addWidget(exportStatusLabel);
    auto* exportProgressBar = new QProgressBar(&exportDialog);
    exportProgressBar->setRange(0, static_cast<int>(selectedSources.size()));
    exportProgressBar->setValue(0);
    exportLayout->addWidget(exportProgressBar);
    auto* cancelExportButton = new QPushButton(tr("Cancel"), &exportDialog);
    exportLayout->addWidget(cancelExportButton, 0, Qt::AlignRight);

    bool cancelRequested = false;
    bool exportDialogClosing = false;
    auto requestCancel = [&]() {
        if (cancelRequested || exportDialogClosing) {
            return;
        }
        cancelRequested = true;
        cancelExportButton->setEnabled(false);
        exportStatusLabel->setText(tr("Cancel requested. Finishing the current clip..."));
    };
    connect(cancelExportButton, &QPushButton::clicked, &exportDialog, requestCancel);
    connect(&exportDialog, &QDialog::rejected, &exportDialog, requestCancel);

    exportDialog.show();
    QCoreApplication::processEvents();
    exportViewport->SetRenderSettings(exportSettings);

    exportViewport->SetSceneResult(currentRenderSceneResult);
    exportViewport->SetAnimationPlayback({});
    exportViewport->SetAnimationEditDraft(std::nullopt);
    exportViewport->SetAnimationEditingState(false, false, QString());
    if (useCurrentEditorCamera) {
        exportViewport->SetCameraState(fixedCamera);
    }
    else {
        exportViewport->FocusScene();
    }

    for (int i = 0; i < static_cast<int>(selectedSources.size()); ++i) {
        const AnimationGifSource& source = selectedSources[static_cast<std::size_t>(i)];
        exportProgressBar->setValue(i);
        exportStatusLabel->setText(tr("Exporting %1 (%2/%3)")
            .arg(source.clipStem)
            .arg(i + 1)
            .arg(static_cast<int>(selectedSources.size())));
        QCoreApplication::processEvents();
        if (cancelRequested) {
            canceled = true;
            break;
        }

        AnimationGifReportRow row;
        row.selectedSkeletonCode = selectedSkeletonCode;
        row.detectedSkeletonCode = source.metadata.skeletonCode.toUpper();
        row.clipStem = source.clipStem;
        row.clipName = source.metadata.clipName;
        row.resolvedW3dPath = QDir::toNativeSeparators(source.resolvedW3dPath);
        row.resolvedJsonPath = source.resolvedJsonPath.isEmpty()
            ? QString()
            : QDir::toNativeSeparators(source.resolvedJsonPath);
        row.hierarchyName = source.metadata.hierarchyName;
        if (source.metadata.parsed) {
            row.numFrames = QString::number(source.metadata.numFrames);
            row.frameRate = QString::number(source.metadata.frameRate, 'f', 2);
        }

        if (!source.metadata.supportedForPlayback) {
            row.status = QStringLiteral("SKIP");
            row.errorMessage = source.metadata.errorMessage.isEmpty()
                ? tr("Animation metadata indicates unsupported playback.")
                : source.metadata.errorMessage;
            ++skippedCount;
            WriteAnimationGifReportCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }
        if (source.metadata.numFrames <= 0) {
            row.status = QStringLiteral("SKIP");
            row.errorMessage = tr("The animation metadata reports zero frames.");
            ++skippedCount;
            WriteAnimationGifReportCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }
        if (source.metadata.numFrames > 1 && source.metadata.frameRate <= 0.0) {
            row.status = QStringLiteral("SKIP");
            row.errorMessage = tr("The animation metadata reports an invalid frame rate.");
            ++skippedCount;
            WriteAnimationGifReportCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }

        currentExternalRenderAssets = exportBaseAssets;
        currentExternalRenderAssetPaths = rebuildExternalPathSet(exportBaseAssets);

        RenderSessionAsset clipAsset;
        QString loadError;
        if (!tryLoadRenderSessionAsset(
            source.resolvedW3dPath,
            RenderSessionAssetRole::AnimationLibrary,
            clipAsset,
            &loadError))
        {
            row.status = QStringLiteral("FAIL");
            row.errorMessage = loadError.isEmpty()
                ? tr("Failed to load the resolved animation W3D.")
                : loadError;
            ++failedCount;
            WriteAnimationGifReportCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }
        if (clipAsset.animationCount <= 0) {
            row.status = QStringLiteral("SKIP");
            row.errorMessage = tr("Resolved source file does not contain animation data.");
            ++skippedCount;
            WriteAnimationGifReportCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }

        currentExternalRenderAssets = exportBaseAssets;
        currentExternalRenderAssets.push_back(clipAsset);
        currentExternalRenderAssetPaths = rebuildExternalPathSet(currentExternalRenderAssets);
        currentRenderAnimationDrafts.clear();
        currentRenderAnimationEditKeysEnabled = false;
        currentRenderAnimationBlendState = {};
        resetRenderAnimationPlayback();
        rebuildRenderScene();

        int animationIndex = -1;
        QString resolveError;
        if (!FindAnimationGifAnimationIndex(
            currentRenderSceneResult.scene,
            clipAsset.displayLabel,
            source,
            animationIndex,
            resolveError))
        {
            row.status = QStringLiteral("SKIP");
            row.errorMessage = resolveError;
            ++skippedCount;
            WriteAnimationGifReportCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }

        const auto& clip = currentRenderSceneResult.scene.animations[static_cast<std::size_t>(animationIndex)];
        row.hierarchyName = QString::fromStdString(clip.hierarchyName);
        row.numFrames = QString::number(clip.numFrames);
        row.frameRate = QString::number(clip.frameRate, 'f', 2);

        if (clip.numFrames == 0u) {
            row.status = QStringLiteral("SKIP");
            row.errorMessage = tr("The selected clip reports zero frames.");
            ++skippedCount;
            WriteAnimationGifReportCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }
        if (clip.numFrames > 1u && clip.frameRate <= 0.0f) {
            row.status = QStringLiteral("SKIP");
            row.errorMessage = tr("The selected clip has an invalid frame rate.");
            ++skippedCount;
            WriteAnimationGifReportCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }

        currentRenderAnimationPlayback = {};
        currentRenderAnimationPlayback.activeAnimationIndex = animationIndex;
        currentRenderAnimationPlayback.loop = true;
        currentRenderAnimationPlayback.speed = 1.0f;
        currentRenderActiveClipIdentity = BuildRenderAnimationClipIdentity(clip);
        syncRenderAnimationUi();

        exportViewport->SetSceneResult(currentRenderSceneResult);
        exportViewport->SetAnimationEditDraft(std::nullopt);
        exportViewport->SetAnimationEditingState(false, false, QString());
        if (useCurrentEditorCamera) {
            exportViewport->SetCameraState(fixedCamera);
        }
        else {
            exportViewport->FocusScene();
        }

        const QString outputGifPath = QDir(outputDir).absoluteFilePath(
            QFileInfo(source.resolvedW3dPath).completeBaseName() + QStringLiteral(".gif"));
        row.outputGifPath = QDir::toNativeSeparators(outputGifPath);

        OW3D::Gif::Writer gifWriter;
        QString gifError;
        if (!gifWriter.Open(outputGifPath, 512, 512, &gifError)) {
            row.status = QStringLiteral("FAIL");
            row.errorMessage = gifError;
            ++failedCount;
            WriteAnimationGifReportCsvRow(reportStream, row);
            reportStream.flush();
            continue;
        }

        const int effectiveFrameCount = (clip.numFrames > 1u)
            ? std::max(1, static_cast<int>(clip.numFrames) - 1)
            : 1;
        const int frameDelayCentiseconds = GifDelayCentisecondsFromClipFrameRate(clip.frameRate);

        bool clipFailed = false;
        int framesWritten = 0;
        for (int frameIndex = 0; frameIndex < effectiveFrameCount; ++frameIndex) {
            OW3D::Render::AnimationPlaybackState playback{};
            playback.activeAnimationIndex = animationIndex;
            playback.timeSeconds = (clip.frameRate > 0.0f)
                ? static_cast<float>(frameIndex) / clip.frameRate
                : 0.0f;
            playback.loop = true;
            playback.playing = false;
            playback.speed = 1.0f;

            exportViewport->SetAnimationPlayback(playback);
            exportViewport->RenderOnce();

            QImage frameImage;
            if (!exportViewport->CaptureCurrentFrame(frameImage)) {
                clipFailed = true;
                row.status = QStringLiteral("FAIL");
                row.errorMessage = tr("Failed to capture a rendered frame.");
                ++failedCount;
                break;
            }
            if (!gifWriter.AddFrame(frameImage, frameDelayCentiseconds, &gifError)) {
                clipFailed = true;
                row.status = QStringLiteral("FAIL");
                row.errorMessage = gifError;
                ++failedCount;
                break;
            }

            ++framesWritten;
            QCoreApplication::processEvents();
            if (cancelRequested) {
                canceled = true;
            }
        }

        QString closeError;
        if (!gifWriter.Close(&closeError) && !clipFailed) {
            clipFailed = true;
            row.status = QStringLiteral("FAIL");
            row.errorMessage = closeError;
            ++failedCount;
        }

        if (clipFailed) {
            QFile::remove(outputGifPath);
        }
        else if (framesWritten <= 0) {
            row.status = QStringLiteral("SKIP");
            row.errorMessage = tr("No frames were written for the selected clip.");
            ++skippedCount;
            QFile::remove(outputGifPath);
        }
        else {
            row.status = QStringLiteral("EXPORTED");
            ++exportedCount;
        }

        WriteAnimationGifReportCsvRow(reportStream, row);
        reportStream.flush();

        if (cancelRequested) {
            canceled = true;
            break;
        }
    }

    exportProgressBar->setValue(static_cast<int>(selectedSources.size()));
    reportFile.close();
    exportDialogClosing = true;
    exportDialog.close();

    restoreRenderState();
    lastDirectory = outputDir;

    const QString summary = tr(
        "%1\n\nSkeleton: %2\nMatching clips: %3\nExported: %4\nSkipped: %5\nFailed: %6\nReport: %7")
        .arg(canceled ? tr("Export canceled.") : tr("Export completed."))
        .arg(selectedSkeletonCode)
        .arg(static_cast<int>(selectedSources.size()))
        .arg(exportedCount)
        .arg(skippedCount)
        .arg(failedCount)
        .arg(QDir::toNativeSeparators(reportPath));

    if (canceled || failedCount > 0) {
        QMessageBox::warning(this, tr("Export Animation GIFs"), summary);
    }
    else {
        QMessageBox::information(this, tr("Export Animation GIFs"), summary);
    }
}


void MainWindow::exportJson() {
    QString path = QFileDialog::getSaveFileName(this, tr("Export to JSON"), lastDirectory, tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty()) return;
    JsonSerializationMode selectedMode = JsonSerializationMode::StructuredPreferred;
    if (!promptSerializationMode(tr("Export to JSON"), tr("Serialization mode"), selectedMode)) {
        return;
    }

    const ordered_json doc = chunkData->toJson(selectedMode);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot write JSON file."));
        return;
    }
    const QByteArray payload = QByteArray::fromStdString(doc.dump(4));
    file.write(payload);
    file.close();
}

void MainWindow::importJson() {
    if (!confirmDiscardChanges()) return;

    QString path = QFileDialog::getOpenFileName(this, tr("Import from JSON"), lastDirectory, tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open JSON file."));
        return;
    }
    const QByteArray data = file.readAll();
    file.close();

    ordered_json doc;
    try {
        doc = ordered_json::parse(data.constBegin(), data.constEnd());
    }
    catch (const nlohmann::json::exception& e) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid JSON content: %1").arg(QString::fromUtf8(e.what())));
        return;
    }

    std::vector<std::string> importWarnings;
    try {
        if (!chunkData->fromJson(doc, &importWarnings)) {
            QMessageBox::warning(this, tr("Error"), tr("Invalid JSON content."));
            return;
        }
    }
    catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid JSON content: %1").arg(QString::fromUtf8(e.what())));
        return;
    }
    clearArchiveRenderContext();
    currentFilePath.clear();
    ClearChunkTree();
    updateWindowTitle();
    setDirty(true);
    lastDirectory = QFileInfo(path).absolutePath();
    populateTree();
    rebuildRenderScene();
    if (renderViewport) {
        renderViewport->FocusScene();
    }
    if (!importWarnings.empty()) {
        QStringList preview;
        const std::size_t previewCount = std::min<std::size_t>(importWarnings.size(), 10);
        preview.reserve(static_cast<int>(previewCount));
        for (std::size_t i = 0; i < previewCount; ++i) {
            preview << QString::fromStdString(importWarnings[i]);
        }
        if (importWarnings.size() > previewCount) {
            preview << tr("... (%1 additional warnings)").arg(static_cast<int>(importWarnings.size() - previewCount));
        }

        QMessageBox::information(
            this,
            tr("JSON Import Warnings"),
            tr("Import completed with %1 warning(s).\n\n%2")
                .arg(static_cast<int>(importWarnings.size()))
                .arg(preview.join("\n")));
    }
    if (QMessageBox::question(this, tr("Rebuild"), tr("Save rebuilt W3D file?"),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        QString out = QFileDialog::getSaveFileName(this, tr("Save W3D File"), lastDirectory, tr("W3D Files (*.w3d);;All Files (*)"));
        if (!out.isEmpty()) {
            SyncHLodCountsForSave(chunkData.get());
            SyncPureAnimationHeaderNameForSave(chunkData.get(), out);
            if (!chunkData->saveToFile(out.toStdString())) {
                QMessageBox::warning(this, tr("Error"), tr("Failed to save W3D file."));
            }
            else {
                currentFilePath = out;
                AddRecentFile(out);
                lastDirectory = QFileInfo(out).absolutePath();
                setDirty(false);
            }
        }
    }
}
