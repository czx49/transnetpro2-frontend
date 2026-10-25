#include "tnattributewidget.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QEvent>
#include <QFile>
#include <QFontMetrics>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include "../publicctrls/messagedialog.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QProgressDialog>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QTextStream>
#include <QVBoxLayout>

#include "../publicctrls/phase2comboboxutil.h"
#include "qgsattributetabledelegate.h"
#include "qgsattributetablefiltermodel.h"
#include "qgsattributetablemodel.h"
#include "tnfloatdisplayattributetablemodel.h"
#include "qgsattributeeditorcontext.h"
#include "qgseditorwidgetregistry.h"
#include "qgseditorwidgetwrapper.h"
#include "qgsexpressioncontextutils.h"
#include "qgsfeaturerequest.h"
#include "qgsfilterlineedit.h"
#include "qgsgui.h"
#include "qgsvectorlayer.h"
#include "qgsvectorlayerjoinbuffer.h"
#include "qgsvectorlayerjoininfo.h"

#include <QPointer>
#include <QTimer>

namespace {

/** 搜索图标 16px + 左侧 padding 4px */
constexpr int kSearchIconAreaWidth = 20;
/** 表头文本右侧余量 */
constexpr int kHeaderTextExtraWidth = 10;
/** 列宽下限，避免拖得过窄 */
constexpr int kMinColumnWidth = 48;

/** LightCompact 色值，无固定高度，适配表格单元格 */
QString attributeCellFieldStyleSheet()
{
    return QStringLiteral(
        "QLineEdit, QSpinBox, QDoubleSpinBox, QDateTimeEdit {"
        "   border: 1px solid #b8c7da;"
        "   border-radius: 4px;"
        "   background-color: #ffffff;"
        "   color: #111827;"
        "   padding: 0 6px;"
        "   font-size: 13px;"
        "   selection-background-color: #dbeafe;"
        "   selection-color: #111827;"
        "}"
        "QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QDateTimeEdit:hover {"
        "   border-color: #4f96e8;"
        "}"
        "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateTimeEdit:focus {"
        "   border-color: #4f96e8;"
        "}"
        "QLineEdit::placeholder {"
        "   color: #94a3b8;"
        "}"
        "QDateTimeEdit::drop-down {"
        "   subcontrol-origin: padding;"
        "   subcontrol-position: top right;"
        "   width: 24px;"
        "   border-left: 1px solid #b8c7da;"
        "   background-color: #f8fafc;"
        "   border-top-right-radius: 4px;"
        "   border-bottom-right-radius: 4px;"
        "}"
        "QSpinBox::up-button, QDoubleSpinBox::up-button {"
        "   subcontrol-origin: border;"
        "   subcontrol-position: top right;"
        "   width: 18px;"
        "   border-left: 1px solid #b8c7da;"
        "   background-color: #f8fafc;"
        "}"
        "QSpinBox::down-button, QDoubleSpinBox::down-button {"
        "   subcontrol-origin: border;"
        "   subcontrol-position: bottom right;"
        "   width: 18px;"
        "   border-left: 1px solid #b8c7da;"
        "   background-color: #f8fafc;"
        "}"
        "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {"
        "   width: 0; height: 0;"
        "   border-left: 4px solid transparent;"
        "   border-right: 4px solid transparent;"
        "   border-bottom: 5px solid #475569;"
        "}"
        "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {"
        "   width: 0; height: 0;"
        "   border-left: 4px solid transparent;"
        "   border-right: 4px solid transparent;"
        "   border-top: 5px solid #475569;"
        "}");
}

void applyLightEditorPalette(QWidget* widget)
{
    if (!widget) {
        return;
    }
    QPalette pal = widget->palette();
    const QColor bg(QStringLiteral("#ffffff"));
    const QColor fg(QStringLiteral("#111827"));
    const QColor selBg(QStringLiteral("#dbeafe"));
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::Base, bg);
    pal.setColor(QPalette::Button, bg);
    pal.setColor(QPalette::Text, fg);
    pal.setColor(QPalette::WindowText, fg);
    pal.setColor(QPalette::ButtonText, fg);
    pal.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#94a3b8")));
    pal.setColor(QPalette::Highlight, selBg);
    pal.setColor(QPalette::HighlightedText, fg);
    widget->setPalette(pal);
    widget->setAutoFillBackground(true);
}

void applyFieldStyleToWidget(QWidget* widget, const QString& sheet)
{
    if (!widget) {
        return;
    }
    applyLightEditorPalette(widget);
    widget->setStyleSheet(sheet);
    widget->setMinimumHeight(0);
    widget->setMaximumHeight(QWIDGETSIZE_MAX);
}

/**
 * QgsFilterLineEdit::onTextChanged 会用空 mStyleSheet 覆盖 setStyleSheet。
 * 在其槽之后再刷浅色样式；null 时保留斜体灰字并带上浅色底。
 */
