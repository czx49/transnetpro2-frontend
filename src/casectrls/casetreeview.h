/*****************************************************************//**
 * \file   casetreeview.h
 * \brief  方案树视图
 * \details 显示方案树结构，支持展开/收起、复选框、三点菜单与右键菜单
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef CASETREEVIEW_H
#define CASETREEVIEW_H

#include "casetreemodel.h"
#include <QMenu>
#include <QRect>
#include <QTreeView>

class CaseTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit CaseTreeView(QWidget* parent = nullptr);
    ~CaseTreeView() override;

    CaseTreeModel* caseModel() const;
    void setCaseModel(CaseTreeModel* model);
    void expandCasesAndGroups();
    /** @brief 收起全部节点后展开指定方案及其图层组，并选中滚动至该方案 */
    void focusCaseNode(int caseId);

    /** @brief 是否为方案树中的可查询图层节点 */
    bool isSchemeLayerIndex(const QModelIndex& index) const;
    /** @brief 图层节点所属方案索引 */
    QModelIndex schemeCaseIndexForLayer(const QModelIndex& layerIndex) const;

signals:
    void caseOpened(int caseId);
    void caseEdited(int caseId);
    void caseDeleted(int caseId);
    void caseRenameRequested(int caseId);
    void caseCloned(int caseId);
    void caseOverwriteRequested(int caseId);
    /** @brief 图层节点右键「导入」：`table` 为 `/api/files/drcsv` 的 table 参数 */
    void schemeLayerCsvImportRequested(int caseId, const QString& table, const QString& layerDisplayName);
    /** @brief 图层节点右键「导出」：`exportType` 为 xlsx 或 txt */
    void schemeLayerExportRequested(int caseId, const QString& table, const QString& exportType, const QString& layerDisplayName);
#ifdef TNP_ENABLE_ZOOM_TO_LAYER
    /** @brief 图层节点右键「缩放至图层」：按 WFS typename 在对应地图中定位 */
    void schemeLayerZoomToRequested(int caseId, const QString& wfsTypeName, const QString& layerDisplayName);
#endif
    /** @brief 单击已勾选图层行（非复选框）：切换地图当前图层 */
    void schemeLayerActivated(int caseId, const QString& wfsTypeName);
    /** @brief 双击图层叶子：请求切换到该图层的数据视图 */
    void schemeLayerDataViewRequested(int caseId, const QString& wfsTypeName);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private slots:
    void onDoubleClicked(const QModelIndex& index);
    void onContextMenuRequested(const QPoint& pos);
    void onOpenCase();
    void onEditCase();
    void onDeleteCase();
    void onRenameCase();
    void onCloneCase();
    void onOverwriteCase();
    void onSchemeLayerImportTriggered();
    void onSchemeLayerExportTriggered();
#ifdef TNP_ENABLE_ZOOM_TO_LAYER
    void onSchemeLayerZoomToTriggered();
#endif

private:
    void initUi();
    QMenu* createContextMenu(const CaseItem& item);
    QMenu* createLayerContextMenu();
    int getCurrentCaseId() const;
    bool isCaseNodeIndex(const QModelIndex& index) const;
    bool isLayerGroupNodeIndex(const QModelIndex& index) const;
    bool isLayerNodeIndex(const QModelIndex& index) const;
    QModelIndex caseIndexForLayer(const QModelIndex& layerIndex) const;
    bool isIndexCheckBoxHit(const QModelIndex& index, const QPoint& viewportPos) const;
    bool isLayerGroupBranchHit(const QModelIndex& index, const QPoint& viewportPos) const;
    void toggleLayerGroupExpanded(const QModelIndex& index);
    QRect actionButtonRect(const QModelIndex& index) const;
    void showContextMenuForIndex(const QModelIndex& index, const QPoint& globalPos);
    void showLayerContextMenuForIndex(const QModelIndex& index, const QPoint& globalPos);
    void onModelReset();
    void expandCaseAndGroups(const QModelIndex& caseIdx);

private:
    CaseTreeModel* m_caseModel;
    QPoint m_contextMenuPos;
    QModelIndex m_contextIndex;
};

#endif // CASETREEVIEW_H
