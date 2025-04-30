#pragma once

#include <QMainWindow>
#include <memory>
#include "backend/ChunkData.h"

class QTreeWidget;
class QTableWidget;




class ChunkData; // forward declare

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);

private slots:
    void openFile(const QString& path = QString());
    void ClearChunkTree();
    void handleTreeSelection();
    void LoadRecentFiles();
    void SaveRecentFiles();
    void UpdateRecentFilesMenu();
    void AddRecentFile(const QString& path);
    void OpenRecentFile();  // will be connected to recent file actions

private:
    void populateTree();

    QTreeWidget* treeWidget = nullptr;
    QTableWidget* tableWidget = nullptr;
    std::unique_ptr<ChunkData> chunkData;
    QString recentFilesPath;
    QStringList recentFiles;
    QMenu* recentFilesMenu = nullptr;
};

