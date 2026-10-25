#include "casemapattributewidget.h"

#include "caseiconutils.h"
#include "../publicctrls/messagedialog.h"
#include "../tnmapwidget/tnattributewidget.h"
#include "../tnmapwidget/tngiseditstackmanager.h"
#include "../tnmapwidget/tnmapwidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <qgsvectorlayer.h>

namespace {

QString attrSaveButtonStyleSheet()
{
    return QStringLiteral(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 8px;"
        "   background-color: transparent;"
        "   color: #334155;"
        "   font-size: 13px;"
        "   font-weight: 600;"
        "   padding: 4px 10px 4px 6px;"
        "   min-height: 28px;"
        "}"
        "QPushButton:hover:!disabled {"
        "   background-color: rgba(34, 137, 255, 0.20);"
        "   color: #1e3a5f;"
        "}"
        "QPushButton:disabled {"
        "   color: #94a3b8;"
        "}");
}

} // namespace

CaseMapAttributeWidget::CaseMapAttributeWidget(QWidget* parent)
    : QWidget(parent)
    , m_stackWidget(new QStackedWidget(this))
    , m_tablePage(new QWidget(this))
    , m_hintRow(new QWidget(m_tablePage))
    , m_layerHintLabel(new QLabel(m_hintRow))
    , m_saveEditsButton(new QPushButton(m_hintRow))
    , m_tableFrame(new QFrame(m_tablePage))
    , m_attributeWidget(new TNAttributeWidget(m_tableFrame))
    , m_emptyLabel(new QLabel(this))
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "CaseMapAttributeWidget {"
        "   background-color: #f7fbff;"
        "}"));

    m_attributeWidget->setPhase2ChromeEnabled(true);
    m_attributeWidget->setInlineEditingEnabled(true);

    m_layerHintLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #334155; font-size: 14px; font-weight: 600; background: transparent; }"));

    m_saveEditsButton->setCursor(Qt::PointingHandCursor);
    m_saveEditsButton->setIcon(CaseIconUtils::mapToolbarSingleStateIcon(QStringLiteral("case_tb_save")));
    m_saveEditsButton->setIconSize(QSize(18, 18));
    m_saveEditsButton->setText(QString::fromUtf8(u8"保存"));
    m_saveEditsButton->setToolTip(QString::fromUtf8(u8"保存编辑"));
    m_saveEditsButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_saveEditsButton->setMinimumHeight(28);
    m_saveEditsButton->setEnabled(false);
    m_saveEditsButton->setStyleSheet(attrSaveButtonStyleSheet());
    connect(m_saveEditsButton, &QPushButton::clicked, this, &CaseMapAttributeWidget::onSaveEditsClicked);

    auto* hintLay = new QHBoxLayout(m_hintRow);
    hintLay->setContentsMargins(0, 0, 0, 0);
    hintLay->setSpacing(8);
    hintLay->addWidget(m_layerHintLabel, 0, Qt::AlignVCenter);
    hintLay->addWidget(m_saveEditsButton, 0, Qt::AlignVCenter);
    hintLay->addStretch(1);
    m_hintRow->setVisible(false);

    m_tableFrame->setObjectName(QStringLiteral("caseMapAttrTableFrame"));
    m_tableFrame->setStyleSheet(QStringLiteral(
        "QFrame#caseMapAttrTableFrame { background: #ffffff; border: 1px solid #cbd5e1; border-radius: 10px; }"));
    auto* frameLay = new QVBoxLayout(m_tableFrame);
    frameLay->setContentsMargins(10, 10, 10, 10);
    frameLay->setSpacing(0);
    frameLay->addWidget(m_attributeWidget);

    auto* tableLay = new QVBoxLayout(m_tablePage);
    tableLay->setContentsMargins(16, 16, 16, 16);
    tableLay->setSpacing(12);
    tableLay->addWidget(m_hintRow);
    tableLay->addWidget(m_tableFrame, 1);

    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "   color: rgba(30, 58, 95, 0.78);"
        "   font-size: 15px;"
        "   font-weight: 600;"
        "   background: transparent;"
        "}"));

    m_stackWidget->addWidget(m_tablePage);
    m_stackWidget->addWidget(m_emptyLabel);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_stackWidget);

    showEmptyTip(QString::fromUtf8(u8"当前图层无属性表数据"));
}

CaseMapAttributeWidget::~CaseMapAttributeWidget()
{
    bindMap(nullptr);
}

