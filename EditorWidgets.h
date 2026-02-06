#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <QWidget>
#include <QString>

class QLineEdit;
class QPushButton;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class QFormLayout;
class QVBoxLayout;
class QPlainTextEdit;

#include "backend/W3DStructs.h"
#include "backend/ChunkItem.h"

class ChunkItem;

class MeshEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit MeshEditorWidget(QWidget* parent = nullptr);
    void setChunk(const std::shared_ptr<ChunkItem>& chunk);

signals:
    void chunkEdited();
    void meshRenamed(const QString& oldMeshName,
        const QString& newMeshName,
        const QString& oldContainerName,
        const QString& newContainerName);

private slots:
    void applyChanges();

private:
    struct FlagControl {
        uint32_t mask = 0;
        QCheckBox* box = nullptr;
    };

    std::weak_ptr<ChunkItem> chunk;
    QLineEdit* meshNameEdit = nullptr;
    QLineEdit* containerNameEdit = nullptr;
    QPushButton* applyButton = nullptr;
    std::vector<FlagControl> flagControls;
};

class StringEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit StringEditorWidget(const QString& label,
        QWidget* parent = nullptr,
        int maxLength = 0);
    void setChunk(const std::shared_ptr<ChunkItem>& chunk);

signals:
    void chunkEdited();

private slots:
    void applyChanges();

private:
    std::weak_ptr<ChunkItem> chunk;
    QLineEdit* lineEdit = nullptr;
    QPushButton* applyButton = nullptr;
};

class MapperArgsEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit MapperArgsEditorWidget(const QString& label, QWidget* parent = nullptr);
    void setChunk(const std::shared_ptr<ChunkItem>& chunk);

signals:
    void chunkEdited();

private slots:
    void applyChanges();

private:
    std::weak_ptr<ChunkItem> chunk;
    QPlainTextEdit* argsEdit = nullptr;
    QPlainTextEdit* referenceEdit = nullptr;
    QPushButton* applyButton = nullptr;
};

class MaterialEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit MaterialEditorWidget(QWidget* parent = nullptr);
    void setChunk(const std::shared_ptr<ChunkItem>& chunk);

signals:
    void chunkEdited();

private slots:
    void applyChanges();

private:
    struct ColorControls {
        QSpinBox* r = nullptr;
        QSpinBox* g = nullptr;
        QSpinBox* b = nullptr;
    };
    struct FlagControl {
        uint32_t mask = 0;
        QCheckBox* box = nullptr;
    };

    std::weak_ptr<ChunkItem> chunk;
    QPushButton* applyButton = nullptr;
    std::vector<FlagControl> basicFlagControls;
    QComboBox* stage0Combo = nullptr;
    QComboBox* stage1Combo = nullptr;
    QSpinBox* stage0CodeSpin = nullptr;
    QSpinBox* stage1CodeSpin = nullptr;
    int stage0UnknownIndex = -1;
    int stage1UnknownIndex = -1;
    ColorControls ambient{};
    ColorControls diffuse{};
    ColorControls specular{};
    ColorControls emissive{};
    QDoubleSpinBox* shininessSpin = nullptr;
    QDoubleSpinBox* opacitySpin = nullptr;
    QDoubleSpinBox* translucencySpin = nullptr;

    void populateStageCombo(QComboBox* combo, int stage);
    void setStageMapping(QComboBox* combo, QSpinBox* spin, int stage, int& unknownIndex, uint8_t code);
    void setColor(const W3dRGBStruct& src, ColorControls& dest);
    void readColor(W3dRGBStruct& dest, const ColorControls& src) const;
};

class ShaderEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit ShaderEditorWidget(QWidget* parent = nullptr);
    void setChunk(const std::shared_ptr<ChunkItem>& chunk);

signals:
    void chunkEdited();

private slots:
    void applyChanges();

private:
    void loadShader(int index);
    void updateControls(const W3dShaderStruct& shader);
    W3dShaderStruct captureCurrent() const;

    std::weak_ptr<ChunkItem> chunk;
    std::vector<W3dShaderStruct> shaders;

    QComboBox* shaderIndexCombo = nullptr;
    QComboBox* depthCompareCombo = nullptr;
    QComboBox* depthMaskCombo = nullptr;
    QComboBox* destBlendCombo = nullptr;
    QComboBox* priGradientCombo = nullptr;
    QComboBox* secGradientCombo = nullptr;
    QComboBox* srcBlendCombo = nullptr;
    QComboBox* texturingCombo = nullptr;
    QComboBox* detailColorCombo = nullptr;
    QComboBox* detailAlphaCombo = nullptr;
    QComboBox* alphaTestCombo = nullptr;
    QComboBox* postDetailColorCombo = nullptr;
    QComboBox* postDetailAlphaCombo = nullptr;

    QSpinBox* colorMaskSpin = nullptr;
    QSpinBox* fogFuncSpin = nullptr;
    QSpinBox* shaderPresetSpin = nullptr;

    QPushButton* applyButton = nullptr;
};

class SurfaceTypeEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit SurfaceTypeEditorWidget(QWidget* parent = nullptr);
    void setChunk(const std::shared_ptr<ChunkItem>& chunk);

signals:
    void chunkEdited();

private slots:
    void applyChanges();

private:
    std::weak_ptr<ChunkItem> chunk;
    QComboBox* surfaceTypeCombo = nullptr;
    QPushButton* applyButton = nullptr;
};

class TriangleSurfaceTypeEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit TriangleSurfaceTypeEditorWidget(QWidget* parent = nullptr);
    void setChunk(const std::shared_ptr<ChunkItem>& chunk);

signals:
    void chunkEdited();

private slots:
    void applyChanges();
    void updateStats();

private:
    std::weak_ptr<ChunkItem> chunk;
    QComboBox* fromCombo = nullptr;
    QComboBox* toCombo = nullptr;
    QLabel* statsLabel = nullptr;
    QPushButton* applyButton = nullptr;
};