void reapplyFilterLineEditStyle(QgsFilterLineEdit* fle)
{
    if (!fle) {
        return;
    }
    const QString sheet = attributeCellFieldStyleSheet();
    applyLightEditorPalette(fle);
    if (fle->isNull()) {
        fle->setStyleSheet(QStringLiteral("QLineEdit { font: italic; color: #94a3b8; } %1").arg(sheet));
    } else {
        fle->setStyleSheet(sheet);
    }
    fle->setMinimumHeight(0);
    fle->setMaximumHeight(QWIDGETSIZE_MAX);
}

void bindFilterLineEditStyleGuard(QgsFilterLineEdit* fle)
{
    if (!fle) {
        return;
    }
    static const char* kBoundProp = "tnPhase2AttrEditorStyleBound";
    if (fle->property(kBoundProp).toBool()) {
        reapplyFilterLineEditStyle(fle);
        return;
    }
    fle->setProperty(kBoundProp, true);

    // QGIS 在 textChanged 上先执行 onTextChanged 清样式；本槽随后恢复
    QObject::connect(fle, &QLineEdit::textChanged, fle, [fle](const QString&) {
        reapplyFilterLineEditStyle(fle);
    });
    reapplyFilterLineEditStyle(fle);

    // 兜住 createEditor 返回后 Qt 调用 setEditorData 的时序
    QPointer<QgsFilterLineEdit> weakFle(fle);
    QTimer::singleShot(0, fle, [weakFle]() {
        if (weakFle) {
            reapplyFilterLineEditStyle(weakFle.data());
        }
    });
}

void applyAttributeCellEditorStyle(QWidget* editor)
{
    if (!editor) {
        return;
    }

    const QString sheet = attributeCellFieldStyleSheet();

    if (auto* combo = qobject_cast<QComboBox*>(editor)) {
        Phase2Ui::setupThemedCombo(combo, Phase2Ui::InputFieldTheme::LightCompact);
        applyLightEditorPalette(combo);
        combo->setStyleSheet(combo->styleSheet()
                             + QStringLiteral("QComboBox { min-height: 0px; max-height: 16777215px; }"));
        combo->setMinimumHeight(0);
        combo->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }

    if (auto* fle = qobject_cast<QgsFilterLineEdit*>(editor)) {
        bindFilterLineEditStyleGuard(fle);
    } else {
        applyFieldStyleToWidget(editor, sheet);
    }

    // 自身已是 FilterLineEdit 时 findChildren 不含自身；复合控件内嵌的也要绑定
    const auto filterEdits = editor->findChildren<QgsFilterLineEdit*>();
    for (QgsFilterLineEdit* fle : filterEdits) {
        bindFilterLineEditStyleGuard(fle);
    }

    for (QLineEdit* le : editor->findChildren<QLineEdit*>()) {
        if (qobject_cast<QgsFilterLineEdit*>(le)) {
            continue;
        }
        applyFieldStyleToWidget(le, sheet);
    }
    const auto spinBoxes = editor->findChildren<QAbstractSpinBox*>();
    for (QAbstractSpinBox* spin : spinBoxes) {
        applyFieldStyleToWidget(spin, sheet);
    }
    const auto dateTimes = editor->findChildren<QDateTimeEdit*>();
    for (QDateTimeEdit* dt : dateTimes) {
        applyFieldStyleToWidget(dt, sheet);
    }
    const auto combos = editor->findChildren<QComboBox*>();
    for (QComboBox* combo : combos) {
        Phase2Ui::setupThemedCombo(combo, Phase2Ui::InputFieldTheme::LightCompact);
        applyLightEditorPalette(combo);
        combo->setStyleSheet(combo->styleSheet()
                             + QStringLiteral("QComboBox { min-height: 0px; max-height: 16777215px; }"));
        combo->setMinimumHeight(0);
        combo->setMaximumHeight(QWIDGETSIZE_MAX);
    }
}

QgsVectorLayer* attributeTableLayer(const QAbstractItemModel* model)
{
    if (const auto* tm = qobject_cast<const QgsAttributeTableModel*>(model)) {
        return tm->layer();
    }
    if (const auto* fm = qobject_cast<const QgsAttributeTableFilterModel*>(model)) {
        return fm->layer();
    }
    return nullptr;
}

const QgsAttributeTableModel* attributeTableMasterModel(const QAbstractItemModel* model)
{
    if (const auto* tm = qobject_cast<const QgsAttributeTableModel*>(model)) {
        return tm;
    }
    if (const auto* fm = qobject_cast<const QgsAttributeTableFilterModel*>(model)) {
        return fm->masterModel();
    }
    return nullptr;
}

