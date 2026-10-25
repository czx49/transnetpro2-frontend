/*****************************************************************//**
 * \file   tnsymbologyeditor.cpp
 * \brief  符号编辑器（FramelessDialog 深色套件）
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "tnsymbologyeditor.h"

#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"

#include <qgssymbol.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsmarkersymbol.h>
#include <qgslinesymbol.h>
#include <qgsfillsymbol.h>
#include <qgssymbolselectordialog.h>
#include <qgscolorbutton.h>
#include <qgsstyle.h>
#include <qgssymbolbutton.h>
#include "qgsrenderer.h"

#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

TNSymbologyEditor::TNSymbologyEditor(QWidget* parent)
    : FramelessDialog(parent)
{
    setupUI();
    setTitleText(QString::fromUtf8(u8"符号编辑器"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    resize(420, 320);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet());
}

TNSymbologyEditor::~TNSymbologyEditor()
{
    delete currentSymbol;
}

void TNSymbologyEditor::setupUI()
{
    QVBoxLayout* mainLayout = contentLayout();
    mainLayout->setContentsMargins(16, 12, 16, 16);
    mainLayout->setSpacing(12);
    QFormLayout* formLayout = new QFormLayout();
    formLayout->setHorizontalSpacing(12);
    formLayout->setVerticalSpacing(10);

    symbolTypeCombo = new QComboBox(this);
    symbolTypeCombo->addItem(QString::fromUtf8(u8"点"), static_cast<int>(Qgis::GeometryType::Point));
    symbolTypeCombo->addItem(QString::fromUtf8(u8"线"), static_cast<int>(Qgis::GeometryType::Line));
    symbolTypeCombo->addItem(QString::fromUtf8(u8"面"), static_cast<int>(Qgis::GeometryType::Polygon));
    Phase2Ui::setupThemedCombo(symbolTypeCombo, Phase2Ui::InputFieldTheme::LightCompact);
    formLayout->addRow(new QLabel(QString::fromUtf8(u8"符号类型："), this), symbolTypeCombo);

    symbolButton = new QgsSymbolButton(this);
    formLayout->addRow(new QLabel(QString::fromUtf8(u8"符号："), this), symbolButton);

    colorButton = new QgsColorButton(this);
    formLayout->addRow(new QLabel(QString::fromUtf8(u8"颜色："), this), colorButton);

    sizeSpinBox = new QDoubleSpinBox(this);
    sizeSpinBox->setRange(0.1, 100);
    sizeSpinBox->setValue(3.0);
    Phase2Ui::applyFieldStyle(sizeSpinBox, Phase2Ui::InputFieldTheme::LightCompact);
    formLayout->addRow(new QLabel(QString::fromUtf8(u8"大小："), this), sizeSpinBox);

    widthSpinBox = new QDoubleSpinBox(this);
    widthSpinBox->setRange(0.1, 100);
    widthSpinBox->setValue(1.0);
    Phase2Ui::applyFieldStyle(widthSpinBox, Phase2Ui::InputFieldTheme::LightCompact);
    formLayout->addRow(new QLabel(QString::fromUtf8(u8"宽度："), this), widthSpinBox);

    mainLayout->addLayout(formLayout);

    applyButton = new QPushButton(QString::fromUtf8(u8"应用"), this);
    applyButton->setCursor(Qt::PointingHandCursor);
    applyButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    mainLayout->addWidget(applyButton, 0, Qt::AlignHCenter);

    connect(symbolTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TNSymbologyEditor::createSymbolWidgets);
    connect(colorButton, &QgsColorButton::colorChanged,
            this, &TNSymbologyEditor::updateSymbolPreview);
    connect(sizeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TNSymbologyEditor::updateSymbolPreview);
    connect(widthSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TNSymbologyEditor::updateSymbolPreview);
    connect(applyButton, &QPushButton::clicked,
            this, &TNSymbologyEditor::applySymbology);
    connect(symbolButton, &QgsSymbolButton::clicked, this, [this]() {
        if (!currentSymbol) {
            return;
        }
        QgsSymbolSelectorDialog dlg(currentSymbol, QgsStyle::defaultStyle(), currentLayer, this);
        if (dlg.exec() == QDialog::Accepted) {
            delete currentSymbol;
            currentSymbol = dlg.symbol()->clone();
            updateSymbolPreview();
        }
    });
}

void TNSymbologyEditor::setLayer(QgsVectorLayer* layer)
{
    if (currentLayer == layer) {
        return;
    }

    currentLayer = layer;
    loadCurrentSymbol();
}

void TNSymbologyEditor::loadCurrentSymbol()
{
    if (!currentLayer || !currentLayer->isValid()) {
        return;
    }

    QgsFeatureRenderer* renderer = currentLayer->renderer();
    auto* singleSymbolRenderer = dynamic_cast<QgsSingleSymbolRenderer*>(renderer);
    if (!singleSymbolRenderer) {
        return;
    }

    delete currentSymbol;
    currentSymbol = singleSymbolRenderer->symbol()->clone();

    if (currentSymbol->type() == Qgis::SymbolType::Marker) {
        symbolTypeCombo->setCurrentIndex(0);
        auto* markerSymbol = static_cast<QgsMarkerSymbol*>(currentSymbol);
        colorButton->setColor(markerSymbol->color());
        sizeSpinBox->setValue(markerSymbol->size());
    } else if (currentSymbol->type() == Qgis::SymbolType::Line) {
        symbolTypeCombo->setCurrentIndex(1);
        auto* lineSymbol = static_cast<QgsLineSymbol*>(currentSymbol);
        colorButton->setColor(lineSymbol->color());
        widthSpinBox->setValue(lineSymbol->width());
    } else if (currentSymbol->type() == Qgis::SymbolType::Fill) {
        symbolTypeCombo->setCurrentIndex(2);
        auto* fillSymbol = static_cast<QgsFillSymbol*>(currentSymbol);
        colorButton->setColor(fillSymbol->color());
    }

    symbolButton->setSymbol(currentSymbol->clone());
}

void TNSymbologyEditor::createSymbolWidgets()
{
    if (!currentLayer) {
        return;
    }

    const auto geomType = static_cast<Qgis::GeometryType>(symbolTypeCombo->currentData().toInt());

    delete currentSymbol;

    switch (geomType) {
    case Qgis::GeometryType::Point:
        currentSymbol = QgsMarkerSymbol::createSimple({});
        sizeSpinBox->setEnabled(true);
        widthSpinBox->setEnabled(false);
        break;
    case Qgis::GeometryType::Line:
        currentSymbol = QgsLineSymbol::createSimple({});
        sizeSpinBox->setEnabled(false);
        widthSpinBox->setEnabled(true);
        break;
    case Qgis::GeometryType::Polygon:
        currentSymbol = QgsFillSymbol::createSimple({});
        sizeSpinBox->setEnabled(false);
        widthSpinBox->setEnabled(false);
        break;
    default:
        currentSymbol = nullptr;
        return;
    }

    symbolButton->setSymbol(currentSymbol->clone());
    updateSymbolPreview();
}

void TNSymbologyEditor::updateSymbolPreview()
{
    if (!currentSymbol) {
        return;
    }

    switch (currentSymbol->type()) {
    case Qgis::SymbolType::Marker: {
        auto* markerSymbol = static_cast<QgsMarkerSymbol*>(currentSymbol);
        markerSymbol->setColor(colorButton->color());
        markerSymbol->setSize(sizeSpinBox->value());
        break;
    }
    case Qgis::SymbolType::Line: {
        auto* lineSymbol = static_cast<QgsLineSymbol*>(currentSymbol);
        lineSymbol->setColor(colorButton->color());
        lineSymbol->setWidth(widthSpinBox->value());
        break;
    }
    case Qgis::SymbolType::Fill: {
        auto* fillSymbol = static_cast<QgsFillSymbol*>(currentSymbol);
        fillSymbol->setColor(colorButton->color());
        break;
    }
    default:
        break;
    }

    symbolButton->setSymbol(currentSymbol->clone());
}

void TNSymbologyEditor::applySymbology()
{
    if (!currentLayer || !currentSymbol) {
        return;
    }

    auto* renderer = new QgsSingleSymbolRenderer(currentSymbol->clone());
    currentLayer->setRenderer(renderer);
    currentLayer->triggerRepaint();
}
