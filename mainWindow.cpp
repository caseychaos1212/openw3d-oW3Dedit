
#include "MainWindow.h"
#include "backend/ChunkData.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QTreeWidget>
#include <QTableWidget>
#include "backend/ChunkData.h" 

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // Menu bar
    QMenu* fileMenu = menuBar()->addMenu("&File");
    QAction* openAction = fileMenu->addAction("&Open");
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    auto* splitter = new QSplitter(this);

    treeWidget = new QTreeWidget(splitter);
    treeWidget->setHeaderLabel("Chunk Tree");

    tableWidget = new QTableWidget(splitter);
    tableWidget->setColumnCount(3);
    tableWidget->setHorizontalHeaderLabels({ "Field", "Type", "Value" });

    chunkData = std::make_unique<ChunkData>();

    splitter->addWidget(treeWidget);
    splitter->addWidget(tableWidget);
    setCentralWidget(splitter);
    setWindowTitle("oW3D Dump Qt");
}

void MainWindow::openFile() {
    QString path = QFileDialog::getOpenFileName(this, "Open W3D File", "", "W3D Files (*.w3d);;All Files (*)");
    if (path.isEmpty()) return;

    if (!chunkData->loadFromFile(path.toStdString())) {
        QMessageBox::warning(this, "Error", "Failed to open file.");
        return;
    }

    populateTree();
}

void MainWindow::populateTree() {
    treeWidget->clear();

    const auto& chunks = chunkData->getChunks();
    for (const auto& chunk : chunks) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, QString("Chunk %1 (size %2)").arg(chunk->id).arg(chunk->length));
        treeWidget->addTopLevelItem(item);

        std::function<void(QTreeWidgetItem*, const std::shared_ptr<ChunkItem>&)> addChildren;
        addChildren = [&](QTreeWidgetItem* parent, const std::shared_ptr<ChunkItem>& current) {
            for (const auto& child : current->children) {
                QTreeWidgetItem* childItem = new QTreeWidgetItem();
                childItem->setText(0, QString("Chunk %1 (size %2)").arg(child->id).arg(child->length));
                parent->addChild(childItem);
                addChildren(childItem, child);
            }
            };

        addChildren(item, chunk);
    }

    treeWidget->expandAll();
}
