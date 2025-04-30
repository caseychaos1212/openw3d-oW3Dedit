
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
#include <QStandardPaths>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QAction>

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
    connect(openAction, &QAction::triggered, this, [this]() {
        openFile("");
        });


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
    recentFilesPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/recent_files.txt";
    recentFilesMenu = fileMenu->addMenu("Open Recent");
    LoadRecentFiles();
    UpdateRecentFilesMenu();

}

void MainWindow::openFile(const QString& path) {
    QString filePath = path;
    if (filePath.isEmpty()) {
        filePath = QFileDialog::getOpenFileName(this, "Open W3D File", "", "W3D Files (*.w3d);;All Files (*)");
        if (filePath.isEmpty()) return;
    }

    if (!chunkData->loadFromFile(filePath.toStdString())) {
        QMessageBox::warning(this, "Error", "Failed to open file.");
        return;
    }

    ClearChunkTree();
    populateTree();

    AddRecentFile(filePath);   //<-- <<< Add this here!
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

        uint16_t flavor = 0xFFFF;
        if (target->id == 0x0282) {
            // Look for 0x0281 among siblings or parent->children
            for (const auto& topChunk : chunkData->getChunks()) {
                std::function<bool(const std::shared_ptr<ChunkItem>&, uint16_t&)> findFlavor;
                findFlavor = [&](const std::shared_ptr<ChunkItem>& node, uint16_t& outFlavor) -> bool {
                    if (node->id == 0x0281 && node->data.size() >= 44) {
                        outFlavor = *reinterpret_cast<const uint16_t*>(&node->data[42]);
                        return true;
                    }
                    for (const auto& child : node->children) {
                        if (findFlavor(child, outFlavor)) return true;
                    }
                    return false;
                    };

                if (findFlavor(topChunk, flavor)) break;
            }
        }
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
        case 0x003B: // W3D_CHUNK_DCG
            fields = InterpretDiffuseColorChunk(target);
            break;
        case 0x00000020: // W3D_CHUNK_TRIANGLES
            fields = InterpretTriangles(target);
            break;
        case 0x00000022: // W3D_CHUNK_VERTEX_SHADE_INDICES
            fields = InterpretVertexShadeIndices(target);
            break;
        case 0x0024: // W3D_CHUNK_PRELIT_VERTEX
            fields = InterpretPrelitVertexWrapper(target);
            break;
        case 0x0026: // W3D_CHUNK_LIGHTMAP_MULTI_TEXTURE
            fields = InterpretLightmapMultiTexture(target);
            break;
        case 0x00000028: // W3D_CHUNK_MATERIAL_INFO
            fields = InterpretMaterialInfo(target);
            break;
        case 0x0282: // W3D_CHUNK_COMPRESSED_ANIMATION_CHANNEL
            fields = InterpretCompressedAnimationChannel(target, (flavor != 0xFFFF ? flavor : 0)); // Default to 0 if not found
            break;
        case 0x0000002C: // W3D_CHUNK_VERTEX_MATERIAL_NAME
            fields = InterpretVertexMaterialName(target);
            break;
        case 0x0000002D: // W3D_CHUNK_VERTEX_MATERIAL_INFO
            fields = InterpretVertexMaterialInfo(target);
            break;
        case 0x0310: 
            fields = InterpretShaderName(target);
            break;
        case 0x0311: 
            fields = InterpretShaderDetail(target);
            break;
        case 0x0283: // W3D_CHUNK_COMPRESSED_BIT_CHANNEL
            fields = InterpretCompressedBitChannel(target);
            break;
        case 0x0029:
            fields = InterpretShaders(target);
            break;
        case 0x0032: // W3D_CHUNK_TEXTURE_NAME
            fields = InterpretTextureName(target); 
            break;  
        case 0x0033: // W3D_CHUNK_TEXTURE_INFO
            fields = InterpretTextureInfo(target);
            break;  
        case 0x0039:
            fields = InterpretVertexMaterialIDs(target);
            break;
        case 0x003A:
            fields = InterpretShaderIDs(target);
            break;
        case 0x0048:
            fields = InterpretTextureStage(target);
            break;
        case 0x0049:
            fields = InterpretTextureIDs(target);
            break;
        case 0x004a:
             fields = InterpretStageTexCoords(target);
            break;
        case 0x0058: // W3D_CHUNK_DEFORM
            fields = InterpretDeform(target);
            break;
        case 0x0059: // W3D_CHUNK_DEFORM_SET
            fields = InterpretDeformSet(target);
            break;
        case 0x005A: // W3D_CHUNK_DEFORM_KEYFRAME
            fields = InterpretDeformKeyframe(target);
            break;
        case 0x005B: // W3D_CHUNK_DEFORM_DATA
            fields = InterpretDeformData(target);
            break;
        case 0x0091:
            fields = InterpretAABTreeHeader(target);
            break;
        case 0x0092:
            fields = InterpretAABTreePolyIndices(target);
            break;
        case 0x0093:
            fields = InterpretAABTreeNodes(target);
            break;
        case 0x0701:
            fields = InterpretHLODHeader(target);
            break;
        case 0x0702: // HLOD_LOD_ARRAY
            fields = InterpretHLODLODArray(target);
            break;
        case 0x0703:
            fields = InterpretHLODSubObjectArrayHeader(target);
            break;
        case 0x0704:
            fields = InterpretHLODSubObject_LodArray(target);
            break;
        case 0x0201: // ANIMATION_HEADER
            fields = InterpretAnimationHeader(target);
            break;
        case 0x0202: // ANIMATION_CHANNEL
            fields = InterpretAnimationChannel(target);
            break;
		case 0x0203: // BIT_CHANNEL
			fields = InterpretBitChannel(target);
			break;
		case 0x0281: // COMPRESSED_ANIMATION_HEADER
			fields = InterpretCompressedAnimationHeader(target);
			break;
		case 0x000E: // W3D_CHUNK_VERTEX_INFLUENCES
			fields = InterpretVertexInfluences(target);
			break;
		case 0x0740: // W3D_CHUNK_BOX
			fields = InterpretBox(target);
			break;
        case 0x000C: // W3D_CHUNK_MESH_USER_TEXT
            fields = InterpretMeshUserText(target);
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

void MainWindow::ClearChunkTree() {
    treeWidget->clear();
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


