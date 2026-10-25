#include "casemaptoolbarwidget.h"

#include "../publicctrls/phase2comboboxutil.h"
#include "caseiconutils.h"
#include <QApplication>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QButtonGroup>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QKeySequence>
#include <QLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTextEdit>
#include <QWidget>

extern QStringList g_fontfamilies;

namespace {
constexpr int kFontWorkbenchTitle = 20;
constexpr int kFontSection = 16;
constexpr int kFontBody = 14;
constexpr int kFontCaption = 12;

QString toolbarFontFamily()
{
    if (!g_fontfamilies.isEmpty()) {
        return g_fontfamilies.first();
    }
    if (qApp) {
        return qApp->font().family();
    }
    return QString();
}

QFont toolbarFont(int pixelSize, QFont::Weight weight = QFont::Normal)
{
    QFont font;
    const QString family = toolbarFontFamily();
    if (!family.isEmpty()) {
        font.setFamily(family);
    }
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

void applyToolbarFont(QWidget* widget, int pixelSize, QFont::Weight weight = QFont::Normal)
{
    if (!widget) {
        return;
    }
    widget->setFont(toolbarFont(pixelSize, weight));
}

QString phaseToolButtonStyleSheet()
{
    return QStringLiteral(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 8px;"
        "   background-color: transparent;"
        "}"
        "QPushButton:checked {"
        "   background-color: #073068;"
        "}"
        "QPushButton:hover:!disabled {"
        "   background-color: rgba(34, 137, 255, 0.20);"
        "}"
    );
}

} // namespace

CaseMapToolbarWidget::CaseMapToolbarWidget(HostContext hostContext, QWidget* parent)
    : QWidget(parent)
    , m_hostContext(hostContext)
    , m_toolbarFrame(nullptr)
    , m_viewGroup(new QButtonGroup(this))
    , m_mutexToolGroup(new QButtonGroup(this))
    , m_standardViewButton(nullptr)
    , m_compareViewButton(nullptr)
    , m_compareCombo(nullptr)
    , m_chartButton(nullptr)
    , m_schemeModeWidget(nullptr)
    , m_dividerBeforeWorkbench(nullptr)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("CaseMapToolbarWidget { background: transparent; }"));
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    QFrame* toolbar = new QFrame(this);
    m_toolbarFrame = toolbar;
    toolbar->setObjectName(QStringLiteral("caseMapToolbarShell"));
    toolbar->setAttribute(Qt::WA_StyledBackground, true);
    toolbar->setFrameShape(QFrame::NoFrame);
    toolbar->setFrameShadow(QFrame::Plain);
    toolbar->setLineWidth(0);
    toolbar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    toolbar->setFixedHeight(40);
    toolbar->setStyleSheet(
        "QFrame#caseMapToolbarShell {"
        "   border: none;"
        "   border-radius: 20px;"
        "   background-color: #051733;"
        "}"
    );

    QHBoxLayout* wrapperLayout = new QHBoxLayout(this);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->setSpacing(0);
    wrapperLayout->addWidget(toolbar);

    QHBoxLayout* layout = new QHBoxLayout(toolbar);
    // 40px 工具条内 (40-32)/2 = 4，保证 32px 按钮垂直居中
    layout->setContentsMargins(18, 4, 18, 4);
    layout->setSpacing(10);
    layout->setSizeConstraint(QLayout::SetMinimumSize);

    m_mutexToolGroup->setExclusive(true);

    struct MutexItem
    {
        ToolType type;
        const char* stem;
        const char* tipZh;
    };
    const QList<MutexItem> mutexItems = {
        {ToolType::Browse, "case_tb_pan", u8"浏览"},
        {ToolType::Select, "case_tb_select", u8"要素选择"},
        {ToolType::Edit, "case_tb_edit", u8"要素编辑"},
        {ToolType::AddFeature, "case_tb_add_feature", u8"添加要素"},
        {ToolType::Delete, "case_tb_delete", u8"删除要素"},
    };

    for (const MutexItem& item : mutexItems) {
        QPushButton* button =
            createPhaseIconButton(toolbar, CaseIconUtils::mapToolbarDualStateIcon(QString::fromLatin1(item.stem)), true, item.type);
        button->setToolTip(item.tipZh);
        m_mutexToolGroup->addButton(button);
        m_primaryToolButtons.append(button);
        layout->addWidget(button);
    }

    m_undoEditsButton = createPhaseIconButton(
        toolbar, CaseIconUtils::mapToolbarSingleStateIcon(QStringLiteral("revoke")), false, ToolType::UndoEdits);
    m_undoEditsButton->setToolTip(QString::fromUtf8(u8"撤回修改"));
    m_undoEditsButton->setEnabled(false);
    m_primaryToolButtons.append(m_undoEditsButton);
    layout->addWidget(m_undoEditsButton);

    m_saveEditsButton = createPhaseIconButton(
        toolbar, CaseIconUtils::mapToolbarSingleStateIcon(QStringLiteral("case_tb_save")), false, ToolType::SaveEdits);
    m_saveEditsButton->setToolTip(QString::fromUtf8(u8"保存编辑"));
    m_saveEditsButton->setEnabled(false);
    m_primaryToolButtons.append(m_saveEditsButton);
    layout->addWidget(m_saveEditsButton);

    m_undoShortcut = new QShortcut(QKeySequence::Undo, this);
    m_undoShortcut->setEnabled(false);
    connect(m_undoShortcut, &QShortcut::activated, this, &CaseMapToolbarWidget::onUndoShortcutActivated);
    connect(qApp, &QApplication::focusChanged, this, &CaseMapToolbarWidget::onApplicationFocusChanged);

    layout->addWidget(createDivider(toolbar));

    struct MomentaryItem
    {
        ToolType type;
        const char* stem;
        const char* tipZh;
    };
    const QList<MomentaryItem> momentaryItems = {
        //{ToolType::Clip, "case_tb_clip", u8"裁剪"}, //暂时不要了
        {ToolType::Symbol, "case_tb_symbol", u8"符号化"},
        {ToolType::ClearSymbol, "case_tb_clear", u8"清除现有符号化"},
        {ToolType::Desireline, "case_tb_desireline", u8"生成期望线"},
        {ToolType::Flow, "case_tb_flow", u8"道路溯源"},
    };

    for (const MomentaryItem& item : momentaryItems) {
        QPushButton* button =
            createPhaseIconButton(toolbar, CaseIconUtils::mapToolbarSingleStateIcon(QString::fromLatin1(item.stem)), false, item.type);
        button->setToolTip(item.tipZh);
        m_primaryToolButtons.append(button);
        if (isGisAsyncToolType(item.type)) {
            m_gisAsyncToolButtons.append(button);
        }
        layout->addWidget(button);
    }

    if (m_hostContext == HostContext::SchemeDetail) {
        QPushButton* attrFilterButton = createPhaseIconButton(
            toolbar, CaseIconUtils::mapToolbarSingleStateIcon(QStringLiteral("case_tb_attr_filter")), false,
            ToolType::AttributeFilter);
        attrFilterButton->setToolTip(QString::fromUtf8(u8"按属性过滤"));
        m_primaryToolButtons.append(attrFilterButton);
        layout->addWidget(attrFilterButton);
    }

    {
        QPushButton* exportMapButton = createPhaseIconButton(
            toolbar, CaseIconUtils::mapToolbarSingleStateIcon(QStringLiteral("case_tb_export_map")), false,
            ToolType::ExportMap);
        exportMapButton->setToolTip(QString::fromUtf8(u8"保存画布"));
        m_primaryToolButtons.append(exportMapButton);
        layout->addWidget(exportMapButton);
    }

    {
        m_refreshDataButton = createPhaseIconButton(
            toolbar, CaseIconUtils::mapToolbarSingleStateIcon(QStringLiteral("refreshdata")), false,
            ToolType::RefreshData);
        m_refreshDataButton->setToolTip(QString::fromUtf8(u8"刷新数据"));
        m_primaryToolButtons.append(m_refreshDataButton);
        layout->addWidget(m_refreshDataButton);
    }

    if (m_hostContext == HostContext::SchemeDetail) {
        layout->addWidget(createDivider(toolbar));
        m_schemeModeWidget = new QWidget(toolbar);
        m_schemeModeWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        QHBoxLayout* schemeLayout = new QHBoxLayout(m_schemeModeWidget);
        schemeLayout->setContentsMargins(0, 0, 0, 0);
        schemeLayout->setSpacing(10);
        schemeLayout->setSizeConstraint(QLayout::SetMinimumSize);
        schemeLayout->addSpacing(12);

        m_viewGroup->setExclusive(true);
        m_standardViewButton = createSegmentButton(QString::fromUtf8(u8"标准视图"), true, m_schemeModeWidget);
        m_compareViewButton = createSegmentButton(QString::fromUtf8(u8"双屏对比"), false, m_schemeModeWidget);
        m_standardViewButton->setToolTip(u8"标准视图");
        m_compareViewButton->setToolTip(u8"双屏对比");
        m_standardViewButton->setProperty("viewMode", static_cast<int>(ViewMode::Standard));
        m_compareViewButton->setProperty("viewMode", static_cast<int>(ViewMode::Compare));
        connect(m_standardViewButton, &QPushButton::clicked, this, &CaseMapToolbarWidget::onViewButtonClicked);
        connect(m_compareViewButton, &QPushButton::clicked, this, &CaseMapToolbarWidget::onViewButtonClicked);
        m_viewGroup->addButton(m_standardViewButton);
        m_viewGroup->addButton(m_compareViewButton);
        schemeLayout->addWidget(m_standardViewButton);
        schemeLayout->addWidget(m_compareViewButton);

        m_compareCombo = new QComboBox(m_schemeModeWidget);
        m_compareCombo->setCursor(Qt::PointingHandCursor);
        m_compareCombo->setToolTip(u8"选择对比方案");
        m_compareCombo->setMinimumWidth(200);
        m_compareCombo->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_compareCombo->setVisible(false);
        Phase2Ui::setupThemedCombo(m_compareCombo, Phase2Ui::InputFieldTheme::DarkToolbar);
        // DarkToolbar 默认 40px 高，工具条 40px 内收窄为 32px（追加规则覆盖同名属性）
        const QString comboHeightOverride =
            QStringLiteral("QComboBox { min-height: 32px; max-height: 32px; border-radius: 8px; }");
        m_compareCombo->setStyleSheet(m_compareCombo->styleSheet() + comboHeightOverride);
        connect(m_compareCombo, &QComboBox::currentTextChanged, this, &CaseMapToolbarWidget::compareSelectionChanged);
        schemeLayout->addWidget(m_compareCombo);

        layout->addWidget(m_schemeModeWidget);

        m_dividerBeforeWorkbench = createDivider(toolbar);
        layout->addWidget(m_dividerBeforeWorkbench);

        m_chartButton = new QPushButton(toolbar);
        m_chartButton->setCursor(Qt::PointingHandCursor);
        m_chartButton->setCheckable(true);
        m_chartButton->setChecked(true);
        m_chartButton->setFixedSize(32, 32);
        m_chartButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_chartButton->setIcon(CaseIconUtils::mapToolbarSingleStateIcon(QStringLiteral("case_tb_workbench")));
        m_chartButton->setIconSize(QSize(24, 24));
        m_chartButton->setToolTip(u8"业务工作台");
        m_chartButton->setStyleSheet(phaseToolButtonStyleSheet());
        connect(m_chartButton, &QPushButton::toggled, this, &CaseMapToolbarWidget::onWorkbenchButtonToggled);
        m_primaryToolButtons.append(m_chartButton);
        layout->addWidget(m_chartButton);
    }

    const QList<QAbstractButton*> mutexButtons = m_mutexToolGroup->buttons();
    if (!mutexButtons.isEmpty()) {
        mutexButtons.first()->setChecked(true);
    }
}

