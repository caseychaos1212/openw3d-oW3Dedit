
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
#include "backend/W3DMesh.h"

Q_DECLARE_METATYPE(void*)


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
    // create the menu & action
    auto batchMenu = menuBar()->addMenu(tr("Batch Tools"));
    auto exportChunksAct = new QAction(tr("Export Chunk List…"), this);
    batchMenu->addAction(exportChunksAct);
    connect(exportChunksAct, &QAction::triggered,
        this, &MainWindow::on_actionExportChunkList_triggered);
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

    AddRecentFile(filePath);  
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

    treeWidget->expandAll();
    connect(treeWidget, &QTreeWidget::itemSelectionChanged, this, &MainWindow::handleTreeSelection);

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

    if (target->id == SOUND_RENDER_DEF
        
        && target->parent
        && (target->parent->id == SOUNDROBJ_DEFINITION
            || target->parent->id == SOUND_RENDER_DEF_EXT))
    {
        // debug log:
        std::cout << "DEBUG: firing InterpretSoundRObjDefinition – "
            << "chunkId=0x" << std::hex << target->id
            << " parentId=0x" << target->parent->id
            << std::dec << "\n";

        fields = InterpretSoundRObjDefinition(target);
        goto show_table;
    }

    if (target->id == MICRO_ID
        && target->parent
        && target->parent->id == CHANNEL_WRAPPER
        && target->parent->parent
        && target->parent->parent->parent)
    {
        auto wrapperId = target->parent->parent->id;           // usually 0x0002–0x0005
        auto headerId = target->parent->parent->parent->id;    // sphere vs ring
        
        std::cout << "Dispatch micro: channel=0x"
            << std::hex << wrapperId
            << " header=0x" << headerId << std::dec << "\n";

     //   if (headerId == SPHERE_ID) {
     //       fields = InterpretSphereMicrochunk(target, wrapperId);
     //   }
     //   else if (headerId == RING_ID) {
      //      fields = InterpretRingMicrochunk(target, wrapperId);
      //  }
    }
    // Skip remaining chunk-wrapper child summary if we've interpreted real data
    if (!fields.empty()) goto show_table;

    // Populate children if present
    if (!target->children.empty()) {
        for (const auto& child : target->children) {
            std::string name = LabelForChunk(child->id, child.get());
            fields.push_back({ name, "chunk", "" });
        }
    }
    else {
        uint16_t flavor = 0xFFFF;
        if (target->id == 0x0282) {
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

        switch (target->id) {
        case 0x0101: fields = InterpretHierarchyHeader(target); break;
        case 0x0102: fields = InterpretPivots(target); break;
        case 0x0103: fields = InterpretPivotFixups(target); break;
        case 0x001F: fields = InterpretMeshHeader3(target); break;
        case 0x0002: fields = InterpretVertices(target); break;
        case 0x0003: fields = InterpretVertexNormals(target); break;
            // case 0x0002: {
       //     uint32_t pid = target->parent ? target->parent->id : 0;
       //     if (pid == 0x0741) {
       //         fields = InterpretSphereChannel(target, target->id);
       //     }
       //     if (pid == 0x0742) {
       //         fields = InterpretRingChannel(target, target->id);
       //     }
       //     else {
       //         fields = InterpretVertices(target);
       //     }
       //     break;
       // }
       // case 0x0003: {
      //      uint32_t pid = target->parent ? target->parent->id : 0;
       //     if (pid == 0x0741) {
       //         fields = InterpretSphereChannel(target, target->id);
        //    }
        //    if (pid == 0x0742) {
        //        fields = InterpretRingChannel(target, target->id);
         //   }
        //    else {
        //        fields = InterpretVertexNormals(target);
        //    }
        //    break;
       // }
        case 0x003B: fields = InterpretDCG(target); break;
        case 0x00000020: fields = InterpretTriangles(target); break;
        case 0x00000022: fields = InterpretVertexShadeIndices(target); break;
        case 0x00000028: fields = InterpretMaterialInfo(target); break;
        case 0x0282: fields = InterpretCompressedAnimationChannel(target, (flavor != 0xFFFF ? flavor : 0)); break;
        case 0x0000002C: fields = InterpretVertexMaterialName(target); break;
        case 0x0000002D: fields = InterpretVertexMaterialInfo(target); break;
        case 0x0301: fields = InterpretHModelHeader(target); break;
		case 0x0302: fields = InterpretNode(target); break;
		case 0x0303: fields = InterpretCollisionNode(target); break;
		case 0x0304: fields = InterpretSkinNode(target); break;
        case 0x0283: fields = InterpretCompressedBitChannel(target); break;
        case 0x0029: fields = InterpretShaders(target); break;
        case 0x0032: fields = InterpretTextureName(target); break;
        case 0x0033: fields = InterpretTextureInfo(target); break;
        case 0x0039: fields = InterpretVertexMaterialIDs(target); break;
        case 0x003A: fields = InterpretShaderIDs(target); break;
        case 0x0049: fields = InterpretTextureIDs(target); break;
        case 0x004a: fields = InterpretStageTexCoords(target); break;
        case 0x0058: fields = InterpretDeform(target); break;
        case 0x0059: fields = InterpretDeformSet(target); break;
        case 0x005A: fields = InterpretDeformKeyframes(target); break;
        case 0x005B: fields = InterpretDeformData(target); break;
        case 0x0091: fields = InterpretAABTreeHeader(target); break;
        case 0x0092: fields = InterpretAABTreePolyIndices(target); break;
        case 0x0093: fields = InterpretAABTreeNodes(target); break;
        case 0x0501: fields = InterpretEmitterHeader(target); break;
        case 0x0502: fields = InterpretEmitterUserData(target); break;
        case 0x0503: fields = InterpretEmitterInfo(target); break;
        case 0x0504: fields = InterpretEmitterInfoV2(target); break;
        case 0x0505: fields = InterpretEmitterProps(target); break;
        case 0x050A: fields = InterpretEmitterRotationKeys(target); break;
        case 0x050B: fields = InterpretEmitterFrameKeys(target); break;
        case 0x0601: fields = InterpretAggregateHeader(target); break;
        case 0x0602: fields = InterpretAggregateInfo(target); break;
        case 0x0603: fields = InterpretTextureReplacerInfo(target); break;
        case 0x0604: fields = InterpretAggregateClassInfo(target); break;
        case 0x0701: fields = InterpretHLODHeader(target); break;
        case 0x0703: fields = InterpretHLODSubObjectArrayHeader(target); break;
        case 0x0704: fields = InterpretHLODSubObject_LodArray(target); break;
        case 0x0001: {
            uint32_t pid = target->parent ? target->parent->id : 0;
            if (pid == 0x0741) {
                fields = InterpretSphereHeader(target);
                std::cout << ">>> HEADER node clicked, parent="
                    << std::hex << pid << std::dec << "\n";
            }
            else if (pid == 0x0742) {
                fields = InterpretRingHeader(target);
                std::cout << ">>> HEADER node clicked, parent="
                    << std::hex << pid << std::dec << "\n";
            }
            break;
        }
        case 0x0201: fields = InterpretAnimationHeader(target); break;
        case 0x0202: fields = InterpretAnimationChannel(target); break;
        case 0x0203: fields = InterpretBitChannel(target); break;
        case 0x0305: fields = InterpretHModelAuxData(target); break;
        case 0x0281: fields = InterpretCompressedAnimationHeader(target); break;
        case 0x000E: fields = InterpretVertexInfluences(target); break;
        case 0x0740: fields = InterpretBox(target); break;
        case 0x000C: fields = InterpretMeshUserText(target); break;
       // case 0x00000004: fields = InterpretRingChannel(target); break;
        case 0x00000005: fields = InterpretSphereChannel(target); break;
        {
            uint32_t pid = target->parent ? target->parent->id : 0;
            if (pid == 0x0741) {
                fields = InterpretSphereChannel(target);
            }
            else if (pid == 0x0742) {
                fields = InterpretRingChannel(target);

            }
            break;
        }
		case 0x461: fields = InterpretLightInfo(target); break;
		case 0x462: fields = InterpretSpotLightInfo(target); break;
		case 0x463: fields = InterpretNearAtten(target); break;
        case 0x464: fields = InterpretFarAtten(target); break;
        case 0x802: fields = InterpretLightTransform(target); break;
		case 0x901: fields = InterpretDazzleName(target); break;
		case 0x902: fields = InterpretDazzleTypeName(target); break;
		case 0xa01: fields = InterpretSoundRObjHeader(target); break;
        case 0x100: fields = InterpretSoundRObjDefinition(target); break;
        case 0x02E: fields = InterpretARG0(target); break;
        case 0x02F: fields = InterpretARG1(target); break;
		case 0x03C: fields = InterpretDIG(target); break;
        case 0x03E: fields = InterpretSCG(target); break;
        case 0x04B: fields = InterpretPerFaceTexcoordIds(target); break;
//		case 0x200: fields = InterpretSoundRObjExt(target); break;
        case 0x2C1: fields = InterpretMorphAnimHeader(target); break;
		case 0x2C3: fields = InterpretMorphAnimPoseName(target); break;
		case 0x2C4: fields = InterpretMorphAnimKeyData(target); break;
		case 0x2C5: fields = InterpretMorphAnimPivotChannelData(target); break;
		case 0x306: fields = InterpretHModelNode(target); break;
		case 0x401: fields = InterpretLODModelHeader(target); break;
		case 0x402: fields = InterpretLOD(target); break;
		case 0x421: fields = InterpretCollectionHeader(target); break;
		case 0x422: fields = InterpretCollectionObjName(target); break;
		case 0x423: fields = InterpretPlaceHolder(target); break;
		case 0x424: fields = InterpretTransformNode(target); break;
		case 0x440: fields = InterpretPoints(target); break;
		case 0x506: fields = InterpretEmitterColorKeyframe(target); break;
		case 0x507: fields = InterpretEmitterOpacityKeyframe(target); break;
		case 0x508: fields = InterpretEmitterSizeKeyframe(target); break;
		case 0x509: fields = InterpretEmitterLineProperties(target); break;
		case 0x50C: fields = InterpretEmitterBlurTimeKeyframes(target); break;
		case 0x750: fields = InterpretNullObject(target); break;
		case 0x080: fields = InterpretPS2Shaders(target); break;
        {

        };
        default: break;
        }
    }

show_table:
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



//extern void recursePrint(const std::shared_ptr<ChunkItem>& c, int depth, std::ostream& out);



static void recursePrint(const std::shared_ptr<ChunkItem>& c,
    int depth,
    QTextStream& out)
{
    // 1) Skip raw micro chunks under the channel wrapper
    if (c->id == MICRO_ID && c->parent && c->parent->id == CHANNEL_WRAPPER)
        return;

    // 2) Skip children of the SOUND_RENDER_DEF node
    if (c->parent && c->parent->id == SOUND_RENDER_DEF)
        return;

    // 3) Print "0x######## NAME"
    out
        << QString(depth * 2, ' ')
        << QString("0x%1 ").arg(c->id, 8, 16, QChar('0')).toUpper()
        << QString::fromStdString(LabelForChunk(c->id, c.get()))
        << "\n";

    // Recurse
    for (auto& ch : c->children) {
        recursePrint(ch, depth + 1, out);
    }
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
        tr("Save Chunk List As…"),
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

    // 4) iterate .w3d files
    QDir dir(srcDir);
    auto w3dFiles = dir.entryList(QStringList{ "*.w3d" },
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
            recursePrint(top, 1, /* std::ostream& */ txt);
        }
        txt << "\n";
    }

    file.close();
    QMessageBox::information(this, tr("Done"),
        tr("Chunk list exported to %1").arg(outPath));
}


