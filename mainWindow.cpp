
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
#include <QAbstractItemView>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QGridLayout>
#include <QKeySequence>
#include <iostream>
#include <QStandardPaths>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include "backend/W3DMesh.h"
#include "backend/W3DStructs.h"
#include "backend/ChunkMutators.h"
#include "backend/ParseUtils.h"
#include "EditorWidgets.h"
#include <map>
#include <array>
#include <vector>
#include <cstring>
#include <variant>
#include <type_traits>
#include <QSignalBlocker>

static std::size_t TruncatedLength(const char* data, std::size_t maxLen) {
    std::size_t len = 0;
    while (len < maxLen && data[len] != '\0') {
        ++len;
    }
    return len;
}

static QString ReadFixedString(const char* data, std::size_t maxLen) {
    return QString::fromLatin1(data, static_cast<int>(TruncatedLength(data, maxLen)));
}

constexpr uint32_t MeshAttrValue(MeshAttr attr) {
    return static_cast<uint32_t>(attr);
}

constexpr int kMeshNameMax = static_cast<int>(W3D_NAME_LEN) - 1;

template <typename Enum>
static void PopulateEnumCombo(QComboBox* combo) {
    combo->clear();
    using Under = std::underlying_type_t<Enum>;
    for (Under i = 0; i < static_cast<Under>(Enum::MAX); ++i) {
        const auto name = ToString(static_cast<Enum>(i));
        combo->addItem(QString::fromUtf8(name), static_cast<int>(i));
    }
}

static void SetComboValue(QComboBox* combo, int value) {
    int idx = combo->findData(value);
    if (idx == -1) {
        combo->addItem(QString::fromLatin1("Unknown (%1)").arg(value), value);
        idx = combo->count() - 1;
    }
    combo->setCurrentIndex(idx);
}

static uint8_t ComboValue(const QComboBox* combo) {
    const QVariant data = combo->currentData();
    if (!data.isValid()) {
        return static_cast<uint8_t>(combo->currentIndex());
    }
    return static_cast<uint8_t>(data.toInt());
}

Q_DECLARE_METATYPE(void*)

MeshEditorWidget::MeshEditorWidget(QWidget* parent) : QWidget(parent) {
    setEnabled(false);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* form = new QFormLayout();
    meshNameEdit = new QLineEdit(this);
    meshNameEdit->setMaxLength(kMeshNameMax);
    containerNameEdit = new QLineEdit(this);
    containerNameEdit->setMaxLength(kMeshNameMax);
    form->addRow(tr("Mesh Name"), meshNameEdit);
    form->addRow(tr("Container Name"), containerNameEdit);
    layout->addLayout(form);

    auto* collisionGroup = new QGroupBox(tr("Collision Flags"), this);
    auto* collisionLayout = new QGridLayout(collisionGroup);
    const std::array<std::pair<MeshAttr, const char*>, 5> kFlags = { {
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_PHYSICAL,   "Physical" },
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_PROJECTILE, "Projectile" },
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_VIS,        "Visibility" },
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_CAMERA,     "Camera" },
        { MeshAttr::W3D_MESH_FLAG_COLLISION_TYPE_VEHICLE,    "Vehicle" },
    } };
    int row = 0;
    for (const auto& [flag, label] : kFlags) {
        auto* check = new QCheckBox(QString::fromLatin1(label), collisionGroup);
        collisionLayout->addWidget(check, row / 2, row % 2);
        collisionControls.push_back({ MeshAttrValue(flag), check });
        ++row;
    }
    layout->addWidget(collisionGroup);

    applyButton = new QPushButton(tr("Apply Mesh Changes"), this);
    connect(applyButton, &QPushButton::clicked,
        this, &MeshEditorWidget::applyChanges);

    layout->addStretch();
    layout->addWidget(applyButton, 0, Qt::AlignRight);
}

void MeshEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    meshNameEdit->clear();
    containerNameEdit->clear();
    for (auto& ctrl : collisionControls) {
        if (ctrl.box) ctrl.box->setChecked(false);
    }
    if (!chunkPtr) {
        setEnabled(false);
        return;
    }

    auto parsed = ParseChunkStruct<W3dMeshHeader3Struct>(chunkPtr);
    if (auto err = std::get_if<std::string>(&parsed)) {
        Q_UNUSED(err);
        setEnabled(false);
        return;
    }
    const auto& header = std::get<W3dMeshHeader3Struct>(parsed);
    meshNameEdit->setText(ReadFixedString(header.MeshName, W3D_NAME_LEN));
    containerNameEdit->setText(ReadFixedString(header.ContainerName, W3D_NAME_LEN));

    const uint32_t attr = header.Attributes;
    for (auto& ctrl : collisionControls) {
        ctrl.box->setChecked((attr & ctrl.mask) != 0);
    }

    setEnabled(true);
}

void MeshEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    const std::string meshName = meshNameEdit->text().toStdString();
    const std::string containerName = containerNameEdit->text().toStdString();
    uint32_t clearMask = 0;
    for (const auto& ctrl : collisionControls) {
        clearMask |= ctrl.mask;
    }

    std::string error;
    const bool ok = W3DEdit::MutateStructChunk<W3dMeshHeader3Struct>(
        chunkPtr,
        [&](W3dMeshHeader3Struct& header) {
            W3DEdit::WriteFixedString(header.MeshName, W3D_NAME_LEN, meshName);
            W3DEdit::WriteFixedString(header.ContainerName, W3D_NAME_LEN, containerName);
            uint32_t attr = header.Attributes & ~clearMask;
            for (const auto& ctrl : collisionControls) {
                if (ctrl.box->isChecked()) {
                    attr |= ctrl.mask;
                }
            }
            header.Attributes = attr;
        },
        &error);

    if (!ok) {
        QMessageBox::warning(this, tr("Error"),
            QString::fromStdString(error.empty() ? "Failed to update mesh header." : error));
        return;
    }

    emit chunkEdited();
}

