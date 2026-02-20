
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
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QGridLayout>
#include <QKeySequence>
#include <QDialog>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
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
#include <QCloseEvent>
#include "backend/W3DMesh.h"
#include "backend/W3DStructs.h"
#include "backend/ChunkMutators.h"
#include "backend/ParseUtils.h"
#include "EditorWidgets.h"
#include <map>
#include <nlohmann/json.hpp>
#include <array>
#include <vector>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <exception>
#include <variant>
#include <type_traits>
#include <optional>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <cctype>
#include <QSignalBlocker>

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

constexpr int kMeshNameMax = static_cast<int>(W3D_NAME_LEN) - 1;
constexpr int kPivotNameMax = static_cast<int>(W3D_NAME_LEN) - 1;

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
        std::function<void*(const QString&)> resolver,
        PivotRenameHandler renameHandler,
        QWidget* parent = nullptr)
        : QDialog(parent)
        , hierarchies(data)
        , onMeshActivated(std::move(meshHandler))
        , resolveChunk(std::move(resolver))
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
        if (!ptr && resolveChunk) {
            const QString name = item->data(0, RoleName).toString();
            ptr = resolveChunk(name);
            if (ptr) {
                item->setData(0, Qt::UserRole, QVariant::fromValue<void*>(ptr));
            }
        }
        if (ptr && onMeshActivated) {
            onMeshActivated(const_cast<void*>(ptr));
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
    std::function<void*(const QString&)> resolveChunk;
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
    setEnabled(false);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* form = new QFormLayout();
    meshNameEdit = new QLineEdit(this);
    meshNameEdit->setMaxLength(kMeshNameMax);
    containerNameEdit = new QLineEdit(this);
    containerNameEdit->setMaxLength(kMeshNameMax);
    form->addRow(tr("Mesh Name"), meshNameEdit);
    form->addRow(tr("Container Name"), containerNameEdit);
    layout->addLayout(form);

    auto* flagGroup = new QGroupBox(tr("Mesh Flags"), this);
    auto* flagLayout = new QGridLayout(flagGroup);
    const std::array<std::pair<MeshAttr, const char*>, 7> kFlags = { {
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_PHYSICAL,   "Physical" },
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_PROJECTILE, "Projectile" },
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_VIS,        "Visibility" },
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_CAMERA,     "Camera" },
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_VEHICLE,    "Vehicle" },
        { MeshAttr::W3D_MESH_FLAG_HIDDEN,                    "Hide" },
        { MeshAttr::W3D_MESH_FLAG_TWO_SIDED,                 "2-Sided" },
    } };
    int row = 0;
    for (const auto& [flag, label] : kFlags) {
        auto* check = new QCheckBox(QString::fromLatin1(label), flagGroup);
        flagLayout->addWidget(check, row / 2, row % 2);
        flagControls.push_back({ MeshAttrValue(flag), check });
        ++row;
    }
    layout->addWidget(flagGroup);

    applyButton = new QPushButton(tr("Apply Mesh Changes"), this);
    connect(applyButton, &QPushButton::clicked,
        this, &MeshEditorWidget::applyChanges);

    layout->addWidget(applyButton, 0, Qt::AlignRight);
    layout->addStretch();
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
    setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* headerForm = new QFormLayout();
    shaderIndexCombo = new QComboBox(this);
    headerForm->addRow(tr("Shader"), shaderIndexCombo);
    layout->addLayout(headerForm);

    auto* form = new QFormLayout();

    depthCompareCombo = new QComboBox(this);
    PopulateEnumCombo<DepthCompare>(depthCompareCombo);
    form->addRow(tr("Depth Compare"), depthCompareCombo);

    depthMaskCombo = new QComboBox(this);
    PopulateEnumCombo<DepthMask>(depthMaskCombo);
    form->addRow(tr("Depth Mask"), depthMaskCombo);

    destBlendCombo = new QComboBox(this);
    PopulateEnumCombo<DestBlend>(destBlendCombo);
    form->addRow(tr("Dest Blend"), destBlendCombo);

    priGradientCombo = new QComboBox(this);
    PopulateEnumCombo<PriGradient>(priGradientCombo);
    form->addRow(tr("Primary Gradient"), priGradientCombo);

    secGradientCombo = new QComboBox(this);
    PopulateEnumCombo<SecGradient>(secGradientCombo);
    form->addRow(tr("Secondary Gradient"), secGradientCombo);

    srcBlendCombo = new QComboBox(this);
    PopulateEnumCombo<SrcBlend>(srcBlendCombo);
    form->addRow(tr("Source Blend"), srcBlendCombo);

    texturingCombo = new QComboBox(this);
    PopulateEnumCombo<Texturing>(texturingCombo);
    form->addRow(tr("Texturing"), texturingCombo);

    detailColorCombo = new QComboBox(this);
    PopulateEnumCombo<DetailColorFunc>(detailColorCombo);
    form->addRow(tr("Detail Color Func"), detailColorCombo);

    detailAlphaCombo = new QComboBox(this);
    PopulateEnumCombo<DetailAlphaFunc>(detailAlphaCombo);
    form->addRow(tr("Detail Alpha Func"), detailAlphaCombo);

    alphaTestCombo = new QComboBox(this);
    PopulateEnumCombo<AlphaTest>(alphaTestCombo);
    form->addRow(tr("Alpha Test"), alphaTestCombo);

    postDetailColorCombo = new QComboBox(this);
    PopulateEnumCombo<DetailColorFunc>(postDetailColorCombo);
    form->addRow(tr("Post Detail Color"), postDetailColorCombo);

    postDetailAlphaCombo = new QComboBox(this);
    PopulateEnumCombo<DetailAlphaFunc>(postDetailAlphaCombo);
    form->addRow(tr("Post Detail Alpha"), postDetailAlphaCombo);

    colorMaskSpin = new QSpinBox(this);
    colorMaskSpin->setRange(0, 255);
    form->addRow(tr("Color Mask"), colorMaskSpin);

    fogFuncSpin = new QSpinBox(this);
    fogFuncSpin->setRange(0, 255);
    form->addRow(tr("Fog Func"), fogFuncSpin);

    shaderPresetSpin = new QSpinBox(this);
    shaderPresetSpin->setRange(0, 255);
    form->addRow(tr("Shader Preset"), shaderPresetSpin);

    layout->addLayout(form);

    applyButton = new QPushButton(tr("Apply Shader Changes"), this);
    layout->addWidget(applyButton, 0, Qt::AlignRight);
    layout->addStretch();

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

