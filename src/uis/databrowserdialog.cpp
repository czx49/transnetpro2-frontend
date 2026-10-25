#include "databrowserdialog.h"

#include "../httpapis/appsession.h"
#include "../publicctrls/phase2dialogstyle.h"
#include "../publicctrls/sqltablewithaliasnamemodel.h"
#include "uiglobalstate.h"

#include <QAbstractItemModel>
#include <QColor>
#include <QDateTime>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include "../publicctrls/messagedialog.h"
#include <QPalette>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlRecord>
#include <QTableView>
#include <QVBoxLayout>

#include <qgsattributetablemodel.h>
#include <qgsfields.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayercache.h>

#include "../tnmapwidget/tnfloatdisplayattributetablemodel.h"

namespace {

constexpr int kDataBrowserHeaderPadding = 12;
constexpr int kDataBrowserHeaderExtra = 10;
constexpr int kDataBrowserMinColumnWidth = 48;

constexpr char kDataBrowserTableStyle[] =
    "QTableView#dataBrowserTable {"
    "   border: 1px solid #b8c7da;"
    "   background-color: #ffffff;"
    "   alternate-background-color: #eef6ff;"
    "   color: #111827;"
    "   gridline-color: #e1e8f2;"
    "   selection-background-color: #dbeafe;"
    "   selection-color: #111827;"
    "}"
    "QTableView#dataBrowserTable QHeaderView::section {"
    "   background-color: #eef4fd;"
    "   color: #111827;"
    "   border: none;"
    "   border-right: 1px solid #d7e1ed;"
    "   padding: 6px;"
    "}";

} // namespace

DataBrowserDialog::DataBrowserDialog(QWidget* parent)
    : FramelessDialog(parent)
{
    setMinimizeButtonVisible(true);
    setMaximizeButtonVisible(true);
    // 外壳样式挂在对话框；表格样式必须设到 QTableView 自身，
    // 父级 QSS 下 alternate-background-color 常不生效（奇偶行透出深色壳）。
    setStyleSheet(Phase2Ui::dialogShellStyleSheet());
    resize(700, 700);

    QVBoxLayout* layout = contentLayout();
    layout->setContentsMargins(8, 0, 8, 8);
    layout->setSpacing(8);

    m_tableView = new QTableView(this);
    m_tableView->setObjectName(QStringLiteral("dataBrowserTable"));
    m_tableView->setStyleSheet(QString::fromUtf8(kDataBrowserTableStyle));
    QPalette tablePal = m_tableView->palette();
    tablePal.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
    tablePal.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#eef6ff")));
    tablePal.setColor(QPalette::Text, QColor(QStringLiteral("#111827")));
    tablePal.setColor(QPalette::Highlight, QColor(QStringLiteral("#dbeafe")));
    tablePal.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#111827")));
    m_tableView->setPalette(tablePal);
    if (m_tableView->viewport()) {
        m_tableView->viewport()->setAutoFillBackground(true);
        m_tableView->viewport()->setPalette(tablePal);
    }
    setupTableView(m_tableView);
    layout->addWidget(m_tableView, 1);

    auto* footer = new QFrame(this);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->addStretch(1);
    QPushButton* closeButton = new QPushButton(QString::fromUtf8(u8"关闭"), footer);
    closeButton->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(closeButton, &QPushButton::clicked, this, &DataBrowserDialog::accept);
    footerLayout->addWidget(closeButton);
    layout->addWidget(footer);
}

DataBrowserDialog::~DataBrowserDialog()
{
    releaseVectorLayerModel();
    if (!m_sqlConnectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_sqlConnectionName);
    }
}

void DataBrowserDialog::setHeaderAliasMap(const QMap<QString, QString>& map)
{
    m_headerAliasMap = map;
    applyHeaderTranslations();
    updateColumnsWidth(true);
}

void DataBrowserDialog::setVisibleFields(const QStringList& fields)
{
    m_visibleFields = fields;
    applyVisibleFields();
    updateColumnsWidth(true);
}

void DataBrowserDialog::setRowFilter(const QString& filter)
{
    m_rowFilter = filter.trimmed();
}