ShaderEditorWidget::ShaderEditorWidget(QWidget* parent)
    : QWidget(parent) {
    setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* headerForm = new QFormLayout();
    shaderIndexCombo = new QComboBox(this);
    headerForm->addRow(tr("Shader"), shaderIndexCombo);
    layout->addLayout(headerForm);

    auto* form = new QFormLayout();

    depthCompareCombo = new QComboBox(this);
    PopulateEnumCombo<DepthCompare>(depthCompareCombo);
    form->addRow(tr("Depth Compare"), depthCompareCombo);

    depthMaskCombo = new QComboBox(this);
    PopulateEnumCombo<DepthMask>(depthMaskCombo);
    form->addRow(tr("Depth Mask"), depthMaskCombo);

    destBlendCombo = new QComboBox(this);
    PopulateEnumCombo<DestBlend>(destBlendCombo);
    form->addRow(tr("Dest Blend"), destBlendCombo);

    priGradientCombo = new QComboBox(this);
    PopulateEnumCombo<PriGradient>(priGradientCombo);
    form->addRow(tr("Primary Gradient"), priGradientCombo);

    secGradientCombo = new QComboBox(this);
    PopulateEnumCombo<SecGradient>(secGradientCombo);
    form->addRow(tr("Secondary Gradient"), secGradientCombo);

    srcBlendCombo = new QComboBox(this);
    PopulateEnumCombo<SrcBlend>(srcBlendCombo);
    form->addRow(tr("Source Blend"), srcBlendCombo);

    texturingCombo = new QComboBox(this);
    PopulateEnumCombo<Texturing>(texturingCombo);
    form->addRow(tr("Texturing"), texturingCombo);

    detailColorCombo = new QComboBox(this);
    PopulateEnumCombo<DetailColorFunc>(detailColorCombo);
    form->addRow(tr("Detail Color Func"), detailColorCombo);

    detailAlphaCombo = new QComboBox(this);
    PopulateEnumCombo<DetailAlphaFunc>(detailAlphaCombo);
    form->addRow(tr("Detail Alpha Func"), detailAlphaCombo);

    alphaTestCombo = new QComboBox(this);
    PopulateEnumCombo<AlphaTest>(alphaTestCombo);
    form->addRow(tr("Alpha Test"), alphaTestCombo);

    postDetailColorCombo = new QComboBox(this);
    PopulateEnumCombo<DetailColorFunc>(postDetailColorCombo);
    form->addRow(tr("Post Detail Color"), postDetailColorCombo);

    postDetailAlphaCombo = new QComboBox(this);
    PopulateEnumCombo<DetailAlphaFunc>(postDetailAlphaCombo);
    form->addRow(tr("Post Detail Alpha"), postDetailAlphaCombo);

    colorMaskSpin = new QSpinBox(this);
    colorMaskSpin->setRange(0, 255);
    form->addRow(tr("Color Mask"), colorMaskSpin);

    fogFuncSpin = new QSpinBox(this);
    fogFuncSpin->setRange(0, 255);
    form->addRow(tr("Fog Func"), fogFuncSpin);

    shaderPresetSpin = new QSpinBox(this);
    shaderPresetSpin->setRange(0, 255);
    form->addRow(tr("Shader Preset"), shaderPresetSpin);

    layout->addLayout(form);

    applyButton = new QPushButton(tr("Apply Shader Changes"), this);
    layout->addStretch();
    layout->addWidget(applyButton, 0, Qt::AlignRight);

    connect(shaderIndexCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int idx) { loadShader(idx); });
    connect(applyButton, &QPushButton::clicked,
        this, &ShaderEditorWidget::applyChanges);
}

void ShaderEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    shaders.clear();

    const QSignalBlocker blocker(shaderIndexCombo);
    shaderIndexCombo->clear();

    if (!chunkPtr) {
        setEnabled(false);
        return;
    }

    auto parsed = ParseChunkArray<W3dShaderStruct>(chunkPtr);
    if (auto err = std::get_if<std::string>(&parsed)) {
        Q_UNUSED(err);
        setEnabled(false);
        return;
    }

    shaders = std::get<std::vector<W3dShaderStruct>>(parsed);
    if (shaders.empty()) {
        setEnabled(false);
        return;
    }

    for (int i = 0; i < static_cast<int>(shaders.size()); ++i) {
        shaderIndexCombo->addItem(tr("Shader %1").arg(i), i);
    }

    setEnabled(true);
    shaderIndexCombo->setCurrentIndex(0);
    loadShader(0);
}

void ShaderEditorWidget::loadShader(int index) {
    if (index < 0 || index >= static_cast<int>(shaders.size())) {
        return;
    }
    updateControls(shaders[static_cast<std::size_t>(index)]);
}