/** 单元格文字居中；createEditor 对空包装器做防护，避免未注册工厂时崩溃 */
class TnPhase2AttributeDelegate final : public QgsAttributeTableDelegate
{
public:
    explicit TnPhase2AttributeDelegate(QObject* parent = nullptr)
        : QgsAttributeTableDelegate(parent)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        opt.displayAlignment = Qt::AlignCenter;
        QgsAttributeTableDelegate::paint(painter, opt, index);
    }

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option)
        if (!index.isValid() || !index.model()) {
            return nullptr;
        }

        QgsVectorLayer* vl = attributeTableLayer(index.model());
        if (!vl) {
            return nullptr;
        }

        const QgsAttributeTableModel* master = attributeTableMasterModel(index.model());
        if (!master) {
            return nullptr;
        }

        const int fieldIdx = index.model()->data(index, static_cast<int>(QgsAttributeTableModel::CustomRole::FieldIndex)).toInt();
        if (fieldIdx < 0 || fieldIdx >= vl->fields().count()) {
            return nullptr;
        }

        QgsAttributeEditorContext context(master->editorContext(), QgsAttributeEditorContext::Popup);
        const QgsFeatureId fid =
            index.model()->data(index, static_cast<int>(QgsAttributeTableModel::CustomRole::FeatureId)).toLongLong();
        context.setFormFeature(vl->getFeature(fid));

        QgsEditorWidgetRegistry* registry = QgsGui::editorWidgetRegistry();
        if (!registry) {
            return nullptr;
        }

        QgsEditorWidgetWrapper* eww = registry->create(vl, fieldIdx, nullptr, parent, context);
        if (!eww) {
            return nullptr;
        }

        QWidget* w = eww->widget();
        if (!w) {
            delete eww;
            return nullptr;
        }

        w->setAutoFillBackground(true);
        w->setFocusPolicy(Qt::StrongFocus);

        const Qgis::FieldOrigin fieldOrigin = vl->fields().fieldOrigin(fieldIdx);
        bool readOnly = true;
        if (fieldOrigin == Qgis::FieldOrigin::Join) {
            int srcFieldIndex = -1;
            const QgsVectorLayerJoinInfo* info = vl->joinBuffer()
                ? vl->joinBuffer()->joinForFieldIndex(fieldIdx, vl->fields(), srcFieldIndex)
                : nullptr;
            if (info && info->isEditable() && info->joinLayer()) {
                readOnly = info->joinLayer()->editFormConfig().readOnly(srcFieldIndex);
            }
        } else {
            readOnly = vl->editFormConfig().readOnly(fieldIdx);
        }
        eww->setEnabled(!readOnly);

        applyAttributeCellEditorStyle(w);
        return w;
    }
};

const char* kPhase2AttrTableStyle =
    "QAbstractItemView#tnAttrTablePhase2 {"
    "  background: #ffffff;"
    "  alternate-background-color: #eef6ff;"
    "  color: #1e293b;"
    "  gridline-color: #e2e8f0;"
    "  selection-background-color: #dbeafe;"
    "  selection-color: #0f172a;"
    "  outline: none;"
    "}"
    "QAbstractItemView#tnAttrTablePhase2::item {"
    "  padding: 6px 8px;"
    "  border: none;"
    "}"
    /* QgsFilterLineEdit 清空自身 QSS 时，父级规则仍可作用于内联编辑器 */
    "QAbstractItemView#tnAttrTablePhase2 QLineEdit {"
    "  background-color: #ffffff;"
    "  color: #111827;"
    "  border: 1px solid #b8c7da;"
    "  border-radius: 4px;"
    "  padding: 0 6px;"
    "  selection-background-color: #dbeafe;"
    "  selection-color: #111827;"
    "}"
    "QAbstractScrollArea#tnAttrTablePhase2::corner {"
    "  background: #e8eef5;"
    "}"
    "QTableView#tnAttrTablePhase2 QTableCornerButton::section {"
    "  background: #e8eef5;"
    "  border: none;"
    "  border-right: 1px solid #d1dae6;"
    "  border-bottom: 1px solid #cbd5e1;"
    "}"
    "QAbstractScrollArea#tnAttrTablePhase2 QScrollBar:vertical {"
    "  background: transparent;"
    "  width: 8px;"
    "  margin: 2px;"
    "}"
    "QAbstractScrollArea#tnAttrTablePhase2 QScrollBar::handle:vertical {"
    "  background: #cbd5e1;"
    "  border-radius: 4px;"
    "  min-height: 24px;"
    "}"
    "QAbstractScrollArea#tnAttrTablePhase2 QScrollBar::add-line:vertical,"
    "QAbstractScrollArea#tnAttrTablePhase2 QScrollBar::sub-line:vertical {"
    "  height: 0;"
    "}"
    "QAbstractScrollArea#tnAttrTablePhase2 QScrollBar::add-page:vertical,"
    "QAbstractScrollArea#tnAttrTablePhase2 QScrollBar::sub-page:vertical {"
    "  background: transparent;"
    "}"
    "QAbstractScrollArea#tnAttrTablePhase2 QScrollBar:horizontal {"
    "  background: transparent;"
    "  height: 8px;"
    "  margin: 2px;"
    "}"
    "QAbstractScrollArea#tnAttrTablePhase2 QScrollBar::handle:horizontal {"
    "  background: #cbd5e1;"
    "  border-radius: 4px;"
    "  min-width: 24px;"
    "}"
    "QAbstractScrollArea#tnAttrTablePhase2 QScrollBar::add-line:horizontal,"
    "QAbstractScrollArea#tnAttrTablePhase2 QScrollBar::sub-line:horizontal {"
    "  width: 0;"
    "}"
    "QAbstractScrollArea#tnAttrTablePhase2 QScrollBar::add-page:horizontal,"
    "QAbstractScrollArea#tnAttrTablePhase2 QScrollBar::sub-page:horizontal {"
    "  background: transparent;"
    "}";

