#ifndef TN_SYMBOLOGYEDITOR_H
#define TN_SYMBOLOGYEDITOR_H

#include "framelessdialog.h"
#include <qgsvectorlayer.h>
#include <qgssymbol.h>

class QComboBox;
class QgsSymbolButton;
class QgsColorButton;
class QDoubleSpinBox;

class TNSymbologyEditor : public FramelessDialog
{
    Q_OBJECT

public:
    explicit TNSymbologyEditor(QWidget* parent = nullptr);
    ~TNSymbologyEditor();

    void setLayer(QgsVectorLayer* layer);

private slots:
    void applySymbology();
    void updateSymbolPreview();

private:
    void setupUI();
    void loadCurrentSymbol();
    void createSymbolWidgets();

    QgsVectorLayer* currentLayer = nullptr;
    QgsSymbol* currentSymbol = nullptr;

    QComboBox* symbolTypeCombo;
    QgsSymbolButton* symbolButton;
    QgsColorButton* colorButton;
    QDoubleSpinBox* sizeSpinBox;
    QDoubleSpinBox* widthSpinBox;
    QPushButton* applyButton;
};

//中文
#endif // TN_SYMBOLOGYEDITOR_H