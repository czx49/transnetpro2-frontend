#include "casemaplegendwidget.h"

#include "../httpapis/appsession.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

#include "qgsgraduatedsymbolrenderer.h"
#include "qgscategorizedsymbolrenderer.h"
#include "qgssymbol.h"

CaseMapLegendWidget::CaseMapLegendWidget(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
{
    setObjectName(QStringLiteral("caseMapLegend"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "QWidget#caseMapLegend {"
        "   background-color: #111827;"
        "   border-radius: 4px;"
        "   border: none;"
        "}"
        "QWidget#caseMapLegend QWidget#legendRow {"
        "   background-color: transparent;"
        "}"
        "QWidget#caseMapLegend QLabel#legendTitle {"
        "   background-color: transparent !important;"
        "   border: none;"
        "   padding: 0px;"
        "   color: #4a5568;"
        "   font-size: 13px;"
        "   font-weight: 600;"
        "}"
        "QWidget#caseMapLegend QLabel#legendName {"
        "   background-color: transparent !important;"
        "   border: none;"
        "   padding: 0px;"
        "   color: #94a3b8;"
        "   font-size: 12px;"
        "}"
    ));
    m_layout->setContentsMargins(12, 10, 12, 10);
    m_layout->setSpacing(8);
}

void CaseMapLegendWidget::setLegendItems(const QList<LegendItem>& items, const QString& title)
{
    if (QCoreApplication::closingDown()) {
        return;
    }

    while (QLayoutItem* child = m_layout->takeAt(0)) {
        if (child->widget()) {
            // 立即隐藏再延迟删除：deleteLater 前旧行仍是子控件，若几何未及时收缩
            // 会以残影形式被画进保存画布的抓图里
            child->widget()->hide();
            child->widget()->deleteLater();
        }
        delete child;
    }

    const QString titleText = title.trimmed().isEmpty() ? QString::fromUtf8(u8"图例") : title.trimmed();
    QLabel* titleLabel = new QLabel(titleText, this);
    titleLabel->setObjectName(QStringLiteral("legendTitle"));
    titleLabel->setAutoFillBackground(false);
    m_layout->addWidget(titleLabel);

    for (const LegendItem& item : items) {
        QWidget* row = new QWidget(this);
        row->setObjectName(QStringLiteral("legendRow"));
        row->setAttribute(Qt::WA_StyledBackground, false);
        row->setAutoFillBackground(false);
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        QLabel* colorBlock = new QLabel(row);
        colorBlock->setFixedSize(36, 4);
        colorBlock->setAutoFillBackground(false);
        colorBlock->setScaledContents(false);
        colorBlock->setStyleSheet(QString(
            "QLabel {"
            "   background-color: %1;"
            "   border-radius: 2px;"
            "   border: none;"
            "   padding: 0px;"
            "}"
        ).arg(item.color.name()));

        QLabel* nameLabel = new QLabel(item.name, row);
        nameLabel->setObjectName(QStringLiteral("legendName"));
        nameLabel->setAutoFillBackground(false);

        rowLayout->addWidget(colorBlock, 0, Qt::AlignVCenter);
        rowLayout->addWidget(nameLabel, 1, Qt::AlignVCenter);
        m_layout->addWidget(row);
    }
}

namespace CaseMapLegend {

namespace {

constexpr int kLegendDecimalPlaceCount = 2;

QString trimTrailingZeros(QString text)
{
    if (!text.contains(QLatin1Char('.'))) {
        return text;
    }
    while (text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    return text;
}

bool usePresetLegendLabel(const QString& label)
{
    if (label.isEmpty()) {
        return false;
    }
    const QString lower = label.toLower();
    return !lower.contains(QStringLiteral("e+")) && !lower.contains(QStringLiteral("e-"));
}

} // namespace

int legendDecimalPlaceCount()
{
    return kLegendDecimalPlaceCount;
}

QString formatLegendNumber(double value)
{
    if (std::isnan(value)) {
        return QStringLiteral("?");
    }
    if (std::isinf(value)) {
        return value > 0 ? QString::fromUtf8(u8"max") : QStringLiteral("?");
    }

    return trimTrailingZeros(QString::number(value, 'f', legendDecimalPlaceCount()));
}

QList<CaseMapLegendWidget::LegendItem> defaultRoadLegendItems()
{
    return {
        {QString::fromUtf8(u8"城市高速道路"), QColor(QStringLiteral("#E54D4D"))},
        {QString::fromUtf8(u8"城市主干路"), QColor(QStringLiteral("#F59E0B"))},
        {QString::fromUtf8(u8"城市次干道"), QColor(QStringLiteral("#FBBF24"))},
        {QString::fromUtf8(u8"城市普通道路"), QColor(QStringLiteral("#94A3B8"))},
        {QString::fromUtf8(u8"城市小路"), QColor(QStringLiteral("#CBD5E0"))},
        {QString::fromUtf8(u8"其他道路"), QColor(QStringLiteral("#64748B"))},
    };
}

void applyDefault(CaseMapLegendWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->hide();
}

void showCustom(CaseMapLegendWidget* widget, const QList<CaseMapLegendWidget::LegendItem>& items,
                const QString& title)
{
    if (!widget) {
        return;
    }
    widget->setLegendItems(items, title);
    widget->show();
    widget->adjustSize();
}

QString displayNameForGraduatedField(const QString& fieldKey)
{
    const QString cn = AppSession::instance().fieldChineseDisplayName(fieldKey);
    if (!cn.isEmpty()) {
        return cn;
    }
    return fieldKey;
}

QString graduatedLegendTitle(const QString& fieldKey)
{
    // 图例标题统一为"图例：字段名"，条目不再重复字段名
    return QString::fromUtf8(u8"图例：") + displayNameForGraduatedField(fieldKey);
}

QString formatGraduatedRangeLabel(double lower, double upper)
{
    if (std::isinf(upper) && upper > 0) {
        return QStringLiteral("%1-max").arg(formatLegendNumber(lower));
    }
    return QStringLiteral("%1-%2").arg(formatLegendNumber(lower), formatLegendNumber(upper));
}

QList<CaseMapLegendWidget::LegendItem> itemsFromGraduatedRenderer(
    const QgsGraduatedSymbolRenderer* renderer, const QString& fieldName)
{
    Q_UNUSED(fieldName);

    QList<CaseMapLegendWidget::LegendItem> items;
    if (!renderer) {
        return items;
    }

    const QList<QgsRendererRange> ranges = renderer->ranges();
    items.reserve(ranges.size());
    for (const QgsRendererRange& range : ranges) {
        CaseMapLegendWidget::LegendItem item;
        if (range.symbol()) {
            item.color = range.symbol()->color();
        }
        if (usePresetLegendLabel(range.label())) {
            item.name = range.label();
        } else {
            item.name = formatGraduatedRangeLabel(range.lowerValue(), range.upperValue());
        }
        items.append(item);
    }
    return items;
}

QList<CaseMapLegendWidget::LegendItem> itemsFromCategorizedRenderer(
    const QgsCategorizedSymbolRenderer* renderer, const QString& fieldName)
{
    Q_UNUSED(fieldName);

    QList<CaseMapLegendWidget::LegendItem> items;
    if (!renderer) {
        return items;
    }

    const QList<QgsRendererCategory> categories = renderer->categories();
    items.reserve(categories.size());
    for (const QgsRendererCategory& category : categories) {
        CaseMapLegendWidget::LegendItem item;
        if (category.symbol()) {
            item.color = category.symbol()->color();
        }
        item.name = category.label();
        items.append(item);
    }
    return items;
}

} // namespace CaseMapLegend
