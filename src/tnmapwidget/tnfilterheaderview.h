#ifndef TN_FILTER_HEADER_VIEW_H
#define TN_FILTER_HEADER_VIEW_H

#include <QHeaderView>
#include <QLineEdit>
#include <QMap>

class TNFilterHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit TNFilterHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);

    /** @brief 浅色表头（与二期属性表/选择集一致） */
    void setPhase2Chrome(bool enabled);
    bool phase2Chrome() const { return m_phase2Chrome; }

    // Set the filter text for a column (programmatically)
    void setFilterValue(int logicalIndex, const QString& value);
    QString filterValue(int logicalIndex) const;
    void clearAllFilters();

signals:
    void filterChanged(int logicalIndex, const QString& filterValue);

protected:
    void paintEvent(QPaintEvent* event) override;
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    
private slots:
    void onFilterReturnPressed();

private:
    void showInput(int logicalIndex);
    void hideInput(int logicalIndex);
    void updateInputGeometries();
    void applyFilterEditorStyle(QLineEdit* edit);

    QMap<int, QString> mFilterValues;
    QMap<int, QLineEdit*> mEditors;
    QIcon mSearchIcon;
    bool m_phase2Chrome = false;
};

#endif // TN_FILTER_HEADER_VIEW_H