void DataBrowserDialog::loadTable(const QString& tableName)
{
    m_tableName = tableName.trimmed();
    if (m_tableName.isEmpty()) {
        return;
    }

    releaseVectorLayerModel();
    ensureSqlModel();
    if (!m_sqlModel) {
        return;
    }

    m_mode = DataSourceMode::PgTable;
    if (m_tableView) {
        m_tableView->setModel(m_sqlModel);
    }

    m_sqlModel->setTable(m_tableName);
    m_sqlModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    if (!m_rowFilter.isEmpty()) {
        m_sqlModel->setFilter(m_rowFilter);
    }
    if (!m_sqlModel->select()) {
        TnUi::showWarning(QString::fromUtf8(u8"错误"), m_sqlModel->lastError().text());
        return;
    }

    applyHeaderTranslations();
    applyVisibleFields();
    m_columnWidthMode = ColumnWidthMode::Content;
    updateColumnsWidth(true);
}

void DataBrowserDialog::loadVectorLayer(QgsVectorLayer* layer)
{
    if (!layer || !layer->isValid()) {
        TnUi::showWarning(QString::fromUtf8(u8"错误"),
                             QString::fromUtf8(u8"图层无效或未就绪。"));
        return;
    }

    releaseVectorLayerModel();
    m_mode = DataSourceMode::None;
    if (m_tableView) {
        m_tableView->setModel(nullptr);
    }

    m_layerCache = new QgsVectorLayerCache(layer, -1, this);
    m_layerCache->setFullCache(true);
    m_layerCache->setCacheGeometry(false);

    m_vectorModel = new TnFloatDisplayAttributeTableModel(m_layerCache, this);
    m_vectorModel->loadLayer();

    m_mode = DataSourceMode::VectorLayer;
    if (m_tableView) {
        m_tableView->setModel(m_vectorModel);
    }

    applyHeaderTranslations();
    applyVisibleFields();
    m_columnWidthMode = ColumnWidthMode::Content;
    updateColumnsWidth(true);
}

void DataBrowserDialog::moveToRightTop(QWidget* parent)
{
    QWidget* mainWindow = parent ? parent->window() : window();
    if (!mainWindow) {
        return;
    }

    show();
    raise();
    activateWindow();

    const QSize dlgSize = size();
    const QRect mainWindowRect = mainWindow->geometry();
    const int targetX = mainWindowRect.topRight().x() - dlgSize.width() - 10;
    const int targetY = mainWindowRect.topRight().y() + 10;
    move(targetX, targetY);

    int targetHeight = mainWindow->height() - 200;
    targetHeight = targetHeight > 200 ? targetHeight : 200;
    resize(700, targetHeight);
}

void DataBrowserDialog::showEvent(QShowEvent* event)
{
    FramelessDialog::showEvent(event);
    updateColumnsWidth(true);
}

void DataBrowserDialog::resizeEvent(QResizeEvent* event)
{
    FramelessDialog::resizeEvent(event);
    if (m_columnWidthMode == ColumnWidthMode::Fill) {
        updateColumnsWidth(false);
    }
}

