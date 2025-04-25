
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

Q_DECLARE_METATYPE(void*)

static std::string LabelForChunk(uint32_t id) {
    std::string name = GetChunkName(id);
    if (name == "UNKNOWN") {
        std::ostringstream fallback;
        fallback << "0x" << std::hex << id;
        return fallback.str();
    }
    return name;
}

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
        item->setData(0, Qt::UserRole, QVariant::fromValue<void*>(chunk.get()));
        treeWidget->addTopLevelItem(item);

        std::function<void(QTreeWidgetItem*, const std::shared_ptr<ChunkItem>&)> addChildren;
        addChildren = [&](QTreeWidgetItem* parent, const std::shared_ptr<ChunkItem>& current) {
            for (const auto& child : current->children) {
                QTreeWidgetItem* childItem = new QTreeWidgetItem();
                QString label = QString("0x%1 (%2)").arg(child->id, 0, 16).arg(QString::fromStdString(GetChunkName(child->id)));
                childItem->setText(0, QString("%1 (size %2)").arg(label).arg(child->length));
                childItem->setData(0, Qt::UserRole, QVariant::fromValue<void*>(child.get()));
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
    QVariant ptrVar = selected->data(0, Qt::UserRole);
    if (!ptrVar.isValid()) return;

    void* targetPtr = ptrVar.value<void*>();
    std::shared_ptr<ChunkItem> target;

    const auto& topChunks = chunkData->getChunks();
    std::function<std::shared_ptr<ChunkItem>(const std::shared_ptr<ChunkItem>&)> findByPtr;
    findByPtr = [&](const std::shared_ptr<ChunkItem>& chunk) -> std::shared_ptr<ChunkItem> {
        if (chunk.get() == targetPtr) return chunk;
        for (const auto& child : chunk->children) {
            auto found = findByPtr(child);
            if (found) return found;
        }
        return nullptr;
        };

    for (const auto& chunk : topChunks) {
        target = findByPtr(chunk);
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
            std::string name = LabelForChunk(child->id);
            fields.push_back({ name, "chunk", "" });
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
        case 0x001F: // W3D_CHUNK_MESH_HEADER3
            fields = InterpretMeshHeader3(target);
            break;
        case 0x0002: // W3D_CHUNK_VERTICES
            fields = InterpretVertices(target);
            break;
		case 0x0003: // W3D_CHUNK_VERTEX_NORMALS
            fields = InterpretVertexNormals(target);
            break;
        case 0x00000020: // W3D_CHUNK_TRIANGLES
            fields = InterpretTriangles(target);
            break;
        case 0x00000022: // W3D_CHUNK_VERTEX_SHADE_INDICES
            fields = InterpretVertexShadeIndices(target);
            break;
        case 0x00000028: // W3D_CHUNK_MATERIAL_INFO
            fields = InterpretMaterialInfo(target);
            break;
        case 0x0000002C: // W3D_CHUNK_VERTEX_MATERIAL_NAME
            fields = InterpretVertexMaterialName(target);
            break;
        case 0x0000002D: // W3D_CHUNK_VERTEX_MATERIAL_INFO
            fields = InterpretVertexMaterialInfo(target);
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

