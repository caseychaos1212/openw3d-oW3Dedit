#pragma once

#include <QMainWindow>
#include <memory>
#include "backend/ChunkData.h"
#include <QString>
#include <QByteArray>

class QTreeWidget;
class QTableWidget;
class QStackedWidget;
class QSplitter;
class QScrollArea;
class QCloseEvent;
class QCheckBox;
class QPlainTextEdit;
class QGroupBox;
class MeshEditorWidget;
class StringEditorWidget;
class TransformNodeEditorWidget;
class MapperArgsEditorWidget;
class MaterialEditorWidget;
class ShaderEditorWidget;
class SurfaceTypeEditorWidget;
class TriangleSurfaceTypeEditorWidget;




class ChunkData; // forward declare

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);

private slots:
    void openFile(const QString& path = QString());
    void ClearChunkTree();
    void handleTreeSelection();
    void saveFile();
    void saveFileAs();
    void onChunkEdited();
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
    void selectChunkInTree(void* chunkPtr);

protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void populateTree();

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
    StringEditorWidget* textureNameEditor = nullptr;
    StringEditorWidget* materialNameEditor = nullptr;
    TransformNodeEditorWidget* transformNodeEditor = nullptr;
    MapperArgsEditorWidget* stage0ArgsEditor = nullptr;
    MapperArgsEditorWidget* stage1ArgsEditor = nullptr;
    MaterialEditorWidget* materialEditor = nullptr;
    ShaderEditorWidget* shaderEditor = nullptr;
    SurfaceTypeEditorWidget* surfaceTypeEditor = nullptr;
    TriangleSurfaceTypeEditorWidget* triangleSurfaceTypeEditor = nullptr;
    QWidget* editorPlaceholder = nullptr;
    std::shared_ptr<ChunkItem> currentChunk;
    QString currentFilePath;
    bool dirty = false;
    QByteArray detailSplitterStateCache;

    void updateEditorForChunk(const std::shared_ptr<ChunkItem>& chunk);
    void updateRawHex(const std::shared_ptr<ChunkItem>& chunk);
    void setDirty(bool value);
    void updateWindowTitle();
    bool confirmDiscardChanges();
    void clearDetails();
};