void ShaderEditorWidget::updateControls(const W3dShaderStruct& shader) {
    const QSignalBlocker block0(depthCompareCombo);
    const QSignalBlocker block1(depthMaskCombo);
    const QSignalBlocker block2(destBlendCombo);
    const QSignalBlocker block3(priGradientCombo);
    const QSignalBlocker block4(secGradientCombo);
    const QSignalBlocker block5(srcBlendCombo);
    const QSignalBlocker block6(texturingCombo);
    const QSignalBlocker block7(detailColorCombo);
    const QSignalBlocker block8(detailAlphaCombo);
    const QSignalBlocker block9(alphaTestCombo);
    const QSignalBlocker block10(postDetailColorCombo);
    const QSignalBlocker block11(postDetailAlphaCombo);
    const QSignalBlocker block12(colorMaskSpin);
    const QSignalBlocker block13(fogFuncSpin);
    const QSignalBlocker block14(shaderPresetSpin);

    SetComboValue(depthCompareCombo, shader.DepthCompare);
    SetComboValue(depthMaskCombo, shader.DepthMask);
    SetComboValue(destBlendCombo, shader.DestBlend);
    SetComboValue(priGradientCombo, shader.PriGradient);
    SetComboValue(secGradientCombo, shader.SecGradient);
    SetComboValue(srcBlendCombo, shader.SrcBlend);
    SetComboValue(texturingCombo, shader.Texturing);
    SetComboValue(detailColorCombo, shader.DetailColorFunc);
    SetComboValue(detailAlphaCombo, shader.DetailAlphaFunc);
    SetComboValue(alphaTestCombo, shader.AlphaTest);
    SetComboValue(postDetailColorCombo, shader.PostDetailColorFunc);
    SetComboValue(postDetailAlphaCombo, shader.PostDetailAlphaFunc);

    colorMaskSpin->setValue(shader.ColorMask);
    fogFuncSpin->setValue(shader.FogFunc);
    shaderPresetSpin->setValue(shader.ShaderPreset);
}

W3dShaderStruct ShaderEditorWidget::captureCurrent() const {
    W3dShaderStruct shader{};
    const int idx = shaderIndexCombo->currentIndex();
    if (idx >= 0 && idx < static_cast<int>(shaders.size())) {
        shader = shaders[static_cast<std::size_t>(idx)];
    }

    shader.DepthCompare = ComboValue(depthCompareCombo);
    shader.DepthMask = ComboValue(depthMaskCombo);
    shader.DestBlend = ComboValue(destBlendCombo);
    shader.PriGradient = ComboValue(priGradientCombo);
    shader.SecGradient = ComboValue(secGradientCombo);
    shader.SrcBlend = ComboValue(srcBlendCombo);
    shader.Texturing = ComboValue(texturingCombo);
    shader.DetailColorFunc = ComboValue(detailColorCombo);
    shader.DetailAlphaFunc = ComboValue(detailAlphaCombo);
    shader.AlphaTest = ComboValue(alphaTestCombo);
    shader.PostDetailColorFunc = ComboValue(postDetailColorCombo);
    shader.PostDetailAlphaFunc = ComboValue(postDetailAlphaCombo);

    shader.ColorMask = static_cast<uint8_t>(colorMaskSpin->value());
    shader.FogFunc = static_cast<uint8_t>(fogFuncSpin->value());
    shader.ShaderPreset = static_cast<uint8_t>(shaderPresetSpin->value());

    return shader;
}

void ShaderEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    const int idx = shaderIndexCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(shaders.size())) return;

    W3dShaderStruct updated = captureCurrent();

    std::string error;
    const bool ok = W3DEdit::MutateStructAtIndex<W3dShaderStruct>(
        chunkPtr,
        static_cast<std::size_t>(idx),
        [&](W3dShaderStruct& target) { target = updated; },
        &error);

    if (!ok) {
        QMessageBox::warning(this, tr("Error"),
            QString::fromStdString(error.empty() ? "Failed to update shader." : error));
        return;
    }

    shaders[static_cast<std::size_t>(idx)] = updated;
    emit chunkEdited();
}

StringEditorWidget::StringEditorWidget(const QString& label, QWidget* parent, int maxLength)
    : QWidget(parent) {
    setEnabled(false);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* form = new QFormLayout();
    lineEdit = new QLineEdit(this);
    if (maxLength > 0) {
        lineEdit->setMaxLength(maxLength);
    }
    form->addRow(label, lineEdit);
    layout->addLayout(form);

    applyButton = new QPushButton(tr("Apply"), this);
    connect(applyButton, &QPushButton::clicked,
        this, &StringEditorWidget::applyChanges);

    layout->addStretch();
    layout->addWidget(applyButton, 0, Qt::AlignRight);
}

void StringEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    if (!chunkPtr) {
        lineEdit->clear();
        setEnabled(false);
        return;
    }

    const char* raw = reinterpret_cast<const char*>(chunkPtr->data.data());
    const auto len = chunkPtr->data.empty()
        ? 0
        : TruncatedLength(raw, chunkPtr->data.size());
    lineEdit->setText(QString::fromLatin1(raw, static_cast<int>(len)));
    setEnabled(true);
}

void StringEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    if (!W3DEdit::UpdateNullTermStringChunk(chunkPtr, lineEdit->text().toStdString())) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to update string chunk."));
        return;
    }
    emit chunkEdited();
}

MaterialEditorWidget::MaterialEditorWidget(QWidget* parent)
    : QWidget(parent) {
    setEnabled(false);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* flagsGroup = new QGroupBox(tr("Basic Flags"), this);
    auto* flagsLayout = new QVBoxLayout(flagsGroup);
    for (const auto& [mask, name] : VERTMAT_BASIC_FLAGS) {
        auto* box = new QCheckBox(QString::fromLatin1(name.data(), static_cast<int>(name.size())), flagsGroup);
        flagsLayout->addWidget(box);
        basicFlagControls.push_back({ static_cast<uint32_t>(mask), box });
    }
    layout->addWidget(flagsGroup);

    auto* stageGroup = new QGroupBox(tr("Stage Mapping"), this);
    auto* stageLayout = new QGridLayout(stageGroup);
    stage0Combo = new QComboBox(stageGroup);
    stage1Combo = new QComboBox(stageGroup);
    populateStageCombo(stage0Combo, 0);
    populateStageCombo(stage1Combo, 1);
    stageLayout->addWidget(new QLabel(tr("Stage 0"), stageGroup), 0, 0);
    stageLayout->addWidget(stage0Combo, 0, 1);
    stageLayout->addWidget(new QLabel(tr("Stage 1"), stageGroup), 1, 0);
    stageLayout->addWidget(stage1Combo, 1, 1);
    layout->addWidget(stageGroup);

    auto makeColorGroup = [&](const QString& title, ColorControls& controls) {
        auto* group = new QGroupBox(title, this);
        auto* grid = new QGridLayout(group);
        controls.r = new QSpinBox(group);
        controls.g = new QSpinBox(group);
        controls.b = new QSpinBox(group);
        for (auto* spin : { controls.r, controls.g, controls.b }) {
            spin->setRange(0, 255);
        }
        grid->addWidget(new QLabel(tr("R"), group), 0, 0);
        grid->addWidget(controls.r, 0, 1);
        grid->addWidget(new QLabel(tr("G"), group), 0, 2);
        grid->addWidget(controls.g, 0, 3);
        grid->addWidget(new QLabel(tr("B"), group), 0, 4);
        grid->addWidget(controls.b, 0, 5);
        layout->addWidget(group);
    };
    makeColorGroup(tr("Ambient"), ambient);
    makeColorGroup(tr("Diffuse"), diffuse);
    makeColorGroup(tr("Specular"), specular);
    makeColorGroup(tr("Emissive"), emissive);

    auto* floatForm = new QFormLayout();
    shininessSpin = new QDoubleSpinBox(this);
    shininessSpin->setRange(0.0, 1000.0);
    shininessSpin->setDecimals(2);
    shininessSpin->setSingleStep(1.0);

    opacitySpin = new QDoubleSpinBox(this);
    opacitySpin->setRange(0.0, 1.0);
    opacitySpin->setDecimals(3);
    opacitySpin->setSingleStep(0.05);

    translucencySpin = new QDoubleSpinBox(this);
    translucencySpin->setRange(0.0, 1.0);
    translucencySpin->setDecimals(3);
    translucencySpin->setSingleStep(0.05);

    floatForm->addRow(tr("Shininess"), shininessSpin);
    floatForm->addRow(tr("Opacity"), opacitySpin);
    floatForm->addRow(tr("Translucency"), translucencySpin);
    layout->addLayout(floatForm);

    applyButton = new QPushButton(tr("Apply Material Changes"), this);
    connect(applyButton, &QPushButton::clicked,
        this, &MaterialEditorWidget::applyChanges);

    layout->addStretch();
    layout->addWidget(applyButton, 0, Qt::AlignRight);
}

