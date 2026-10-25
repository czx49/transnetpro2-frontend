#include "tnfilterheaderview.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QStyleOptionHeader>
#include <QStyle>
#include <QPixmap>

namespace {

const char* kFilterEditorObjectName = "tnAttrHeaderFilterEdit";

QString filterEditorStyleSheet(bool phase2Chrome)
{
    const QString borderColor = phase2Chrome ? QStringLiteral("#93c5fd") : QStringLiteral("#c0c0c0");
    const QString textColor = phase2Chrome ? QStringLiteral("#0f172a") : QStringLiteral("#333333");
    const QString placeholderColor = phase2Chrome ? QStringLiteral("#94a3b8") : QStringLiteral("#888888");

    return QStringLiteral(
        "QLineEdit#tnAttrHeaderFilterEdit {"
        "  background: #ffffff;"
        "  color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 3px;"
        "  padding: 0 6px;"
        "  selection-background-color: #dbeafe;"
        "  selection-color: #0f172a;"
        "}"
        "QLineEdit#tnAttrHeaderFilterEdit:focus {"
        "  border-color: #4f96e8;"
        "}"
        "QLineEdit#tnAttrHeaderFilterEdit::placeholder {"
        "  color: %3;"
        "}")
        .arg(textColor, borderColor, placeholderColor);
}

} // namespace

TNFilterHeaderView::TNFilterHeaderView(Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent)
{
    // Always use generated icon for better quality (High-DPI & Antialiasing)
    // mSearchIcon = QIcon(":/styles/default/images/search.png");
    
    int dpr = qApp->devicePixelRatio();
    int size = 16 * dpr;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(dpr);
    
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::gray, 1.5));
    
    int radius = 6;
    p.drawEllipse(QPointF(7.0, 7.0), radius, radius);
    p.drawLine(QPointF(11.5, 11.5), QPointF(14.5, 14.5));
    mSearchIcon = QIcon(pixmap);
    
    setSectionsClickable(true);
}

void TNFilterHeaderView::setPhase2Chrome(bool enabled)
{
    m_phase2Chrome = enabled;
    for (QLineEdit* edit : mEditors) {
        applyFilterEditorStyle(edit);
    }
    viewport()->update();
}

void TNFilterHeaderView::setFilterValue(int logicalIndex, const QString& value)
{
    if (value.isEmpty()) {
        mFilterValues.remove(logicalIndex);
        hideInput(logicalIndex);
    } else {
        mFilterValues[logicalIndex] = value;
        showInput(logicalIndex);
        if (mEditors.contains(logicalIndex)) {
            mEditors[logicalIndex]->setText(value);
        }
    }
    viewport()->update();
}

QString TNFilterHeaderView::filterValue(int logicalIndex) const
{
    return mFilterValues.value(logicalIndex);
}

void TNFilterHeaderView::clearAllFilters()
{
    mFilterValues.clear();
    // Hide all editors
    QList<int> keys = mEditors.keys();
    for(int key : keys) {
        hideInput(key);
    }
    viewport()->update();
}

void TNFilterHeaderView::paintEvent(QPaintEvent* event)
{
    if (m_phase2Chrome && viewport()) {
        QPainter painter(viewport());
        painter.fillRect(viewport()->rect(), QColor(QStringLiteral("#d6ebfc")));
    }
    QHeaderView::paintEvent(event);
}

void TNFilterHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    // If there is an editor, just ensure background is drawn (by base class) and editor geometry is correct.
    // However, base class paintSection draws text which might interfere if editor is transparent.
    // Since QLineEdit is usually opaque, it's fine.
    if (mEditors.contains(logicalIndex)) {
        if (m_phase2Chrome) {
            painter->fillRect(rect, QColor(QStringLiteral("#d6ebfc")));
        }
        QHeaderView::paintSection(painter, rect, logicalIndex);

        QLineEdit* edit = mEditors[logicalIndex];
        if (edit) {
            const_cast<QLineEdit*>(edit)->setGeometry(rect);
             if (!edit->isVisible()) const_cast<QLineEdit*>(edit)->show();
        }
        return;
    }

    // Use QStyleOptionHeader to draw the section with icon
    QStyleOptionHeader opt;
    initStyleOption(&opt);
    opt.rect = rect;
    opt.section = logicalIndex;
    
    // Inject our search icon（左侧留白略增，避免与文字挤在一起）
    opt.icon = mSearchIcon;
    opt.iconAlignment = Qt::AlignLeft | Qt::AlignVCenter;

    // Ensure text is properly set from the model
    QAbstractItemModel* model = this->model();
    if (model) {
        opt.text = model->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
        opt.textAlignment = Qt::AlignCenter;

        // Handle sorting indicator if needed
        if (isSortIndicatorShown() && sortIndicatorSection() == logicalIndex) {
            opt.sortIndicator = (sortIndicatorOrder() == Qt::AscendingOrder)
                                    ? QStyleOptionHeader::SortDown
                                    : QStyleOptionHeader::SortUp;
        }
    }

    if (m_phase2Chrome) {
        const QColor headerBg(QStringLiteral("#d6ebfc"));
        painter->fillRect(rect, headerBg);
        opt.palette.setBrush(QPalette::Button, headerBg);
        opt.palette.setBrush(QPalette::Window, headerBg);
        opt.palette.setBrush(QPalette::Light, headerBg);
        opt.palette.setBrush(QPalette::Midlight, headerBg);
        opt.palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#0f172a")));
    }

    // Draw the control
    style()->drawControl(QStyle::CE_Header, &opt, painter, this);

    if (m_phase2Chrome) {
        painter->save();
        painter->setPen(QColor(QStringLiteral("#bcdcff")));
        painter->drawLine(rect.topRight(), rect.bottomRight());
        painter->setPen(QColor(QStringLiteral("#93c5fd")));
        painter->drawLine(rect.bottomLeft(), rect.bottomRight());
        painter->restore();
    }
}