const char* kPhase2AttrVerticalHeaderStyle =
    "QHeaderView {"
    "  background: #e8eef5;"
    "}"
    "QHeaderView::section {"
    "  background: #e8eef5;"
    "  color: #334155;"
    "  font-weight: 600;"
    "  border: none;"
    "  border-right: 1px solid #d1dae6;"
    "  border-bottom: 1px solid #cbd5e1;"
    "  padding: 4px 6px;"
    "}";

} // namespace

TNAttributeWidget::TNAttributeWidget(QWidget* parent)
    : QWidget(parent)
{
    // 设置布局
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // 初始化表格视图
    mTableView = new QgsAttributeTableView(this);
    mTableView->setSortingEnabled(true);
    mTableView->installEventFilter(this);
    mTableView->viewport()->installEventFilter(this);
    applyInlineEditingTriggers();

    // 自定义表头
    mHeaderView = new TNFilterHeaderView(Qt::Horizontal, mTableView);
    mHeaderView->setSectionResizeMode(QHeaderView::Interactive);
    mHeaderView->setStretchLastSection(false);
    mHeaderView->setMinimumSectionSize(kMinColumnWidth);
    mTableView->setHorizontalHeader(mHeaderView);
    connect(mHeaderView, &TNFilterHeaderView::filterChanged,
            this, &TNAttributeWidget::onColumnFilterChanged);

    mainLayout->addWidget(mTableView);

    // 连接选择变化信号
    connect(mTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &TNAttributeWidget::slotSelectionChanged);

    applyDefaultVisualChrome();
}

TNAttributeWidget::~TNAttributeWidget()
{
    ++mLoadGeneration;
    cancelLoadProgress();
    releaseResources();
}

void TNAttributeWidget::setCanvas(QgsMapCanvas* canvas)
{
    mMapCanvas = canvas;
}

void TNAttributeWidget::setCurrentLayer(QgsVectorLayer* layer)
{
    // 同图层且已加载：跳过，避免 showEvent 与 refreshFromCurrentLayer 双重 loadLayer
    if (layer && layer->isValid() && layer == mCurrentLayer && !mTableDirty && mTableModel) {
        return;
    }

    ++mLoadGeneration;
    cancelLoadProgress();

    // 清空过滤状态
    if (mHeaderView) {
        mHeaderView->clearAllFilters();
    }
    mColumnFilters.clear();
    mCurrentFilterExpression.clear();

    if (!layer || !layer->isValid()) {
        mCurrentLayer = nullptr;
        if (mLoading) {
            // 等当前 loadLayer 中止后由 ensureTableLoaded 清理
            mTableDirty = true;
            return;
        }
        releaseResources();
        if (mTableView) {
            mTableView->setModel(nullptr);
        }
        mTableDirty = false;
        return;
    }

    mCurrentLayer = layer;
    mTableDirty = true;

    // 加载过程中的重入：仅更新目标图层，等当前 loadLayer 中止后由 ensureTableLoaded 重试
    if (mLoading) {
        return;
    }

    if (isVisible()) {
        ensureTableLoaded();
        if (mTableModel && !isHidden()) {
            resetColumnWidth(true);
        }
    } else if (mTableView) {
        // 渲染页勾选时不建表，清空旧模型避免残留
        releaseResources();
        mTableView->setModel(nullptr);
        mTableDirty = true;
    }
}

QgsVectorLayer* TNAttributeWidget::currentLayer() const
{
    return mCurrentLayer;
}

void TNAttributeWidget::refreshTable()
{
    if (!mCurrentLayer || !mCurrentLayer->isValid()) {
        return;
    }

    // 保存当前过滤条件
    const QString tempFilter = mCurrentFilterExpression;

    ++mLoadGeneration;
    cancelLoadProgress();
    mTableDirty = true;
    ensureTableLoaded();

    // 恢复过滤条件
    if (!tempFilter.isEmpty()) {
        setFilter(tempFilter);
    }

    if (mTableModel && !isHidden()) {
        resetColumnWidth(true);
    }
}