void MaterialEditorWidget::populateStageCombo(QComboBox* combo, int stage) {
    combo->clear();
    for (const auto& entry : VERTMAT_STAGE_MAPPING_CODES) {
        combo->addItem(QString::fromStdString(StageMappingName(entry.first, stage)),
            static_cast<int>(entry.first));
    }
}

void MaterialEditorWidget::setColor(const W3dRGBStruct& src, ColorControls& dest) {
    dest.r->setValue(static_cast<int>(src.R));
    dest.g->setValue(static_cast<int>(src.G));
    dest.b->setValue(static_cast<int>(src.B));
}

void MaterialEditorWidget::readColor(W3dRGBStruct& dest, const ColorControls& src) const {
    dest.R = static_cast<uint8_t>(src.r->value());
    dest.G = static_cast<uint8_t>(src.g->value());
    dest.B = static_cast<uint8_t>(src.b->value());
}

void MaterialEditorWidget::setChunk(const std::shared_ptr<ChunkItem>& chunkPtr) {
    chunk = chunkPtr;
    if (!chunkPtr) {
        setEnabled(false);
        return;
    }

    auto parsed = ParseChunkStruct<W3dVertexMaterialStruct>(chunkPtr);
    if (auto err = std::get_if<std::string>(&parsed)) {
        Q_UNUSED(err);
        setEnabled(false);
        return;
    }

    const auto& data = std::get<W3dVertexMaterialStruct>(parsed);
    for (auto& ctrl : basicFlagControls) {
        ctrl.box->setChecked((data.Attributes & ctrl.mask) != 0);
    }

    auto applyStage = [&](QComboBox* combo, int stage) {
        const uint8_t code = ExtractStageMapping(data.Attributes, stage);
        const int idx = combo->findData(static_cast<int>(code));
        if (idx >= 0) combo->setCurrentIndex(idx);
    };
    applyStage(stage0Combo, 0);
    applyStage(stage1Combo, 1);

    setColor(data.Ambient, ambient);
    setColor(data.Diffuse, diffuse);
    setColor(data.Specular, specular);
    setColor(data.Emissive, emissive);
    shininessSpin->setValue(data.Shininess);
    opacitySpin->setValue(data.Opacity);
    translucencySpin->setValue(data.Translucency);

    setEnabled(true);
}

