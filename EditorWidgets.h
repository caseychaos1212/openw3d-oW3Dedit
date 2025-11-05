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

class ChunkItem;
struct W3dRGBStruct;

class MeshEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit MeshEditorWidget(QWidget* parent = nullptr);
    void setChunk(const std::shared_ptr<ChunkItem>& chunk);

signals:
    void chunkEdited();

private slots:
    void applyChanges();

private:
    struct CollisionControl {
        uint32_t mask = 0;
        QCheckBox* box = nullptr;
    };

    std::weak_ptr<ChunkItem> chunk;
    QLineEdit* meshNameEdit = nullptr;
    QLineEdit* containerNameEdit = nullptr;
    QPushButton* applyButton = nullptr;
    std::vector<CollisionControl> collisionControls;
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
    ColorControls ambient{};
    ColorControls diffuse{};
    ColorControls specular{};
    ColorControls emissive{};
    QDoubleSpinBox* shininessSpin = nullptr;
    QDoubleSpinBox* opacitySpin = nullptr;
    QDoubleSpinBox* translucencySpin = nullptr;

    void populateStageCombo(QComboBox* combo, int stage);
    void setColor(const W3dRGBStruct& src, ColorControls& dest);
    void readColor(W3dRGBStruct& dest, const ColorControls& src) const;
};
