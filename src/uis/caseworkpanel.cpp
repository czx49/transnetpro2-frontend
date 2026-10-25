/*****************************************************************//**
 * \file   caseworkpanel.cpp
 * \brief  方案详情业务工作台壳层实现
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "caseworkpanel.h"

#include "../casectrls/caseiconutils.h"
#include "aiconfigmodel.h"
#include "caseworksessionpage.h"
#include "messagedialog.h"

#include <QApplication>
#include <QButtonGroup>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

extern QStringList g_fontfamilies;

namespace {
constexpr int kFontWorkbenchTitle = 20;
constexpr int kFontSection = 16;
constexpr int kFontBody = 14;
constexpr char kWorkbenchBg[] = "#030914";
constexpr char kCardBg[] = "#061a35";
constexpr char kModuleCheckedBg[] = "#3583f7";
constexpr int kModuleIconBox = 40;
constexpr int kToggleButtonWidth = 40;
constexpr int kToggleButtonHeight = 56;

QString workbenchFontFamily()
{
    if (!g_fontfamilies.isEmpty()) {
        return g_fontfamilies.first();
    }
    if (qApp) {
        return qApp->font().family();
    }
    return QString();
}

QFont workbenchFont(int pixelSize, QFont::Weight weight = QFont::Normal)
{
    QFont font;
    const QString family = workbenchFontFamily();
    if (!family.isEmpty()) {
        font.setFamily(family);
    }
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

void applyWorkbenchFont(QWidget* widget, int pixelSize, QFont::Weight weight = QFont::Normal)
{
    if (!widget) {
        return;
    }
    widget->setFont(workbenchFont(pixelSize, weight));
}

QFrame* createCard(QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("workbenchCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(QStringLiteral(
        "QFrame#workbenchCard {"
        "   border-radius: 8px;"
        "   border: 1px solid rgba(41, 101, 180, 0.36);"
        "   background-color: %1;"
        "}").arg(QLatin1String(kCardBg)));
    return card;
}

QLabel* createSectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("QLabel { color: white; }"));
    applyWorkbenchFont(label, kFontSection);
    return label;
}

QPixmap loadWorkbenchModulePixmap(const QString& iconStem, bool checked)
{
    const QString checkedPath = CaseIconUtils::workbenchIconPath(iconStem + QStringLiteral("_checked"));
    const QString normalPath = CaseIconUtils::workbenchIconPath(iconStem);
    QPixmap pixmap(checked ? checkedPath : normalPath);
    if (pixmap.isNull()) {
        pixmap = QPixmap(normalPath);
    }
    return pixmap;
}

int aiConfigModuleId(CaseWorkPanel::AnalysisModule module)
{
    switch (module) {
    case CaseWorkPanel::AnalysisModule::SlowTraffic:
        return AiConfigModel::SlowTrafficModuleId;
    case CaseWorkPanel::AnalysisModule::PublicTransport:
        return AiConfigModel::PublicTransportModuleId;
    case CaseWorkPanel::AnalysisModule::MotorVehicle:
    default:
        return AiConfigModel::MotorVehicleModuleId;
    }
}
} // namespace

CaseWorkPanel::CaseWorkPanel(QWidget* parent)
    : QWidget(parent)
    , mAnim(nullptr)
    , mCurrentWidth(388)
    , mExpandWidth(388)
    , mCollapseWidth(0)
    , mIsCollapsed(false)
    , mCurrentModule(AnalysisModule::MotorVehicle)
    , mContentWidget(nullptr)
    , mTitleLabel(nullptr)
    , mToggleButton(nullptr)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setWindowFlags(Qt::Widget);
    setFixedWidth(mCurrentWidth);

    m_aiConfig = QSharedPointer<AiConfigModel>::create();

    if (parent) {
        parent->installEventFilter(this);
        if (parent->height() > 0) {
            resize(mExpandWidth, parent->height());
        }
    }

    mAnim = new QPropertyAnimation(this, "currentWidth", this);
    mAnim->setDuration(300);
    mAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(mAnim, &QPropertyAnimation::valueChanged, this, [this]() {
        scheduleToggleGeometryUpdate();
    });
    connect(mAnim, &QPropertyAnimation::finished, this, [this]() {
        updateToggleGeometry();
    });

    initUi();
    updatePosition();
}

CaseWorkPanel::~CaseWorkPanel()
{
    prepareForShutdown();
}

void CaseWorkPanel::prepareForShutdown()
{
    const QList<CaseWorkSessionPage*> sessions = mSessions.values();
    mSessions.clear();
    for (CaseWorkSessionPage* session : sessions) {
        if (!session) {
            continue;
        }
        session->prepareForShutdown();
        session->detachSharedCaches();
        if (mSessionStack) {
            mSessionStack->removeWidget(session);
        }
        session->deleteLater();
    }
    mChoosers.clear();
    mPendingResultViews.clear();
    mLastModuleByCaseKey.clear();
    mActiveAiSelectSessionKey.clear();
    mModuleSelected = false;

    if (mToggleButton) {
        mToggleButton->hide();
        disconnect(mToggleButton, nullptr, this, nullptr);
        QPushButton* btn = mToggleButton.data();
        mToggleButton.clear();
        if (btn) {
            btn->setParent(nullptr);
            btn->deleteLater();
        }
    }
}

void CaseWorkPanel::clearSessionsForCase(int caseId)
{
    if (caseId <= 0) {
        return;
    }

    const bool clearingBoundCase = (mCurrentCase.id == caseId);

    QStringList keysToRemove;
    for (auto it = mSessions.constBegin(); it != mSessions.constEnd(); ++it) {
        const QStringList parts = it.key().split(QLatin1Char('|'));
        if (parts.size() >= 1 && parts.at(0).toInt() == caseId) {
            keysToRemove.append(it.key());
        }
    }
    for (const QString& key : keysToRemove) {
        if (CaseWorkSessionPage* session = mSessions.take(key)) {
            session->prepareForShutdown();
            session->detachSharedCaches();
            mSessionStack->removeWidget(session);
            session->deleteLater();
        }
    }

    QStringList chooserKeysToRemove;
    for (auto it = mChoosers.constBegin(); it != mChoosers.constEnd(); ++it) {
        const QStringList parts = it.key().split(QLatin1Char('|'));
        if (parts.size() >= 1 && parts.at(0).toInt() == caseId) {
            chooserKeysToRemove.append(it.key());
        }
    }
    for (const QString& key : chooserKeysToRemove) {
        if (QWidget* chooser = mChoosers.take(key)) {
            mSessionStack->removeWidget(chooser);
            chooser->deleteLater();
        }
    }

    mPendingResultViews.erase(
        std::remove_if(mPendingResultViews.begin(),
                       mPendingResultViews.end(),
                       [caseId](const PendingResultView& item) { return item.caseId == caseId; }),
        mPendingResultViews.end());

    mLastModuleByCaseKey.remove(caseKey(caseId, false));
    mLastModuleByCaseKey.remove(caseKey(caseId, true));

    if (mActiveAiSelectSessionKey.startsWith(QString::number(caseId) + QLatin1Char('|'))) {
        mActiveAiSelectSessionKey.clear();
    }

    // 删除当前绑定方案时，避免 QStackedWidget 自动露出其它方案的缓存页
    if (clearingBoundCase) {
        mModuleSelected = false;
        if (mSessionStack) {
            auto* placeholder = new QWidget(mSessionStack);
            placeholder->setObjectName(QStringLiteral("caseWorkSessionPlaceholder"));
            mSessionStack->addWidget(placeholder);
            mSessionStack->setCurrentWidget(placeholder);
        }
    }
}

int CaseWorkPanel::currentWidth() const
{
    return mCurrentWidth;
}

void CaseWorkPanel::setCurrentWidth(int width)
{
    mCurrentWidth = width;
    setFixedWidth(width);
    updatePosition();
    scheduleToggleGeometryUpdate();
}

QString CaseWorkPanel::caseKey(int caseId, bool isBase) const
{
    return QStringLiteral("%1|%2").arg(caseId).arg(isBase ? 1 : 0);
}

QString CaseWorkPanel::sessionKey(int caseId, bool isBase, AnalysisModule module) const
{
    return QStringLiteral("%1|%2|%3").arg(caseId).arg(isBase ? 1 : 0).arg(static_cast<int>(module));
}

QString CaseWorkPanel::chooserKey(int caseId, bool isBase) const
{
    return QStringLiteral("%1|%2|chooser").arg(caseId).arg(isBase ? 1 : 0);
}

void CaseWorkPanel::setCurrentCase(const CaseItem& caseItem)
{
    const AnalysisModule previousModule = mCurrentModule;
    const bool wasModuleSelected = mModuleSelected;
    mCurrentCase = caseItem;
    refreshHeader();

    const QString ck = caseKey(caseItem.id, caseItem.isBase == 1);
    if (mLastModuleByCaseKey.contains(ck)) {
        mModuleSelected = true;
        mCurrentModule = mLastModuleByCaseKey.value(ck);
        CaseWorkSessionPage* session = ensureSession(mCurrentModule);
        session->setCaseInfo(caseItem);
        showSession(session);
    } else {
        mModuleSelected = false;
        showChooser();
    }

    if (mModuleSelected && (!wasModuleSelected || previousModule != mCurrentModule)) {
        emit analysisModuleChanged(mCurrentModule);
    }

    flushPendingAnalysisResultViewed();
}

void CaseWorkPanel::setAnalysisModule(AnalysisModule module)
{
    if (mCurrentCase.id <= 0) {
        return;
    }

    const bool wasModuleSelected = mModuleSelected;
    const AnalysisModule previousModule = mCurrentModule;
    mCurrentModule = module;
    mModuleSelected = true;

    const QString ck = caseKey(mCurrentCase.id, mCurrentCase.isBase == 1);
    mLastModuleByCaseKey.insert(ck, module);

    CaseWorkSessionPage* session = ensureSession(module);
    session->setCaseInfo(mCurrentCase);
    showSession(session);

    if (!wasModuleSelected || previousModule != module) {
        emit analysisModuleChanged(module);
    }
}

void CaseWorkPanel::resetWorkflow()
{
    if (CaseWorkSessionPage* session = currentSession()) {
        session->resetWorkflow();
    }
}

bool CaseWorkPanel::isExpanded() const
{
    return !mIsCollapsed;
}

void CaseWorkPanel::setExpanded(bool expand)
{
    if (expand == !mIsCollapsed) {
        return;
    }
    togglePanel();
}

bool CaseWorkPanel::isAnalysisModuleSelected() const
{
    return mModuleSelected;
}

void CaseWorkPanel::startTrafficMicroDiagnosis(const QString& ids)
{
    if (!mModuleSelected || !currentSession()) {
        TnUi::showWarning(QString::fromUtf8(u8"中微观诊断"),
                          QString::fromUtf8(u8"请先在工作台选择交通方式。"));
        return;
    }
    setExpanded(true);
    if (CaseWorkSessionPage* session = currentSession()) {
        session->startTrafficMicroDiagnosis(ids);
    }
}

void CaseWorkPanel::setAiSelectedLinkIds(const QString& stepKey, const QStringList& linkIds)
{
    CaseWorkSessionPage* session = nullptr;
    if (!mActiveAiSelectSessionKey.isEmpty()) {
        session = mSessions.value(mActiveAiSelectSessionKey);
    }
    if (!session) {
        session = currentSession();
    }
    if (session) {
        session->setAiSelectedLinkIds(stepKey, linkIds);
    }
}

void CaseWorkPanel::setAiDataSelectChecked(const QString& stepKey, bool checked)
{
    CaseWorkSessionPage* session = nullptr;
    if (!mActiveAiSelectSessionKey.isEmpty()) {
        session = mSessions.value(mActiveAiSelectSessionKey);
    }
    if (!session) {
        session = currentSession();
    }
    if (session) {
        session->setAiDataSelectChecked(stepKey, checked);
    }
}

void CaseWorkPanel::finishAiDataSelect(const QString& stepKey, const QStringList& linkIds)
{
    CaseWorkSessionPage* session = nullptr;
    if (!mActiveAiSelectSessionKey.isEmpty()) {
        session = mSessions.value(mActiveAiSelectSessionKey);
    }
    if (!session) {
        session = currentSession();
    }
    if (session) {
        session->finishAiDataSelect(stepKey, linkIds);
    }
    mActiveAiSelectSessionKey.clear();
}

QString CaseWorkPanel::grpcTransportType() const
{
    if (CaseWorkSessionPage* session = currentSession()) {
        return session->grpcTransportType();
    }
    return QStringLiteral("motor");
}

bool CaseWorkPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget()
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updatePosition();
        scheduleToggleGeometryUpdate();
    }
    return QWidget::eventFilter(watched, event);
}

void CaseWorkPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (parentWidget() && parentWidget()->layout() && parentWidget()->layout()->indexOf(this) >= 0) {
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
    }
    updateToggleGeometry();
}

void CaseWorkPanel::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    if (mToggleButton) {
        mToggleButton->hide();
    }
}

void CaseWorkPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    scheduleToggleGeometryUpdate();
}

void CaseWorkPanel::togglePanel()
{
    if (mIsCollapsed) {
        mAnim->setStartValue(mCollapseWidth);
        mAnim->setEndValue(mExpandWidth);
        if (mToggleButton) {
            mToggleButton->setText(QStringLiteral(">>"));
        }
        if (mContentWidget) {
            mContentWidget->show();
        }
    } else {
        mAnim->setStartValue(mExpandWidth);
        mAnim->setEndValue(mCollapseWidth);
        if (mToggleButton) {
            mToggleButton->setText(QStringLiteral("<<"));
        }
    }

    mAnim->start();
    mIsCollapsed = !mIsCollapsed;
    if (mIsCollapsed && mContentWidget) {
        QTimer::singleShot(mAnim->duration(), mContentWidget, &QWidget::hide);
    }
    scheduleToggleGeometryUpdate();
    emit workbenchExpandedChanged(!mIsCollapsed);
}

void CaseWorkPanel::onChooserModuleClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }
    setAnalysisModule(static_cast<AnalysisModule>(button->property("analysisModule").toInt()));
}

void CaseWorkPanel::onSessionModuleSwitchRequested(CaseWorkPanel::AnalysisModule module)
{
    setAnalysisModule(module);
}

void CaseWorkPanel::onSessionAnalysisResultViewed(CaseWorkPanel::AnalysisModule module,
                                                  const QString& resultKey,
                                                  const QString& resultName)
{
    auto* session = qobject_cast<CaseWorkSessionPage*>(sender());
    if (!session) {
        return;
    }

    if (session->caseId() == mCurrentCase.id) {
        emit analysisResultViewed(module, resultKey, resultName);
    } else {
        PendingResultView pending;
        pending.caseId = session->caseId();
        pending.module = module;
        pending.resultKey = resultKey;
        pending.resultName = resultName;
        mPendingResultViews.append(pending);
    }
}

void CaseWorkPanel::flushPendingAnalysisResultViewed()
{
    if (mCurrentCase.id <= 0) {
        return;
    }

    for (int i = mPendingResultViews.size() - 1; i >= 0; --i) {
        const PendingResultView pending = mPendingResultViews.at(i);
        if (pending.caseId != mCurrentCase.id) {
            continue;
        }
        emit analysisResultViewed(pending.module, pending.resultKey, pending.resultName);
        mPendingResultViews.removeAt(i);
    }
}

void CaseWorkPanel::initUi()
{
    setStyleSheet(QStringLiteral(
        "CaseWorkPanel {"
        "   background-color: %1;"
        "}").arg(QLatin1String(kWorkbenchBg)));

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    mToggleButton = new QPushButton(QStringLiteral(">>"), this);
    mToggleButton->setCursor(Qt::PointingHandCursor);
    mToggleButton->setFixedSize(kToggleButtonWidth, kToggleButtonHeight);
    mToggleButton->setAutoFillBackground(true);
    mToggleButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   border: none;"
        "   border-right: 1px solid rgba(255, 255, 255, 0.08);"
        "   border-top-left-radius: 6px;"
        "   border-bottom-left-radius: 6px;"
        "   border-top-right-radius: 0;"
        "   border-bottom-right-radius: 0;"
        "   background-color: #020b18;"
        "   color: rgba(229, 240, 255, 0.92);"
        "   font-weight: 700;"
        "}"
        "QPushButton:hover { background-color: #0d2c58; }"));
    applyWorkbenchFont(mToggleButton, kFontBody, QFont::Bold);
    connect(mToggleButton, &QPushButton::clicked, this, &CaseWorkPanel::togglePanel);
    if (QWidget* host = parentWidget()) {
        mToggleButton->setParent(host);
    }

    mContentWidget = new QWidget(this);
    mContentWidget->setObjectName(QStringLiteral("caseWorkbenchContent"));
    mContentWidget->setAttribute(Qt::WA_StyledBackground, true);
    mContentWidget->setStyleSheet(QStringLiteral(
        "QWidget#caseWorkbenchContent {"
        "   background-color: %1;"
        "   border-left: 1px solid rgba(70, 106, 158, 0.45);"
        "}").arg(QLatin1String(kWorkbenchBg)));
    auto* contentLayout = new QVBoxLayout(mContentWidget);
    contentLayout->setContentsMargins(10, 10, 10, 10);
    contentLayout->setSpacing(10);
    contentLayout->addWidget(createHeader());

    mSessionStack = new QStackedWidget(mContentWidget);
    contentLayout->addWidget(mSessionStack, 1);
    rootLayout->addWidget(mContentWidget, 1);

    updateToggleGeometry();
}

void CaseWorkPanel::scheduleToggleGeometryUpdate()
{
    QTimer::singleShot(0, this, [this]() { updateToggleGeometry(); });
}

void CaseWorkPanel::updateToggleGeometry()
{
    QWidget* host = parentWidget();
    if (!host || !mToggleButton) {
        return;
    }

    if (!isVisibleTo(host)) {
        mToggleButton->hide();
        return;
    }

    const QPoint topLeft = mapTo(host, QPoint(0, 0));
    const int panelH = qMax(height(), 1);
    const int toggleX = topLeft.x() - kToggleButtonWidth;
    const int btnY = topLeft.y() + (panelH - kToggleButtonHeight) / 2;

    mToggleButton->setGeometry(toggleX, btnY, kToggleButtonWidth, kToggleButtonHeight);
    mToggleButton->show();
    mToggleButton->raise();
}

void CaseWorkPanel::updatePosition()
{
    QWidget* pw = parentWidget();
    if (!pw) {
        return;
    }

    if (pw->layout() && pw->layout()->indexOf(this) >= 0) {
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }

    if (pw->height() <= 0 || pw->width() <= 0) {
        return;
    }

    move(pw->width() - width(), 0);
    setFixedHeight(pw->height());
    raise();
}

QWidget* CaseWorkPanel::createHeader()
{
    QWidget* header = new QWidget(this);
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QLabel* iconLabel = new QLabel(header);
    iconLabel->setFixedSize(18, 18);
    iconLabel->setPixmap(CaseIconUtils::mapToolbarSingleStateIcon(QStringLiteral("case_tb_workbench")).pixmap(18, 18));
    iconLabel->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));
    layout->addWidget(iconLabel);

    mTitleLabel = new QLabel(QString::fromUtf8(u8"工作台"), header);
    mTitleLabel->setStyleSheet(QStringLiteral("QLabel { color: white; }"));
    applyWorkbenchFont(mTitleLabel, kFontWorkbenchTitle);
    layout->addWidget(mTitleLabel, 1);
    return header;
}

void CaseWorkPanel::refreshHeader()
{
    if (mTitleLabel) {
        mTitleLabel->setText(QString::fromUtf8(u8"工作台"));
    }
}

QWidget* CaseWorkPanel::createChooserPage()
{
    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 6px; background: transparent; }"
        "QScrollBar::handle:vertical { border-radius: 3px; background: rgba(75,139,220,0.36); }"));

    QWidget* content = new QWidget(scrollArea);
    auto* outerLayout = new QVBoxLayout(content);
    outerLayout->setContentsMargins(0, 0, 4, 0);
    outerLayout->setSpacing(10);

    QFrame* card = createCard(content);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    layout->addWidget(createSectionTitle(QString::fromUtf8(u8"交通方式选择"), card));

    QWidget* row = new QWidget(card);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(10);

    auto makeModule = [this, row](const QString& text, const QString& iconStem, AnalysisModule module) {
        auto* button = new QPushButton(row);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("analysisModule", static_cast<int>(module));
        button->setMinimumHeight(76);
        auto* btnLayout = new QVBoxLayout(button);
        btnLayout->setContentsMargins(6, 8, 6, 8);
        btnLayout->setSpacing(2);
        QLabel* iconLabel = new QLabel(button);
        iconLabel->setObjectName(QStringLiteral("moduleIconLabel"));
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setFixedSize(kModuleIconBox, kModuleIconBox);
        iconLabel->setPixmap(loadWorkbenchModulePixmap(iconStem, false));
        iconLabel->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));
        QLabel* textLabel = new QLabel(text, button);
        textLabel->setAlignment(Qt::AlignCenter);
        textLabel->setStyleSheet(QStringLiteral("QLabel { color: white; background: transparent; }"));
        applyWorkbenchFont(textLabel, kFontBody);
        btnLayout->addWidget(iconLabel, 0, Qt::AlignCenter);
        btnLayout->addWidget(textLabel, 1);
        button->setStyleSheet(QStringLiteral(
            "QPushButton { border: 1px solid transparent; border-radius: 8px; background-color: transparent; }"
            "QPushButton:hover { border-color: rgba(24, 144, 255, 0.72); }"));
        connect(button, &QPushButton::clicked, this, &CaseWorkPanel::onChooserModuleClicked);
        return button;
    };

    rowLayout->addWidget(makeModule(QString::fromUtf8(u8"机动车分析"), QStringLiteral("motor"),
                                    AnalysisModule::MotorVehicle), 1);
    rowLayout->addWidget(makeModule(QString::fromUtf8(u8"慢行交通分析"), QStringLiteral("traffic"),
                                    AnalysisModule::SlowTraffic), 1);
    rowLayout->addWidget(makeModule(QString::fromUtf8(u8"公交运行分析"), QStringLiteral("bus"),
                                    AnalysisModule::PublicTransport), 1);
    layout->addWidget(row);
    outerLayout->addWidget(card);
    outerLayout->addStretch();
    scrollArea->setWidget(content);
    return scrollArea;
}

CaseWorkSessionPage* CaseWorkPanel::ensureSession(AnalysisModule module)
{
    const QString key = sessionKey(mCurrentCase.id, mCurrentCase.isBase == 1, module);
    if (CaseWorkSessionPage* existing = mSessions.value(key)) {
        return existing;
    }

    if (m_aiConfig) {
        m_aiConfig->setModuleId(aiConfigModuleId(module));
    }

    auto* session = new CaseWorkSessionPage(mCurrentCase, module, m_aiConfig, &m_commonQuestionCache, mSessionStack);
    mSessions.insert(key, session);
    mSessionStack->addWidget(session);
    connectSessionSignals(session, key);
    return session;
}

QWidget* CaseWorkPanel::ensureChooserPage()
{
    const QString key = chooserKey(mCurrentCase.id, mCurrentCase.isBase == 1);
    if (QWidget* existing = mChoosers.value(key)) {
        return existing;
    }

    QWidget* chooser = createChooserPage();
    mChoosers.insert(key, chooser);
    mSessionStack->addWidget(chooser);
    return chooser;
}

void CaseWorkPanel::connectSessionSignals(CaseWorkSessionPage* session, const QString& key)
{
    Q_UNUSED(key)
    connect(session, &CaseWorkSessionPage::analysisDataRequested, this,
            [this, session](CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                            const QString& tableName, const QString& resultName) {
                if (currentSession() == session) {
                    emit analysisDataRequested(module, resultKey, tableName, resultName);
                }
            });
    connect(session, &CaseWorkSessionPage::analysisResultViewed, this, &CaseWorkPanel::onSessionAnalysisResultViewed);
    connect(session, &CaseWorkSessionPage::deriveCaseFromBaseRequested, this,
            [this, session]() {
                if (currentSession() == session) {
                    emit deriveCaseFromBaseRequested();
                }
            });
    connect(session, &CaseWorkSessionPage::stepRunFinished, this,
            [this, session](const QString& stepKey) {
                if (currentSession() == session) {
                    emit stepRunFinished(stepKey);
                }
            });
    connect(session, &CaseWorkSessionPage::aiDataSelectRequested, this,
            [this, session, key](CaseWorkPanel::AnalysisModule module, const QString& stepKey, bool activate) {
                if (currentSession() != session) {
                    return;
                }
                if (activate) {
                    mActiveAiSelectSessionKey = key;
                } else if (mActiveAiSelectSessionKey == key) {
                    mActiveAiSelectSessionKey.clear();
                }
                emit aiDataSelectRequested(module, stepKey, activate);
            });
    connect(session, &CaseWorkSessionPage::moduleSwitchRequested, this, &CaseWorkPanel::onSessionModuleSwitchRequested);
}

void CaseWorkPanel::showSession(CaseWorkSessionPage* session)
{
    if (!session || !mSessionStack) {
        return;
    }
    if (m_aiConfig) {
        m_aiConfig->setModuleId(aiConfigModuleId(session->analysisModule()));
    }
    mSessionStack->setCurrentWidget(session);
    mModuleSelected = true;
    mCurrentModule = session->analysisModule();
    session->prepareForDisplay();
}

void CaseWorkPanel::showChooser()
{
    if (!mSessionStack) {
        return;
    }
    QWidget* chooser = ensureChooserPage();
    mSessionStack->setCurrentWidget(chooser);
    mModuleSelected = false;
}

CaseWorkSessionPage* CaseWorkPanel::currentSession() const
{
    if (!mSessionStack || !mModuleSelected) {
        return nullptr;
    }
    return qobject_cast<CaseWorkSessionPage*>(mSessionStack->currentWidget());
}