CaseMapToolbarWidget::~CaseMapToolbarWidget()
{
}

QPushButton* CaseMapToolbarWidget::createPhaseIconButton(QFrame* toolbar, const QIcon& icon, bool checkable, ToolType type)
{
    QPushButton* button = new QPushButton(toolbar);
    button->setCursor(Qt::PointingHandCursor);
    button->setCheckable(checkable);
    button->setFixedSize(32, 32);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setIcon(icon);
    button->setIconSize(QSize(24, 24));
    button->setProperty("toolType", static_cast<int>(type));
    button->setStyleSheet(phaseToolButtonStyleSheet());
    connect(button, &QPushButton::clicked, this, &CaseMapToolbarWidget::onPhaseOneToolClicked);
    return button;
}

void CaseMapToolbarWidget::setViewMode(ViewMode mode)
{
    if (!m_standardViewButton || !m_compareViewButton) {
        return;
    }

    const bool blockStd = m_standardViewButton->blockSignals(true);
    const bool blockCmp = m_compareViewButton->blockSignals(true);

    if (mode == ViewMode::Compare) {
        m_standardViewButton->setChecked(false);
        m_compareViewButton->setChecked(true);
    } else {
        m_standardViewButton->setChecked(true);
        m_compareViewButton->setChecked(false);
    }

    m_standardViewButton->blockSignals(blockStd);
    m_compareViewButton->blockSignals(blockCmp);
}

