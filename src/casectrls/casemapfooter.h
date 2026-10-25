#ifndef CASEMAPFOOTER_H
#define CASEMAPFOOTER_H

#include <QString>

class QPushButton;
class QWidget;

namespace CaseMapFooter {

QPushButton* createSegmentButton(const QString& text, bool checked, QWidget* parent);
QString darkMapFooterLabelStyle();

} // namespace CaseMapFooter

#endif // CASEMAPFOOTER_H
