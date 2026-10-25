#ifndef CASEMAPTOOLBARWIDGET_H
#define CASEMAPTOOLBARWIDGET_H

#include <QList>
#include <QStringList>
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QFrame;
class QHideEvent;
class QPushButton;
class QShortcut;
class QShowEvent;
class QWidget;

class CaseMapToolbarWidget : public QWidget
{
    Q_OBJECT

public:
    enum class HostContext
    {
        SchemeDetail = 0,
        BaseData
    };

    enum class ViewMode
    {
        Standard = 0,
        Compare
    };

    /** @brief 与一期 CaseEditPanel::initToolBar 动作语义对齐 */
    enum class ToolType
    {
        Browse = 0,
        Select,
        Edit,
        AddFeature,
        Delete,
        SaveEdits,
        Clip,
        Symbol,
        ClearSymbol,
        Desireline,
        Flow,
        AttributeFilter,
        ExportMap,
        RefreshData,
        UndoEdits
    };

    explicit CaseMapToolbarWidget(HostContext hostContext, QWidget* parent = nullptr);
    ~CaseMapToolbarWidget() override;

    HostContext hostContext() const { return m_hostContext; }

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;

    /** @brief 双屏对比下禁用一期工具与工作台等（标准视图/对比切换保持可用） */
    void setToolButtonsEnabled(bool enabled);
    void setCompareSelectorVisible(bool visible);
    void setCompareOptions(const QStringList& options);
    void setCurrentCompareText(const QString& text);
    QString currentCompareText() const;

    /** @brief 是否显示「标准视图 / 双屏对比」及对比方案下拉（基础数据地图应设为 false） */
    void setSchemeModeSwitcherVisible(bool visible);

    /** @brief 按当前子控件重新计算紧凑宽度（悬浮宿主 reposition 前应调用） */
    void relayoutToContent();

    /** @brief 同步业务工作台勾选状态（内部 blockSignals，不触发 workbenchToggled） */
    void setWorkbenchChecked(bool checked);

    /** @brief 同步「保存编辑」按钮可用态（仍受双屏对比全局禁用约束） */
    void setSaveEditsEnabled(bool enabled);

    /** @brief 同步「撤回修改」按钮可用态（仍受双屏对比全局禁用约束） */
    void setUndoEditsEnabled(bool enabled);

    /** @brief 同步「撤回修改」按钮 tooltip */
    void setUndoEditsToolTip(const QString& text);

    /** @brief 同步互斥工具勾选（blockSignals，不触发 toolTriggered） */
    void setMutexTool(ToolType toolType);

    /** @brief 期望线/道路溯源接口进行中禁用相关按钮 */
    void setGisAsyncToolsBusy(bool busy);

    /** @brief 刷新 WFS 服务信息期间禁用「刷新数据」按钮 */
    void setRefreshDataBusy(bool busy);

    QPushButton* saveEditsButton() const { return m_saveEditsButton; }

signals:
    void viewModeChanged(CaseMapToolbarWidget::ViewMode mode);
    void toolTriggered(CaseMapToolbarWidget::ToolType toolType);
    void compareSelectionChanged(const QString& compareText);
    /** @brief 用户切换业务工作台按钮：true 表示展开工作台 */
    void workbenchToggled(bool expanded);

private slots:
    void onWorkbenchButtonToggled(bool checked);
    void onViewButtonClicked();
    void onPhaseOneToolClicked();
    void onUndoShortcutActivated();
    void onApplicationFocusChanged(QWidget* oldWidget, QWidget* nowWidget);

private:
    QPushButton* createPhaseIconButton(QFrame* toolbar, const QIcon& icon, bool checkable, ToolType type);
    QPushButton* createSegmentButton(const QString& text, bool checked, QWidget* parent);
    QFrame* createDivider(QWidget* parent) const;
    void shrinkWrapToolbar();
    void applyGisAsyncToolButtonState();
    void applyRefreshDataButtonState();
    void applySaveUndoButtonState();
    void syncUndoShortcutEnabled();
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    static bool isGisAsyncToolType(ToolType toolType);
    static bool isTextInputFocusWidget(QWidget* widget);

private:
    HostContext m_hostContext;
    QFrame* m_toolbarFrame;
    QButtonGroup* m_viewGroup;
    QButtonGroup* m_mutexToolGroup;
    QPushButton* m_standardViewButton;
    QPushButton* m_compareViewButton;
    QComboBox* m_compareCombo;
    QPushButton* m_chartButton;
    QPushButton* m_saveEditsButton = nullptr;
    QPushButton* m_undoEditsButton = nullptr;
    QShortcut* m_undoShortcut = nullptr;
    QPushButton* m_refreshDataButton = nullptr;
    QList<QPushButton*> m_primaryToolButtons;
    QList<QPushButton*> m_gisAsyncToolButtons;
    bool m_toolsGloballyEnabled = true;
    bool m_gisAsyncToolsBusy = false;
    bool m_refreshDataBusy = false;
    bool m_saveEditsWanted = false;
    bool m_undoEditsWanted = false;
    QWidget* m_schemeModeWidget;
    QFrame* m_dividerBeforeWorkbench;
};

#endif // CASEMAPTOOLBARWIDGET_H
