#ifndef CASEMAPLEGENDWIDGET_H
#define CASEMAPLEGENDWIDGET_H

#include <QColor>
#include <QList>
#include <QString>
#include <QWidget>

class QVBoxLayout;
class QgsGraduatedSymbolRenderer;
class QgsCategorizedSymbolRenderer;

class CaseMapLegendWidget : public QWidget
{
    Q_OBJECT

public:
    struct LegendItem {
        QString name;
        QColor color;
    };

    explicit CaseMapLegendWidget(QWidget* parent = nullptr);

    /** @brief 设置图例内容；title 为空时使用默认标题"图例" */
    void setLegendItems(const QList<LegendItem>& items, const QString& title = QString());

private:
    QVBoxLayout* m_layout;
};

namespace CaseMapLegend {
QList<CaseMapLegendWidget::LegendItem> defaultRoadLegendItems();
/** @brief 无定制分级时隐藏图例（不再展示默认道路等级内容） */
void applyDefault(CaseMapLegendWidget* widget);
/** @brief 显示定制分级图例内容；title 为空时使用默认标题"图例" */
void showCustom(CaseMapLegendWidget* widget, const QList<CaseMapLegendWidget::LegendItem>& items,
                const QString& title = QString());
/** @brief 图例区间数值保留的小数位数（修改 casemaplegendwidget.cpp 内 kLegendDecimalPlaceCount 即可全局调整） */
int legendDecimalPlaceCount();
QString displayNameForGraduatedField(const QString& fieldKey);
/** @brief 分级字段图例标题："图例：字段显示名" */
QString graduatedLegendTitle(const QString& fieldKey);
QString formatLegendNumber(double value);
QString formatGraduatedRangeLabel(double lower, double upper);
QList<CaseMapLegendWidget::LegendItem> itemsFromGraduatedRenderer(
    const QgsGraduatedSymbolRenderer* renderer, const QString& fieldName);
/** @brief 从分类渲染器构造图例条目（字段名仅用于标题，条目不重复字段名） */
QList<CaseMapLegendWidget::LegendItem> itemsFromCategorizedRenderer(
    const QgsCategorizedSymbolRenderer* renderer, const QString& fieldName);
} // namespace CaseMapLegend

#endif // CASEMAPLEGENDWIDGET_H
