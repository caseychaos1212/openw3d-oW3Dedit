
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
#include <QTreeWidget>
#include <QTableWidget>
#include <iostream>
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
        
        QString label = QString("0x%1 (%2)").arg(chunk->id, 0, 16).arg(QString::fromStdString(GetChunkName(chunk->id)));
        item->setText(0, QString("%1 (size %2)").arg(label).arg(chunk->length));
        treeWidget->addTopLevelItem(item);

        std::function<void(QTreeWidgetItem*, const std::shared_ptr<ChunkItem>&)> addChildren;
        addChildren = [&](QTreeWidgetItem* parent, const std::shared_ptr<ChunkItem>& current) {
            for (const auto& child : current->children) {
                QTreeWidgetItem* childItem = new QTreeWidgetItem();
                QString label = QString("0x%1 (%2)").arg(child->id, 0, 16).arg(QString::fromStdString(GetChunkName(child->id)));
                childItem->setText(0, QString("%1 (size %2)").arg(label).arg(child->length));
                parent->addChild(childItem);
                addChildren(childItem, child);
            }
            };

        addChildren(item, chunk);
    }

    treeWidget->expandAll();
    connect(treeWidget, &QTreeWidget::itemSelectionChanged, this, &MainWindow::handleTreeSelection);

}


void MainWindow::handleTreeSelection() {
    auto selectedItems = treeWidget->selectedItems();
    if (selectedItems.empty()) return;

    QTreeWidgetItem* selected = selectedItems.first();

    // Walk the tree to find the ChunkItem that matches this item
    std::shared_ptr<ChunkItem> target;

    std::function<std::shared_ptr<ChunkItem>(QTreeWidgetItem*, const std::shared_ptr<ChunkItem>&)> findChunk;
    findChunk = [&](QTreeWidgetItem* item, const std::shared_ptr<ChunkItem>& chunk) -> std::shared_ptr<ChunkItem> {
        QString label = QString("0x%1 (%2)").arg(chunk->id, 0, 16).arg(QString::fromStdString(GetChunkName(chunk->id)));
        QString expected = QString("%1 (size %2)").arg(label).arg(chunk->length);
        if (item->text(0) == expected)
            return chunk;

        for (const auto& child : chunk->children) {
            auto result = findChunk(item, child);
            if (result) return result;
        }

        return nullptr;
        };

    const auto& topChunks = chunkData->getChunks();
    for (const auto& chunk : topChunks) {
        target = findChunk(selected, chunk);
        if (target) break;
    }

    tableWidget->clearContents();
    tableWidget->setRowCount(0);

    if (!target) return;

    std::vector<ChunkField> fields;
    std::cout << "Selected chunk ID: 0x" << std::hex << target->id << std::dec << "\n";
    std::cout << "Chunk length: " << target->length << " bytes\n";
    
    if (!target->children.empty()) {
        for (const auto& child : target->children) {
            std::string chunkName = GetChunkName(child->id);
            if (chunkName == "UNKNOWN") {
                std::ostringstream fallback;
                fallback << "0x" << std::hex << child->id;
                chunkName = fallback.str();
            }

            fields.push_back({ chunkName, "Chunk", "" });
        }
    }

    else {
        // No children: decode based on ID
        switch (target->id) {
        case 0x0101: // HIERARCHY_HEADER
            fields = InterpretHierarchyHeader(target);
            break;
        case 0x0102: // PIVOTS
            fields = InterpretPivots(target);
            break;
        case 0x0103: // PIVOT_FIXUPS
            fields = InterpretPivotFixups(target);
            break;
        default:
            break;
        }
    }

    tableWidget->setRowCount(static_cast<int>(fields.size()));
    for (int i = 0; i < fields.size(); ++i) {
        tableWidget->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(fields[i].field)));
        tableWidget->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(fields[i].type)));
        tableWidget->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(fields[i].value)));
    }
}

