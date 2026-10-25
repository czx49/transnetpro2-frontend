#ifndef DATABROWSERDIALOG_H
#define DATABROWSERDIALOG_H

#include "../publicctrls/framelessdialog.h"

#include <QMap>
#include <QStringList>

class QAbstractItemModel;

class QTableView;
class SqlTableWithAliasNameModel;
class QgsAttributeTableModel;
class QgsVectorLayer;
class QgsVectorLayerCache;

class DataBrowserDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit DataBrowserDialog(QWidget* parent = nullptr);
    ~DataBrowserDialog() override;

    void setHeaderAliasMap(const QMap<QString, QString>& map);
    void setVisibleFields(const QStringList& fields);
    void setRowFilter(const QString& filter);
    void loadTable(const QString& tableName);
    void loadVectorLayer(QgsVectorLayer* layer);
    void moveToRightTop(QWidget* parent);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class DataSourceMode { None, PgTable, VectorLayer };
    enum class ColumnWidthMode { Content, Fill };

    void ensureSqlModel();
    void releaseVectorLayerModel();
    void applyHeaderTranslations();
    void applyVisibleFields();
    /** @brief forceFull 为 true 时按表头内容策略重算；false 时仅 Fill 模式均分 */
    void updateColumnsWidth(bool forceFull = true);
    void setupTableView(QTableView* view);
    QAbstractItemModel* activeTableModel() const;
    int preferredColumnWidth(int column) const;
    int tableContentWidth() const;

    QTableView* m_tableView = nullptr;
    SqlTableWithAliasNameModel* m_sqlModel = nullptr;
    QgsAttributeTableModel* m_vectorModel = nullptr;
    QgsVectorLayerCache* m_layerCache = nullptr;
    DataSourceMode m_mode = DataSourceMode::None;
    ColumnWidthMode m_columnWidthMode = ColumnWidthMode::Content;
    QString m_tableName;
    QString m_rowFilter;
    QString m_sqlConnectionName;
    QMap<QString, QString> m_headerAliasMap;
    QStringList m_visibleFields;
};

#endif // DATABROWSERDIALOG_H