void DataBrowserDialog::ensureSqlModel()
{
    if (m_sqlModel) {
        return;
    }

    const QSharedPointer<ProjectDetailJson> projectDetail = UIG().currentProjectDetail();
    if (projectDetail.isNull()) {
        TnUi::showCritical(QString::fromUtf8(u8"数据库错误"),
                              QString::fromUtf8(u8"当前未打开项目。"));
        return;
    }

    const DBConfigJson dbConfig = projectDetail->j_db_conn_str();
    m_sqlConnectionName = QStringLiteral("DataBrowser_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), m_sqlConnectionName);
    database.setHostName(dbConfig.j_host());
    database.setPort(dbConfig.j_port());
    database.setDatabaseName(dbConfig.j_dbname());
    database.setUserName(dbConfig.j_user());
    database.setPassword(dbConfig.j_password());
    if (!database.open()) {
        TnUi::showCritical(QString::fromUtf8(u8"数据库错误"), database.lastError().text());
        m_sqlConnectionName.clear();
        return;
    }

    m_sqlModel = new SqlTableWithAliasNameModel(this, database);
}

void DataBrowserDialog::releaseVectorLayerModel()
{
    if (m_tableView && m_tableView->model() == m_vectorModel) {
        m_tableView->setModel(nullptr);
    }

    if (m_vectorModel) {
        delete m_vectorModel;
        m_vectorModel = nullptr;
    }
    if (m_layerCache) {
        delete m_layerCache;
        m_layerCache = nullptr;
    }

    if (m_mode == DataSourceMode::VectorLayer) {
        m_mode = DataSourceMode::None;
    }
}

void DataBrowserDialog::setupTableView(QTableView* view)
{
    if (!view) {
        return;
    }
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setFrameShape(QFrame::NoFrame);
    view->setAlternatingRowColors(true);
    if (view->verticalHeader()) {
        view->verticalHeader()->hide();
        view->verticalHeader()->setDefaultSectionSize(32);
    }
    if (view->horizontalHeader()) {
        view->horizontalHeader()->setStretchLastSection(false);
        view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        view->horizontalHeader()->setMinimumSectionSize(kDataBrowserMinColumnWidth);
        view->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
        view->horizontalHeader()->setHighlightSections(false);
    }
    view->setShowGrid(true);
}

QAbstractItemModel* DataBrowserDialog::activeTableModel() const
{
    if (m_mode == DataSourceMode::PgTable) {
        return m_sqlModel;
    }
    if (m_mode == DataSourceMode::VectorLayer) {
        return m_vectorModel;
    }
    return nullptr;
}

void DataBrowserDialog::applyHeaderTranslations()
{
    if (m_mode == DataSourceMode::PgTable && m_sqlModel) {
        const QSqlRecord rec = m_sqlModel->record();
        for (int i = 0; i < rec.count(); ++i) {
            const QString field = rec.fieldName(i);
            QString displayName = m_headerAliasMap.value(field);
            if (displayName.isEmpty()) {
                displayName = AppSession::instance().fieldChineseDisplayName(field);
            }
            if (!displayName.isEmpty()) {
                m_sqlModel->setHeaderData(i, Qt::Horizontal, displayName, Qt::DisplayRole);
            }
        }
        return;
    }

    if (m_mode == DataSourceMode::VectorLayer && m_vectorModel && m_layerCache && m_layerCache->layer()) {
        // QgsAttributeTableModel 的 headerData 始终读 field.displayName()，setHeaderData 无效；
        // 与数据页一致：对图层 setFieldAlias 后表头才会显示中文。
        QgsVectorLayer* layer = m_layerCache->layer();
        const QgsFields& fields = layer->fields();
        const int columnCount = m_vectorModel->columnCount();
        int firstChanged = -1;
        int lastChanged = -1;
        for (int col = 0; col < columnCount; ++col) {
            const int fieldIdx = m_vectorModel->fieldCol(col);
            if (fieldIdx < 0 || fieldIdx >= fields.count()) {
                continue;
            }
            const QgsField& field = fields.at(fieldIdx);
            QString displayName = m_headerAliasMap.value(field.name());
            if (displayName.isEmpty()) {
                displayName = field.displayName();
            }
            if (displayName == field.name()) {
                displayName = AppSession::instance().fieldChineseDisplayName(field.name());
            }
            if (displayName.isEmpty() || displayName == field.name()) {
                continue;
            }
            if (field.alias() == displayName) {
                continue;
            }
            layer->setFieldAlias(fieldIdx, displayName);
            if (firstChanged < 0) {
                firstChanged = col;
            }
            lastChanged = col;
        }
        if (firstChanged >= 0) {
            emit m_vectorModel->headerDataChanged(Qt::Horizontal, firstChanged, lastChanged);
        }
    }
}

void DataBrowserDialog::applyVisibleFields()
{
    QAbstractItemModel* model = activeTableModel();
    if (!model || !m_tableView) {
        return;
    }

    if (m_visibleFields.isEmpty()) {
        for (int i = 0; i < model->columnCount(); ++i) {
            m_tableView->setColumnHidden(i, false);
        }
        return;
    }

    if (m_mode == DataSourceMode::PgTable && m_sqlModel) {
        const QSqlRecord rec = m_sqlModel->record();
        QSet<int> showIdx;
        for (const QString& field : m_visibleFields) {
            const int idx = rec.indexOf(field);
            if (idx >= 0) {
                showIdx.insert(idx);
            }
        }
        for (int i = 0; i < model->columnCount(); ++i) {
            m_tableView->setColumnHidden(i, !showIdx.contains(i));
        }
        return;
    }

    if (m_mode == DataSourceMode::VectorLayer && m_vectorModel && m_layerCache && m_layerCache->layer()) {
        const QgsFields& fields = m_layerCache->layer()->fields();
        QSet<int> showCols;
        const int columnCount = m_vectorModel->columnCount();
        for (int col = 0; col < columnCount; ++col) {
            const int fieldIdx = m_vectorModel->fieldCol(col);
            if (fieldIdx < 0 || fieldIdx >= fields.count()) {
                continue;
            }
            const QString field = fields.at(fieldIdx).name();
            if (m_visibleFields.contains(field)) {
                showCols.insert(col);
            }
        }
        for (int i = 0; i < columnCount; ++i) {
            m_tableView->setColumnHidden(i, !showCols.contains(i));
        }
    }
}

void DataBrowserDialog::updateColumnsWidth(bool forceFull)
{
    QAbstractItemModel* model = activeTableModel();
    if (!model || !m_tableView) {
        return;
    }

    const int columnCount = model->columnCount();
    if (columnCount == 0) {
        return;
    }

    QList<int> visibleColumns;
    visibleColumns.reserve(columnCount);
    for (int i = 0; i < columnCount; ++i) {
        if (!m_tableView->isColumnHidden(i)) {
            visibleColumns.append(i);
        }
    }
    if (visibleColumns.isEmpty()) {
        return;
    }

    const int contentWidth = tableContentWidth();
    if (contentWidth <= 0) {
        return;
    }

    const int visibleCount = visibleColumns.size();

    if (forceFull || m_columnWidthMode == ColumnWidthMode::Content) {
        QList<int> preferredWidths;
        preferredWidths.reserve(visibleCount);
        int sumPreferred = 0;
        for (int col : visibleColumns) {
            const int w = preferredColumnWidth(col);
            preferredWidths.append(w);
            sumPreferred += w;
        }

        if (sumPreferred < contentWidth) {
            m_columnWidthMode = ColumnWidthMode::Fill;
        } else {
            m_columnWidthMode = ColumnWidthMode::Content;
            for (int i = 0; i < visibleCount; ++i) {
                m_tableView->setColumnWidth(visibleColumns.at(i), preferredWidths.at(i));
            }
            return;
        }
    }

    const int perWidth = qMax(kDataBrowserMinColumnWidth, contentWidth / visibleCount);
    int assigned = 0;
    for (int i = 0; i < visibleCount; ++i) {
        const int w = (i == visibleCount - 1) ? qMax(kDataBrowserMinColumnWidth, contentWidth - assigned) : perWidth;
        assigned += w;
        m_tableView->setColumnWidth(visibleColumns.at(i), w);
    }
}

int DataBrowserDialog::tableContentWidth() const
{
    if (!m_tableView) {
        return 0;
    }
    int contentWidth = m_tableView->viewport() ? m_tableView->viewport()->width() : (m_tableView->width() - 34);
    if (m_tableView->verticalHeader() && !m_tableView->verticalHeader()->isHidden()) {
        contentWidth -= m_tableView->verticalHeader()->width();
    }
    return qMax(0, contentWidth);
}

int DataBrowserDialog::preferredColumnWidth(int column) const
{
    QAbstractItemModel* model = activeTableModel();
    if (!model || column < 0 || column >= model->columnCount()) {
        return kDataBrowserMinColumnWidth;
    }

    const QString headerText = model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
    QHeaderView* header = m_tableView ? m_tableView->horizontalHeader() : nullptr;
    QFontMetrics metrics(header ? header->font() : font());
    const int textWidth = metrics.horizontalAdvance(headerText);
    return qMax(kDataBrowserMinColumnWidth, textWidth + kDataBrowserHeaderPadding + kDataBrowserHeaderExtra);
}
