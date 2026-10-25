#include "tableview.h"
#include <QHeaderView>

TableView::TableView(QWidget* parent)
    : QTableView { parent }
{
    if (verticalHeader())
        verticalHeader()->setVisible(false);
}

void TableView::setDataState(DataReadState state)
{
    if (state != m_dataState) {
        m_dataState = state;
        emit dataStateChanged();
    }
}

TableView::DataReadState TableView::dataState() const
{
    return m_dataState;
}
