#ifndef TN_ATTRIBUTETABLEDIALOG_H
#define TN_ATTRIBUTETABLEDIALOG_H

#include "framelessdialog.h"
#include <qgsvectorlayer.h>
#include <qgsfeature.h>

class QTableView;
class QStandardItemModel;

//中文
class TNAttributeTable : public FramelessDialog
{
    Q_OBJECT

public:
    explicit TNAttributeTable(QWidget* parent = nullptr);
    ~TNAttributeTable();

    void setLayer(QgsVectorLayer* layer);
    void highlightFeature(const QgsFeatureId& fid);

signals:
    void featureSelected(const QgsFeature& feature);
public slots:
    void onFeatureSelected(const QgsFeature& feature);

private:
    void setupUI();
    void populateTable();
    void clearTable();

    QgsVectorLayer* currentLayer = nullptr;
    QTableView* tableView;
    QStandardItemModel* tableModel;
};

#endif // TN_ATTRIBUTETABLEDIALOG_H