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
    void openFile();
    void handleTreeSelection();

private:
    void populateTree();

    QTreeWidget* treeWidget = nullptr;
    QTableWidget* tableWidget = nullptr;
    std::unique_ptr<ChunkData> chunkData;
};
