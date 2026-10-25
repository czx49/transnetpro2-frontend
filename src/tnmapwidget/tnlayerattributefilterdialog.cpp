/*****************************************************************//**
 * \file   tnlayerattributefilterdialog.cpp
 * \brief  图层按属性过滤对话框实现
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "tnlayerattributefilterdialog.h"

#include "../publicctrls/phase2dialogstyle.h"
#include "tnlayerfilterapply.h"
#include "tnlayerfilterclausewidget.h"
#include "tnlayerfiltersqlbuilder.h"
#include "tnmapwidget.h"
#include "../publicctrls/messagedialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>

namespace {

void configureToolButton(QPushButton* button)
{
    button->setCheckable(false);
    button->setAutoDefault(false);
    button->setDefault(false);
    button->setFocusPolicy(Qt::StrongFocus);
    button->setStyleSheet(Phase2Ui::dialogSecondaryButtonStyleSheet());
}

} // namespace

TNLayerAttributeFilterDialog::TNLayerAttributeFilterDialog(TNMapWidget* mapWidget, QgsVectorLayer* layer,
                                                         QWidget* parent)
    : FramelessDialog(parent)
    , m_mapWidget(mapWidget)
    , m_layer(layer)
    , m_layerLabel(nullptr)
    , m_warningLabel(nullptr)
    , m_whereLabel(nullptr)
    , m_clausePanel(nullptr)
    , m_clauseLayout(nullptr)
    , m_previewEdit(nullptr)
    , m_firstClauseRowHost(nullptr)
{
    setTitleText(QString::fromUtf8(u8"按属性过滤"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    resize(720, 520);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                  + QStringLiteral(
                      "#framelessDialogContent { background: transparent; }"
                      "#framelessDialogContent QLabel { color: #ffffff; font-size: 13px; }"));

    initUi();
    restoreClausesOnOpen();
    updatePreview();
}

TNLayerAttributeFilterDialog::~TNLayerAttributeFilterDialog() = default;

void TNLayerAttributeFilterDialog::initUi()
{
    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("attrFilterDialogRoot"));
    auto* mainLayout = new QVBoxLayout(root);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(10);

    m_layerLabel = new QLabel(root);
    if (m_layer) {
        QString layerText = QString::fromUtf8(u8"图层：%1").arg(m_layer->name());
        if (TNLayerFilter::SqlBuilder::layerUsesQgsExpressionSubset(m_layer)) {
            layerText += QString::fromUtf8(u8"（WFS · 客户端图层过滤，不下发服务端查询）");
        }
        m_layerLabel->setText(layerText);
    }
    m_layerLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: 600; color: #ffffff; }"));
    mainLayout->addWidget(m_layerLabel);

    m_warningLabel = new QLabel(root);
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setStyleSheet(QStringLiteral("QLabel { color: #b45309; }"));
    m_warningLabel->hide();
    mainLayout->addWidget(m_warningLabel);

    auto* toolRow = new QHBoxLayout();
    toolRow->setSpacing(8);
    auto* addBtn = new QPushButton(QString::fromUtf8(u8"+ 添加子句"), root);
    auto* clearBtn = new QPushButton(QString::fromUtf8(u8"清除查询"), root);
    auto* verifyBtn = new QPushButton(QString::fromUtf8(u8"验证"), root);
    addBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setCursor(Qt::PointingHandCursor);
    verifyBtn->setCursor(Qt::PointingHandCursor);
    configureToolButton(addBtn);
    configureToolButton(clearBtn);
    configureToolButton(verifyBtn);
    connect(addBtn, &QPushButton::clicked, this, &TNLayerAttributeFilterDialog::onAddClause);
    connect(clearBtn, &QPushButton::clicked, this, &TNLayerAttributeFilterDialog::onClearQuery);
    connect(verifyBtn, &QPushButton::clicked, this, &TNLayerAttributeFilterDialog::onVerify);
    toolRow->addWidget(addBtn);
    toolRow->addWidget(clearBtn);
    toolRow->addWidget(verifyBtn);
    toolRow->addStretch();
    mainLayout->addLayout(toolRow);

    m_clausePanel = new QFrame(root);
    m_clausePanel->setObjectName(QStringLiteral("attrFilterClausePanel"));
    m_clausePanel->setAttribute(Qt::WA_StyledBackground, true);
    m_clausePanel->setStyleSheet(QStringLiteral(
        "QFrame#attrFilterClausePanel {"
        "   background-color: #ffffff;"
        "   border: 1px solid #b8c7da;"
        "   border-radius: 6px;"
        "}"));

    auto* scroll = new QScrollArea(m_clausePanel);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));

    auto* clauseHost = new QWidget(scroll);
    clauseHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_clauseLayout = new QVBoxLayout(clauseHost);
    m_clauseLayout->setContentsMargins(8, 8, 8, 8);
    m_clauseLayout->setSpacing(8);
    m_clauseLayout->setAlignment(Qt::AlignTop);
    scroll->setWidget(clauseHost);

    m_whereLabel = new QLabel(QStringLiteral("WHERE"), clauseHost);
    m_whereLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "   color: #0f4d9b;"
        "   font-weight: 700;"
        "   font-size: 13px;"
        "   font-family: Consolas, monospace;"
        "   padding: 0 4px;"
        "}"));
    m_whereLabel->hide();

    auto* panelLayout = new QVBoxLayout(m_clausePanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->addWidget(scroll);

    mainLayout->addWidget(m_clausePanel, 1);

    auto* previewLabel = new QLabel(root);
    previewLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: 600; color: #ffffff; }"));
    mainLayout->addWidget(previewLabel);
    if (m_layer && TNLayerFilter::SqlBuilder::layerUsesQgsExpressionSubset(m_layer)) {
        previewLabel->setText(QString::fromUtf8(u8"表达式预览（客户端图层过滤）"));
    } else {
        previewLabel->setText(QString::fromUtf8(u8"SQL 预览"));
    }

    m_previewEdit = new QPlainTextEdit(root);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setMaximumHeight(72);
    m_previewEdit->setPlaceholderText(QString::fromUtf8(u8"（无过滤条件）"));
    m_previewEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        "   background: #ffffff;"
        "   border: 1px solid #b8c7da;"
        "   border-radius: 4px;"
        "   font-family: Consolas, monospace;"
        "   font-size: 12px;"
        "   color: #111827;"
        "   padding: 6px 8px;"
        "}"));
    mainLayout->addWidget(m_previewEdit);

    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->addWidget(root, 1);

    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(6, 8, 6, 6);
    btnRow->setSpacing(10);
    auto* applyBtn = new QPushButton(QString::fromUtf8(u8"应用"), this);
    auto* okBtn = new QPushButton(QString::fromUtf8(u8"确定"), this);
    auto* cancelBtn = new QPushButton(QString::fromUtf8(u8"取消"), this);
    applyBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setDefault(true);
    applyBtn->setStyleSheet(Phase2Ui::dialogSecondaryButtonStyleSheet());
    okBtn->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    cancelBtn->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(applyBtn, &QPushButton::clicked, this, &TNLayerAttributeFilterDialog::onApply);
    connect(okBtn, &QPushButton::clicked, this, &TNLayerAttributeFilterDialog::onOk);
    connect(cancelBtn, &QPushButton::clicked, this, &TNLayerAttributeFilterDialog::onCancel);
    btnRow->addStretch();
    btnRow->addWidget(applyBtn);
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    contentLayout()->addLayout(btnRow);
}

QWidget* TNLayerAttributeFilterDialog::clauseHostWidget() const
{
    return m_clauseLayout ? m_clauseLayout->parentWidget() : m_clausePanel;
}

void TNLayerAttributeFilterDialog::detachWhereLabelFromFirstRow()
{
    QWidget* host = clauseHostWidget();
    if (!m_whereLabel || !host) {
        return;
    }
    m_whereLabel->setParent(host);
    m_whereLabel->hide();
}

void TNLayerAttributeFilterDialog::removeAllClauseRows()
{
    detachWhereLabelFromFirstRow();

    if (m_firstClauseRowHost) {
        m_firstClauseRowHost->deleteLater();
        m_firstClauseRowHost = nullptr;
    }
    for (TNLayerFilter::ClauseWidget* w : m_clauseWidgets) {
        w->deleteLater();
    }
    m_clauseWidgets.clear();
}

void TNLayerAttributeFilterDialog::rebuildClauseChrome()
{
    QWidget* clauseHost = clauseHostWidget();
    if (!m_clauseLayout || !clauseHost) {
        return;
    }

    if (m_firstClauseRowHost) {
        detachWhereLabelFromFirstRow();
        if (!m_clauseWidgets.isEmpty()) {
            m_clauseWidgets.first()->setParent(clauseHost);
        }
        m_firstClauseRowHost->deleteLater();
        m_firstClauseRowHost = nullptr;
    }

    while (QLayoutItem* item = m_clauseLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            if (w != m_whereLabel) {
                w->setParent(clauseHost);
                w->hide();
            }
        }
        delete item;
    }

    for (int i = 1; i < m_clauseWidgets.size(); ++i) {
        m_clauseWidgets.at(i)->setParent(clauseHost);
        m_clauseWidgets.at(i)->show();
    }

    if (m_clauseWidgets.isEmpty()) {
        if (m_whereLabel) {
            m_whereLabel->hide();
        }
        m_clauseLayout->addStretch(1);
        return;
    }

    if (m_whereLabel) {
        m_whereLabel->show();
    }

    m_firstClauseRowHost = new QWidget(clauseHost);
    m_firstClauseRowHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* firstRow = new QHBoxLayout(m_firstClauseRowHost);
    firstRow->setContentsMargins(0, 0, 0, 0);
    firstRow->setSpacing(8);
    firstRow->setAlignment(Qt::AlignVCenter);
    firstRow->addWidget(m_whereLabel, 0, Qt::AlignVCenter);
    m_clauseWidgets.first()->setParent(m_firstClauseRowHost);
    m_clauseWidgets.first()->show();
    firstRow->addWidget(m_clauseWidgets.first(), 1);
    m_clauseLayout->addWidget(m_firstClauseRowHost);
    m_firstClauseRowHost->show();

    for (int i = 1; i < m_clauseWidgets.size(); ++i) {
        m_clauseLayout->addWidget(m_clauseWidgets.at(i));
    }

    m_clauseLayout->addStretch(1);
    rebuildJoinVisibility();
}

void TNLayerAttributeFilterDialog::restoreClausesOnOpen()
{
    QString warning;
    QVector<TNLayerFilter::Clause> clauses = TNLayerFilter::SqlBuilder::clausesForDialogOpen(m_layer, &warning);

    if (!warning.isEmpty()) {
        m_warningLabel->setText(warning);
        m_warningLabel->show();
    }

    removeAllClauseRows();

    if (clauses.isEmpty()) {
        rebuildClauseChrome();
        if (m_layer && m_previewEdit) {
            const QString activeFilter = TNLayerFilter::displayFilterExpression(m_layer);
            if (!activeFilter.isEmpty()) {
                m_previewEdit->setPlainText(activeFilter);
            }
        }
        return;
    }

    for (const TNLayerFilter::Clause& c : clauses) {
        addClauseRow(c);
    }
}

void TNLayerAttributeFilterDialog::addClauseRow(const TNLayerFilter::Clause& initial)
{
    const bool showJoin = !m_clauseWidgets.isEmpty();
    QWidget* clauseHost = clauseHostWidget();
    auto* row = new TNLayerFilter::ClauseWidget(m_layer, showJoin, clauseHost);
    connect(row, &TNLayerFilter::ClauseWidget::clauseChanged, this, &TNLayerAttributeFilterDialog::onClauseChanged);
    connect(row, &TNLayerFilter::ClauseWidget::removeRequested, this, &TNLayerAttributeFilterDialog::onClauseRemoveRequested);

    if (initial.isValid()) {
        row->setClause(initial);
    }

    m_clauseWidgets.append(row);
    rebuildClauseChrome();
}

void TNLayerAttributeFilterDialog::rebuildJoinVisibility()
{
    for (int i = 0; i < m_clauseWidgets.size(); ++i) {
        m_clauseWidgets.at(i)->setShowJoin(i > 0);
    }
}

void TNLayerAttributeFilterDialog::onAddClause()
{
    TNLayerFilter::Clause c;
    if (!m_clauseWidgets.isEmpty()) {
        c.join = TNLayerFilter::JoinType::And;
    }
    addClauseRow(c);
    updatePreview();
}

void TNLayerAttributeFilterDialog::onClearQuery()
{
    removeAllClauseRows();
    rebuildClauseChrome();

    if (!m_layer || !m_mapWidget) {
        updatePreview();
        return;
    }

    const QString oldFilter = TNLayerFilter::displayFilterExpression(m_layer);
    QString applyErr;
    if (!TNLayerFilter::applyDefinitionQuery(m_layer, QString(), &applyErr).success) {
        TnUi::showWarning(QString::fromUtf8(u8"按属性过滤"), applyErr);
        return;
    }
    TNLayerFilter::SqlBuilder::clearPersistedState(m_layer);
    TNLayerFilter::refreshLayerAfterDefinitionQuery(m_layer, m_mapWidget);

    m_warningLabel->hide();
    updatePreview();
    emit filterApplied();

    if (!oldFilter.isEmpty()) {
        TnUi::showInformation(QString::fromUtf8(u8"按属性过滤"),
                                 QString::fromUtf8(u8"已清除图层的客户端过滤。"));
    }
}

void TNLayerAttributeFilterDialog::onClauseChanged()
{
    updatePreview();
}

void TNLayerAttributeFilterDialog::onClauseRemoveRequested()
{
    auto* senderRow = qobject_cast<TNLayerFilter::ClauseWidget*>(sender());
    if (!senderRow) {
        return;
    }

    m_clauseWidgets.removeAll(senderRow);
    senderRow->deleteLater();
    rebuildClauseChrome();
    updatePreview();
}

QVector<TNLayerFilter::Clause> TNLayerAttributeFilterDialog::collectClauses() const
{
    QVector<TNLayerFilter::Clause> list;
    for (TNLayerFilter::ClauseWidget* w : m_clauseWidgets) {
        list.append(w->clause());
    }
    return list;
}

void TNLayerAttributeFilterDialog::updatePreview()
{
    QString err;
    const QVector<TNLayerFilter::Clause> clauses = collectClauses();
    const QString subset = TNLayerFilter::SqlBuilder::buildForLayer(m_layer, clauses, &err);
    if (subset.isEmpty() && !err.isEmpty()) {
        m_previewEdit->setPlainText(err);
        return;
    }
    m_previewEdit->setPlainText(subset.isEmpty() ? QString() : subset);
}

void TNLayerAttributeFilterDialog::onVerify()
{
    QString err;
    const QVector<TNLayerFilter::Clause> clauses = collectClauses();
    const QString subset = TNLayerFilter::SqlBuilder::buildForLayer(m_layer, clauses, &err);
    if (subset.isEmpty() && clauses.isEmpty()) {
        TnUi::showInformation(QString::fromUtf8(u8"验证"),
                                 QString::fromUtf8(u8"当前无有效过滤条件（将显示全部要素）。"));
        return;
    }
    if (!err.isEmpty() || subset.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"验证"), err.isEmpty() ? QString::fromUtf8(u8"无法生成过滤条件。") : err);
        return;
    }

    if (!m_layer) {
        return;
    }

    QString validateErr;
    if (!TNLayerFilter::SqlBuilder::validateSubsetForLayer(m_layer, subset, &validateErr)) {
        TnUi::showWarning(QString::fromUtf8(u8"验证"), validateErr);
        return;
    }

    const bool wfsLayer = TNLayerFilter::SqlBuilder::layerUsesQgsExpressionSubset(m_layer);

    if (wfsLayer) {
        QString countErr;
        const qlonglong filteredCount = TNLayerFilter::countFeaturesMatchingExpression(m_layer, subset, &countErr);
        if (filteredCount < 0) {
            TnUi::showWarning(QString::fromUtf8(u8"验证"),
                                 countErr.isEmpty() ? QString::fromUtf8(u8"表达式无法解析或应用。") : countErr);
            return;
        }

        QString message =
            QString::fromUtf8(u8"条件表达式有效（客户端过滤）。当前条件下约 %1 条要素。").arg(filteredCount);
        message += QString::fromUtf8(u8"\n\n地图渲染按表达式过滤，WFS 请求不下发服务端过滤参数。");
        TnUi::showInformation(QString::fromUtf8(u8"验证"), message);
        return;
    }

    const QString backup = m_layer->subsetString();
    QString applyErr;
    const TNLayerFilter::ApplyDefinitionQueryResult applyResult =
        TNLayerFilter::applyDefinitionQuery(m_layer, subset, &applyErr);
    if (!applyResult.success) {
        TnUi::showWarning(QString::fromUtf8(u8"验证"),
                             QString::fromUtf8(u8"无法应用该条件：%1").arg(applyErr));
        return;
    }

    const qlonglong filteredCount = m_layer->featureCount();
    TNLayerFilter::applyDefinitionQuery(m_layer, backup, nullptr);
    TNLayerFilter::refreshLayerAfterDefinitionQuery(m_layer, m_mapWidget);

    QString message = QString::fromUtf8(u8"条件表达式有效。当前条件下约 %1 条要素。").arg(filteredCount);
    if (!applyResult.warning.isEmpty()) {
        message += QStringLiteral("\n\n") + applyResult.warning;
    }
    TnUi::showInformation(QString::fromUtf8(u8"验证"), message);
}

bool TNLayerAttributeFilterDialog::applyFilter(QString* errorOut, QString* warningOut)
{
    if (!m_layer || !m_mapWidget) {
        if (errorOut) {
            *errorOut = QString::fromUtf8(u8"地图或图层无效。");
        }
        return false;
    }

    const QVector<TNLayerFilter::Clause> clauses = collectClauses();
    QString err;
    const QString subset = TNLayerFilter::SqlBuilder::buildForLayer(m_layer, clauses, &err);

    if (!err.isEmpty()) {
        if (errorOut) {
            *errorOut = err;
        }
        return false;
    }

    if (!subset.isEmpty()) {
        QString validateErr;
        if (!TNLayerFilter::SqlBuilder::validateSubsetForLayer(m_layer, subset, &validateErr)) {
            if (errorOut) {
                *errorOut = validateErr;
            }
            return false;
        }
    }

    const TNLayerFilter::ApplyDefinitionQueryResult applyResult =
        TNLayerFilter::applyDefinitionQuery(m_layer, subset, errorOut);
    if (!applyResult.success) {
        return false;
    }

    if (warningOut) {
        *warningOut = applyResult.warning;
    }

    if (subset.isEmpty()) {
        TNLayerFilter::SqlBuilder::clearPersistedState(m_layer);
    } else {
        TNLayerFilter::SqlBuilder::savePersistedState(m_layer, clauses, subset);
    }

    TNLayerFilter::refreshLayerAfterDefinitionQuery(m_layer, m_mapWidget);
    emit filterApplied();
    return true;
}

void TNLayerAttributeFilterDialog::onApply()
{
    QString err;
    QString warning;
    if (!applyFilter(&err, &warning)) {
        if (!err.isEmpty()) {
            TnUi::showWarning(QString::fromUtf8(u8"按属性过滤"), err);
        }
        return;
    }
    m_warningLabel->hide();
    QString message = QString::fromUtf8(u8"过滤条件已应用到图层。");
    if (!warning.isEmpty()) {
        message += QStringLiteral("\n\n") + warning;
    }
    TnUi::showInformation(QString::fromUtf8(u8"按属性过滤"), message);
}

void TNLayerAttributeFilterDialog::onOk()
{
    QString err;
    QString warning;
    if (!applyFilter(&err, &warning)) {
        if (!err.isEmpty()) {
            TnUi::showWarning(QString::fromUtf8(u8"按属性过滤"), err);
        }
        return;
    }
    if (!warning.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"按属性过滤"), warning);
    }
    accept();
}

void TNLayerAttributeFilterDialog::onCancel()
{
    reject();
}