CaseMapToolbarWidget::ViewMode CaseMapToolbarWidget::viewMode() const
{
    if (m_compareViewButton && m_compareViewButton->isChecked()) {
        return ViewMode::Compare;
    }

    return ViewMode::Standard;
}

void CaseMapToolbarWidget::setToolButtonsEnabled(bool enabled)
{
    m_toolsGloballyEnabled = enabled;
    for (QPushButton* button : m_primaryToolButtons) {
        if (!button || button == m_saveEditsButton || button == m_undoEditsButton) {
            continue;
        }
        button->setEnabled(enabled);
    }
    applySaveUndoButtonState();
    applyGisAsyncToolButtonState();
    applyRefreshDataButtonState();
}

bool CaseMapToolbarWidget::isGisAsyncToolType(ToolType toolType)
{
    return toolType == ToolType::Desireline || toolType == ToolType::Flow;
}

void CaseMapToolbarWidget::applyGisAsyncToolButtonState()
{
    const bool enabled = m_toolsGloballyEnabled && !m_gisAsyncToolsBusy;
    for (QPushButton* button : m_gisAsyncToolButtons) {
        if (button) {
            button->setEnabled(enabled);
        }
    }
}

void CaseMapToolbarWidget::setGisAsyncToolsBusy(bool busy)
{
    if (m_gisAsyncToolsBusy == busy) {
        return;
    }
    m_gisAsyncToolsBusy = busy;
    applyGisAsyncToolButtonState();
}

