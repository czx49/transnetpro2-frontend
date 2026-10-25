/*****************************************************************//**
 * \file   tnlayerattributefilterdialog.h
 * \brief  图层按属性过滤（Definition Query）对话框
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef TNLAYERATTRIBUTEFILTERDIALOG_H
#define TNLAYERATTRIBUTEFILTERDIALOG_H

#include "../publicctrls/framelessdialog.h"

#include "tnlayerfiltertypes.h"

#include <QVector>

class QFrame;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;
class TNMapWidget;
class QgsVectorLayer;

namespace TNLayerFilter {
class ClauseWidget;
}

class TNLayerAttributeFilterDialog : public FramelessDialog
{
    Q_OBJECT

public:
    TNLayerAttributeFilterDialog(TNMapWidget* mapWidget, QgsVectorLayer* layer, QWidget* parent = nullptr);
    ~TNLayerAttributeFilterDialog() override;

signals:
    /** @brief 过滤已写入图层（应用 / 确定 / 清除查询） */
    void filterApplied();

private slots:
    void onAddClause();
    void onClearQuery();
    void onVerify();
    void onApply();
    void onOk();
    void onCancel();
    void onClauseChanged();
    void onClauseRemoveRequested();

private:
    void initUi();
    void restoreClausesOnOpen();
    void addClauseRow(const TNLayerFilter::Clause& initial = TNLayerFilter::Clause());
    void removeAllClauseRows();
    QWidget* clauseHostWidget() const;
    void detachWhereLabelFromFirstRow();
    void rebuildClauseChrome();
    void rebuildJoinVisibility();
    void updatePreview();
    QVector<TNLayerFilter::Clause> collectClauses() const;
    bool applyFilter(QString* errorOut, QString* warningOut = nullptr);

    TNMapWidget* m_mapWidget;
    QgsVectorLayer* m_layer;
    QLabel* m_layerLabel;
    QLabel* m_warningLabel;
    QLabel* m_whereLabel;
    QFrame* m_clausePanel;
    QVBoxLayout* m_clauseLayout;
    QPlainTextEdit* m_previewEdit;
    QVector<TNLayerFilter::ClauseWidget*> m_clauseWidgets;
    QWidget* m_firstClauseRowHost;
};

#endif // TNLAYERATTRIBUTEFILTERDIALOG_H
