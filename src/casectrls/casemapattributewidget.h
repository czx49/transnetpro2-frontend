#ifndef CASEMAPATTRIBUTEWIDGET_H
#define CASEMAPATTRIBUTEWIDGET_H

#include <QPointer>
#include <QWidget>

class QLabel;
class QFrame;
class QPushButton;
class QStackedWidget;
class TNAttributeWidget;
class TNMapWidget;

class CaseMapAttributeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CaseMapAttributeWidget(QWidget* parent = nullptr);
    ~CaseMapAttributeWidget() override;

    void bindMap(TNMapWidget* mapWidget);
    void refreshFromCurrentLayer();
    /** @brief 清除列头筛选并重新加载当前图层属性表（定义查询应用后调用） */
    void clearColumnFiltersAndRefresh();
    /** @brief 在数据视图居中显示提示文案（无属性表时） */
    void showTip(const QString& message);

private slots:
    void onMapCurrentLayerChanged();
    void onEditStackChanged();
    void onSaveEditsClicked();

private:
    void showEmptyTip(const QString& message);
    void syncSaveButtonState();
    void setLayerHintVisible(bool visible);

private:
    QPointer<TNMapWidget> m_mapWidget;
    QStackedWidget* m_stackWidget;
    QWidget* m_tablePage;
    QWidget* m_hintRow;
    QLabel* m_layerHintLabel;
    QPushButton* m_saveEditsButton;
    QFrame* m_tableFrame;
    TNAttributeWidget* m_attributeWidget;
    QLabel* m_emptyLabel;
};

#endif // CASEMAPATTRIBUTEWIDGET_H