void MaterialEditorWidget::applyChanges() {
    auto chunkPtr = chunk.lock();
    if (!chunkPtr) return;

    std::string error;
    const bool ok = W3DEdit::MutateStructChunk<W3dVertexMaterialStruct>(
        chunkPtr,
        [&](W3dVertexMaterialStruct& data) {
            uint32_t basicMask = 0;
            for (const auto& ctrl : basicFlagControls) {
                basicMask |= ctrl.mask;
            }
            uint32_t attr = data.Attributes & ~basicMask;
            attr &= ~VERTMAT_STAGE0_MAPPING_MASK;
            attr &= ~VERTMAT_STAGE1_MAPPING_MASK;

            for (const auto& ctrl : basicFlagControls) {
                if (ctrl.box->isChecked()) {
                    attr |= ctrl.mask;
                }
            }

            const uint32_t stage0 = static_cast<uint32_t>(stage0Combo->currentData().toInt() & 0xFF);
            const uint32_t stage1 = static_cast<uint32_t>(stage1Combo->currentData().toInt() & 0xFF);
            attr |= (stage0 << VERTMAT_STAGE0_MAPPING_SHIFT);
            attr |= (stage1 << VERTMAT_STAGE1_MAPPING_SHIFT);
            data.Attributes = attr;

            readColor(data.Ambient, ambient);
            readColor(data.Diffuse, diffuse);
            readColor(data.Specular, specular);
            readColor(data.Emissive, emissive);
            data.Shininess = static_cast<float>(shininessSpin->value());
            data.Opacity = static_cast<float>(opacitySpin->value());
            data.Translucency = static_cast<float>(translucencySpin->value());
        },
        &error);

    if (!ok) {
        QMessageBox::warning(this, tr("Error"),
            QString::fromStdString(error.empty() ? "Failed to update material." : error));
        return;
    }

    emit chunkEdited();
}


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* openAction = fileMenu->addAction(tr("&Open"));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() {
        openFile("");
        });

    QAction* saveAction = fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);

    QAction* saveAsAction = fileMenu->addAction(tr("Save &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);

    auto* splitter = new QSplitter(this);

    treeWidget = new QTreeWidget(splitter);
    treeWidget->setHeaderLabel(tr("Chunk Tree"));

    auto* detailContainer = new QWidget(splitter);
    auto* detailLayout = new QVBoxLayout(detailContainer);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(6);

    tableWidget = new QTableWidget(detailContainer);
    tableWidget->setColumnCount(3);
    tableWidget->setHorizontalHeaderLabels({ tr("Field"), tr("Type"), tr("Value") });
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detailLayout->addWidget(tableWidget);

    editorStack = new QStackedWidget(detailContainer);
    editorStack->setContentsMargins(0, 0, 0, 0);
    editorPlaceholder = new QWidget(editorStack);
    auto* placeholderLayout = new QVBoxLayout(editorPlaceholder);
    placeholderLayout->addStretch();
    auto* placeholderLabel = new QLabel(tr("Select a mesh, texture, or material chunk to edit."), editorPlaceholder);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLayout->addWidget(placeholderLabel);
    placeholderLayout->addStretch();
    editorStack->addWidget(editorPlaceholder);

    meshEditor = new MeshEditorWidget(editorStack);
    editorStack->addWidget(meshEditor);

    textureNameEditor = new StringEditorWidget(tr("Texture Name"), editorStack);
    editorStack->addWidget(textureNameEditor);

    materialNameEditor = new StringEditorWidget(tr("Material Name"), editorStack);
    editorStack->addWidget(materialNameEditor);

    materialEditor = new MaterialEditorWidget(editorStack);
    editorStack->addWidget(materialEditor);

    shaderEditor = new ShaderEditorWidget(editorStack);
    editorStack->addWidget(shaderEditor);

    editorStack->setCurrentWidget(editorPlaceholder);
    editorStack->setVisible(false);
    detailLayout->addWidget(editorStack);

    splitter->addWidget(treeWidget);
    splitter->addWidget(detailContainer);
    splitter->setStretchFactor(1, 1);

    setCentralWidget(splitter);

    chunkData = std::make_unique<ChunkData>();

    connect(meshEditor, &MeshEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(textureNameEditor, &StringEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(materialNameEditor, &StringEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(materialEditor, &MaterialEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);
    connect(shaderEditor, &ShaderEditorWidget::chunkEdited, this, &MainWindow::onChunkEdited);

    updateWindowTitle();
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
    if (!confirmDiscardChanges()) return;

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
    currentFilePath = filePath;
    setDirty(false);
    clearDetails();
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
    if (selectedItems.isEmpty()) {
        clearDetails();
        return;
    }

    QTreeWidgetItem* selected = selectedItems.first();
    const QVariant ptrVar = selected->data(0, Qt::UserRole);
    if (!ptrVar.isValid()) {
        clearDetails();
        return;
    }

    void* targetPtr = ptrVar.value<void*>();
    if (!targetPtr) {
        clearDetails();
        return;
    }

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
    if (!target) {
        clearDetails();
        return;
    }

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

    updateEditorForChunk(target);
}

void MainWindow::saveFile() {
    if (!chunkData || chunkData->getChunks().empty()) {
        return;
    }

    if (currentFilePath.isEmpty()) {
        saveFileAs();
        return;
    }

    if (!chunkData->saveToFile(currentFilePath.toStdString())) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to save file."));
        return;
    }

    setDirty(false);
}

void MainWindow::saveFileAs() {
    if (!chunkData || chunkData->getChunks().empty()) {
        return;
    }

    QString startDir;
    if (!currentFilePath.isEmpty()) {
        startDir = QFileInfo(currentFilePath).absolutePath();
    }
    else {
        startDir = lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory;
    }

    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save W3D File"),
        startDir,
        tr("W3D Files (*.w3d);;All Files (*)"));

    if (filePath.isEmpty()) return;

    if (!chunkData->saveToFile(filePath.toStdString())) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to save file."));
        return;
    }

    currentFilePath = filePath;
    AddRecentFile(filePath);
    lastDirectory = QFileInfo(filePath).absolutePath();
    setDirty(false);
}

void MainWindow::onChunkEdited() {
    setDirty(true);
    handleTreeSelection();
}

void MainWindow::updateEditorForChunk(const std::shared_ptr<ChunkItem>& chunk) {
    currentChunk = chunk;

    meshEditor->setChunk(nullptr);
    textureNameEditor->setChunk(nullptr);
    materialNameEditor->setChunk(nullptr);
    materialEditor->setChunk(nullptr);
    shaderEditor->setChunk(nullptr);

    if (!chunk) {
        editorStack->setCurrentWidget(editorPlaceholder);
        editorStack->setVisible(false);
        return;
    }

    switch (chunk->id) {
    case 0x001F: // W3D_CHUNK_MESH_HEADER3
        meshEditor->setChunk(chunk);
        editorStack->setCurrentWidget(meshEditor);
        editorStack->setVisible(true);
        break;
    case 0x0032: // W3D_CHUNK_TEXTURE_NAME
        textureNameEditor->setChunk(chunk);
        editorStack->setCurrentWidget(textureNameEditor);
        editorStack->setVisible(true);
        break;
    case 0x002C: // W3D_CHUNK_VERTEX_MATERIAL_NAME
        materialNameEditor->setChunk(chunk);
        editorStack->setCurrentWidget(materialNameEditor);
        editorStack->setVisible(true);
        break;
    case 0x002D: // W3D_CHUNK_VERTEX_MATERIAL_INFO
        materialEditor->setChunk(chunk);
        editorStack->setCurrentWidget(materialEditor);
        editorStack->setVisible(true);
        break;
    case 0x0029: // W3D_CHUNK_SHADERS
        shaderEditor->setChunk(chunk);
        editorStack->setCurrentWidget(shaderEditor);
        editorStack->setVisible(true);
        break;
    default:
        editorStack->setCurrentWidget(editorPlaceholder);
        editorStack->setVisible(false);
        break;
    }
}

void MainWindow::setDirty(bool value) {
    if (dirty == value) return;
    dirty = value;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
    QString title = tr("oW3DEdit");
    if (!currentFilePath.isEmpty()) {
        title += QStringLiteral(" - ") + QFileInfo(currentFilePath).fileName();
    }
    if (dirty) {
        title += QLatin1Char('*');
    }
    setWindowTitle(title);
}

bool MainWindow::confirmDiscardChanges() {
    if (!dirty) return true;

    const auto reply = QMessageBox::question(
        this,
        tr("Unsaved Changes"),
        tr("Discard unsaved changes?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    return reply == QMessageBox::Yes;
}

void MainWindow::clearDetails() {
    tableWidget->clearContents();
    tableWidget->setRowCount(0);
    updateEditorForChunk(nullptr);
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
    clearDetails();
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

