/*****************************************************************//**
 * \file   tnattributetable.cpp
 * \brief  属性表弹窗（FramelessDialog 深色套件）
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "tnattributetable.h"

#include "../publicctrls/numberdisplayutil.h"
#include "../publicctrls/phase2dialogstyle.h"

#include <qgsvectorlayer.h>
#include <qgsfeatureiterator.h>
#include <qgsfield.h>

#include <QHeaderView>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

TNAttributeTable::TNAttributeTable(QWidget* parent)
    : FramelessDialog(parent)
{
    setupUI();
    setTitleText(QString::fromUtf8(u8"属性表"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    resize(600, 400);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                  + Phase2Ui::dialogLightContentPanelStyleSheet(QStringLiteral("tnAttributeTableView")));
}

TNAttributeTable::~TNAttributeTable() = default;

void TNAttributeTable::setupUI()
{
    QVBoxLayout* layout = contentLayout();
    layout->setContentsMargins(12, 12, 12, 12);

    tableView = new QTableView(this);
    tableView->setObjectName(QStringLiteral("tnAttributeTableView"));
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->verticalHeader()->setVisible(false);
    tableView->setStyleSheet(QStringLiteral(
        "QTableView#tnAttributeTableView {"
        "   background: #ffffff; color: #111827; border: 1px solid #c7d8ee; border-radius: 10px;"
        "   gridline-color: #e2e8f0;"
        "}"
        "QHeaderView::section {"
        "   background: #f1f5f9; color: #0f172a; padding: 8px 10px; border: none;"
        "   border-right: 1px solid #e2e8f0; border-bottom: 1px solid #e2e8f0;"
        "}"));

    layout->addWidget(tableView);

    tableModel = new QStandardItemModel(this);
    tableView->setModel(tableModel);
}

void TNAttributeTable::setLayer(QgsVectorLayer* layer)
{
    if (currentLayer == layer) {
        return;
    }

    currentLayer = layer;
    populateTable();
}

void TNAttributeTable::populateTable()
{
    clearTable();

    if (!currentLayer || !currentLayer->isValid()) {
        return;
    }

    const QgsFields fields = currentLayer->fields();
    QStringList headers;
    headers << QStringLiteral("ID");

    for (int i = 0; i < fields.count(); ++i) {
        headers << fields.at(i).name();
    }

    tableModel->setHorizontalHeaderLabels(headers);

    QgsFeatureIterator features = currentLayer->getFeatures();
    QgsFeature feature;

    while (features.nextFeature(feature)) {
        QList<QStandardItem*> rowItems;
        rowItems << new QStandardItem(QString::number(feature.id()));

        for (int i = 0; i < fields.count(); ++i) {
            const QgsField& field = fields.at(i);
            const QVariant value = feature.attribute(i);
            rowItems << new QStandardItem(
                NumberDisplayUtil::formatAttributeDisplay(value, static_cast<int>(field.type())));
        }

        tableModel->appendRow(rowItems);
    }
}

void TNAttributeTable::clearTable()
{
    tableModel->clear();
    tableModel->setColumnCount(0);
}

void TNAttributeTable::highlightFeature(const QgsFeatureId& fid)
{
    for (int row = 0; row < tableModel->rowCount(); ++row) {
        if (tableModel->item(row, 0)->text().toLongLong() == fid) {
            tableView->selectRow(row);
            tableView->scrollTo(tableModel->index(row, 0));
            break;
        }
    }
}

void TNAttributeTable::onFeatureSelected(const QgsFeature& feature)
{
    highlightFeature(feature.id());
}