void CaseMapAttributeWidget::bindMap(TNMapWidget* mapWidget)
{
    if (m_mapWidget == mapWidget) {
        if (mapWidget) {
            refreshFromCurrentLayer();
        }
        return;
    }

    if (m_mapWidget) {
        disconnect(m_mapWidget, &TNMapWidget::currentLayerChanged, this, &CaseMapAttributeWidget::onMapCurrentLayerChanged);
        if (TNGisEditStackManager* stack = m_mapWidget->editStackManager()) {
            disconnect(stack, &TNGisEditStackManager::editStackChanged, this, &CaseMapAttributeWidget::onEditStackChanged);
        }
    }

    m_mapWidget = mapWidget;
    if (m_attributeWidget) {
        m_attributeWidget->setCanvas(m_mapWidget ? m_mapWidget->canvas() : nullptr);
    }
    if (m_mapWidget) {
        connect(m_mapWidget, &TNMapWidget::currentLayerChanged, this, &CaseMapAttributeWidget::onMapCurrentLayerChanged);
        if (TNGisEditStackManager* stack = m_mapWidget->editStackManager()) {
            connect(stack, &TNGisEditStackManager::editStackChanged, this, &CaseMapAttributeWidget::onEditStackChanged);
        }
    }

    refreshFromCurrentLayer();
}

void CaseMapAttributeWidget::clearColumnFiltersAndRefresh()
{
    if (!m_attributeWidget) {
        return;
    }

    m_attributeWidget->clearFilter();

    if (!m_mapWidget) {
        return;
    }

    auto* vectorLayer = qobject_cast<QgsVectorLayer*>(m_mapWidget->currentLayer());
    if (!vectorLayer || !vectorLayer->isValid()) {
        m_attributeWidget->clearTable();
        setLayerHintVisible(false);
        showEmptyTip(QString::fromUtf8(u8"当前图层无属性表数据"));
        return;
    }

    m_layerHintLabel->setText(QString::fromUtf8(u8"图层：%1").arg(vectorLayer->name()));
    setLayerHintVisible(true);
    m_attributeWidget->refreshTable();
    m_stackWidget->setCurrentWidget(m_tablePage);
    syncSaveButtonState();
}

void CaseMapAttributeWidget::refreshFromCurrentLayer()
{
    if (!m_mapWidget) {
        m_attributeWidget->clearTable();
        setLayerHintVisible(false);
        showEmptyTip(QString::fromUtf8(u8"未绑定地图控件"));
        return;
    }

    QgsVectorLayer* vectorLayer = qobject_cast<QgsVectorLayer*>(m_mapWidget->currentLayer());
    if (!vectorLayer || !vectorLayer->isValid()) {
        m_attributeWidget->clearTable();
        setLayerHintVisible(false);
        showEmptyTip(QString::fromUtf8(u8"当前图层无属性表数据"));
        return;
    }

    m_layerHintLabel->setText(QString::fromUtf8(u8"图层：%1").arg(vectorLayer->name()));
    setLayerHintVisible(true);

    m_attributeWidget->setCurrentLayer(vectorLayer);
    m_stackWidget->setCurrentWidget(m_tablePage);
    syncSaveButtonState();
}

void CaseMapAttributeWidget::onMapCurrentLayerChanged()
{
    refreshFromCurrentLayer();
}

void CaseMapAttributeWidget::onEditStackChanged()
{
    syncSaveButtonState();
}

void CaseMapAttributeWidget::onSaveEditsClicked()
{
    if (!m_mapWidget) {
        return;
    }

    QgsVectorLayer* vectorLayer = qobject_cast<QgsVectorLayer*>(m_mapWidget->currentLayer());
    TNGisEditStackManager* stack = m_mapWidget->editStackManager();
    if (!vectorLayer || !stack) {
        return;
    }

    QString summary;
    const bool ok = stack->saveLayer(vectorLayer, &summary);
    if (ok) {
        TnUi::showInformation(QString::fromUtf8(u8"保存编辑"), summary);
    } else {
        TnUi::showWarning(QString::fromUtf8(u8"保存编辑"), summary);
    }
    syncSaveButtonState();
}

void CaseMapAttributeWidget::syncSaveButtonState()
{
    if (!m_saveEditsButton) {
        return;
    }

    bool enabled = false;
    if (m_mapWidget) {
        if (auto* vl = qobject_cast<QgsVectorLayer*>(m_mapWidget->currentLayer())) {
            enabled = vl->isEditable() && vl->isModified();
        }
    }
    m_saveEditsButton->setEnabled(enabled);
}

void CaseMapAttributeWidget::setLayerHintVisible(bool visible)
{
    if (m_hintRow) {
        m_hintRow->setVisible(visible);
    }
    if (!visible && m_layerHintLabel) {
        m_layerHintLabel->clear();
    }
    if (!visible) {
        syncSaveButtonState();
    }
}

void CaseMapAttributeWidget::showTip(const QString& message)
{
    m_attributeWidget->clearTable();
    setLayerHintVisible(false);
    showEmptyTip(message);
}

void CaseMapAttributeWidget::showEmptyTip(const QString& message)
{
    m_emptyLabel->setText(message);
    m_stackWidget->setCurrentWidget(m_emptyLabel);
    syncSaveButtonState();
}
