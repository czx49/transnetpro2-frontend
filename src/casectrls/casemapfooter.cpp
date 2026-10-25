#include "casemapfooter.h"

#include <QPushButton>

namespace {

constexpr char kDesignAccentBlue[] = "#1890ff";

} // namespace

namespace CaseMapFooter {

QPushButton* createSegmentButton(const QString& text, bool checked, QWidget* parent)
{
    QPushButton* button = new QPushButton(text, parent);
    button->setCheckable(true);
    button->setChecked(checked);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumSize(72, 30);
    button->setStyleSheet(QString(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 7px;"
        "   padding: 0 18px;"
        "   background-color: transparent;"
        "   color: rgba(255,255,255,0.72);"
        "   font-size: 13px;"
        "   font-weight: 700;"
        "}"
        "QPushButton:checked {"
        "   background-color: %1;"
        "   color: white;"
        "}"
        "QPushButton:!checked:hover {"
        "   background-color: rgba(255,255,255,0.08);"
        "   color: rgba(255,255,255,0.92);"
        "}"
    ).arg(QLatin1String(kDesignAccentBlue)));
    return button;
}

QString darkMapFooterLabelStyle()
{
    return QStringLiteral(
        "QLabel {"
        "   color: rgba(255,255,255,0.82);"
        "   font-size: 13px;"
        "}"
    );
}

} // namespace CaseMapFooter
