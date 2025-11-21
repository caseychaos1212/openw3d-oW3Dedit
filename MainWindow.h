#pragma once

#include <QMainWindow>
#include <memory>
#include "backend/ChunkData.h"
#include <QString>

class QTreeWidget;
class QTableWidget;
class QStackedWidget;
class MeshEditorWidget;
class StringEditorWidget;
class MaterialEditorWidget;
class ShaderEditorWidget;




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
    void LoadRecentFiles();
    void SaveRecentFiles();
    void UpdateRecentFilesMenu();
    void AddRecentFile(const QString& path);
    void OpenRecentFile();  // will be connected to recent file actions
    void on_actionExportChunkList_triggered();
    void showHierarchyBrowser();
    void selectChunkInTree(void* chunkPtr);
private:
    void populateTree();

    QTreeWidget* treeWidget = nullptr;
    QTableWidget* tableWidget = nullptr;
    std::unique_ptr<ChunkData> chunkData;
    QString recentFilesPath;
    QStringList recentFiles;
    QMenu* recentFilesMenu = nullptr;
    QString lastDirectory;
    QStackedWidget* editorStack = nullptr;
    MeshEditorWidget* meshEditor = nullptr;
    StringEditorWidget* textureNameEditor = nullptr;
    StringEditorWidget* materialNameEditor = nullptr;
    MaterialEditorWidget* materialEditor = nullptr;
    ShaderEditorWidget* shaderEditor = nullptr;
    QWidget* editorPlaceholder = nullptr;
    std::shared_ptr<ChunkItem> currentChunk;
    QString currentFilePath;
    bool dirty = false;

    void updateEditorForChunk(const std::shared_ptr<ChunkItem>& chunk);
    void setDirty(bool value);
    void updateWindowTitle();
    bool confirmDiscardChanges();
    void clearDetails();
};
