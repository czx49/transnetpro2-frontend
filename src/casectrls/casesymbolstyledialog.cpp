/*****************************************************************//**
 * \file   casesymbolstyledialog.cpp
 * \brief  字段分级符号化设置
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "casesymbolstyledialog.h"

#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"
#include "../tnmapwidget/layerstylerenderer.h"
#include "../tnmapwidget/symbolrendererconfig.h"
#include "../tnmapwidget/tnmapwidget.h"
#include "casemaplegendwidget.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSize>
#include <QVBoxLayout>
#include <QtGlobal>

#include <algorithm>
#include <memory>

#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsgraduatedsymbolrenderer.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgssymbolselectordialog.h>
#include <qgsstyle.h>
#include <qgsvectorlayer.h>

QString CaseSymbolStyleDialog::layerStorageKey(QgsVectorLayer* layer)
{
    return LayerStylePersistence::layerStorageKey(layer);
}

QPixmap CaseSymbolStyleDialog::colorRampPreviewPixmap(const QString& colorRampName, bool inverted, int width, int height)
{
    QPixmap pixmap(qMax(1, width), qMax(1, height));
    pixmap.fill(Qt::transparent);

    auto ramp = SymbolRendererConfig::createColorRamp(colorRampName);
    if (!ramp) {
        return pixmap;
    }
    if (inverted) {
        ramp->invert();
    }

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, false);
    for (int x = 0; x < pixmap.width(); ++x) {
        const double t = (pixmap.width() <= 1) ? 0.0
                                               : static_cast<double>(x) / static_cast<double>(pixmap.width() - 1);
        painter.setPen(ramp->color(t));
        painter.drawLine(x, 0, x, pixmap.height() - 1);
    }
    painter.end();
    return pixmap;
}

CaseSymbolStyleDialog::CaseSymbolStyleDialog(TNMapWidget* mapWidget, int projectId, int caseId, QWidget* parent)
    : FramelessDialog(parent)
    , m_mapWidget(mapWidget)
    , m_projectId(projectId)
    , m_caseId(caseId)
    , m_layer(nullptr)
    , m_layerCombo(nullptr)
    , m_fieldCombo(nullptr)
    , m_classCombo(nullptr)
    , m_colorRampCombo(nullptr)
    , m_rampInvertCheck(nullptr)
    , m_opacitySpin(nullptr)
    , m_lineWidthSpin(nullptr)
    , m_colorList(nullptr)
    , m_statusLabel(nullptr)
{
    if (m_mapWidget) {
        m_mapWidget->setStyleContext(m_projectId, m_caseId);
    }

    setTitleText(QString::fromUtf8(u8"符号化设置"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    resize(640, 560);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                  + QStringLiteral(
                      "#framelessDialogContent QWidget { background: transparent; }"
                      "#framelessDialogContent QLabel { background: transparent; color: #ffffff; }"));
    initUi();
}

CaseSymbolStyleDialog::~CaseSymbolStyleDialog() = default;

QgsVectorLayer* CaseSymbolStyleDialog::vectorLayerById(const QString& layerId) const
{
    if (!m_mapWidget || !m_mapWidget->canvas()) {
        return nullptr;
    }

    const QList<QgsMapLayer*> layers = m_mapWidget->canvas()->layers();
    for (QgsMapLayer* layer : layers) {
        auto* vectorLayer = qobject_cast<QgsVectorLayer*>(layer);
        if (vectorLayer && vectorLayer->id() == layerId) {
            return vectorLayer;
        }
    }
    return nullptr;
}

void CaseSymbolStyleDialog::refreshLayerCombo()
{
    if (!m_layerCombo) {
        return;
    }

    const QSignalBlocker blocker(m_layerCombo);
    m_layerCombo->clear();
    m_layer = nullptr;

    if (!m_mapWidget || !m_mapWidget->canvas()) {
        return;
    }

    QgsMapLayer* selected = m_mapWidget->selectedLayer();
    auto* selectedVector = qobject_cast<QgsVectorLayer*>(selected);
    int preferredIndex = 0;

    const QList<QgsMapLayer*> layers = m_mapWidget->canvas()->layers();
    for (QgsMapLayer* layer : layers) {
        auto* vectorLayer = qobject_cast<QgsVectorLayer*>(layer);
        if (!vectorLayer || vectorLayer->geometryType() == Qgis::GeometryType::Null) {
            continue;
        }

        m_layerCombo->addItem(vectorLayer->name(), vectorLayer->id());
        if (selectedVector && vectorLayer == selectedVector) {
            preferredIndex = m_layerCombo->count() - 1;
        }
    }

    if (m_layerCombo->count() > 0) {
        m_layerCombo->setCurrentIndex(preferredIndex);
        m_layer = vectorLayerById(m_layerCombo->currentData().toString());
    }
}

void CaseSymbolStyleDialog::refreshFieldCombo()
{
    if (!m_fieldCombo) {
        return;
    }

    const QSignalBlocker blocker(m_fieldCombo);
    m_fieldCombo->clear();
    m_fieldRule.reset();

    if (!m_layer) {
        updateColorList();
        return;
    }

    const QgsFields fields = m_layer->fields();
    for (int i = 0; i < fields.count(); ++i) {
        const QgsField field = fields.at(i);
        if (!LayerStyleRenderer::isNumericFieldType(field.type())) {
            continue;
        }

        const QString label = field.alias().trimmed().isEmpty() ? field.name() : field.alias();
        m_fieldCombo->addItem(label, field.name());
    }
}

void CaseSymbolStyleDialog::populateColorRampCombo()
{
    if (!m_colorRampCombo) {
        return;
    }

    const QSignalBlocker blocker(m_colorRampCombo);
    m_colorRampCombo->clear();
    const bool inverted = currentRampInverted();
    for (const QString& rampName : SymbolRendererConfig::availableColorRampNames()) {
        m_colorRampCombo->addItem(SymbolRendererConfig::colorRampDisplayName(rampName), rampName);
        const int idx = m_colorRampCombo->count() - 1;
        m_colorRampCombo->setItemIcon(idx, QIcon(colorRampPreviewPixmap(rampName, inverted)));
    }
    const int defaultIdx = m_colorRampCombo->findData(QStringLiteral("RdYlGn"));
    if (defaultIdx >= 0) {
        m_colorRampCombo->setCurrentIndex(defaultIdx);
    }
}

void CaseSymbolStyleDialog::syncColorRampControlsFromRule()
{
    if (m_colorRampCombo) {
        const QSignalBlocker blocker(m_colorRampCombo);
        int idx = m_colorRampCombo->findData(m_fieldRule.colorRampName);
        if (idx < 0) {
            idx = m_colorRampCombo->findData(QStringLiteral("RdYlGn"));
        }
        if (idx >= 0) {
            m_colorRampCombo->setCurrentIndex(idx);
        }
    }
    if (m_rampInvertCheck) {
        const QSignalBlocker blocker(m_rampInvertCheck);
        m_rampInvertCheck->setChecked(m_fieldRule.rampInverted);
    }
    if (m_colorRampCombo) {
        const bool inverted = currentRampInverted();
        for (int i = 0; i < m_colorRampCombo->count(); ++i) {
            const QString rampName = m_colorRampCombo->itemData(i).toString();
            m_colorRampCombo->setItemIcon(i, QIcon(colorRampPreviewPixmap(rampName, inverted)));
        }
    }
}

void CaseSymbolStyleDialog::initUi()
{
    QVBoxLayout* root = contentLayout();
    root->setSpacing(12);
    root->setContentsMargins(16, 12, 16, 16);

    auto* formGroup = new QGroupBox(QString::fromUtf8(u8"字段分级渲染"), this);
    formGroup->setStyleSheet(QStringLiteral(
        "QGroupBox { color: #ffffff; font-weight: 700; border: 1px solid rgba(255,255,255,0.28); "
        "border-radius: 10px; margin-top: 10px; padding: 10px; background: transparent; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; color: #ffffff; }"));
    auto* formLayout = new QGridLayout(formGroup);
    formLayout->setContentsMargins(12, 16, 12, 12);
    formLayout->setHorizontalSpacing(10);
    formLayout->setVerticalSpacing(10);

    m_layerCombo = new QComboBox(formGroup);
    m_fieldCombo = new QComboBox(formGroup);
    m_classCombo = new QComboBox(formGroup);
    m_colorRampCombo = new QComboBox(formGroup);
    m_rampInvertCheck = new QCheckBox(QString::fromUtf8(u8"反转色带"), formGroup);
    m_opacitySpin = new QDoubleSpinBox(formGroup);
    m_lineWidthSpin = new QDoubleSpinBox(formGroup);

    for (QComboBox* combo : {m_layerCombo, m_fieldCombo, m_classCombo, m_colorRampCombo}) {
        combo->setMinimumHeight(30);
        Phase2Ui::setupThemedCombo(combo, Phase2Ui::InputFieldTheme::LightCompactSoft);
    }
    for (int i = 1; i <= 10; ++i) {
        m_classCombo->addItem(QString::number(i), i);
    }
    m_classCombo->setCurrentIndex(4);
    m_colorRampCombo->setIconSize(QSize(72, 16));
    populateColorRampCombo();

    m_rampInvertCheck->setCursor(Qt::PointingHandCursor);
    m_rampInvertCheck->setStyleSheet(QStringLiteral(
        "QCheckBox { color: #ffffff; spacing: 8px; background: transparent; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }"));

    m_opacitySpin->setRange(0.0, 100.0);
    m_opacitySpin->setDecimals(0);
    m_opacitySpin->setValue(100.0);
    m_opacitySpin->setSuffix(QStringLiteral("%"));
    m_opacitySpin->setMinimumHeight(30);
    Phase2Ui::applyFieldStyle(m_opacitySpin, Phase2Ui::InputFieldTheme::LightCompactSoft);

    m_lineWidthSpin->setRange(0.0, 20.0);
    m_lineWidthSpin->setDecimals(2);
    m_lineWidthSpin->setSingleStep(0.1);
    m_lineWidthSpin->setValue(0.8);
    m_lineWidthSpin->setMinimumHeight(30);
    Phase2Ui::applyFieldStyle(m_lineWidthSpin, Phase2Ui::InputFieldTheme::LightCompactSoft);

    formLayout->addWidget(new QLabel(QString::fromUtf8(u8"图层："), formGroup), 0, 0);
    formLayout->addWidget(m_layerCombo, 0, 1, 1, 3);
    formLayout->addWidget(new QLabel(QString::fromUtf8(u8"数字字段："), formGroup), 1, 0);
    formLayout->addWidget(m_fieldCombo, 1, 1, 1, 3);
    formLayout->addWidget(new QLabel(QString::fromUtf8(u8"分级数："), formGroup), 2, 0);
    formLayout->addWidget(m_classCombo, 2, 1);
    formLayout->addWidget(new QLabel(QString::fromUtf8(u8"透明度："), formGroup), 2, 2);
    formLayout->addWidget(m_opacitySpin, 2, 3);
    formLayout->addWidget(new QLabel(QString::fromUtf8(u8"线宽："), formGroup), 3, 0);
    formLayout->addWidget(m_lineWidthSpin, 3, 1);
    formLayout->addWidget(new QLabel(QString::fromUtf8(u8"色带："), formGroup), 3, 2);
    formLayout->addWidget(m_colorRampCombo, 3, 3);
    formLayout->addWidget(m_rampInvertCheck, 4, 3);
    root->addWidget(formGroup);

    m_colorList = new QListWidget(this);
    m_colorList->setMinimumHeight(160);
    m_colorList->setFocusPolicy(Qt::NoFocus);
    m_colorList->setStyleSheet(QStringLiteral(
        "QListWidget { background: #ffffff; border: 1px solid #c7d8ee; border-radius: 10px; padding: 6px; }"
        "QListWidget::item { color: #0f172a; background: #ffffff; border-radius: 6px; padding: 6px 8px; margin: 2px; }"
        "QListWidget::item:hover { background: #eff6ff; color: #0f172a; }"
        "QListWidget::item:selected { background: #dbeafe; color: #0f172a; }"));
    root->addWidget(m_colorList, 1);

    auto* fieldButtonRow = new QHBoxLayout();
    QPushButton* saveBtn = new QPushButton(QString::fromUtf8(u8"保存字段样式"), this);
    QPushButton* loadBtn = new QPushButton(QString::fromUtf8(u8"加载字段样式"), this);
    QPushButton* clearBtn = new QPushButton(QString::fromUtf8(u8"清除字段样式"), this);
    QPushButton* editBtn = new QPushButton(QString::fromUtf8(u8"编辑单符号"), this);
    for (QPushButton* button : {saveBtn, loadBtn, clearBtn, editBtn}) {
        button->setStyleSheet(Phase2Ui::dialogSecondaryButtonStyleSheet());
    }
    for (QPushButton* button : {saveBtn, loadBtn, clearBtn, editBtn}) {
        button->setCursor(Qt::PointingHandCursor);
        fieldButtonRow->addWidget(button);
    }
    fieldButtonRow->addStretch(1);
    root->addLayout(fieldButtonRow);

    m_statusLabel = new QLabel(QString::fromUtf8(u8"选择数字字段后，可生成分级渲染并保存为字段样式。"), this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("QLabel { color: #cbd5e1; font-size: 12px; background: transparent; }"));

    auto* bottomRow = new QHBoxLayout();
    QPushButton* applyBtn = new QPushButton(QString::fromUtf8(u8"应用"), this);
    QPushButton* closeBtn = new QPushButton(QString::fromUtf8(u8"关闭"), this);
    applyBtn->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    closeBtn->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    applyBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setCursor(Qt::PointingHandCursor);
    bottomRow->addWidget(m_statusLabel, 1);
    bottomRow->addWidget(applyBtn);
    bottomRow->addWidget(closeBtn);
    root->addLayout(bottomRow);

    connect(m_layerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CaseSymbolStyleDialog::onLayerChanged);
    connect(m_fieldCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CaseSymbolStyleDialog::onFieldChanged);
    connect(m_classCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CaseSymbolStyleDialog::onClassLevelChanged);
    connect(m_colorRampCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CaseSymbolStyleDialog::onColorRampChanged);
    connect(m_rampInvertCheck, &QCheckBox::stateChanged, this, &CaseSymbolStyleDialog::onRampInvertedChanged);
    connect(m_colorList, &QListWidget::itemClicked, this, &CaseSymbolStyleDialog::onColorItemClicked);
    connect(m_opacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        m_fieldRule.opacity = value;
        applyCurrentFieldRule();
    });
    connect(m_lineWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        m_fieldRule.lineWidth = value;
        applyCurrentFieldRule();
    });
    connect(applyBtn, &QPushButton::clicked, this, &CaseSymbolStyleDialog::onApplyFieldRuleClicked);
    connect(saveBtn, &QPushButton::clicked, this, &CaseSymbolStyleDialog::onSaveFieldRuleClicked);
    connect(loadBtn, &QPushButton::clicked, this, &CaseSymbolStyleDialog::onLoadFieldRuleClicked);
    connect(clearBtn, &QPushButton::clicked, this, &CaseSymbolStyleDialog::onClearFieldRuleClicked);
    connect(editBtn, &QPushButton::clicked, this, &CaseSymbolStyleDialog::onEditSymbolClicked);
    connect(closeBtn, &QPushButton::clicked, this, &CaseSymbolStyleDialog::accept);

    refreshLayerCombo();
    refreshFieldCombo();
    loadCurrentFieldRule(true);

    if (m_layerCombo->count() == 0 && m_statusLabel) {
        m_statusLabel->setText(QString::fromUtf8(u8"当前地图无可符号化的矢量图层。"));
    } else if (m_fieldCombo->count() == 0 && m_statusLabel) {
        m_statusLabel->setText(QString::fromUtf8(u8"当前图层没有可用于分级渲染的数字字段。"));
    }
}

void CaseSymbolStyleDialog::onLayerChanged(int index)
{
    Q_UNUSED(index)
    m_layer = m_layerCombo ? vectorLayerById(m_layerCombo->currentData().toString()) : nullptr;
    refreshFieldCombo();
    loadCurrentFieldRule(true);
}

void CaseSymbolStyleDialog::onFieldChanged(int index)
{
    Q_UNUSED(index)
    loadCurrentFieldRule(true);
}

void CaseSymbolStyleDialog::onClassLevelChanged(int index)
{
    Q_UNUSED(index)
    if (generateFieldColorRule()) {
        updateColorList();
        applyCurrentFieldRule();
    }
}

void CaseSymbolStyleDialog::onColorRampChanged(int index)
{
    Q_UNUSED(index)
    if (generateFieldColorRule()) {
        updateColorList();
        applyCurrentFieldRule();
    }
}

void CaseSymbolStyleDialog::onRampInvertedChanged(int state)
{
    Q_UNUSED(state)
    if (m_colorRampCombo) {
        const bool inverted = currentRampInverted();
        for (int i = 0; i < m_colorRampCombo->count(); ++i) {
            const QString rampName = m_colorRampCombo->itemData(i).toString();
            m_colorRampCombo->setItemIcon(i, QIcon(colorRampPreviewPixmap(rampName, inverted)));
        }
    }
    if (generateFieldColorRule()) {
        updateColorList();
        applyCurrentFieldRule();
    }
}

void CaseSymbolStyleDialog::onColorItemClicked()
{
    if (!m_colorList) {
        return;
    }
    const int row = m_colorList->currentRow();
    if (row < 0 || row >= m_fieldRule.colors.size()) {
        return;
    }

    FieldColorRuleItem& item = m_fieldRule.colors[row];
    const QColor color = QColorDialog::getColor(QColor(item.color), this, QString::fromUtf8(u8"选择分级颜色"));
    if (!color.isValid()) {
        return;
    }

    item.color = color.name();
    updateColorList();
    applyCurrentFieldRule();
}

void CaseSymbolStyleDialog::onApplyFieldRuleClicked()
{
    if (!m_fieldRule.isValid() && !generateFieldColorRule()) {
        return;
    }
    applyCurrentFieldRule();
}

void CaseSymbolStyleDialog::onSaveFieldRuleClicked()
{
    if (!m_fieldRule.isValid() && !generateFieldColorRule()) {
        return;
    }
    if (!applyCurrentFieldRule()) {
        return;
    }

    QString errorMessage;
    const bool saved = LayerStylePersistence::instance().saveFieldColorRule(
        m_projectId,
        m_caseId,
        currentLayerKey(),
        currentFieldName(),
        m_fieldRule,
        &errorMessage);
    if (m_statusLabel) {
        m_statusLabel->setText(saved ? QString::fromUtf8(u8"字段样式已保存。") : errorMessage);
    }
}

void CaseSymbolStyleDialog::onLoadFieldRuleClicked()
{
    loadCurrentFieldRule(true);
}

void CaseSymbolStyleDialog::onClearFieldRuleClicked()
{
    const QString layerKey = currentLayerKey();
    const QString fieldName = currentFieldName();
    if (layerKey.isEmpty() || fieldName.isEmpty()) {
        if (m_statusLabel) {
            m_statusLabel->setText(QString::fromUtf8(u8"请先选择图层和字段。"));
        }
        return;
    }

    QString errorMessage;
    const bool deleted = LayerStylePersistence::instance().deleteFieldColorRule(
        m_projectId, m_caseId, layerKey, fieldName, &errorMessage);
    if (deleted && m_layer) {
        TNMapWidget::setDefaultRendererForVectorLayer(m_layer);
        m_layer->triggerRepaint();
        if (m_mapWidget && m_mapWidget->canvas()) {
            m_mapWidget->canvas()->refresh();
        }
        m_fieldRule.reset();
        syncColorRampControlsFromRule();
        updateColorList();
    }
    if (m_statusLabel) {
        m_statusLabel->setText(deleted ? QString::fromUtf8(u8"字段样式已清除。") : errorMessage);
    }
}

void CaseSymbolStyleDialog::onEditSymbolClicked()
{
    if (!m_layer || !m_layer->renderer()) {
        if (m_statusLabel) {
            m_statusLabel->setText(QString::fromUtf8(u8"请先在下拉框中选择矢量图层。"));
        }
        return;
    }

    auto* single = dynamic_cast<QgsSingleSymbolRenderer*>(m_layer->renderer());
    if (!single || !single->symbol()) {
        if (m_statusLabel) {
            m_statusLabel->setText(QString::fromUtf8(u8"仅支持单一符号渲染器的可视化编辑；分级渲染请使用字段样式。"));
        }
        return;
    }

    std::unique_ptr<QgsSymbol> clone(single->symbol()->clone());
    QgsSymbolSelectorDialog dlg(clone.get(), QgsStyle::defaultStyle(), m_layer, nullptr);
    dlg.setWindowTitle(QString::fromUtf8(u8"符号编辑器"));
    if (dlg.exec() == QDialog::Accepted) {
        single->setSymbol(clone.release());
        m_layer->triggerRepaint();
        if (m_statusLabel) {
            m_statusLabel->setText(QString::fromUtf8(u8"单符号已更新（仅当前图层会话生效）。"));
        }
    }
}

void CaseSymbolStyleDialog::loadCurrentFieldRule(bool applyAfterLoad)
{
    m_fieldRule.reset();
    updateColorList();

    if (!m_layer || currentFieldName().isEmpty()) {
        return;
    }

    FieldColorRule loadedRule;
    QString errorMessage;
    const bool loaded = LayerStylePersistence::instance().loadFieldColorRule(
        m_projectId,
        m_caseId,
        currentLayerKey(),
        currentFieldName(),
        &loadedRule,
        &errorMessage);
    if (loaded) {
        m_fieldRule = loadedRule;
        {
            const QSignalBlocker blocker(m_classCombo);
            const int idx = m_classCombo->findData(m_fieldRule.classLevel);
            if (idx >= 0) {
                m_classCombo->setCurrentIndex(idx);
            }
        }
        {
            const QSignalBlocker blocker(m_opacitySpin);
            m_opacitySpin->setValue(m_fieldRule.opacity);
        }
        {
            const QSignalBlocker blocker(m_lineWidthSpin);
            m_lineWidthSpin->setValue(m_fieldRule.lineWidth);
        }
        syncColorRampControlsFromRule();
        updateColorList();
        if (applyAfterLoad) {
            applyCurrentFieldRule();
        }
        if (m_statusLabel) {
            m_statusLabel->setText(QString::fromUtf8(u8"已加载字段样式。"));
        }
        return;
    }

    if (!errorMessage.trimmed().isEmpty()) {
        qDebug() << errorMessage;
    }
    if (generateFieldColorRule()) {
        updateColorList();
        if (applyAfterLoad) {
            applyCurrentFieldRule();
        }
        if (m_statusLabel) {
            m_statusLabel->setText(QString::fromUtf8(u8"当前字段暂无已存样式，已生成预览分级。"));
        }
    }
}

bool CaseSymbolStyleDialog::generateFieldColorRule()
{
    m_fieldRule.reset();

    const QString fieldName = currentFieldName();
    if (!m_layer || fieldName.isEmpty()) {
        if (m_statusLabel) {
            m_statusLabel->setText(QString::fromUtf8(u8"请先选择图层和数字字段。"));
        }
        return false;
    }

    QList<double> values = numericFieldValues(fieldName);
    if (values.isEmpty()) {
        if (m_statusLabel) {
            m_statusLabel->setText(QString::fromUtf8(u8"当前字段没有可用于分级的数字值。"));
        }
        return false;
    }

    m_fieldRule.category = FieldStyleCategory::Number;
    m_fieldRule.opacity = m_opacitySpin ? m_opacitySpin->value() : 100.0;
    m_fieldRule.lineWidth = m_lineWidthSpin ? m_lineWidthSpin->value() : 0.8;
    m_fieldRule.classLevel = m_classCombo ? m_classCombo->currentData().toInt() : 5;
    m_fieldRule.colorRampName = currentColorRampName();
    m_fieldRule.rampInverted = currentRampInverted();

    auto ramp = SymbolRendererConfig::createColorRamp(m_fieldRule.colorRampName);
    if (!ramp) {
        if (m_statusLabel) {
            m_statusLabel->setText(QString::fromUtf8(u8"色带创建失败。"));
        }
        return false;
    }
    if (m_fieldRule.rampInverted) {
        ramp->invert();
    }

    const int intervalCount = qMin(m_fieldRule.classLevel, values.size());
    const double minValue = values.first();
    const double maxValue = values.last();
    if (qAbs(maxValue - minValue) <= 1e-9) {
        FieldColorRuleItem item;
        item.color = ramp->color(0.0).name();
        item.min = minValue;
        item.max = maxValue;
        m_fieldRule.colors.append(item);
        return true;
    }

    const double step = (maxValue - minValue) / intervalCount;
    for (int i = 0; i < intervalCount; ++i) {
        FieldColorRuleItem item;
        item.min = (i == 0) ? minValue : (minValue + step * i - 1e-6);
        item.max = (i == intervalCount - 1) ? maxValue : (minValue + step * (i + 1));
        if (item.min >= item.max) {
            item.min = item.max - 1e-6;
        }

        const double t = (intervalCount <= 1)
            ? 0.0
            : static_cast<double>(i) / static_cast<double>(intervalCount - 1);
        item.color = ramp->color(t).name();
        m_fieldRule.colors.append(item);
    }

    return true;
}

bool CaseSymbolStyleDialog::applyCurrentFieldRule()
{
    if (!m_layer || !m_fieldRule.isValid() || currentFieldName().isEmpty()) {
        return false;
    }

    m_fieldRule.opacity = m_opacitySpin ? m_opacitySpin->value() : m_fieldRule.opacity;
    m_fieldRule.lineWidth = m_lineWidthSpin ? m_lineWidthSpin->value() : m_fieldRule.lineWidth;
    QString errorMessage;
    const bool applied = LayerStyleRenderer::applyFieldColorRule(m_layer, currentFieldName(), m_fieldRule, &errorMessage);
    if (applied && m_mapWidget && m_mapWidget->canvas()) {
        m_mapWidget->canvas()->refresh();
        syncLegendFromAppliedRule();
    }
    if (!applied && m_statusLabel && !errorMessage.trimmed().isEmpty()) {
        m_statusLabel->setText(errorMessage);
    }
    return applied;
}

void CaseSymbolStyleDialog::syncLegendFromAppliedRule()
{
    // 字段符号化生效后同步地图左下角图例（标题"图例：字段名"，条目不再重复字段名）
    if (!m_mapWidget || !m_layer) {
        return;
    }
    auto* legend = qobject_cast<CaseMapLegendWidget*>(m_mapWidget->bottomLeftOverlayWidget());
    if (!legend) {
        return;
    }

    const QString fieldName = currentFieldName();
    auto* graduated = dynamic_cast<QgsGraduatedSymbolRenderer*>(m_layer->renderer());
    if (graduated) {
        CaseMapLegend::showCustom(legend,
                                  CaseMapLegend::itemsFromGraduatedRenderer(graduated, fieldName),
                                  CaseMapLegend::graduatedLegendTitle(fieldName));
    } else {
        auto* categorized = dynamic_cast<QgsCategorizedSymbolRenderer*>(m_layer->renderer());
        if (categorized) {
            CaseMapLegend::showCustom(legend,
                                      CaseMapLegend::itemsFromCategorizedRenderer(categorized, fieldName),
                                      CaseMapLegend::graduatedLegendTitle(fieldName));
        }
    }
    // 图例内容变化后同步刷新浮层几何，避免保存画布抓图时被截断
    m_mapWidget->refreshBottomLeftOverlay();
}

void CaseSymbolStyleDialog::updateColorList()
{
    if (!m_colorList) {
        return;
    }
    m_colorList->clear();

    for (const FieldColorRuleItem& item : m_fieldRule.colors) {
        auto* listItem = new QListWidgetItem(item.label(m_fieldRule.category), m_colorList);
        const QColor color(item.color);
        if (color.isValid()) {
            QPixmap pixmap(18, 18);
            pixmap.fill(color);
            listItem->setIcon(QIcon(pixmap));
        }
        listItem->setToolTip(QString::fromUtf8(u8"点击修改该分级颜色"));
    }
}

QString CaseSymbolStyleDialog::currentFieldName() const
{
    return m_fieldCombo ? m_fieldCombo->currentData().toString().trimmed() : QString();
}

QString CaseSymbolStyleDialog::currentLayerKey() const
{
    return layerStorageKey(m_layer);
}

QString CaseSymbolStyleDialog::currentColorRampName() const
{
    if (!m_colorRampCombo) {
        return QStringLiteral("RdYlGn");
    }
    const QString name = m_colorRampCombo->currentData().toString().trimmed();
    return name.isEmpty() ? QStringLiteral("RdYlGn") : name;
}

bool CaseSymbolStyleDialog::currentRampInverted() const
{
    return m_rampInvertCheck && m_rampInvertCheck->isChecked();
}

QList<double> CaseSymbolStyleDialog::numericFieldValues(const QString& fieldName) const
{
    QList<double> values;
    if (!m_layer || fieldName.trimmed().isEmpty()) {
        return values;
    }

    const int fieldIndex = m_layer->fields().indexFromName(fieldName);
    if (fieldIndex < 0) {
        return values;
    }

    QgsFeatureIterator it = m_layer->getFeatures();
    QgsFeature feature;
    while (it.nextFeature(feature)) {
        const QVariant value = feature.attribute(fieldIndex);
        if (value.isNull() || !value.isValid()) {
            continue;
        }

        bool ok = false;
        const double number = value.toDouble(&ok);
        if (ok) {
            values.append(number);
        }
    }

    std::sort(values.begin(), values.end());
    auto last = std::unique(values.begin(), values.end(), [](double lhs, double rhs) {
        return qAbs(lhs - rhs) <= 1e-9;
    });
    values.erase(last, values.end());
    return values;
}