TransformNodeEditorWidget::TransformNodeEditorWidget(QWidget* parent)
    : QWidget(parent) {
    setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* form = new QFormLayout();
    fileNameEdit = new QLineEdit(this);
    form->addRow(tr("Linked W3D File"), fileNameEdit);
    layout->addLayout(form);

    applyButton = new QPushButton(tr("Apply"), this);
    connect(applyButton, &QPushButton::clicked,
        this, &TransformNodeEditorWidget::applyChanges);
    layout->addWidget(applyButton, 0, Qt::AlignRight);
    layout->addStretch();
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
    setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* form = new QFormLayout();
    surfaceTypeCombo = new QComboBox(this);

    for (uint32_t i = 0; i <= 255; ++i) {
        const char* name = SurfaceTypeName(i);
        if (!name) continue;
        surfaceTypeCombo->addItem(QString::fromLatin1("%1 (%2)").arg(QString::fromLatin1(name)).arg(i),
            static_cast<int>(i));
    }

    form->addRow(tr("Surface Type"), surfaceTypeCombo);
    layout->addLayout(form);

    applyButton = new QPushButton(tr("Apply"), this);
    connect(applyButton, &QPushButton::clicked,
        this, &SurfaceTypeEditorWidget::applyChanges);

    layout->addWidget(applyButton, 0, Qt::AlignRight);
    layout->addStretch();
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
    setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* form = new QFormLayout();
    fromCombo = new QComboBox(this);
    toCombo = new QComboBox(this);

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

    form->addRow(tr("Replace"), fromCombo);
    form->addRow(tr("With"), toCombo);
    layout->addLayout(form);

    statsLabel = new QLabel(this);
    statsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(statsLabel);

    applyButton = new QPushButton(tr("Apply"), this);
    connect(applyButton, &QPushButton::clicked,
        this, &TriangleSurfaceTypeEditorWidget::applyChanges);
    connect(fromCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &TriangleSurfaceTypeEditorWidget::updateStats);

    layout->addWidget(applyButton, 0, Qt::AlignRight);
    layout->addStretch();
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

MaterialEditorWidget::MaterialEditorWidget(QWidget* parent)
    : QWidget(parent) {
    setEnabled(false);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* flagsGroup = new QGroupBox(tr("Basic Flags"), this);
    auto* flagsLayout = new QVBoxLayout(flagsGroup);
    for (const auto& [mask, name] : VERTMAT_BASIC_FLAGS) {
        auto* box = new QCheckBox(QString::fromLatin1(name.data(), static_cast<int>(name.size())), flagsGroup);
        flagsLayout->addWidget(box);
        basicFlagControls.push_back({ static_cast<uint32_t>(mask), box });
    }
    layout->addWidget(flagsGroup);

    auto* stageGroup = new QGroupBox(tr("Stage Mapping"), this);
    auto* stageLayout = new QGridLayout(stageGroup);
    stage0Combo = new QComboBox(stageGroup);
    stage1Combo = new QComboBox(stageGroup);
    stage0CodeSpin = new QSpinBox(stageGroup);
    stage1CodeSpin = new QSpinBox(stageGroup);
    populateStageCombo(stage0Combo, 0);
    populateStageCombo(stage1Combo, 1);
    for (auto* spin : { stage0CodeSpin, stage1CodeSpin }) {
        spin->setRange(0, 255);
        spin->setSingleStep(1);
    }

    stageLayout->addWidget(new QLabel(tr("Mapping"), stageGroup), 0, 1);
    stageLayout->addWidget(new QLabel(tr("Code"), stageGroup), 0, 2);
    stageLayout->addWidget(new QLabel(tr("Stage 0"), stageGroup), 1, 0);
    stageLayout->addWidget(stage0Combo, 1, 1);
    stageLayout->addWidget(stage0CodeSpin, 1, 2);
    stageLayout->addWidget(new QLabel(tr("Stage 1"), stageGroup), 2, 0);
    stageLayout->addWidget(stage1Combo, 2, 1);
    stageLayout->addWidget(stage1CodeSpin, 2, 2);
    stageLayout->setColumnStretch(1, 1);
    layout->addWidget(stageGroup);

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

    auto makeColorGroup = [&](const QString& title, ColorControls& controls) {
        auto* group = new QGroupBox(title, this);
        auto* grid = new QGridLayout(group);
        controls.r = new QSpinBox(group);
        controls.g = new QSpinBox(group);
        controls.b = new QSpinBox(group);
        for (auto* spin : { controls.r, controls.g, controls.b }) {
            spin->setRange(0, 255);
        }
        grid->addWidget(new QLabel(tr("R"), group), 0, 0);
        grid->addWidget(controls.r, 0, 1);
        grid->addWidget(new QLabel(tr("G"), group), 0, 2);
        grid->addWidget(controls.g, 0, 3);
        grid->addWidget(new QLabel(tr("B"), group), 0, 4);
        grid->addWidget(controls.b, 0, 5);
        layout->addWidget(group);
    };
    makeColorGroup(tr("Ambient"), ambient);
    makeColorGroup(tr("Diffuse"), diffuse);
    makeColorGroup(tr("Specular"), specular);
    makeColorGroup(tr("Emissive"), emissive);

    auto* floatForm = new QFormLayout();
    shininessSpin = new QDoubleSpinBox(this);
    shininessSpin->setRange(0.0, 1000.0);
    shininessSpin->setDecimals(2);
    shininessSpin->setSingleStep(1.0);

    opacitySpin = new QDoubleSpinBox(this);
    opacitySpin->setRange(0.0, 1.0);
    opacitySpin->setDecimals(3);
    opacitySpin->setSingleStep(0.05);

    translucencySpin = new QDoubleSpinBox(this);
    translucencySpin->setRange(0.0, 1.0);
    translucencySpin->setDecimals(3);
    translucencySpin->setSingleStep(0.05);

    floatForm->addRow(tr("Shininess"), shininessSpin);
    floatForm->addRow(tr("Opacity"), opacitySpin);
    floatForm->addRow(tr("Translucency"), translucencySpin);
    layout->addLayout(floatForm);

    applyButton = new QPushButton(tr("Apply Material Changes"), this);
    connect(applyButton, &QPushButton::clicked,
        this, &MaterialEditorWidget::applyChanges);

    layout->addWidget(applyButton, 0, Qt::AlignRight);
    layout->addStretch();
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


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* openAction = fileMenu->addAction(tr("&Open"));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() {
        openFile("");
        });
    QAction* exportJsonAct = fileMenu->addAction("Export to JSON...");
    connect(exportJsonAct, &QAction::triggered, this, &MainWindow::exportJson);
    QAction* importJsonAct = fileMenu->addAction("Import from JSON...");
    connect(importJsonAct, &QAction::triggered, this, &MainWindow::importJson);

    QAction* saveAction = fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);

    QAction* saveAsAction = fileMenu->addAction(tr("Save &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);

    splitter = new QSplitter(this);

    treeWidget = new QTreeWidget(splitter);
    treeWidget->setHeaderLabel(tr("Chunk Tree"));
    treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(treeWidget, &QTreeWidget::itemSelectionChanged, this, &MainWindow::handleTreeSelection);

    auto* detailContainer = new QWidget(splitter);
    auto* detailLayout = new QVBoxLayout(detailContainer);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(0);

    detailSplitter = new QSplitter(Qt::Vertical, detailContainer);
    detailSplitter->setChildrenCollapsible(true);
    detailSplitter->setCollapsible(0, false);
    detailSplitter->setCollapsible(1, true);
    detailSplitter->setStretchFactor(0, 3);
    detailSplitter->setStretchFactor(1, 1);
    detailLayout->addWidget(detailSplitter, 1);

    auto* tableContainer = new QWidget(detailSplitter);
    auto* tableLayout = new QVBoxLayout(tableContainer);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tableLayout->setSpacing(6);

    rawHexToggle = new QCheckBox(tr("Show raw hex"), tableContainer);
    tableLayout->addWidget(rawHexToggle);

    tableWidget = new QTableWidget(tableContainer);
    tableWidget->setColumnCount(3);
    tableWidget->setHorizontalHeaderLabels({ tr("Field"), tr("Type"), tr("Value") });
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableLayout->addWidget(tableWidget, 1);

    rawHexContainer = new QGroupBox(tr("Raw Hex"), tableContainer);
    auto* rawHexLayout = new QVBoxLayout(rawHexContainer);
    rawHexEdit = new QPlainTextEdit(rawHexContainer);
    rawHexEdit->setReadOnly(true);
    rawHexLayout->addWidget(rawHexEdit);
    rawHexContainer->setVisible(false);
    tableLayout->addWidget(rawHexContainer);

    detailSplitter->addWidget(tableContainer);

    editorScrollArea = new QScrollArea(detailSplitter);
    editorScrollArea->setWidgetResizable(true);
    editorScrollArea->setFrameShape(QFrame::NoFrame);
    detailSplitter->addWidget(editorScrollArea);

    editorStack = new QStackedWidget();
    editorStack->setContentsMargins(0, 0, 0, 0);
    editorScrollArea->setWidget(editorStack);
    editorPlaceholder = new QWidget(editorStack);
    auto* placeholderLayout = new QVBoxLayout(editorPlaceholder);
    placeholderLayout->addStretch();
    auto* placeholderLabel = new QLabel(tr("Select a mesh, texture, or material chunk to edit."), editorPlaceholder);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLayout->addWidget(placeholderLabel);
    placeholderLayout->addStretch();
    editorStack->addWidget(editorPlaceholder);

    meshEditor = new MeshEditorWidget(editorStack);
    editorStack->addWidget(meshEditor);

    textureNameEditor = new StringEditorWidget(tr("Texture Name"), editorStack);
    editorStack->addWidget(textureNameEditor);

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
    detailSplitter->setSizes({ 800, 180 });
    detailSplitterStateCache = detailSplitter->saveState();
    editorScrollArea->setVisible(false);

    splitter->addWidget(treeWidget);
    splitter->addWidget(detailContainer);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setChildrenCollapsible(false);
    splitter->setSizes({ 1120, 400 }); // approx 75% tree / 25% editor starting layout

    setCentralWidget(splitter);

    chunkData = std::make_unique<ChunkData>();

    connect(meshEditor, &MeshEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(meshEditor, &MeshEditorWidget::meshRenamed, this, &MainWindow::onMeshRenamed);
    connect(textureNameEditor, &StringEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(materialNameEditor, &StringEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(transformNodeEditor, &TransformNodeEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(stage0ArgsEditor, &MapperArgsEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(stage1ArgsEditor, &MapperArgsEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(materialEditor, &MaterialEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(shaderEditor, &ShaderEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(surfaceTypeEditor, &SurfaceTypeEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(triangleSurfaceTypeEditor, &TriangleSurfaceTypeEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(rawHexToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (rawHexContainer) rawHexContainer->setVisible(on);
        updateRawHex(currentChunk);
        });

    updateWindowTitle();
    recentFilesPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/recent_files.txt";
    recentFilesMenu = fileMenu->addMenu("Open Recent");
    LoadRecentFiles();
    if (lastDirectory.isEmpty())
        lastDirectory = QDir::homePath();
    UpdateRecentFilesMenu();
    // create the menu & action
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* addTopLevelChunkAction = editMenu->addAction(tr("Add Top-Level Chunk..."));
    QAction* insertChunkBeforeAction = editMenu->addAction(tr("Insert Chunk Before..."));
    QAction* insertChunkAfterAction = editMenu->addAction(tr("Insert Chunk After..."));
    QAction* addChildChunkAction = editMenu->addAction(tr("Add Child Chunk..."));
    editMenu->addSeparator();
    QAction* moveChunkUpAction = editMenu->addAction(tr("Move Chunk Up"));
    QAction* moveChunkDownAction = editMenu->addAction(tr("Move Chunk Down"));
    QAction* deleteChunkAction = editMenu->addAction(tr("Delete Selected Chunk"));
    editMenu->addSeparator();
    QAction* moveHierarchyBoneToEndAction = editMenu->addAction(tr("Move Hierarchy Bone To End..."));

    connect(addTopLevelChunkAction, &QAction::triggered, this, &MainWindow::addTopLevelChunk);
    connect(insertChunkBeforeAction, &QAction::triggered, this, &MainWindow::insertChunkBefore);
    connect(insertChunkAfterAction, &QAction::triggered, this, &MainWindow::insertChunkAfter);
    connect(addChildChunkAction, &QAction::triggered, this, &MainWindow::addChildChunk);
    connect(moveChunkUpAction, &QAction::triggered, this, &MainWindow::moveChunkUp);
    connect(moveChunkDownAction, &QAction::triggered, this, &MainWindow::moveChunkDown);
    connect(deleteChunkAction, &QAction::triggered, this, &MainWindow::deleteSelectedChunk);
    connect(moveHierarchyBoneToEndAction, &QAction::triggered, this, &MainWindow::moveHierarchyBoneToEnd);

    QMenu* viewMenu = menuBar()->addMenu("&View");
    QAction* expandAllAction = viewMenu->addAction("Expand All");
    QAction* collapseAllAction = viewMenu->addAction("Collapse All");
    connect(expandAllAction, &QAction::triggered, treeWidget, &QTreeWidget::expandAll);
    connect(collapseAllAction, &QAction::triggered, treeWidget, &QTreeWidget::collapseAll);
    QAction* hierarchyBrowserAction = viewMenu->addAction(tr("Hierarchy Browser..."));
    connect(hierarchyBrowserAction, &QAction::triggered, this, &MainWindow::showHierarchyBrowser);
    auto batchMenu = menuBar()->addMenu(tr("Batch Tools"));
    auto exportChunksAct = new QAction(tr("Export Chunk List..."), this);
    batchMenu->addAction(exportChunksAct);
    connect(exportChunksAct, &QAction::triggered,
        this, &MainWindow::on_actionExportChunkList_triggered);
    auto exportJsonBatchAct = new QAction(tr("Export JSON Batch..."), this);
    batchMenu->addAction(exportJsonBatchAct);
    connect(exportJsonBatchAct, &QAction::triggered,
        this, &MainWindow::on_actionExportJsonBatch_triggered);
    auto validateRoundTripBatchAct = new QAction(tr("Round-Trip Validate Batch..."), this);
    batchMenu->addAction(validateRoundTripBatchAct);
    connect(validateRoundTripBatchAct, &QAction::triggered,
        this, &MainWindow::on_actionValidateRoundTripBatch_triggered);

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
    }
}


void MainWindow::openFile(const QString& path) {
    if (!confirmDiscardChanges()) return;

    QString filePath = path;
    if (filePath.isEmpty()) {
        QString startDir = lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory;
        filePath = QFileDialog::getOpenFileName(this, "Open W3D File", startDir, "W3D Files (*.w3d);;All Files (*)");
        if (filePath.isEmpty()) return;
    }

    if (!chunkData->loadFromFile(filePath.toStdString())) {
        QMessageBox::warning(this, "Error", "Failed to open file.");
        return;
    }

    ClearChunkTree();
    currentFilePath = filePath;
    setDirty(false);
    clearDetails();
    populateTree();
    AddRecentFile(filePath);
    lastDirectory = QFileInfo(filePath).absolutePath();
     
}



void MainWindow::populateTree() {
    treeWidget->clear();

    const auto& chunks = chunkData->getChunks();
    for (const auto& chunk : chunks) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        
        QString label = QString("0x%1 (%2)")
            .arg(chunk->id, 0, 16)
            .arg(QString::fromStdString(LabelForChunk(chunk->id, chunk.get())));


        item->setText(0, QString("%1 (size %2)").arg(label).arg(chunk->length));
        item->setData(0, Qt::UserRole, QVariant::fromValue<void*>(chunk.get()));
        treeWidget->addTopLevelItem(item);

        std::function<void(QTreeWidgetItem*, const std::shared_ptr<ChunkItem>&)> addChildren;
        addChildren = [&](QTreeWidgetItem* parent, const std::shared_ptr<ChunkItem>& current) {
            
            
            
            // only suppress microchunks under the 0x0100 definition node
            constexpr uint32_t SOUND_RENDER_DEF = 0x0100;
            constexpr uint32_t SOUNDROBJ_DEFINITION = 0x0A02;
			constexpr uint32_t SOUNDROBJ_DEFINITION_EXT = 0x0200;
            // 1) if *I* am a 0x0100 under 0x0A02 or under its EXT, stop right here:
            if (current->id == SOUND_RENDER_DEF
                && current->parent
                && (current->parent->id == SOUNDROBJ_DEFINITION
                    || current->parent->id == SOUNDROBJ_DEFINITION_EXT))
            {
                return;
            }

            // 2) if *my parent* is a 0x0100 under 0x0A02 or under its EXT, also stop:
            if (current->parent
                && (current->parent->id == SOUND_RENDER_DEF
                    && current->parent->parent
                    && (current->parent->parent->id == SOUNDROBJ_DEFINITION
                        || current->parent->parent->id == SOUNDROBJ_DEFINITION_EXT)))
            {
                return;
            }

            // otherwise, recurse
            for (auto& child : current->children) {
                QTreeWidgetItem* childItem = new QTreeWidgetItem(parent);
                QString lbl = QString("0x%1 (%2)")
                    .arg(child->id, 0, 16)
                    .arg(QString::fromStdString(LabelForChunk(child->id, child.get())));
                childItem->setText(0, QString("%1 (size %2)").arg(lbl).arg(child->length));
                childItem->setData(0, Qt::UserRole,
                    QVariant::fromValue<void*>(child.get()));
                addChildren(childItem, child);
            }
            };

        addChildren(item, chunk);
    }

    treeWidget->collapseAll();
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

    const std::shared_ptr<ChunkItem> target = FindChunkByPtr(chunkData->getChunks(), targetPtr);

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
            for (const auto& r : chunkData->getChunks()) {
                if (tryFindFlavor(r)) break;
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

void MainWindow::saveFile() {
    if (!chunkData || chunkData->getChunks().empty()) {
        return;
    }

    if (currentFilePath.isEmpty()) {
        saveFileAs();
        return;
    }

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

    if (!chunkData->saveToFile(filePath.toStdString())) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to save file."));
        return;
    }

    currentFilePath = filePath;
    AddRecentFile(filePath);
    lastDirectory = QFileInfo(filePath).absolutePath();
    setDirty(false);
}

void MainWindow::onChunkEdited() {
    setDirty(true);
    handleTreeSelection();
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

    auto renameFullName = [&](const QString& current) -> QString {
        const int dot = current.indexOf(QLatin1Char('.'));
        if (dot < 0) {
            if (containerChanged && hasOldContainer && current == oldContainerName) {
                return newContainerName;
            }
            if (!hasOldContainer && meshChanged && current == oldMeshName) {
                return newMeshName;
            }
            return current;
        }

        const QString containerPart = current.left(dot);
        const QString objectPart = current.mid(dot + 1);
        if (!hasOldContainer || containerPart != oldContainerName) {
            return current;
        }

        QString updatedContainer = containerPart;
        QString updatedObject = objectPart;
        if (containerChanged) {
            updatedContainer = newContainerName;
        }
        if (meshChanged && objectPart == oldMeshName) {
            updatedObject = newMeshName;
        }
        return updatedContainer + QLatin1Char('.') + updatedObject;
        };

    std::function<void(const std::shared_ptr<ChunkItem>&)> dfs =
        [&](const std::shared_ptr<ChunkItem>& node) {
        if (!node) return;

        if (containerChanged && hasOldContainer && node->id == 0x001F) { // W3D_CHUNK_MESH_HEADER3
            auto parsed = ParseChunkStruct<W3dMeshHeader3Struct>(node);
            if (auto header = std::get_if<W3dMeshHeader3Struct>(&parsed)) {
                const QString containerName = ReadFixedString(header->ContainerName, W3D_NAME_LEN);
                if (containerName == oldContainerName) {
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

            if (containerChanged && headerChunk && modelName == oldContainerName) {
                (void)W3DEdit::MutateStructChunk<W3dHModelHeaderStruct>(
                    headerChunk,
                    [&](W3dHModelHeaderStruct& header) {
                        if (ReadFixedString(header.Name, W3D_NAME_LEN) == oldContainerName) {
                            W3DEdit::WriteFixedString(header.Name, W3D_NAME_LEN, newContainerName.toStdString());
                        }
                    });
            }

            if (modelName == oldContainerName) {
                if (meshChanged) {
                    for (const auto& child : node->children) {
                        if (child->id != 0x0302 && child->id != 0x0303
                            && child->id != 0x0304 && child->id != 0x0306) {
                            continue;
                        }

                        auto parsed = ParseChunkStruct<W3dHModelNodeStruct>(child);
                        if (auto nodeStruct = std::get_if<W3dHModelNodeStruct>(&parsed)) {
                            const QString renderName = ReadFixedString(nodeStruct->RenderObjName, W3D_NAME_LEN);
                            if (renderName == oldMeshName) {
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
        case 0x0701: { // W3D_CHUNK_HLOD_HEADER
            if (!containerChanged || !hasOldContainer) break;
            auto parsed = ParseChunkStruct<W3dHLodHeaderStruct>(node);
            if (auto header = std::get_if<W3dHLodHeaderStruct>(&parsed)) {
                const QString currentName = ReadFixedString(header->Name, W3D_NAME_LEN);
                if (currentName == oldContainerName) {
                    (void)W3DEdit::MutateStructChunk<W3dHLodHeaderStruct>(
                        node,
                        [&](W3dHLodHeaderStruct& target) {
                            if (ReadFixedString(target.Name, W3D_NAME_LEN) == oldContainerName) {
                                W3DEdit::WriteFixedString(target.Name, W3D_NAME_LEN, newContainerName.toStdString());
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
    textureNameEditor->setChunk(nullptr);
    materialNameEditor->setChunk(nullptr);
    transformNodeEditor->setChunk(nullptr);
    stage0ArgsEditor->setChunk(nullptr);
    stage1ArgsEditor->setChunk(nullptr);
    materialEditor->setChunk(nullptr);
    shaderEditor->setChunk(nullptr);
    surfaceTypeEditor->setChunk(nullptr);
    triangleSurfaceTypeEditor->setChunk(nullptr);

    if (!chunk) {
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

    switch (chunk->id) {
    case 0x001F: // W3D_CHUNK_MESH_HEADER3
        meshEditor->setChunk(chunk);
        editorStack->setCurrentWidget(meshEditor);
        showEditor();
        break;
    case 0x0032: // W3D_CHUNK_TEXTURE_NAME
        textureNameEditor->setChunk(chunk);
        editorStack->setCurrentWidget(textureNameEditor);
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

void MainWindow::setDirty(bool value) {
    if (dirty == value) return;
    dirty = value;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
    QString title = tr("oW3DEdit");
    if (!currentFilePath.isEmpty()) {
        title += QStringLiteral(" - ") + QFileInfo(currentFilePath).fileName();
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
    if (!chunkPtr || !treeWidget) return;

    std::function<QTreeWidgetItem * (QTreeWidgetItem*)> dfs =
        [&](QTreeWidgetItem* item) -> QTreeWidgetItem* {
        if (!item) return nullptr;
        if (item->data(0, Qt::UserRole).value<void*>() == chunkPtr) {
            return item;
        }
        for (int i = 0; i < item->childCount(); ++i) {
            if (auto* r = dfs(item->child(i))) return r;
        }
        return nullptr;
        };

    QTreeWidgetItem* found = nullptr;
    for (int i = 0; i < treeWidget->topLevelItemCount() && !found; ++i) {
        found = dfs(treeWidget->topLevelItem(i));
    }

    if (found) {
        treeWidget->setCurrentItem(found);
        treeWidget->scrollToItem(found);
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
        [meshIndex](const QString& name) -> void* {
            const std::string key = NormalizeName(name.toStdString());
            auto range = meshIndex.equal_range(key);
            if (range.first != range.second) {
                return range.first->second.get();
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

    setDirty(true);
    populateTree();
    selectChunkInTree(newChunk.get());
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

    setDirty(true);
    populateTree();
    selectChunkInTree(newChunk.get());
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

    setDirty(true);
    populateTree();
    selectChunkInTree(newChunk.get());
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

    setDirty(true);
    populateTree();
    selectChunkInTree(newChunk.get());
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

    setDirty(true);
    populateTree();
    if (nextSelection) {
        selectChunkInTree(nextSelection);
    }
    else {
        clearDetails();
    }
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

    setDirty(true);
    populateTree();
    selectChunkInTree(selectedPtr);
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

    setDirty(true);
    populateTree();
    selectChunkInTree(selectedPtr);
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
    treeWidget->clear();
    clearDetails();
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
    // 1) pick source folder
    QString srcDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select W3D Directory"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (srcDir.isEmpty()) return;

    // 2) pick output file
    QString outPath = QFileDialog::getSaveFileName(
        this,
        tr("Save Chunk List As..."),
        QString(),
        tr("Text Files (*.txt);;All Files (*)")
    );
    if (outPath.isEmpty()) return;

    // 3) open output file
    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Cannot write to %1").arg(outPath));
        return;
    }
    QTextStream txt(&file);

    // 4) iterate .w3d files and collect counts
    std::map<uint32_t, int> counts;
    QDir dir(srcDir);
    auto w3dFiles = dir.entryList(QStringList{ "*.w3d", "*.W3D", "*.wlt" },
        QDir::Files | QDir::NoSymLinks);
    for (auto& fn : w3dFiles) {
        QString full = dir.absoluteFilePath(fn);
        txt << "=== " << full << " ===\n";

        ChunkData cd;
        if (!cd.loadFromFile(full.toStdString())) {
            txt << "[ parse error ]\n\n";
            continue;
        }
        // for each top chunk, recurse
        for (auto& top : cd.getChunks()) {
            recursePrint(top, 1, txt, counts);
        }
        txt << "\n";
    }
    // 5) Output totals
    txt << "=== Chunk Type Totals ===\n";
    for (const auto& [id, count] : counts) {
        txt << QString("0x%1 ").arg(id, 8, 16, QChar('0')).toUpper()
            << QString::fromStdString(LabelForChunk(id, nullptr))
            << ": " << count << "\n";
    }

    file.close();
    QMessageBox::information(this, tr("Done"),
        tr("Chunk list exported to %1").arg(outPath));
}

void MainWindow::on_actionExportJsonBatch_triggered()
{
    QString startDir = lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory;
    QString srcDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select W3D Directory"),
        startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (srcDir.isEmpty()) return;

    QString outDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        srcDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (outDir.isEmpty()) return;

    QDir dir(srcDir);
    auto w3dFiles = dir.entryList(QStringList{ "*.w3d", "*.W3D", "*.wlt" },
        QDir::Files | QDir::NoSymLinks);

    if (w3dFiles.isEmpty()) {
        QMessageBox::information(this, tr("Export JSON"),
            tr("No W3D files found in %1.").arg(srcDir));
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

    for (const QString& fileName : w3dFiles) {
        QString inputPath = dir.absoluteFilePath(fileName);
        ChunkData cd;
        if (!cd.loadFromFile(inputPath.toStdString())) {
            failures << tr("%1 (load failed)").arg(inputPath);
            continue;
        }

        const ordered_json doc = cd.toJson(selectedMode);
        QString baseName = QFileInfo(fileName).completeBaseName();
        QString outputPath = outputDir.absoluteFilePath(baseName + ".json");
        QFile outFile(outputPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            failures << tr("%1 (write open failed)").arg(outputPath);
            continue;
        }

        QByteArray payload = QByteArray::fromStdString(doc.dump(4));
        auto written = outFile.write(payload);
        outFile.close();
        if (written != payload.size()) {
            failures << tr("%1 (write failed)").arg(outputPath);
            continue;
        }

        ++successCount;
    }

    lastDirectory = srcDir;

    QString summary = tr("Exported %1 file(s) to %2.\nMode: %3")
        .arg(successCount)
        .arg(outDir)
        .arg(SerializationModeToken(selectedMode));
    if (failures.isEmpty()) {
        QMessageBox::information(this, tr("Export JSON"), summary);
    }
    else {
        summary += tr("\n\nFailures:\n%1").arg(failures.join("\n"));
        QMessageBox::warning(this, tr("Export JSON"), summary);
    }
}

void MainWindow::on_actionValidateRoundTripBatch_triggered()
{
    const QString startDir = lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory;
    const QString srcDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select W3D Directory"),
        startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (srcDir.isEmpty()) return;

    const QString outDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        srcDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (outDir.isEmpty()) return;

    QStringList inputFiles;
    QDirIterator it(
        srcDir,
        QStringList{ "*.w3d", "*.W3D", "*.wlt", "*.WLT" },
        QDir::Files | QDir::NoSymLinks,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        inputFiles << QDir::cleanPath(it.next());
    }
    std::sort(inputFiles.begin(), inputFiles.end(), [](const QString& a, const QString& b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
        });

    if (inputFiles.isEmpty()) {
        QMessageBox::information(this, tr("Round-Trip Validate"),
            tr("No W3D/WLT files found in %1.").arg(srcDir));
        return;
    }
    const int discoveredFileCount = static_cast<int>(inputFiles.size());

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

    QDir sourceRoot(srcDir);
    QDir failuresRoot(failuresRootPath);

    int processedRuns = 0;
    int passCount = 0;
    int failCount = 0;
    bool canceled = false;
    int runCounter = 0;

    for (int i = 0; i < discoveredFileCount; ++i) {
        const QString sourcePath = QDir::cleanPath(inputFiles[i]);
        const QString relativePath = SanitizeRelativePath(sourceRoot.relativeFilePath(sourcePath));

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
            row.sourcePath = QDir::toNativeSeparators(QFileInfo(sourcePath).absoluteFilePath());
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
                if (!ReadAllBytes(sourcePath, originalBytes, ioError)) {
                    row.stage = QStringLiteral("LOAD_W3D");
                    row.errorMessage = ioError;
                    break;
                }
                row.originalSize = originalBytes.size();

                ChunkData sourceData;
                if (!sourceData.loadFromFile(sourcePath.toStdString())) {
                    row.stage = QStringLiteral("LOAD_W3D");
                    row.errorMessage = tr("Failed to load source W3D/WLT.");
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

    const QString summary = tr("%1\n\nDiscovered files: %2\nTotal mode-runs: %3\nProcessed mode-runs: %4\nPass: %5\nFail: %6\nReport: %7\nFailure artifacts: %8")
        .arg(canceled ? tr("Validation canceled.") : tr("Validation completed."))
        .arg(discoveredFileCount)
        .arg(totalRuns)
        .arg(processedRuns)
        .arg(passCount)
        .arg(failCount)
        .arg(QDir::toNativeSeparators(reportPath))
        .arg(QDir::toNativeSeparators(failuresRootPath));

    if (canceled || failCount > 0) {
        QMessageBox::warning(this, tr("Round-Trip Validate"), summary);
    }
    else {
        QMessageBox::information(this, tr("Round-Trip Validate"), summary);
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
    ClearChunkTree();
    populateTree();
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
            if (!chunkData->saveToFile(out.toStdString())) {
                QMessageBox::warning(this, tr("Error"), tr("Failed to save W3D file."));
            }
        }
    }
}