void CaseMapToolbarWidget::applyRefreshDataButtonState()
{
    if (!m_refreshDataButton) {
        return;
    }
    m_refreshDataButton->setEnabled(m_toolsGloballyEnabled && !m_refreshDataBusy);
}

void CaseMapToolbarWidget::setRefreshDataBusy(bool busy)
{
    if (m_refreshDataBusy == busy) {
        return;
    }
    m_refreshDataBusy = busy;
    applyRefreshDataButtonState();
}

void CaseMapToolbarWidget::setWorkbenchChecked(bool checked)
{
    if (!m_chartButton) {
        return;
    }
    const bool blocked = m_chartButton->blockSignals(true);
    m_chartButton->setChecked(checked);
    m_chartButton->blockSignals(blocked);
}

void CaseMapToolbarWidget::setMutexTool(ToolType toolType)
{
    if (!m_mutexToolGroup) {
        return;
    }

    for (QAbstractButton* button : m_mutexToolGroup->buttons()) {
        if (!button) {
            continue;
        }
        const bool block = button->blockSignals(true);
        const bool match = button->property("toolType").toInt() == static_cast<int>(toolType);
        button->setChecked(match);
        button->blockSignals(block);
    }
}

void CaseMapToolbarWidget::applySaveUndoButtonState()
{
    if (m_saveEditsButton) {
        m_saveEditsButton->setEnabled(m_toolsGloballyEnabled && m_saveEditsWanted);
    }
    if (m_undoEditsButton) {
        m_undoEditsButton->setEnabled(m_toolsGloballyEnabled && m_undoEditsWanted);
    }
    syncUndoShortcutEnabled();
}

void CaseMapToolbarWidget::setSaveEditsEnabled(bool enabled)
{
    m_saveEditsWanted = enabled;
    applySaveUndoButtonState();
}

void CaseMapToolbarWidget::setUndoEditsEnabled(bool enabled)
{
    m_undoEditsWanted = enabled;
    applySaveUndoButtonState();
}

void CaseMapToolbarWidget::setUndoEditsToolTip(const QString& text)
{
    if (m_undoEditsButton) {
        m_undoEditsButton->setToolTip(text);
    }
}

bool CaseMapToolbarWidget::isTextInputFocusWidget(QWidget* widget)
{
    if (!widget) {
        return false;
    }
    return qobject_cast<QLineEdit*>(widget)
        || qobject_cast<QTextEdit*>(widget)
        || qobject_cast<QPlainTextEdit*>(widget)
        || qobject_cast<QAbstractSpinBox*>(widget);
}

void CaseMapToolbarWidget::syncUndoShortcutEnabled()
{
    if (!m_undoShortcut) {
        return;
    }
    const bool allow = isVisible()
        && m_undoEditsButton && m_undoEditsButton->isEnabled()
        && !isTextInputFocusWidget(QApplication::focusWidget());
    m_undoShortcut->setEnabled(allow);
}

void CaseMapToolbarWidget::onApplicationFocusChanged(QWidget*, QWidget*)
{
    syncUndoShortcutEnabled();
}

void CaseMapToolbarWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    syncUndoShortcutEnabled();
}

void CaseMapToolbarWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    syncUndoShortcutEnabled();
}

