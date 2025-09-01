
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
#include <QStandardPaths>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include "backend/W3DMesh.h"
#include <map>
#include <QJsonDocument>
#include <QJsonParseError>

Q_DECLARE_METATYPE(void*)


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // Menu bar
    QMenu* fileMenu = menuBar()->addMenu("&File");
    QAction* openAction = fileMenu->addAction("&Open");
    connect(openAction, &QAction::triggered, this, [this]() {
        openFile("");
        });
    QAction* exportJsonAct = fileMenu->addAction("Export to JSON...");
    connect(exportJsonAct, &QAction::triggered, this, &MainWindow::exportJson);
    QAction* importJsonAct = fileMenu->addAction("Import from JSON...");
    connect(importJsonAct, &QAction::triggered, this, &MainWindow::importJson);


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
    setWindowTitle("oW3DEdit");
    recentFilesPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/recent_files.txt";
    recentFilesMenu = fileMenu->addMenu("Open Recent");
    LoadRecentFiles();
    if (lastDirectory.isEmpty())
        lastDirectory = QDir::homePath();
    UpdateRecentFilesMenu();
    // create the menu & action
    QMenu* viewMenu = menuBar()->addMenu("&View");
    QAction* expandAllAction = viewMenu->addAction("Expand All");
    QAction* collapseAllAction = viewMenu->addAction("Collapse All");
    connect(expandAllAction, &QAction::triggered, treeWidget, &QTreeWidget::expandAll);
    connect(collapseAllAction, &QAction::triggered, treeWidget, &QTreeWidget::collapseAll);
    auto batchMenu = menuBar()->addMenu(tr("Batch Tools"));
    auto exportChunksAct = new QAction(tr("Export Chunk List..."), this);
    batchMenu->addAction(exportChunksAct);
    connect(exportChunksAct, &QAction::triggered,
        this, &MainWindow::on_actionExportChunkList_triggered);
}


void MainWindow::openFile(const QString& path) {
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
    const auto selectedItems = treeWidget->selectedItems();
    if (selectedItems.isEmpty()) return;

    QTreeWidgetItem* selected = selectedItems.first();
    const QVariant ptrVar = selected->data(0, Qt::UserRole);
    if (!ptrVar.isValid()) return;

    void* targetPtr = ptrVar.value<void*>();
    if (!targetPtr) return;

    // Resolve ChunkItem*
    std::shared_ptr<ChunkItem> target;
    {
        std::function<std::shared_ptr<ChunkItem>(const std::shared_ptr<ChunkItem>&)> dfs =
            [&](const std::shared_ptr<ChunkItem>& node) -> std::shared_ptr<ChunkItem> {
            if (node.get() == targetPtr) return node;
            for (const auto& c : node->children) {
                if (auto r = dfs(c)) return r;
            }
            return nullptr;
            };
        for (const auto& root : chunkData->getChunks()) {
            target = dfs(root);
            if (target) break;
        }
    }

    tableWidget->clearContents();
    tableWidget->setRowCount(0);
    if (!target) return;

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


void MainWindow::exportJson() {
    QString path = QFileDialog::getSaveFileName(this, tr("Export to JSON"), lastDirectory, tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty()) return;
    QJsonDocument doc = chunkData->toJson();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot write JSON file."));
        return;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
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
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();
    if (err.error != QJsonParseError::NoError || !chunkData->fromJson(doc)) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid JSON content."));
        return;
    }
    ClearChunkTree();
    populateTree();
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