void TNFilterHeaderView::mouseReleaseEvent(QMouseEvent *e)
{
    int logicalIndex = logicalIndexAt(e->pos());
    if (logicalIndex >= 0) {
        // If there is already an editor, do nothing (click goes to editor or we shouldn't interfere)
        if (mEditors.contains(logicalIndex)) {
             QHeaderView::mouseReleaseEvent(e);
             return;
        }

        int width = this->sectionSize(logicalIndex);
        int sectionPos = this->sectionViewportPosition(logicalIndex);
        
        int iconSize = 16;
        int padding = 4;
        
        // Check click on icon
        // We need to calculate icon rect relative to viewport
        // sectionPos is relative to viewport
        QRect rect(sectionPos, 0, width, height());
        // Match the painting logic: left aligned
        QRect iconRect(rect.left() + padding, rect.top() + (rect.height() - iconSize) / 2, iconSize, iconSize);

        if (iconRect.contains(e->pos())) {
            showInput(logicalIndex);
            e->accept();
            return;
        }
    }
    QHeaderView::mouseReleaseEvent(e);
}

void TNFilterHeaderView::showInput(int logicalIndex)
{
    if (mEditors.contains(logicalIndex)) {
        mEditors[logicalIndex]->setFocus();
        return;
    }

    QLineEdit* edit = new QLineEdit(this);
    edit->setObjectName(QString::fromLatin1(kFilterEditorObjectName));
    edit->setPlaceholderText(u8"搜索...");
    edit->setProperty("column", logicalIndex);
    applyFilterEditorStyle(edit);

    if (mFilterValues.contains(logicalIndex)) {
        edit->setText(mFilterValues[logicalIndex]);
    }

    connect(edit, &QLineEdit::editingFinished, this, &TNFilterHeaderView::onFilterReturnPressed);
    
    // Set initial geometry
    int sectionPos = sectionViewportPosition(logicalIndex);
    int width = sectionSize(logicalIndex);
    edit->setGeometry(sectionPos, 0, width, height());

    mEditors[logicalIndex] = edit;
    edit->show();
    edit->setFocus();
}

void TNFilterHeaderView::hideInput(int logicalIndex)
{
    if (mEditors.contains(logicalIndex)) {
        QLineEdit* edit = mEditors[logicalIndex];
        mEditors.remove(logicalIndex);
        edit->deleteLater();
        viewport()->update();
    }
}

void TNFilterHeaderView::onFilterReturnPressed()
{
    QLineEdit* edit = qobject_cast<QLineEdit*>(sender());
    if (!edit) return;

    int logicalIndex = edit->property("column").toInt();
    QString text = edit->text();

    if (text.isEmpty()) {
        mFilterValues.remove(logicalIndex);
        hideInput(logicalIndex);
    } else {
        mFilterValues[logicalIndex] = text;
    }

    emit filterChanged(logicalIndex, text);
}

void TNFilterHeaderView::updateInputGeometries()
{
    for (auto it = mEditors.begin(); it != mEditors.end(); ++it) {
        int logicalIndex = it.key();
        QLineEdit* edit = it.value();
        if (edit) {
             int pos = sectionViewportPosition(logicalIndex);
             int size = sectionSize(logicalIndex);
             edit->setGeometry(pos, 0, size, height());
        }
    }
}

void TNFilterHeaderView::applyFilterEditorStyle(QLineEdit* edit)
{
    if (!edit) {
        return;
    }
    edit->setStyleSheet(filterEditorStyleSheet(m_phase2Chrome));
}