void CaseMapToolbarWidget::onUndoShortcutActivated()
{
    if (!isVisible() || !m_undoEditsButton || !m_undoEditsButton->isEnabled()) {
        return;
    }
    if (isTextInputFocusWidget(QApplication::focusWidget())) {
        return;
    }
    emit toolTriggered(ToolType::UndoEdits);
}

void CaseMapToolbarWidget::onWorkbenchButtonToggled(bool checked)
{
    emit workbenchToggled(checked);
}

void CaseMapToolbarWidget::setCompareSelectorVisible(bool visible)
{
    if (!m_compareCombo) {
        return;
    }
    m_compareCombo->setVisible(visible);
    if (visible) {
        m_compareCombo->setMinimumWidth(200);
    } else {
        m_compareCombo->setMinimumWidth(0);
        m_compareCombo->setMaximumWidth(QWIDGETSIZE_MAX);
    }
    shrinkWrapToolbar();
}

void CaseMapToolbarWidget::setCompareOptions(const QStringList& options)
{
    if (!m_compareCombo) {
        return;
    }

    m_compareCombo->blockSignals(true);
    m_compareCombo->clear();
    m_compareCombo->addItems(options);
    m_compareCombo->blockSignals(false);
    shrinkWrapToolbar();
}

void CaseMapToolbarWidget::setCurrentCompareText(const QString& text)
{
    if (!m_compareCombo) {
        return;
    }

    const int index = m_compareCombo->findText(text);
    if (index >= 0) {
        m_compareCombo->setCurrentIndex(index);
    } else if (!text.isEmpty()) {
        m_compareCombo->addItem(text);
        m_compareCombo->setCurrentText(text);
    }
    shrinkWrapToolbar();
}

QString CaseMapToolbarWidget::currentCompareText() const
{
    return m_compareCombo ? m_compareCombo->currentText() : QString();
}

void CaseMapToolbarWidget::setSchemeModeSwitcherVisible(bool visible)
{
    if (m_schemeModeWidget) {
        m_schemeModeWidget->setVisible(visible);
    }
    if (m_dividerBeforeWorkbench) {
        m_dividerBeforeWorkbench->setVisible(visible);
    }
    shrinkWrapToolbar();
}

void CaseMapToolbarWidget::relayoutToContent()
{
    shrinkWrapToolbar();
}

void CaseMapToolbarWidget::shrinkWrapToolbar()
{
    if (m_schemeModeWidget) {
        if (QLayout* lay = m_schemeModeWidget->layout()) {
            lay->invalidate();
            lay->activate();
        }
        m_schemeModeWidget->adjustSize();
    }
    if (m_toolbarFrame) {
        if (QLayout* lay = m_toolbarFrame->layout()) {
            lay->invalidate();
            lay->activate();
        }
        m_toolbarFrame->adjustSize();
    }
    setMinimumWidth(0);
    setMaximumWidth(QWIDGETSIZE_MAX);
    adjustSize();
    updateGeometry();
}

void CaseMapToolbarWidget::onViewButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    const ViewMode mode = static_cast<ViewMode>(button->property("viewMode").toInt());
    emit viewModeChanged(mode);
}

void CaseMapToolbarWidget::onPhaseOneToolClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    const ToolType toolType = static_cast<ToolType>(button->property("toolType").toInt());
    if (m_gisAsyncToolsBusy && isGisAsyncToolType(toolType)) {
        return;
    }
    if (toolType == ToolType::RefreshData && m_refreshDataBusy) {
        return;
    }
    emit toolTriggered(toolType);
}

QPushButton* CaseMapToolbarWidget::createSegmentButton(const QString& text, bool checked, QWidget* parent)
{
    QPushButton* button = new QPushButton(text, parent);
    button->setCheckable(true);
    button->setChecked(checked);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(94, 32);
    button->setStyleSheet(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 8px;"
        "   background-color: transparent;"
        "   color: white;"
        "}"
        "QPushButton:checked {"
        "   background-color: #083777;"
        "}"
    );

    applyToolbarFont(button, 16, QFont::Bold);
    return button;
}

QFrame* CaseMapToolbarWidget::createDivider(QWidget* parent) const
{
    QFrame* divider = new QFrame(parent);
    divider->setFrameShape(QFrame::NoFrame);
    divider->setAttribute(Qt::WA_StyledBackground, true);
    divider->setFixedSize(1, 20);
    divider->setStyleSheet(QStringLiteral("QFrame { background-color: rgba(255,255,255,0.14); border: none; }"));
    return divider;
}