bool TNAttributeWidget::selectFeatureById(int fid, bool clearExisting)
{
    ensureTableLoaded();
    if (!mFilterModel || !mTableView->selectionModel()) {
        return false;
    }

    // 转换FID到模型索引
    const QModelIndex idx = mFilterModel->fidToIndex(QgsFeatureId(fid));
    if (!idx.isValid()) {
        return false;
    }

    // 执行选择
    auto selectionModel = mTableView->selectionModel();
    if (clearExisting) {
        selectionModel->clearSelection();
    }
    selectionModel->select(idx, QItemSelectionModel::Select);

    // 滚动到选中行
    mTableView->scrollTo(idx);
    return true;
}

QList<QgsFeatureId> TNAttributeWidget::selectedFeatureIds() const
{
    QList<QgsFeatureId> ids;
    if (!mFilterModel || !mTableView->selectionModel()) {
        return ids;
    }

    // 获取选中的索引
    const auto selectedIndexes = mTableView->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selectedIndexes) {
        if (idx.isValid()) {
            ids << mFilterModel->rowToId(idx);
        }
    }
    return ids;
}

void TNAttributeWidget::setFilteredFeatures(const QgsFeatureIds& ids)
{
    ensureTableLoaded();
    if (!mFilterModel) {
        return;
    }
    // 只显示选中要素
    mFilterModel->setFilteredFeatures(ids);
}

void TNAttributeWidget::setFilter(const QString& filterString)
{
    ensureTableLoaded();
    if (!mTableModel || !mCurrentLayer) {
        return;
    }

    mCurrentFilterExpression = filterString;

    // 创建表达式上下文（包含图层和项目信息）
    QgsExpressionContext context = QgsExpressionContext();
    context.appendScopes(QgsExpressionContextUtils::globalProjectLayerScopes(mCurrentLayer));

    // 解析表达式
    QgsExpression expression(filterString);
    if (expression.hasParserError()) {
        TnUi::showWarning(u8"筛选错误",
            QString(u8"无法解析的条件表达式：%1").arg(expression.parserErrorString()));
        return;
    }

    // 使用QgsAttributeTableModel::setRequest进行底层数据筛选
    // 这样直接作用于源模型，效率更高且逻辑更清晰
    QgsFeatureRequest request;
    request.setFilterExpression(filterString);
    request.setExpressionContext(context);

    mTableModel->setRequest(request);
    runLoadLayerWithProgress();
    if (mTableDirty) {
        ensureTableLoaded();
    }

    // 过滤后滚动到顶部
    if (mTableView) {
        mTableView->scrollToTop();
    }
}

void TNAttributeWidget::clearFilter()
{
    if (mHeaderView) {
        mHeaderView->clearAllFilters();
    }
    mColumnFilters.clear();

    if (mTableModel) {
        mCurrentFilterExpression.clear();

        // 重置为默认请求（无过滤）
        QgsFeatureRequest request;
        mTableModel->setRequest(request);
        runLoadLayerWithProgress();
        if (mTableDirty) {
            ensureTableLoaded();
        }
    }
}

void TNAttributeWidget::sortByColumn(int column, Qt::SortOrder order)
{
    mTableView->sortByColumn(column, order);
}

bool TNAttributeWidget::exportToCsv(const QString& filePath)
{
    if (!mCurrentLayer || !mCurrentLayer->isValid()) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        TnUi::showCritical(u8"导出失败",
            QString(u8"无法打开文件（%1）进行写入").arg(file.errorString()));
        return false;
    }

    QTextStream out(&file);
    // 写入表头
    const QgsFields& fields = mCurrentLayer->fields();
    for (int i = 0; i < fields.count(); ++i) {
        if (i > 0) out << ",";
        out << "\"" << fields.at(i).name() << "\"";
    }
    out << "\n";

    // 获取过滤后的要素（使用当前过滤条件）
    QgsFeatureRequest request;
    if (!mCurrentFilterExpression.isEmpty()) {
        QgsExpression expression(mCurrentFilterExpression);
        QgsExpressionContext context = QgsExpressionContext();
        context.appendScopes(QgsExpressionContextUtils::globalProjectLayerScopes(mCurrentLayer));
        request.setFilterExpression(expression);
        request.setExpressionContext(context);
    }
    request.setSubsetOfAttributes(fields.allAttributesList());

    // 写入要素属性
    QgsFeatureIterator it = mCurrentLayer->getFeatures(request);
    QgsFeature feature;
    while (it.nextFeature(feature)) {
        const QgsAttributes& attrs = feature.attributes();
        for (int i = 0; i < attrs.count(); ++i) {
            if (i > 0) out << ",";
            // 处理包含逗号的字符串
            out << "\"" << attrs.at(i).toString().replace("\"", "\"\"") << "\"";
        }
        out << "\n";
    }

    file.close();
    return true;
}

void TNAttributeWidget::clearTable()
{
    ++mLoadGeneration;
    cancelLoadProgress();
    if (!mLoading) {
        releaseResources();
        if (mTableView) {
            mTableView->setModel(nullptr);
        }
        mTableDirty = false;
    } else {
        mTableDirty = true;
        mCurrentLayer = nullptr;
    }
}

void TNAttributeWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);

    ensureTableLoaded();
    if (!mInited && mTableModel && mTableView) {
        mInited = true;
        resetColumnWidth(true);
    }
}

void TNAttributeWidget::resizeEvent(QResizeEvent* e)
{
    // Content 模式保持首次定宽，仅 Fill 模式随窗口均分铺满
    if (!isHidden() && m_columnWidthMode == ColumnWidthMode::Fill) {
        resetColumnWidth(false);
    }

    return QWidget::resizeEvent(e);
}

bool TNAttributeWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (!m_inlineEditingEnabled || !mTableView || !event) {
        return QWidget::eventFilter(watched, event);
    }

    const bool isTableTarget = (watched == mTableView || watched == mTableView->viewport());
    if (!isTableTarget) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        if (!ensureLayerEditableForInlineEdit()) {
            return true;
        }
    } else if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_F2 || keyEvent->key() == Qt::Key_Return
            || keyEvent->key() == Qt::Key_Enter) {
            if (!ensureLayerEditableForInlineEdit()) {
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TNAttributeWidget::setInlineEditingEnabled(bool enabled)
{
    if (m_inlineEditingEnabled == enabled) {
        return;
    }
    m_inlineEditingEnabled = enabled;
    applyInlineEditingTriggers();
}

void TNAttributeWidget::applyInlineEditingTriggers()
{
    if (!mTableView) {
        return;
    }
    if (m_inlineEditingEnabled) {
        mTableView->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    } else {
        mTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
}

bool TNAttributeWidget::ensureLayerEditableForInlineEdit()
{
    if (!mCurrentLayer || !mCurrentLayer->isValid()) {
        return false;
    }
    if (mCurrentLayer->isEditable()) {
        return true;
    }
    if (!mCurrentLayer->startEditing()) {
        TnUi::showWarning(QString::fromUtf8(u8"编辑失败"),
                             QString::fromUtf8(u8"无法进入图层编辑模式，请检查图层是否可写。"));
        return false;
    }
    return true;
}

int TNAttributeWidget::tableContentWidth() const
{
    if (!mTableView) {
        return 0;
    }

    int contentWidth = mTableView->viewport() ? mTableView->viewport()->width() : width();
    if (contentWidth <= 0) {
        contentWidth = width() - 32;
        if (mTableView->verticalHeader() && !mTableView->verticalHeader()->isHidden()) {
            contentWidth -= mTableView->verticalHeader()->width();
        }
    }
    return qMax(0, contentWidth);
}

int TNAttributeWidget::preferredColumnWidth(int column) const
{
    QAbstractItemModel* model = mTableView ? mTableView->model() : nullptr;
    if (!model) {
        model = mTableModel;
    }
    if (!model || column < 0 || column >= model->columnCount()) {
        return kMinColumnWidth;
    }

    const QString headerText = model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
    QFontMetrics metrics(mHeaderView ? mHeaderView->font() : font());
    const int textWidth = metrics.horizontalAdvance(headerText);
    return qMax(kMinColumnWidth, kSearchIconAreaWidth + textWidth + kHeaderTextExtraWidth);
}

void TNAttributeWidget::resetColumnWidth(bool forceFull)
{
    if (!mTableModel || !mTableView) {
        return;
    }

    QAbstractItemModel* model = mTableView->model() ? mTableView->model() : static_cast<QAbstractItemModel*>(mTableModel);
    const int count = model->columnCount();
    if (count <= 0) {
        return;
    }

    const int contentWidth = tableContentWidth();
    if (contentWidth <= 0) {
        return;
    }

    if (forceFull || m_columnWidthMode == ColumnWidthMode::Content) {
        QList<int> preferredWidths;
        preferredWidths.reserve(count);
        int sumPreferred = 0;
        for (int index = 0; index < count; ++index) {
            const int w = preferredColumnWidth(index);
            preferredWidths.append(w);
            sumPreferred += w;
        }

        if (sumPreferred < contentWidth) {
            m_columnWidthMode = ColumnWidthMode::Fill;
        } else {
            m_columnWidthMode = ColumnWidthMode::Content;
            for (int index = 0; index < count; ++index) {
                if (mHeaderView) {
                    mHeaderView->resizeSection(index, preferredWidths.at(index));
                } else {
                    mTableView->setColumnWidth(index, preferredWidths.at(index));
                }
            }
            return;
        }
    }

    // Fill：各列均分可视宽度
    const int perWidth = qMax(kMinColumnWidth, contentWidth / count);
    int assigned = 0;
    for (int index = 0; index < count; ++index) {
        const int w = (index == count - 1) ? qMax(kMinColumnWidth, contentWidth - assigned) : perWidth;
        assigned += w;
        if (mHeaderView) {
            mHeaderView->resizeSection(index, w);
        } else {
            mTableView->setColumnWidth(index, w);
        }
    }
}

void TNAttributeWidget::slotSelectionChanged()
{
    emit featureSelectionChanged(selectedFeatureIds());
}

void TNAttributeWidget::onColumnFilterChanged(int column, const QString& value)
{
    if (value.isEmpty()) {
        mColumnFilters.remove(column);
    } else {
        mColumnFilters[column] = value;
    }

    // 构建过滤表达式
    QStringList conditions;
    for (auto it = mColumnFilters.begin(); it != mColumnFilters.end(); ++it) {
        int col = it.key();
        QString val = it.value().trimmed(); // 去除首尾空格
        
        if (mCurrentLayer && mTableModel) {
              // QgsAttributeTableModel uses fieldIndex() or maps directly. 
              // Assuming 'col' corresponds to the field index if no specific method is found.
              // In many versions, QgsAttributeTableModel::fieldIdx(int col) exists, but if not:
              // We'll assume column matches field index for now as fallback, or use `fieldCol` logic.
              int fieldIdx = mTableModel->fieldCol(col);
              // Check bounds
              if (fieldIdx < 0 || fieldIdx >= mCurrentLayer->fields().count()) continue;

             QgsField field = mCurrentLayer->fields().at(fieldIdx);
             QString fieldName = field.name();
             QString quotedField = QgsExpression::quotedColumnRef(fieldName);
             
             // 转义单引号
             QString escapedVal = val;
             escapedVal.replace("'", "''");
             
             // 检查是否是数值类型，如果是数值类型，需要进行等值匹配（用户要求）
             // Use int cast to compatible with both Qt5 (QVariant::Type) and Qt6 (QMetaType::Type)
             int fieldType = static_cast<int>(field.type());
             if (fieldType == static_cast<int>(QVariant::Int) || 
                 fieldType == static_cast<int>(QVariant::LongLong) || fieldType == static_cast<int>(QVariant::ULongLong)) {
                 // 整型：尝试转换为数字，如果成功则使用数字字面量，否则使用字符串字面量
                 bool ok;
                 qlonglong iVal = val.toLongLong(&ok);
                 if (ok) {
                     conditions << QString("%1 = %2").arg(quotedField).arg(iVal);
                 } else {
                     conditions << QString("%1 = '%2'").arg(quotedField).arg(escapedVal);
                 }
             } else if (fieldType == static_cast<int>(QVariant::Double)) {
                 // 浮点型：尝试解析输入值为double，进行近似匹配
                 bool ok;
                 double dVal = val.toDouble(&ok);
                 if (ok) {
                      // 使用 abs(a-b) < epsilon 进行比较，epsilon取 0.000001
                      conditions << QString("abs(%1 - %2) < 0.000001").arg(quotedField).arg(dVal, 0, 'f', 10);
                 } else {
                      // 无法解析为数字，回退到字符串等值（通常会匹配不到，但符合逻辑）
                      conditions << QString("%1 = '%2'").arg(quotedField).arg(escapedVal);
                 }
             } else {
                 // 字符串或其他：使用ILIKE进行模糊搜索
                 conditions << QString("%1 ILIKE '%%2%'").arg(quotedField).arg(escapedVal);
             }
        }
    }
    
    QString expression = conditions.join(" AND ");
    
    // 如果表达式为空，说明没有筛选条件，清空过滤
    if (expression.isEmpty()) {
         if (mTableModel) {
             mCurrentFilterExpression.clear();
             QgsFeatureRequest request;
             mTableModel->setRequest(request);
             runLoadLayerWithProgress();
             if (mTableDirty) {
                 ensureTableLoaded();
             }
         }
     } else {
         // 调用setFilter会更新mCurrentFilterExpression
        setFilter(expression);
    }
}

void TNAttributeWidget::setPhase2ChromeEnabled(bool enabled)
{
    if (m_phase2Chrome == enabled) {
        return;
    }
    m_phase2Chrome = enabled;
    if (m_phase2Chrome) {
        applyPhase2VisualChrome();
    }
    else {
        applyDefaultVisualChrome();
    }
}

void TNAttributeWidget::applyPhase2VisualChrome()
{
    if (!mTableView || !mHeaderView) {
        return;
    }

    mTableView->setObjectName(QStringLiteral("tnAttrTablePhase2"));
    mTableView->setAlternatingRowColors(true);
    mTableView->setShowGrid(true);
    mHeaderView->setPhase2Chrome(true);

    mTableView->setStyleSheet(QString::fromLatin1(kPhase2AttrTableStyle));

    if (mTableView->verticalHeader()) {
        mTableView->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
        mTableView->verticalHeader()->setStyleSheet(QString::fromLatin1(kPhase2AttrVerticalHeaderStyle));
    }

    mTableView->setItemDelegate(new TnPhase2AttributeDelegate(mTableView));
}

void TNAttributeWidget::applyDefaultVisualChrome()
{
    if (!mTableView || !mHeaderView) {
        return;
    }
    mHeaderView->setPhase2Chrome(false);
    mTableView->setObjectName(QString());
    mTableView->setAlternatingRowColors(false);
    mTableView->setStyleSheet(QString());
    if (mTableView->verticalHeader()) {
        mTableView->verticalHeader()->setStyleSheet(QString());
    }
    mTableView->setItemDelegate(nullptr);
}

void TNAttributeWidget::releaseResources()
{
    destroyLoadProgress();

    if (mTableView) {
        mTableView->setItemDelegate(nullptr);
    }
    // 释放QGIS组件资源
    if (mFilterModel) {
        delete mFilterModel;
        mFilterModel = nullptr;
    }
    if (mTableModel) {
        disconnect(mTableModel, nullptr, this, nullptr);
        delete mTableModel;
        mTableModel = nullptr;
    }
    if (mLayerCache) {
        delete mLayerCache;
        mLayerCache = nullptr;
    }
}

void TNAttributeWidget::cancelLoadProgress()
{
    if (mProgressDlg) {
        mProgressDlg->cancel();
    }
}

void TNAttributeWidget::destroyLoadProgress()
{
    if (!mProgressDlg) {
        return;
    }
    mProgressDlg->hide();
    delete mProgressDlg;
    mProgressDlg = nullptr;
}

void TNAttributeWidget::onLoadProgress(int i, bool& cancel)
{
    if (mLoadGeneration != mActiveLoadGeneration) {
        cancel = true;
        return;
    }

    if (!mProgressDlg) {
        mProgressDlg = new QProgressDialog(
            QString::fromUtf8(u8"正在加载要素…"),
            QString::fromUtf8(u8"中止"),
            0,
            0,
            this);
        mProgressDlg->setWindowTitle(QString::fromUtf8(u8"属性表"));
        mProgressDlg->setWindowModality(Qt::WindowModal);
        mProgressDlg->setMinimumDuration(0);
        mProgressDlg->show();
    }

    mProgressDlg->setLabelText(QString::fromUtf8(u8"已加载 %L1 个要素。").arg(i));
    QApplication::processEvents();

    cancel = (mProgressDlg && mProgressDlg->wasCanceled())
        || (mLoadGeneration != mActiveLoadGeneration);
}

void TNAttributeWidget::onLoadFinished()
{
    destroyLoadProgress();
}

void TNAttributeWidget::runLoadLayerWithProgress()
{
    if (!mTableModel) {
        return;
    }

    mActiveLoadGeneration = mLoadGeneration;
    mLoading = true;
    mTableModel->loadLayer();
    mLoading = false;
    destroyLoadProgress();
}

void TNAttributeWidget::ensureTableLoaded()
{
    if (mLoading) {
        return;
    }

    while (mTableDirty) {
        if (!mCurrentLayer || !mCurrentLayer->isValid() || !mMapCanvas) {
            releaseResources();
            if (mTableView) {
                mTableView->setModel(nullptr);
            }
            mTableDirty = false;
            return;
        }

        const quint64 genAtStart = mLoadGeneration;
        releaseResources();
        initTableComponents();

        if (mLoadGeneration != genAtStart) {
            // 加载过程中切换了图层，丢弃本次结果并重试
            releaseResources();
            if (mTableView) {
                mTableView->setModel(nullptr);
            }
            mTableDirty = true;
            continue;
        }

        mTableDirty = false;
        break;
    }
}

void TNAttributeWidget::initTableComponents()
{
    if (!mCurrentLayer || !mMapCanvas) {
        return;
    }

    // 创建图层缓存（-1表示缓存所有要素）
    mLayerCache = new QgsVectorLayerCache(mCurrentLayer, -1, this);
    mLayerCache->setFullCache(true);
    mLayerCache->setCacheGeometry(false); // 不需要缓存几何信息

    // 创建属性表模型
    mTableModel = new TnFloatDisplayAttributeTableModel(mLayerCache, this);
    QgsAttributeEditorContext editorContext;
    editorContext.setMapCanvas(mMapCanvas);
    mTableModel->setEditorContext(editorContext);

    connect(mTableModel, &QgsAttributeTableModel::progress,
            this, &TNAttributeWidget::onLoadProgress);
    connect(mTableModel, &QgsAttributeTableModel::finished,
            this, &TNAttributeWidget::onLoadFinished);

    runLoadLayerWithProgress();

    // 加载中途切层：不挂过滤模型，由 ensureTableLoaded 重试
    if (mLoadGeneration != mActiveLoadGeneration) {
        return;
    }

    // 创建过滤模型（使用正确的构造函数，传入地图画布）
    mFilterModel = new QgsAttributeTableFilterModel(mMapCanvas, mTableModel, this);

    // 设置视图模型
    mTableView->setModel(mFilterModel);
    if (mTableView->selectionModel()) {
        connect(mTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &TNAttributeWidget::slotSelectionChanged, Qt::UniqueConnection);
    }
    applyInlineEditingTriggers();

    if (m_phase2Chrome) {
        applyPhase2VisualChrome();
    }

    mInited = false;
    m_columnWidthMode = ColumnWidthMode::Content;
}
