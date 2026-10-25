/*****************************************************************//**
 * \file   caseworksessionpage.cpp
 * \brief  方案工作台单会话页实现
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "caseworksessionpage.h"

#include "../casectrls/caseiconutils.h"
#include "algorithmparamsdialog.h"
#include "aiclient/aiclient.h"
#include "framelessdialog.h"
#include "grpcaistreamclient.h"
#include "grpcstreamclient.h"
#include "httpapis.h"
#include "httpapijson.h"
#include "messagedialog.h"
#include "../utils/filedownloader.h"
#include "../utils/reporthtmlhelper.h"
#include "aichatwidget.h"
#include "aiconfigmodel.h"
#include "aiwordselectbar.h"
#include "uiglobalstate.h"
#include "webview2dialog.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLabel>
#include <QLayout>
#include <QMovie>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QApplication>
#include <QFont>
#include <QVBoxLayout>

extern QStringList g_fontfamilies;

namespace {
constexpr int kFontWorkbenchTitle = 20;
constexpr int kFontSection = 16;
constexpr int kFontBody = 14;
constexpr int kFontCaption = 12;

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

constexpr char kWorkbenchBg[] = "#030914";
constexpr char kPanelBg[] = "#07172f";
constexpr char kCardBg[] = "#061a35";
constexpr char kAccentBlue[] = "#1890ff";
constexpr char kMutedText[] = "rgba(213, 229, 255, 0.72)";
constexpr char kRunningGifPath[] = ":/icons/icons/workbench_running.gif";
constexpr char kModuleCheckedBg[] = "#3583f7";
constexpr char kDataCheckOuterBg[] = "#071632";
constexpr char kDataCheckInnerBg[] = "#030a16";
constexpr char kDataCheckStatusBg[] = "#284370";
constexpr char kDataCheckStatusText[] = "#578de4";
constexpr int kModuleIconBox = 40;
constexpr int kToggleButtonWidth = 40;
constexpr int kToggleButtonHeight = 56;

int aiConfigModuleId(CaseWorkSessionPage::AnalysisModule module)
{
    switch (module) {
    case CaseWorkSessionPage::AnalysisModule::SlowTraffic:
        return AiConfigModel::SlowTrafficModuleId;
    case CaseWorkSessionPage::AnalysisModule::PublicTransport:
        return AiConfigModel::PublicTransportModuleId;
    case CaseWorkSessionPage::AnalysisModule::MotorVehicle:
    default:
        return AiConfigModel::MotorVehicleModuleId;
    }
}

QString jsonValueDisplayText(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString().trimmed();
    }
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Indented)).trimmed();
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Indented)).trimmed();
    }
    if (!value.isUndefined() && !value.isNull()) {
        return value.toVariant().toString().trimmed();
    }
    return QString();
}

QString runAlgDisplayText(const GrpcRunAlgResultJson& result)
{
    const QJsonValue payload = result.j_payload();
    if (!payload.isObject()) {
        return jsonValueDisplayText(payload);
    }

    const QJsonObject rawObj = payload.toObject();
    const QJsonObject dataObj = rawObj.value(QStringLiteral("data")).toObject();
    const QJsonObject resultObj = rawObj.value(QStringLiteral("result")).toObject();
    const QJsonObject dataResultObj = dataObj.value(QStringLiteral("result")).toObject();
    const QList<QJsonValue> candidates = {
        dataObj.value(QStringLiteral("content")),
        dataResultObj.value(QStringLiteral("content")),
        resultObj.value(QStringLiteral("content")),
        rawObj.value(QStringLiteral("content")),
        dataObj.value(QStringLiteral("result")),
        rawObj.value(QStringLiteral("result")),
        dataObj.value(QStringLiteral("message")),
        rawObj.value(QStringLiteral("message"))
    };

    for (const QJsonValue& candidate : candidates) {
        const QString text = jsonValueDisplayText(candidate);
        if (!text.isEmpty()) {
            return text;
        }
    }

    const QString dataText = jsonValueDisplayText(dataObj);
    if (!dataText.isEmpty() && dataText != QStringLiteral("{}")) {
        return dataText;
    }
    return QString::fromUtf8(QJsonDocument(rawObj).toJson(QJsonDocument::Indented)).trimmed();
}

QString runAlgContentUrl(const GrpcRunAlgResultJson& result)
{
    const QJsonValue payload = result.j_payload();
    if (!payload.isObject()) {
        return QString();
    }

    const QJsonObject rawObj = payload.toObject();
    const QJsonObject dataObj = rawObj.value(QStringLiteral("data")).toObject();
    const QJsonObject resultObj = rawObj.value(QStringLiteral("result")).toObject();
    const QJsonObject dataResultObj = dataObj.value(QStringLiteral("result")).toObject();
    const QList<QJsonValue> candidates = {
        dataObj.value(QStringLiteral("content_url")),
        dataResultObj.value(QStringLiteral("content_url")),
        resultObj.value(QStringLiteral("content_url")),
        rawObj.value(QStringLiteral("content_url"))
    };

    for (const QJsonValue& candidate : candidates) {
        if (!candidate.isString()) {
            continue;
        }
        const QString url = candidate.toString().trimmed();
        if (!url.isEmpty()) {
            return url;
        }
    }
    return QString();
}

QString suggestionDetailLinkHtml(const QString& contentUrl)
{
    if (contentUrl.isEmpty()) {
        return QString();
    }
    return QStringLiteral("<p style=\"margin-top:8px;margin-bottom:0;\"><a style=\"color:rgba(226,238,255,0.95);\" href=\"%1\">%2</a></p>")
        .arg(contentUrl.toHtmlEscaped(), QString::fromUtf8(u8"查看详情"));
}

QString suggestionResultHtml(const QString& content, const QString& contentUrl)
{
    const QString body = QStringLiteral(
                             "<pre style=\"margin:0;white-space:pre-wrap;word-wrap:break-word;"
                             "word-break:break-all;overflow-wrap:anywhere;"
                             "color:rgba(226,238,255,0.92);font-family:inherit;\">%1</pre>")
                             .arg(content.toHtmlEscaped());
    return body + suggestionDetailLinkHtml(contentUrl);
}

QString formatEditLogDisplayText(const PagedDataJson& page)
{
    const int total = page.j_total();
    const int pageNum = page.j_page();
    const int limit = page.j_limit();
    const QList<QJsonObject> items = page.j_data();

    QString text = QString::fromUtf8(u8"共 %1 条记录（第 %2 页，每页 %3 条）")
                       .arg(total)
                       .arg(pageNum)
                       .arg(limit);

    if (items.isEmpty()) {
        text += QString::fromUtf8(u8"\n\n暂无修改记录。");
        return text;
    }

    text += QString::fromUtf8(u8"\n");
    for (const QJsonObject& item : items) {
        const QString createdAt = item.value(QStringLiteral("created_at")).toString().trimmed();
        const QString demo = item.value(QStringLiteral("demo")).toString().trimmed();
        text += QString::fromUtf8(u8"\n\n%1 %2").arg(createdAt, demo);
    }
    return text;
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

QPushButton* createTextButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setCheckable(false);
    button->setMinimumHeight(28);
    button->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   border: 1px solid rgba(47, 132, 227, 0.50);"
        "   border-radius: 6px;"
        "   background-color: #071b35;"
        "   color: #6eb8ff;"
        "   font-weight: 600;"
        "   padding: 0 10px;"
        "}"
        "QPushButton:hover {"
        "   border-color: #66b5ff;"
        "   color: white;"
        "}"
        "QPushButton:disabled {"
        "   border-color: rgba(72, 112, 164, 0.28);"
        "   background-color: rgba(7, 27, 53, 0.60);"
        "   color: rgba(175, 199, 230, 0.36);"
        "}"));
    applyWorkbenchFont(button, kFontBody, QFont::DemiBold);
    return button;
}

QPushButton* createRunButton(QWidget* parent)
{
    auto* button = createTextButton(QString::fromUtf8(u8"运行"), parent);
    button->setFixedSize(72, 26);
    button->setIcon(CaseIconUtils::workbenchSingleStateIcon(QStringLiteral("run")));
    button->setIconSize(QSize(12, 12));
    button->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 13px;"
        "   background-color: #1e78df;"
        "   color: white;"
        "   font-weight: 700;"
        "}"
        "QPushButton:hover { background-color: #3294ff; }"
        "QPushButton:disabled {"
        "   background-color: rgba(30, 120, 223, 0.30);"
        "   color: rgba(255,255,255,0.36);"
        "}"));
    applyWorkbenchFont(button, kFontBody, QFont::Bold);
    return button;
}

QLabel* createSectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral(
        "QLabel {"
        "   color: white;"
        "}"));
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

QWidget* createSectionTitleRow(const QString& text, const QString& iconStem, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    const QPixmap iconPixmap(CaseIconUtils::workbenchIconPath(iconStem));
    auto* iconLabel = new QLabel(row);
    if (!iconPixmap.isNull()) {
        iconLabel->setPixmap(iconPixmap);
        iconLabel->setFixedSize(iconPixmap.size());
    }
    iconLabel->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));
    layout->addWidget(iconLabel, 0, Qt::AlignVCenter);
    layout->addWidget(createSectionTitle(text, row), 1);
    return row;
}

void clearLayout(QLayout* layout)
{
    if (!layout) {
        return;
    }
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

void applyAnalysisResultButtonProperties(QPushButton* button, const CaseWorkPanel::AnalysisResultItem& resultItem)
{
    if (!button) {
        return;
    }
    button->setProperty("resultName", resultItem.name);
    button->setProperty("resultKey", resultItem.renderLayer);
    button->setProperty("overrideModule", resultItem.overrideModule);
    button->setProperty("targetModule", static_cast<int>(resultItem.targetModule));
}

CaseWorkPanel::AnalysisModule analysisModuleFromButton(const QPushButton* button,
                                                       CaseWorkPanel::AnalysisModule currentModule)
{
    if (button && button->property("overrideModule").toBool()) {
        return static_cast<CaseWorkPanel::AnalysisModule>(button->property("targetModule").toInt());
    }
    return currentModule;
}

CaseWorkPanel::AnalysisResultItem makeNetworkAnalysisResult(
    const QString& name,
    const QString& layer,
    const QString& table,
    bool overrideModule = false,
    CaseWorkPanel::AnalysisModule targetModule = CaseWorkPanel::AnalysisModule::MotorVehicle)
{
    CaseWorkPanel::AnalysisResultItem item;
    item.name = name;
    item.renderLayer = layer;
    item.tableName = table;
    item.overrideModule = overrideModule;
    item.targetModule = targetModule;
    return item;
}
} // namespace

CaseWorkSessionPage::CaseWorkSessionPage(const CaseItem& caseItem,
                                         AnalysisModule module,
                                         QSharedPointer<AiConfigModel> aiConfig,
                                         QHash<QString, QStringList>* commonQuestionCache,
                                         QWidget* parent)
    : QWidget(parent)
    , mCurrentCase(caseItem)
    , mProjectId(caseItem.projectId)
    , mCurrentModule(module)
    , m_aiConfig(std::move(aiConfig))
    , m_commonQuestionCache(commonQuestionCache)
{
    setAttribute(Qt::WA_StyledBackground, true);
    initUi();
    initializeWorkflow();
}

CaseWorkSessionPage::~CaseWorkSessionPage()
{
    prepareForShutdown();
}

void CaseWorkSessionPage::prepareForShutdown()
{
    if (m_grpcStream) {
        m_grpcStream->stop();
    }
    if (m_grpcAiStream) {
        m_grpcAiStream->stop();
    }
    if (m_trafficMicroStream) {
        m_trafficMicroStream->stop();
    }
}

void CaseWorkSessionPage::detachSharedCaches()
{
    m_commonQuestionCache = nullptr;
    m_aiConfig.clear();
}

void CaseWorkSessionPage::setCaseInfo(const CaseItem& caseItem)
{
    if (mCurrentCase.id != caseItem.id || mProjectId != caseItem.projectId) {
        ++mAlgorithmRunRequestSeq;
        mAlgorithmRunLoaded = false;
        mAlgorithmRunPassed = false;
        mAlgorithmStatusPendingApply = false;
    }

    mCurrentCase = caseItem;
    mProjectId = caseItem.projectId;
}

void CaseWorkSessionPage::initializeWorkflow()
{
    mModuleSelected = true;
    mNormalReviewCompleted = false;
    m_activeSuggestionType2.clear();
    mActiveStepKey.clear();
    if (m_aiConfig) {
        m_aiConfig->setModuleId(aiConfigModuleId(mCurrentModule));
        applyAiConfigToChatWidgets();
    }
    rebuildStepsForCurrentModule();
    ensureCommonQuestionsLoaded();
    refreshGrpcCheck();
    loadAlgorithmRunStatus();
    refreshWorkPage();
}

void CaseWorkSessionPage::setAiSelectedLinkIds(const QString& stepKey, const QStringList& linkIds)
{
    FlowStep* step = stepByKey(stepKey);
    if (!step) {
        return;
    }
    step->aiSelectedLinkIds = linkIds;
    if (step->aiChatWidget) {
        if (AiWordSelectBar* wordBar = step->aiChatWidget->wordSelectBar()) {
            wordBar->setSelectedCount(linkIds.size());
        }
    }
}

void CaseWorkSessionPage::setAiDataSelectChecked(const QString& stepKey, bool checked)
{
    FlowStep* step = stepByKey(stepKey);
    if (!step || !step->aiChatWidget) {
        return;
    }
    if (AiWordSelectBar* wordBar = step->aiChatWidget->wordSelectBar()) {
        wordBar->setDataSelectChecked(checked);
    }
}

void CaseWorkSessionPage::finishAiDataSelect(const QString& stepKey, const QStringList& linkIds)
{
    setAiSelectedLinkIds(stepKey, linkIds);
    setAiDataSelectChecked(stepKey, false);
}

void CaseWorkSessionPage::resetWorkflow()
{
    resetStateForCurrentCase();
    rebuildStepsForCurrentModule();
    ensureCommonQuestionsLoaded();
    refreshGrpcCheck();
    loadAlgorithmRunStatus();
    refreshWorkPage();
}

void CaseWorkSessionPage::startTrafficMicroDiagnosis(const QString& ids)
{
    const QString token = authToken();
    if (token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"中微观诊断"),
                          QString::fromUtf8(u8"登录状态或方案信息无效。"));
        return;
    }

    if (ids.trimmed().isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"中微观诊断"),
                          QString::fromUtf8(u8"未获取到有效的路段 link_id。"));
        return;
    }

    const QString hostKey = smartDialogHostKey();
    setStepSmartDialogVisible(hostKey, true, true);
    refreshWorkPage();

    ensureTrafficMicroStreamClient();
    appendStepLog(hostKey,
                  QStringLiteral("[%1] %2")
                      .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                           QString::fromUtf8(u8"启动运行：中微观诊断")));

    m_trafficMicroStream->start(mProjectId,
                                mCurrentCase.id,
                                mCurrentCase.isBase,
                                grpcTransportType(),
                                QStringLiteral("traffic_micro"),
                                token,
                                ids,
                                QString());
}

void CaseWorkSessionPage::prepareForDisplay()
{
    syncModuleButtonsToCurrentModule();
}

void CaseWorkSessionPage::syncModuleButtonsToCurrentModule()
{
    if (!mMotorButton || !mSlowButton || !mPublicButton) {
        return;
    }

    const bool blockMotor = mMotorButton->blockSignals(true);
    const bool blockSlow = mSlowButton->blockSignals(true);
    const bool blockPublic = mPublicButton->blockSignals(true);

    mMotorButton->setChecked(mCurrentModule == AnalysisModule::MotorVehicle);
    mSlowButton->setChecked(mCurrentModule == AnalysisModule::SlowTraffic);
    mPublicButton->setChecked(mCurrentModule == AnalysisModule::PublicTransport);

    mMotorButton->blockSignals(blockMotor);
    mSlowButton->blockSignals(blockSlow);
    mPublicButton->blockSignals(blockPublic);

    const auto refreshModuleIcon = [](QPushButton* button, const QString& iconStem) {
        if (!button) {
            return;
        }
        if (QLabel* iconLabel = button->findChild<QLabel*>(QStringLiteral("moduleIconLabel"))) {
            const QPixmap pixmap = loadWorkbenchModulePixmap(iconStem, button->isChecked());
            iconLabel->setPixmap(pixmap);
            if (button->isChecked()) {
                iconLabel->setFixedSize(kModuleIconBox, kModuleIconBox);
                iconLabel->setStyleSheet(QStringLiteral(
                    "QLabel {"
                    "   background-color: %1;"
                    "   border-radius: 8px;"
                    "}").arg(QLatin1String(kModuleCheckedBg)));
            } else {
                iconLabel->setFixedSize(kModuleIconBox, kModuleIconBox);
                iconLabel->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));
            }
        }
    };
    refreshModuleIcon(mMotorButton, QStringLiteral("motor"));
    refreshModuleIcon(mSlowButton, QStringLiteral("traffic"));
    refreshModuleIcon(mPublicButton, QStringLiteral("bus"));
}

void CaseWorkSessionPage::onModuleClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }
    const AnalysisModule module = static_cast<AnalysisModule>(button->property("analysisModule").toInt());
    // exclusive Group 会先改掉本页 checked；会话模块固定，先滚回再请求切页
    syncModuleButtonsToCurrentModule();
    if (module != mCurrentModule) {
        emit moduleSwitchRequested(module);
    }
}

void CaseWorkSessionPage::onStepRunClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    const QString stepKey = button->property("stepKey").toString();
    const FlowStep* step = stepByKey(stepKey);
    if (!step || !isStepStateRunnable(*step)) {
        return;
    }
    if (!canRunStep(*step)) {
        showGrpcCheckRunBlockedMessage();
        return;
    }

    if (step->key == QLatin1String("suggestion")) {
        startGrpcRunAlgForSuggestion();
        return;
    }

    if (step->type2 == QLatin1String("network")) {
        AlgorithmParamsDialog dlg(nullptr);
        if (mCurrentModule == AnalysisModule::SlowTraffic)
        {
            dlg.resize(520, 580);
            if (!dlg.loadAlgorithm(QStringLiteral("trafficAllocationAlgorithm2"))) {
                return;
            }
        }
        else
        {
            if (!dlg.loadAlgorithm(QStringLiteral("trafficAllocationAlgorithm"))) {
                return;
            }
        }
 
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        startGrpcStreamForStep(stepKey, dlg.parametersJsonString());
        return;
    }

    startGrpcStreamForStep(stepKey);
}

void CaseWorkSessionPage::onSuggestionButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    FlowStep* step = stepByKey(QStringLiteral("suggestion"));
    const QString type2 = button->property("aiType2").toString();
    AnalysisResultItem* item = suggestionResultByAiType2(type2);
    if (!step || !item || !canRunSuggestionItem(*step, *item)) {
        if (!canRunAlgorithms()) {
            showGrpcCheckRunBlockedMessage();
        }
        return;
    }

    m_activeSuggestionType2 = type2;
    step->activeAiType2 = type2;
    const QString prompt = QString::fromUtf8(u8"%1，请生成方案建议").arg(item->name);
    startAiStreamForSuggestion(type2, prompt);
}

void CaseWorkSessionPage::onEditLogClicked()
{
    const QString token = authToken();
    if (token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"修改方案记录"),
                          QString::fromUtf8(u8"登录状态或方案信息无效。"));
        return;
    }
    if (m_httpBusy || m_sseBusy) {
        return;
    }

    FlowStep* step = stepByKey(QStringLiteral("editLog"));
    if (!step) {
        return;
    }

    step->editLogOutputVisible = true;
    step->editLogOutputText = QString::fromUtf8(u8"正在查询修改方案记录…");
    refreshWorkPage();

    setHttpBusy(true);
    QPointer<CaseWorkSessionPage> self(this);
    HttpApis::caseEditLog(token,
                          mProjectId,
                          mCurrentCase.id,
                          step->editLogPage,
                          step->editLogLimit,
                          grpcTransportType(),
                          [self](bool suc, const QSharedPointer<PagedDataJson>& res, const QString& errMsg) {
                              if (!self) {
                                  return;
                              }
                              self->setHttpBusy(false);
                              FlowStep* currentStep = self->stepByKey(QStringLiteral("editLog"));
                              if (!currentStep) {
                                  return;
                              }

                              if (!suc || res.isNull()) {
                                  const QString message = errMsg.trimmed().isEmpty()
                                                              ? QString::fromUtf8(u8"查询失败")
                                                              : errMsg.trimmed();
                                  currentStep->editLogOutputText = message;
                                  TnUi::showWarning(QString::fromUtf8(u8"修改方案记录"), message);
                                  self->refreshWorkPage();
                                  return;
                              }

                              currentStep->editLogOutputText = formatEditLogDisplayText(*res);
                              self->refreshWorkPage();
                          });
}

void CaseWorkSessionPage::onCostBenefitClicked()
{
    const QString token = authToken();
    if (token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"成本效益分析"),
                          QString::fromUtf8(u8"登录状态或方案信息无效。"));
        return;
    }
    if (m_httpBusy || m_sseBusy) {
        return;
    }

    setHttpBusy(true);
    QPointer<CaseWorkSessionPage> self(this);
    HttpApis::caseCostView(token,
                           mProjectId,
                           mCurrentCase.id,
                           grpcTransportType(),
                           [self](bool suc, const QSharedPointer<CaseReportViewJson>& res, const QString& errMsg) {
                               if (!self) {
                                   return;
                               }
                               self->setHttpBusy(false);
                               if (!suc || res.isNull()) {
                                   TnUi::showWarning(QString::fromUtf8(u8"成本效益分析"),
                                                     errMsg.isEmpty() ? QString::fromUtf8(u8"未知错误") : errMsg);
                                   return;
                               }
                               const QString reportUrl = res->j_content().trimmed();
                               openHtmlReportWithFallback(self ? self->window() : nullptr,
                                                          QString::fromUtf8(u8"成本效益分析"),
                                                          reportUrl,
                                                          QStringLiteral("cost_benefit.html"));
                           });
}

void CaseWorkSessionPage::onRenovationReportClicked()
{
    const QString token = authToken();
    if (token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"改造方案报告"),
                          QString::fromUtf8(u8"登录状态或方案信息无效。"));
        return;
    }
    if (m_httpBusy || m_sseBusy) {
        return;
    }

    setHttpBusy(true);
    QPointer<CaseWorkSessionPage> self(this);
    HttpApis::exportPtCaseReport(token,
                                 mProjectId,
                                 mCurrentCase.id,
                                 grpcTransportType(),
                                 [self](bool suc, const QSharedPointer<DownLoadCaseJson>& res, const QString& errMsg) {
                                     if (!self) {
                                         return;
                                     }
                                     self->setHttpBusy(false);
                                     if (!suc || res.isNull()) {
                                         TnUi::showWarning(QString::fromUtf8(u8"改造方案报告"),
                                                           errMsg.isEmpty() ? QString::fromUtf8(u8"未知错误") : errMsg);
                                         return;
                                     }
                                     QString reportUrl = res->j_content().trimmed();
                                     if (reportUrl.isEmpty()) {
                                         reportUrl = ReportHtmlHelper::extract(res->jsonObject());
                                     }
                                     if (!reportUrl.isEmpty()) {
                                         openHtmlReportWithFallback(self ? self->window() : nullptr,
                                                                    QString::fromUtf8(u8"改造方案报告"),
                                                                    reportUrl,
                                                                    QStringLiteral("ptcase_report.html"));
                                         return;
                                     }
                                     if (!res->resolvedDownloadUrl().isEmpty()) {
                                         self->setHttpBusy(true);
                                         self->downloadCaseFile(res, QString::fromUtf8(u8"改造方案报告"));
                                         return;
                                     }
                                     TnUi::showInformation(QString::fromUtf8(u8"改造方案报告"),
                                                           QString::fromUtf8(u8"报告已生成"));
                                 });
}

void CaseWorkSessionPage::onGenerateAnalysisReportClicked()
{
    const QString token = authToken();
    if (token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"分析报告"),
                          QString::fromUtf8(u8"登录状态或方案信息无效。"));
        return;
    }
    if (m_httpBusy || m_sseBusy) {
        return;
    }

    setHttpBusy(true);
    QPointer<CaseWorkSessionPage> self(this);
    HttpApis::grpcCaseExport(token,
                             mProjectId,
                             mCurrentCase.id,
                             grpcTransportType(),
                             [self](bool suc, const QSharedPointer<DownLoadCaseJson>&, const QString& errMsg) {
                                 if (!self) {
                                     return;
                                 }
                                 self->setHttpBusy(false);
                                 if (!suc) {
                                     TnUi::showWarning(QString::fromUtf8(u8"分析报告"),
                                                       errMsg.isEmpty() ? QString::fromUtf8(u8"未知错误") : errMsg);
                                     return;
                                 }
                                 self->refreshWorkPage();
                                 TnUi::showInformation(QString::fromUtf8(u8"分析报告"),
                                                       QString::fromUtf8(u8"报告已生成"));
                             });
}

void CaseWorkSessionPage::onAnalysisReportDownloadClicked()
{
    const QString token = authToken();
    if (token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"分析报告"),
                          QString::fromUtf8(u8"登录状态或方案信息无效。"));
        return;
    }
    if (m_httpBusy || m_sseBusy) {
        return;
    }

    setHttpBusy(true);
    QPointer<CaseWorkSessionPage> self(this);
    HttpApis::caseReportDownload(token,
                                 mProjectId,
                                 mCurrentCase.id,
                                 grpcTransportType(),
                                 [self](bool downloadSuc,
                                     const QSharedPointer<DownLoadCaseJson>& downloadRes,
                                     const QString& downloadErrMsg) {
                                     if (!self) {
                                         return;
                                     }
                                     if (!downloadSuc || downloadRes.isNull()) {
                                         self->setHttpBusy(false);
                                         TnUi::showWarning(QString::fromUtf8(u8"下载失败"),
                                                           downloadErrMsg.isEmpty()
                                                               ? QString::fromUtf8(u8"未知错误")
                                                               : downloadErrMsg);
                                         return;
                                     }
                                     self->downloadCaseFile(downloadRes, QString::fromUtf8(u8"分析报告"));
                                 });
}

void CaseWorkSessionPage::onReportViewClicked()
{
    const QString reportTitle = analysisReportTitle();
    const QString token = authToken();
    if (token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        TnUi::showWarning(reportTitle,
                          QString::fromUtf8(u8"登录状态或方案信息无效。"));
        return;
    }
    if (m_httpBusy || m_sseBusy) {
        return;
    }

    setHttpBusy(true);
    QPointer<CaseWorkSessionPage> self(this);
    HttpApis::caseReportView(token,
                             mProjectId,
                             mCurrentCase.id,
                             grpcTransportType(),
                             [self, reportTitle](bool suc, const QSharedPointer<CaseReportViewJson>& res, const QString& errMsg) {
                                 if (!self) {
                                     return;
                                 }
                                 self->setHttpBusy(false);
                                 if (!suc || res.isNull()) {
                                     TnUi::showWarning(QString::fromUtf8(u8"查看失败"),
                                                       errMsg.isEmpty() ? QString::fromUtf8(u8"未知错误") : errMsg);
                                     return;
                                 }
                                 const QString reportUrl = res->j_content().trimmed();
                                 openHtmlReportWithFallback(self ? self->window() : nullptr,
                                                            reportTitle,
                                                            reportUrl,
                                                            QStringLiteral("analysis_report.html"));
                             });
}

void CaseWorkSessionPage::onOsmDownloadClicked()
{
    const QString token = authToken();
    if (token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"OSM地图数据"),
                          QString::fromUtf8(u8"登录状态或方案信息无效。"));
        return;
    }
    if (m_httpBusy || m_sseBusy) {
        return;
    }

    setHttpBusy(true);
    QPointer<CaseWorkSessionPage> self(this);
    HttpApis::exportCaseOsm(token,
                            mProjectId,
                            mCurrentCase.id,
                            grpcTransportType(),
                            [self](bool suc, const QSharedPointer<DownLoadCaseJson>& res, const QString& errMsg) {
                                if (!self) {
                                    return;
                                }
                                if (!suc || res.isNull()) {
                                    self->setHttpBusy(false);
                                    TnUi::showWarning(QString::fromUtf8(u8"下载失败"),
                                                      errMsg.isEmpty() ? QString::fromUtf8(u8"未知错误") : errMsg);
                                    return;
                                }
                                if (res->resolvedDownloadUrl().isEmpty()) {
                                    self->setHttpBusy(false);
                                    TnUi::showWarning(QString::fromUtf8(u8"OSM地图数据"),
                                                      QString::fromUtf8(u8"接口未返回下载地址。"));
                                    return;
                                }
                                self->downloadCaseFile(res, QString::fromUtf8(u8"OSM地图数据"));
                            });
}

void CaseWorkSessionPage::onSimulateDownloadClicked()
{
    const QString token = authToken();
    if (token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"仿真流量数据"),
                          QString::fromUtf8(u8"登录状态或方案信息无效。"));
        return;
    }
    if (m_httpBusy || m_sseBusy) {
        return;
    }

    setHttpBusy(true);
    QPointer<CaseWorkSessionPage> self(this);
    HttpApis::exportCaseSimulate(token,
                                 mProjectId,
                                 mCurrentCase.id,
                                 grpcTransportType(),
                                 [self](bool suc, const QSharedPointer<DownLoadCaseJson>& res, const QString& errMsg) {
                                     if (!self) {
                                         return;
                                     }
                                     if (!suc || res.isNull()) {
                                         self->setHttpBusy(false);
                                         TnUi::showWarning(QString::fromUtf8(u8"下载失败"),
                                                           errMsg.isEmpty() ? QString::fromUtf8(u8"未知错误") : errMsg);
                                         return;
                                     }
                                     if (res->resolvedDownloadUrl().isEmpty()) {
                                         self->setHttpBusy(false);
                                         TnUi::showWarning(QString::fromUtf8(u8"仿真流量数据"),
                                                           QString::fromUtf8(u8"接口未返回下载地址。"));
                                         return;
                                     }
                                     self->downloadCaseFile(res, QString::fromUtf8(u8"仿真流量数据"));
                                 });
}

void CaseWorkSessionPage::onDeriveCaseClicked()
{
    emit deriveCaseFromBaseRequested();
}

void CaseWorkSessionPage::onResultDataClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    const QString tableName = button->property("tableName").toString();
    const QString resultKey = button->property("resultKey").toString();
    const QString resultName = button->property("resultName").toString();
    emit analysisDataRequested(analysisModuleFromButton(button, mCurrentModule), resultKey, tableName,
                               resultName);
}

void CaseWorkSessionPage::onResultViewClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    const QString resultName = button->property("resultName").toString();
    const QString resultKey = button->property("resultKey").toString();
    emit analysisResultViewed(analysisModuleFromButton(button, mCurrentModule), resultKey, resultName);
}

void CaseWorkSessionPage::initUi()
{
    setStyleSheet(QStringLiteral(
        "CaseWorkSessionPage {"
        "   background-color: %1;"
        "}").arg(QLatin1String(kWorkbenchBg)));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(createWorkPage(), 1);
}

void CaseWorkSessionPage::rebuildWorkflow()
{
    if (!mWorkContentLayout) {
        return;
    }

    m_workbenchGateButtons.clear();
    clearWorkContentLayout();

    mWorkContentLayout->addWidget(createAnalysisModulePanel());
    if (!mModuleSelected) {
        mWorkContentLayout->addStretch();
        return;
    }

    mWorkContentLayout->addWidget(createDataCheckPanel());
    if (mCurrentCase.isBase) {
        mWorkContentLayout->addWidget(createBaseFlowPanel());
        // 底部输出区不再依赖算法状态门控，直接显示
        mWorkContentLayout->addWidget(createBottomReportPanel());
        mWorkContentLayout->addWidget(createFinalOutputPanel());
    } else {
        mWorkContentLayout->addWidget(createNormalFlowPanel());
        mWorkContentLayout->addWidget(createNormalReportPanel());
    }
    mWorkContentLayout->addStretch();
    updateWorkbenchGateButtons();
}

void CaseWorkSessionPage::refreshWorkPage()
{
    rebuildWorkflow();
}

void CaseWorkSessionPage::clearWorkContentLayout()
{
    for (FlowStep& step : mFlowSteps) {
        step.aiChatWidget = nullptr;
    }
    clearLayout(mWorkContentLayout);
}

QWidget* CaseWorkSessionPage::createWorkPage()
{
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 6px; background: transparent; }"
        "QScrollBar::handle:vertical { border-radius: 3px; background: rgba(75,139,220,0.36); }"));

    mWorkContentWidget = new QWidget(scrollArea);
    mWorkContentWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    mWorkContentLayout = new QVBoxLayout(mWorkContentWidget);
    mWorkContentLayout->setContentsMargins(0, 0, 4, 0);
    mWorkContentLayout->setSpacing(10);
    scrollArea->setWidget(mWorkContentWidget);
    return scrollArea;
}

QPushButton* CaseWorkSessionPage::createStepAiAssistantButton(QWidget* parent, const QString& stepKey)
{
    auto* button = new QPushButton(parent);
    button->setProperty("stepKey", stepKey);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(28, 28);
    button->setToolTip(QString::fromUtf8(u8"AI智能助手"));
    button->setIcon(CaseIconUtils::workbenchSingleStateIcon(QStringLiteral("ai_assistant")));
    button->setIconSize(QSize(20, 20));
    button->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   border: none;"
        "   background: transparent;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(24, 144, 255, 0.18);"
        "   border-radius: 6px;"
        "}"
        "QPushButton:disabled {"
        "   opacity: 0.35;"
        "}"));
    connect(button, &QPushButton::clicked, this, &CaseWorkSessionPage::onStepAiAssistantClicked);
    return button;
}

QString CaseWorkSessionPage::smartDialogHostKey() const
{
    return mCurrentCase.isBase ? QStringLiteral("traffic") : QStringLiteral("review");
}

QString CaseWorkSessionPage::resolveSmartDialogKey(const QString& stepKey) const
{
    if (stepKey == QLatin1String("network")) {
        return QStringLiteral("traffic");
    }
    if (stepKey == QLatin1String("suggestion") || stepKey == QLatin1String("editLog")) {
        return QStringLiteral("review");
    }
    return stepKey;
}

bool CaseWorkSessionPage::shouldAttachSmartDialog(const QString& stepKey) const
{
    return stepKey == smartDialogHostKey();
}

void CaseWorkSessionPage::setStepSmartDialogVisible(const QString& stepKey, bool visible, bool expand)
{
    FlowStep* step = stepByKey(resolveSmartDialogKey(stepKey));
    if (!step) {
        return;
    }

    step->logVisible = visible;
    if (visible && expand) {
        step->logCollapsed = false;
    }
}

void CaseWorkSessionPage::ensureSmartDialogShownForRun(const QString& stepKey)
{
    const QString hostKey = resolveSmartDialogKey(stepKey);
    FlowStep* step = stepByKey(hostKey);
    if (!step || step->logVisible) {
        return;
    }
    setStepSmartDialogVisible(stepKey, true, true);
}

void CaseWorkSessionPage::attachStepAiChatWidget(QVBoxLayout* layout, FlowStep& step, QWidget* card)
{
    if (!layout || !step.logVisible) {
        return;
    }

    ensureAiConfigLoaded();
    ensureCommonQuestionsLoaded();

    step.aiChatWidget = new AIChatWidget(card);
    step.aiChatWidget->setCollapsed(step.logCollapsed);
    step.aiChatWidget->setUseBackendAiApi(true);
    const AiConfigModel* config = (m_aiConfig && m_aiConfig->isValid()) ? m_aiConfig.data() : nullptr;
    step.aiChatWidget->setAiConfig(config);
    step.aiChatWidget->setWordSelection(step.aiL1Index, step.aiL2Index);
    step.aiChatWidget->setCommonQuestions(m_commonQuestionCache
                                              ? m_commonQuestionCache->value(commonQuestionCacheKey())
                                              : QStringList());

    const QString stepKey = step.key;
    connect(step.aiChatWidget, &AIChatWidget::promptSubmitted, this,
            [this, stepKey](const QString& prompt, int l1, int l2, const QString& commonQuestion) {
                onAiChatPromptSubmitted(stepKey, prompt, l1, l2, commonQuestion);
            });
    if (AiWordSelectBar* wordBar = step.aiChatWidget->wordSelectBar()) {
        wordBar->setSelectedCount(step.aiSelectedLinkIds.size());
        connect(wordBar, &AiWordSelectBar::selectionChanged, this,
                [this, stepKey](int l1, int l2) {
                    FlowStep* flowStep = stepByKey(stepKey);
                    if (!flowStep) {
                        return;
                    }
                    flowStep->aiL1Index = l1;
                    flowStep->aiL2Index = l2;
                    if (m_aiConfig && m_aiConfig->hasBoxSelectL1Option()
                        && !m_aiConfig->isBoxSelectL1(l1, l2)) {
                        if (!flowStep->aiSelectedLinkIds.isEmpty()) {
                            flowStep->aiSelectedLinkIds.clear();
                            if (flowStep->aiChatWidget) {
                                if (AiWordSelectBar* bar = flowStep->aiChatWidget->wordSelectBar()) {
                                    bar->setSelectedCount(0);
                                }
                            }
                        }
                        if (flowStep->aiChatWidget) {
                            if (AiWordSelectBar* bar = flowStep->aiChatWidget->wordSelectBar()) {
                                if (bar->isDataSelectChecked()) {
                                    bar->setDataSelectChecked(false);
                                    emit aiDataSelectRequested(mCurrentModule, stepKey, false);
                                }
                            }
                        }
                    }
                });
        connect(wordBar, &AiWordSelectBar::dataSelectToggled, this,
                [this, stepKey](bool checked) {
                    if (checked) {
                        for (FlowStep& otherStep : mFlowSteps) {
                            if (otherStep.key == stepKey || !otherStep.aiChatWidget) {
                                continue;
                            }
                            if (AiWordSelectBar* otherBar = otherStep.aiChatWidget->wordSelectBar()) {
                                if (otherBar->isDataSelectChecked()) {
                                    otherBar->setDataSelectChecked(false);
                                    emit aiDataSelectRequested(mCurrentModule, otherStep.key, false);
                                }
                            }
                        }
                    }
                    emit aiDataSelectRequested(mCurrentModule, stepKey, checked);
                });
    }
    connect(step.aiChatWidget, &AIChatWidget::dialogMessageAdded, this,
            [this, stepKey](const AIChatMessage& message) {
                FlowStep* flowStep = stepByKey(stepKey);
                if (!flowStep) {
                    return;
                }
                for (const AIChatMessage& existing : flowStep->chatTimeline) {
                    if (existing.id == message.id) {
                        return;
                    }
                }
                flowStep->chatTimeline.append(message);
            });
    connect(step.aiChatWidget, &AIChatWidget::dialogMessageUpdated, this,
            [this, stepKey](const QString& messageId,
                            const QString& content,
                            bool isError,
                            const QString& downloadUrl) {
                FlowStep* flowStep = stepByKey(stepKey);
                if (!flowStep) {
                    return;
                }
                for (AIChatMessage& message : flowStep->chatTimeline) {
                    if (message.id != messageId) {
                        continue;
                    }
                    message.content = content;
                    message.downloadUrl = downloadUrl;
                    message.role = isError ? AIMessageRole::Error : AIMessageRole::Assistant;
                    return;
                }
            });
    connect(step.aiChatWidget, &AIChatWidget::downloadLinkActivated, this,
            [this](const QString& url) {
                downloadFromUrl(url, QString::fromUtf8(u8"AI 报告下载"));
            });

    layout->addWidget(step.aiChatWidget);
    step.aiChatWidget->restoreChatTimeline(step.chatTimeline);
}

void CaseWorkSessionPage::ensureAiConfigLoaded()
{
    if (m_aiConfigRequested) {
        return;
    }
    m_aiConfigRequested = true;
    if (!m_aiConfig) {
        m_aiConfig = QSharedPointer<AiConfigModel>::create();
    }

    HttpApis::getAiConfig([this](bool suc, const QSharedPointer<PropertyJson>& res, const QString& errMsg) {
        if (!suc || res.isNull()) {
            if (!m_aiConfigLoadNotified) {
                m_aiConfigLoadNotified = true;
                for (FlowStep& step : mFlowSteps) {
                    if (step.aiChatWidget) {
                        step.aiChatWidget->appendSystemLine(
                            errMsg.isEmpty() ? QString::fromUtf8(u8"AI 选词配置加载失败，将使用默认索引 0。")
                                             : errMsg);
                    }
                }
            }
            return;
        }
        m_aiConfig->loadFromJson(*res);
        m_aiConfig->setModuleId(aiConfigModuleId(mCurrentModule));
        applyAiConfigToChatWidgets();
    });
}

void CaseWorkSessionPage::applyAiConfigToChatWidgets()
{
    if (!m_aiConfig || !m_aiConfig->isValid()) {
        return;
    }
    for (FlowStep& step : mFlowSteps) {
        if (!step.aiChatWidget) {
            continue;
        }
        step.aiChatWidget->setAiConfig(m_aiConfig.data());
        m_aiConfig->clampSelection(step.aiL1Index, step.aiL2Index);
        step.aiChatWidget->setWordSelection(step.aiL1Index, step.aiL2Index);
    }
}

QString CaseWorkSessionPage::commonQuestionCacheKey() const
{
    return QStringLiteral("%1|%2").arg(mCurrentCase.isBase ? 1 : 0).arg(grpcTransportType());
}

void CaseWorkSessionPage::ensureCommonQuestionsLoaded()
{
    if (!m_commonQuestionCache) {
        return;
    }
    const QString key = commonQuestionCacheKey();
    if (m_commonQuestionCache->contains(key) || m_commonQuestionRequests.contains(key)) {
        applyCommonQuestionsToChatWidgets();
        return;
    }

    m_commonQuestionRequests.append(key);
    const int isbase = mCurrentCase.isBase ? 1 : 0;
    const QString type = grpcTransportType();
    QPointer<CaseWorkSessionPage> self(this);
    HttpApis::getCommonQuestions(isbase,
                                 type,
                                 [self, key](bool suc,
                                             const QSharedPointer<QuestionListJson>& res,
                                             const QString& /*errMsg*/) {
                                     if (!self || !self->m_commonQuestionCache) {
                                         return;
                                     }
                                     self->m_commonQuestionRequests.removeAll(key);

                                     QStringList questions;
                                     if (suc && !res.isNull()) {
                                         for (const QuestionItemJson& item : res->j_data()) {
                                             const QString name = item.j_name().trimmed();
                                             if (!name.isEmpty() && !questions.contains(name)) {
                                                 questions.append(name);
                                             }
                                         }
                                     }
                                     self->m_commonQuestionCache->insert(key, questions);
                                     self->applyCommonQuestionsToChatWidgets();
                                 });
}

void CaseWorkSessionPage::applyCommonQuestionsToChatWidgets()
{
    const QStringList questions = m_commonQuestionCache
                                      ? m_commonQuestionCache->value(commonQuestionCacheKey())
                                      : QStringList();
    for (FlowStep& step : mFlowSteps) {
        if (step.aiChatWidget) {
            step.aiChatWidget->setCommonQuestions(questions);
        }
    }
}

void CaseWorkSessionPage::onStepAiAssistantClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    const QString stepKey = button->property("stepKey").toString();
    const QString hostKey = resolveSmartDialogKey(stepKey);
    FlowStep* step = stepByKey(hostKey);
    if (!step) {
        return;
    }

    if (!step->logVisible) {
        setStepSmartDialogVisible(stepKey, true, true);
    } else {
        setStepSmartDialogVisible(stepKey, false);
    }
    refreshWorkPage();
}

QWidget* CaseWorkSessionPage::createAnalysisModulePanel()
{
    QFrame* card = createCard(mWorkContentWidget);
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
        auto* layout = new QVBoxLayout(button);
        layout->setContentsMargins(6, 8, 6, 8);
        layout->setSpacing(2);
        QLabel* iconLabel = new QLabel(button);
        iconLabel->setObjectName(QStringLiteral("moduleIconLabel"));
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setScaledContents(false);
        iconLabel->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));
        iconLabel->setFixedSize(kModuleIconBox, kModuleIconBox);
        const auto updateIcon = [iconLabel, iconStem](bool checked) {
            const QPixmap pixmap = loadWorkbenchModulePixmap(iconStem, checked);
            iconLabel->setPixmap(pixmap);
            if (checked) {
                iconLabel->setStyleSheet(QStringLiteral(
                    "QLabel {"
                    "   background-color: %1;"
                    "   border-radius: 8px;"
                    "}").arg(QLatin1String(kModuleCheckedBg)));
            } else {
                iconLabel->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));
            }
        };
        QLabel* textLabel = new QLabel(text, button);
        textLabel->setAlignment(Qt::AlignCenter);
        //textLabel->setWordWrap(true);
        textLabel->setStyleSheet(QStringLiteral("QLabel { color: white; background: transparent; }"));
        textLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        applyWorkbenchFont(textLabel, kFontBody);

        layout->addWidget(iconLabel, 0, Qt::AlignCenter);
        layout->addWidget(textLabel, 1);
        button->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "   border: 1px solid transparent;"
            "   border-radius: 8px;"
            "   background-color: transparent;"
            "}"
            "QPushButton:hover {"
            "   border-color: rgba(24, 144, 255, 0.72);"
            "}"
            "QPushButton:checked {"
            "   border-color: transparent;"
            "   background-color: transparent;"
            "}"));

        connect(button, &QPushButton::toggled, button, [updateIcon](bool checked) {
            updateIcon(checked);
        });
        connect(button, &QPushButton::clicked, this, &CaseWorkSessionPage::onModuleClicked);
        updateIcon(button->isChecked());
        return button;
    };

    mMotorButton = makeModule(QString::fromUtf8(u8"机动车分析"), QStringLiteral("motor"), AnalysisModule::MotorVehicle);
    mSlowButton = makeModule(QString::fromUtf8(u8"慢行交通分析"), QStringLiteral("traffic"), AnalysisModule::SlowTraffic);
    mPublicButton = makeModule(QString::fromUtf8(u8"公交运行分析"), QStringLiteral("bus"), AnalysisModule::PublicTransport);

    auto* group = new QButtonGroup(card);
    group->setExclusive(true);
    group->addButton(mMotorButton);
    group->addButton(mSlowButton);
    group->addButton(mPublicButton);

    syncModuleButtonsToCurrentModule();

    rowLayout->addWidget(mMotorButton, 1);
    rowLayout->addWidget(mSlowButton, 1);
    rowLayout->addWidget(mPublicButton, 1);
    layout->addWidget(row);
    return card;
}

QWidget* CaseWorkSessionPage::createDataCheckPanel()
{
    QFrame* card = new QFrame(mWorkContentWidget);
    card->setObjectName(QStringLiteral("dataCheckCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(QStringLiteral(
        "QFrame#dataCheckCard {"
        "   border-radius: 10px;"
        "   border: 1px solid rgba(41, 101, 180, 0.28);"
        "   background-color: %1;"
        "}").arg(QLatin1String(kDataCheckOuterBg)));

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    layout->addWidget(createSectionTitleRow(QString::fromUtf8(u8"数据支撑集检查"), QStringLiteral("data_support"), card));

    QFrame* innerBlock = new QFrame(card);
    innerBlock->setObjectName(QStringLiteral("dataCheckInnerBlock"));
    innerBlock->setAttribute(Qt::WA_StyledBackground, true);
    innerBlock->setStyleSheet(QStringLiteral(
        "QFrame#dataCheckInnerBlock {"
        "   border-radius: 8px;"
        "   border: none;"
        "   background-color: %1;"
        "}").arg(QLatin1String(kDataCheckInnerBg)));
    auto* innerLayout = new QVBoxLayout(innerBlock);
    innerLayout->setContentsMargins(14, 12, 14, 12);
    innerLayout->setSpacing(10);

    const QStringList rows = mGrpcCheckSummary.isEmpty()
                                 ? QStringList{QString::fromUtf8(u8"正在加载检查数据…")}
                                 : mGrpcCheckSummary.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& rowText : rows) {
        const QStringList parts = rowText.split(QString::fromUtf8(u8"："));
        const QString label = parts.value(0);
        const QString status = parts.value(1, QString::fromUtf8(u8"已加载"));
        const bool ok = status.contains(QString::fromUtf8(u8"已加载"));

        QWidget* row = new QWidget(innerBlock);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);
        QLabel* nameLabel = new QLabel(label + QString::fromUtf8(u8"："), row);
        nameLabel->setStyleSheet(QStringLiteral("QLabel { color: rgba(255, 255, 255, 0.92); }"));
        applyWorkbenchFont(nameLabel, kFontSection);

        QLabel* statusLabel = new QLabel(status, row);
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setMinimumWidth(52);
        statusLabel->setFixedHeight(22);
        statusLabel->setStyleSheet(QStringLiteral(
            "QLabel {"
            "   border-radius: 11px;"
            "   padding: 0 10px;"
            "   background-color: %1;"
            "   color: %2;"
            "}").arg(ok ? QLatin1String(kDataCheckStatusBg) : QStringLiteral("#b36913"),
                     ok ? QLatin1String(kDataCheckStatusText) : QStringLiteral("#ffffff")));
        applyWorkbenchFont(statusLabel, kFontBody);

        rowLayout->addWidget(nameLabel, 1);
        rowLayout->addWidget(statusLabel, 0, Qt::AlignVCenter);
        innerLayout->addWidget(row);
    }

    layout->addWidget(innerBlock);

    if (mModuleSelected && !mGrpcCheckPending && !mGrpcCheckAllPassed) {
        QLabel* hintLabel = new QLabel(QString::fromUtf8(u8"数据支撑集未全部通过，下方流程暂不可运行。"), card);
        hintLabel->setWordWrap(true);
        hintLabel->setStyleSheet(QStringLiteral("QLabel { color: #f4a43a; padding-top: 2px; }"));
        applyWorkbenchFont(hintLabel, kFontCaption);
        layout->addWidget(hintLabel);
    }

    return card;
}

QWidget* CaseWorkSessionPage::createBaseFlowPanel()
{
    QFrame* panel = createCard(mWorkContentWidget);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    layout->addWidget(createSectionTitleRow(QString::fromUtf8(u8"算法流程"), QStringLiteral("flow"), panel));
    for (int i = 0; i < mFlowSteps.size(); ++i) {
        layout->addWidget(createStepCard(i));
    }
    return panel;
}

QWidget* CaseWorkSessionPage::createNormalFlowPanel()
{
    QFrame* panel = createCard(mWorkContentWidget);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    layout->addWidget(createSectionTitleRow(QString::fromUtf8(u8"改造方案流程"), QStringLiteral("flow"), panel));
    layout->addWidget(createSuggestionStepCard());
    layout->addWidget(createEditLogStepCard());
    layout->addWidget(createNormalReviewStepCard());
    return panel;
}

QWidget* CaseWorkSessionPage::createStepCard(int stepIndex)
{
    FlowStep& step = mFlowSteps[stepIndex];
    QFrame* card = createCard(mWorkContentWidget);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(7);

    QWidget* header = new QWidget(card);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(7);

    QLabel* indexLabel = new QLabel(QString::number(stepIndex + 1), header);
    indexLabel->setFixedSize(22, 22);
    indexLabel->setAlignment(Qt::AlignCenter);
    indexLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "   border-radius: 11px;"
        "   background-color: %1;"
        "   color: white;"
        "   font-weight: 700;"
        "}").arg(step.state == StepState::Finished ? QStringLiteral("#20c267") : QStringLiteral("#2f8df7")));
    applyWorkbenchFont(indexLabel, kFontCaption, QFont::Bold);
    headerLayout->addWidget(indexLabel);

    QLabel* titleLabel = new QLabel(step.title, header);
    titleLabel->setStyleSheet(QStringLiteral("QLabel { color: white; }"));
    applyWorkbenchFont(titleLabel, kFontSection);

    headerLayout->addWidget(titleLabel, 1);

    QLabel* stateLabel = new QLabel(stepStateText(step.state), header);
    stateLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-weight: 700; }").arg(stepStateColor(step.state)));
    applyWorkbenchFont(stateLabel, kFontCaption, QFont::Bold);
    headerLayout->addWidget(stateLabel);

    QPushButton* runButton = createRunButton(header);
    runButton->setProperty("stepKey", step.key);
    runButton->setEnabled(canRunStep(step));
    if (step.state == StepState::Running) {
        runButton->setText(QString::fromUtf8(u8"运行中"));
        if (QFile::exists(runningMoviePath())) {
            runButton->setIcon(QIcon(runningMoviePath()));
        }
    }
    connect(runButton, &QPushButton::clicked, this, &CaseWorkSessionPage::onStepRunClicked);
    registerWorkbenchGateButton(runButton);
    headerLayout->addWidget(runButton);

    QPushButton* aiButton = createStepAiAssistantButton(header, step.key);
    headerLayout->addWidget(aiButton);
    layout->addWidget(header);

    appendFinishedAnalysisResultRows(layout, step, card);

    if (shouldAttachSmartDialog(step.key)) {
        attachStepAiChatWidget(layout, step, card);
    }

    return card;
}

QWidget* CaseWorkSessionPage::createSuggestionStepCard()
{
    FlowStep& step = *stepByKey(QStringLiteral("suggestion"));
    QFrame* card = createCard(mWorkContentWidget);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(7);

    QWidget* header = new QWidget(card);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(7);
    QLabel* indexLabel = new QLabel(QStringLiteral("1"), header);
    indexLabel->setFixedSize(22, 22);
    indexLabel->setAlignment(Qt::AlignCenter);
    indexLabel->setStyleSheet(QStringLiteral("QLabel { border-radius: 11px; background-color: %1; color: white; font-weight: 700; }")
                                  .arg(step.state == StepState::Finished ? QStringLiteral("#20c267") : QStringLiteral("#2f8df7")));
    applyWorkbenchFont(indexLabel, kFontCaption, QFont::Bold);
    headerLayout->addWidget(indexLabel);
    QLabel* titleLabel = new QLabel(QString::fromUtf8(u8"方案建议"), header);
    titleLabel->setStyleSheet(QStringLiteral("QLabel { color: white; }"));
    applyWorkbenchFont(titleLabel, kFontSection);
    headerLayout->addWidget(titleLabel, 1);
    QLabel* stateLabel = new QLabel(stepStateText(step.state), header);
    stateLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-weight: 700; }").arg(stepStateColor(step.state)));
    applyWorkbenchFont(stateLabel, kFontCaption, QFont::Bold);
    headerLayout->addWidget(stateLabel);
    QPushButton* runButton = createRunButton(header);
    runButton->setProperty("stepKey", step.key);
    runButton->setEnabled(canRunStep(step));
    if (step.state == StepState::Running) {
        runButton->setText(QString::fromUtf8(u8"运行中"));
        if (QFile::exists(runningMoviePath())) {
            runButton->setIcon(QIcon(runningMoviePath()));
        }
    }
    connect(runButton, &QPushButton::clicked, this, &CaseWorkSessionPage::onStepRunClicked);
    registerWorkbenchGateButton(runButton);
    headerLayout->addWidget(runButton);

    QPushButton* aiButton = createStepAiAssistantButton(header, step.key);
    headerLayout->addWidget(aiButton);
    layout->addWidget(header);

    if (step.suggestionOutputVisible) {
        auto* resultEdit = new QTextBrowser(card);
        resultEdit->setReadOnly(true);
        resultEdit->setOpenLinks(false);
        resultEdit->setOpenExternalLinks(false);
        resultEdit->setMinimumHeight(118);
        resultEdit->setStyleSheet(QStringLiteral(
            "QTextBrowser {"
            "   border: 1px solid rgba(47, 132, 227, 0.38);"
            "   border-radius: 6px;"
            "   background-color: #030a16;"
            "   color: rgba(226,238,255,0.92);"
            "   padding: 8px;"
            "}"));
        resultEdit->setFont(workbenchFont(kFontBody));
        if (step.suggestionContentUrl.trimmed().isEmpty()) {
            resultEdit->setPlainText(step.suggestionRunText);
        } else {
            resultEdit->setHtml(suggestionResultHtml(step.suggestionRunText, step.suggestionContentUrl.trimmed()));
            connect(resultEdit, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
                openWebViewUrlOrWarn(window(), QString::fromUtf8(u8"方案建议"), url.toString());
            });
        }
        layout->addWidget(resultEdit);
    }

    return card;
}

QWidget* CaseWorkSessionPage::createEditLogStepCard()
{
    FlowStep& step = *stepByKey(QStringLiteral("editLog"));
    QFrame* card = createCard(mWorkContentWidget);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(7);

    QWidget* header = new QWidget(card);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(7);

    QLabel* indexLabel = new QLabel(QStringLiteral("2"), header);
    indexLabel->setFixedSize(22, 22);
    indexLabel->setAlignment(Qt::AlignCenter);
    indexLabel->setStyleSheet(QStringLiteral(
        "QLabel { border-radius: 11px; background-color: #2f8df7; color: white; font-weight: 700; }"));
    applyWorkbenchFont(indexLabel, kFontCaption, QFont::Bold);
    headerLayout->addWidget(indexLabel);

    QPushButton* editLogBtn = createTextButton(QString::fromUtf8(u8"修改方案记录"), header);
    connect(editLogBtn, &QPushButton::clicked, this, &CaseWorkSessionPage::onEditLogClicked);
    registerWorkbenchGateButton(editLogBtn);
    headerLayout->addWidget(editLogBtn, 1);

    QPushButton* aiButton = createStepAiAssistantButton(header, step.key);
    headerLayout->addWidget(aiButton);
    layout->addWidget(header);

    if (step.editLogOutputVisible) {
        QTextEdit* resultEdit = new QTextEdit(card);
        resultEdit->setReadOnly(true);
        resultEdit->setPlainText(step.editLogOutputText);
        resultEdit->setMinimumHeight(118);
        resultEdit->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "   border: 1px solid rgba(47, 132, 227, 0.38);"
            "   border-radius: 6px;"
            "   background-color: #030a16;"
            "   color: rgba(226,238,255,0.92);"
            "   padding: 8px;"
            "}"));
        resultEdit->setFont(workbenchFont(kFontBody));
        layout->addWidget(resultEdit);
    }

    return card;
}

QWidget* CaseWorkSessionPage::createNormalReviewStepCard()
{
    FlowStep& step = *stepByKey(QStringLiteral("review"));
    QFrame* card = createCard(mWorkContentWidget);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(7);

    QWidget* header = new QWidget(card);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(7);
    QLabel* indexLabel = new QLabel(QStringLiteral("3"), header);
    indexLabel->setFixedSize(22, 22);
    indexLabel->setAlignment(Qt::AlignCenter);
    indexLabel->setStyleSheet(QStringLiteral("QLabel { border-radius: 11px; background-color: %1; color: white; font-weight: 700; }")
                                  .arg(step.state == StepState::Finished ? QStringLiteral("#20c267") : QStringLiteral("#2f8df7")));
    applyWorkbenchFont(indexLabel, kFontCaption, QFont::Bold);
    headerLayout->addWidget(indexLabel);
    QLabel* titleLabel = new QLabel(QString::fromUtf8(u8"方案评审"), header);
    titleLabel->setStyleSheet(QStringLiteral("QLabel { color: white; }"));
    applyWorkbenchFont(titleLabel, kFontSection);
    headerLayout->addWidget(titleLabel, 1);
    QLabel* stateLabel = new QLabel(stepStateText(step.state), header);
    stateLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-weight: 700; }").arg(stepStateColor(step.state)));
    applyWorkbenchFont(stateLabel, kFontCaption, QFont::Bold);
    headerLayout->addWidget(stateLabel);
    QPushButton* runButton = createRunButton(header);
    runButton->setProperty("stepKey", step.key);
    runButton->setEnabled(canRunStep(step));
    if (step.state == StepState::Running) {
        runButton->setText(QString::fromUtf8(u8"运行中"));
        if (QFile::exists(runningMoviePath())) {
            runButton->setIcon(QIcon(runningMoviePath()));
        }
    }
    connect(runButton, &QPushButton::clicked, this, &CaseWorkSessionPage::onStepRunClicked);
    registerWorkbenchGateButton(runButton);
    headerLayout->addWidget(runButton);

    QPushButton* aiButton = createStepAiAssistantButton(header, step.key);
    headerLayout->addWidget(aiButton);
    layout->addWidget(header);

    appendFinishedAnalysisResultRows(layout, step, card);

    // 暂不展示「成本效益分析」按钮，槽 onCostBenefitClicked 保留以便后续恢复。
    // if (step.state == StepState::Finished) {
    //     QPushButton* costBtn = createTextButton(QString::fromUtf8(u8"成本效益分析"), card);
    //     connect(costBtn, &QPushButton::clicked, this, &CaseWorkSessionPage::onCostBenefitClicked);
    //     registerWorkbenchGateButton(costBtn);
    //     layout->addWidget(costBtn);
    // }

    attachStepAiChatWidget(layout, step, card);

    return card;
}

/*QWidget* CaseWorkSessionPage::createResultRow(const AnalysisResultItem& resultItem)
{
    QWidget* row = new QWidget(mWorkContentWidget);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(30, 0, 0, 0);
    layout->setSpacing(8);

    QPushButton* titleButton = createTextButton(resultItem.name, row);
    titleButton->setProperty("resultName", resultItem.name);
    titleButton->setProperty("resultKey", resultItem.renderLayer);
    connect(titleButton, &QPushButton::clicked, this, &CaseWorkSessionPage::onResultViewClicked);
    layout->addWidget(titleButton, 1);

    QPushButton* dataButton = createTextButton(QString::fromUtf8(u8"数据"), row);
    dataButton->setFixedWidth(48);
    dataButton->setProperty("tableName", resultItem.tableName);
    dataButton->setProperty("resultKey", resultItem.renderLayer);
    connect(dataButton, &QPushButton::clicked, this, &CaseWorkSessionPage::onResultDataClicked);
    layout->addWidget(dataButton);
    return row;
}*/

QWidget* CaseWorkSessionPage::createResultRow(const AnalysisResultItem& resultItem)
{
    // 根行容器（整行圆角边框载体）
    QWidget* rowWidget = new QWidget(mWorkContentWidget);
    rowWidget->setObjectName("ResultRow");

    // 外层布局：控制整行上下内边距，实现行高度
    QHBoxLayout* rootLayout = new QHBoxLayout(rowWidget);
    rootLayout->setContentsMargins(14, 10, 14, 10);
    rootLayout->setSpacing(0);

    // ========== 左侧区域：图标 + 标题文字按钮 ==========
    QWidget* leftContainer = new QWidget();
    leftContainer->setObjectName("leftContainer");
    const QString leftContainerStyle = R"(
        #leftContainer{
            background-color: transparent; 
            border: none; 
        }
        #leftContainer:hover{
            background-color: transparent; 
            border: none; 
        }
    )";
    leftContainer->setStyleSheet(leftContainerStyle);

    QHBoxLayout* leftLayout = new QHBoxLayout(leftContainer);
    leftLayout->setContentsMargins(8, 0, 0, 0);
    leftLayout->setSpacing(10);
    leftLayout->setAlignment(Qt::AlignVCenter);

    // 1. 左侧图标Label
    QLabel* iconLabel = new QLabel();
    iconLabel->setObjectName("iconLabel"); 
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(16, 16);

    // 2. 标题按钮（占满剩余宽度、透明背景）
    const QFont bodyFont = workbenchFont(kFontBody);

    QPushButton* titleBtn = createTextButton(resultItem.name, rowWidget);
    applyAnalysisResultButtonProperties(titleBtn, resultItem);
    titleBtn->setMinimumWidth(0);
    titleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    titleBtn->setStyleSheet(R"(
    QPushButton {
        border: none;
        color: #e5e5e5;     
        background-color: transparent;  
        text-align: left; 
        min-height: 15px;           
    }
    QPushButton:hover {
        background-color: transparent;  
        border: none;
        color: #509af8; 
    }
    QPushButton:pressed {
        background-color: transparent;  
        border: none;
        color: #509af8; 
    }
    )");
    titleBtn->setFont(bodyFont);

    connect(titleBtn, &QPushButton::clicked, this, &CaseWorkSessionPage::onResultViewClicked);

    leftLayout->addWidget(iconLabel);
    leftLayout->addWidget(titleBtn, 1);
    rootLayout->addWidget(leftContainer, 1); // 弹性拉伸，占满剩余宽度

    // ========== 右侧功能按钮==========
    QPushButton* rightBtn = createTextButton(QString::fromUtf8(u8"数据"), rowWidget);
    applyAnalysisResultButtonProperties(rightBtn, resultItem);
    rightBtn->setProperty("tableName", resultItem.tableName);
    rightBtn->setFixedSize(48, 30);
    rightBtn->setStyleSheet(R"(
    QPushButton {
        background-color: #ffffff;
        border-radius: 10px;
        border: none;          
        color: #509af8;    
    }
    QPushButton:hover {
        background-color: #273344; 
        color: #509af8; 
    }
    QPushButton:pressed {
        background-color: #273344; 
        color: #509af8; 
    }
    )");
    rightBtn->setFont(bodyFont);
    connect(rightBtn, &QPushButton::clicked, this, &CaseWorkSessionPage::onResultDataClicked);

    rootLayout->addWidget(rightBtn);

    QString iconNormal = ":/icons/icons/diff.png";
    QString iconHover = ":/icons/icons/flow_data.png";

    const QString rowStyle = QString(R"(
        #ResultRow{
            background-color: #09172a;
            border: 1px solid #e5e5e5;
            border-radius: 10px;
            min-height: 48px;
        }

        #ResultRow:hover{
            background-color: #09172a;
            border-color: #509af8;
        }
        
        #ResultRow #iconLabel{
        background-image:url("%1");
        background-repeat:no-repeat;
        background-position:center;
        background-color: transparent;
        }

        #ResultRow:hover #iconLabel{
            background-image:url("%2");
        }
        )").arg(iconNormal).arg(iconNormal);

    rowWidget->setStyleSheet(rowStyle);

    return rowWidget;
}

QWidget* CaseWorkSessionPage::createBottomReportPanel()
{
    QWidget* panel = new QWidget(mWorkContentWidget);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QPushButton* reportBtn = createTextButton(QString::fromUtf8(u8"生成分析报告"), panel);
    reportBtn->setFixedSize(160, 30);
    reportBtn->setIcon(QIcon(QStringLiteral(":/icons/icons/report.png")));
    reportBtn->setIconSize(QSize(14, 14));
    // 设置按钮样式
    reportBtn->setStyleSheet(R"(
    QPushButton {
        background-color: #0683f5;
        border-radius: 15px;
        border: none;          /* 去除默认边框 */
        color: white;          /* 文字白色更适配蓝色背景 */
    }
    QPushButton:hover {
        background-color: #0572d9; /* hover加深蓝色，交互更好 */
    }
    QPushButton:pressed {
        background-color: #0462bc; /* 按下更深 */
    }
    )");

    applyWorkbenchFont(reportBtn, kFontBody);

    connect(reportBtn, &QPushButton::clicked, this, &CaseWorkSessionPage::onGenerateAnalysisReportClicked);
    registerWorkbenchGateButton(reportBtn);
    layout->addWidget(reportBtn, 0, Qt::AlignHCenter);

    QPushButton* deriveBtn = createTextButton(QString::fromUtf8(u8"新建改造方案"), panel);
    deriveBtn->setFixedSize(160, 30);
    deriveBtn->setIcon(QIcon(QStringLiteral(":/icons/icons/edit_case.png")));
    deriveBtn->setIconSize(QSize(14, 14));
    // 样式设置
    deriveBtn->setStyleSheet(R"(
    QPushButton {
        background-color: #f8f9fe;
        border-radius: 15px;
        border: none;          
        color: #000000;  
    }
    QPushButton:hover {
        background-color: #e9edfc; 
    }
    QPushButton:pressed {
        background-color: #dde4f8; 
    }
    )");
    applyWorkbenchFont(deriveBtn, kFontBody);

    connect(deriveBtn, &QPushButton::clicked, this, &CaseWorkSessionPage::onDeriveCaseClicked);
    layout->addWidget(deriveBtn, 0, Qt::AlignHCenter);
    return panel;
}

QWidget* CaseWorkSessionPage::createFinalOutputPanel()
{
    QFrame* card = createCard(mWorkContentWidget);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    QWidget* titleRow = new QWidget(card);
    auto* titleLayout = new QHBoxLayout(titleRow);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(6);

    QLabel* icon = new QLabel(QStringLiteral("OUT"), titleRow);
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedWidth(28);
    icon->setStyleSheet(QStringLiteral("QLabel { color: #8bbfff; font-weight: 700; }"));
    applyWorkbenchFont(icon, kFontCaption, QFont::Bold);
    titleLayout->addWidget(icon);

    QLabel* title = createSectionTitle(QString::fromUtf8(u8"最终输出"), titleRow);
    titleLayout->addWidget(title, 1);
    layout->addWidget(titleRow);

    layout->addWidget(createFinalOutputRow(QStringLiteral(""), QStringLiteral(":/icons/icons/data_report.png"),
                                           analysisReportTitle(),
                                           QString::fromUtf8(u8"完整的分析报告文档"),
                                           true,
                                           SLOT(onReportViewClicked()),
                                           SLOT(onAnalysisReportDownloadClicked())));
    layout->addWidget(createFinalOutputRow(QStringLiteral(""), QStringLiteral(":/icons/icons/osm_data.png"),
                                           QString::fromUtf8(u8"OSM地图数据"),
                                           QString::fromUtf8(u8"包括路网拓扑结构数据"),
                                           false,
                                           nullptr,
                                           SLOT(onOsmDownloadClicked())));
    layout->addWidget(createFinalOutputRow(QStringLiteral(""), QStringLiteral(":/icons/icons/flow_data.png"),
                                           QString::fromUtf8(u8"仿真流量数据"),
                                           QString::fromUtf8(u8"路网仿真流量分布数据"),
                                           false,
                                           nullptr,
                                           SLOT(onSimulateDownloadClicked())));
    return card;
}

QWidget* CaseWorkSessionPage::createFinalOutputRow(const QString& iconText,
                                             const QString& iconPath,
                                             const QString& title,
                                             const QString& desc,
                                             bool hasViewButton,
                                             const char* viewSlot,
                                             const char* downloadSlot)
{
    QFrame* row = createCard(mWorkContentWidget);
    row->setStyleSheet(QStringLiteral(
        "QFrame#workbenchCard {"
        "   border-radius: 7px;"
        "   border: 1px solid rgba(104, 151, 210, 0.56);"
        "   background-color: rgba(8, 25, 50, 0.82);"
        "}"));

    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    QLabel* icon = new QLabel(iconText, row);
    icon->setStyleSheet(QString(R"(
    QLabel{
        background-image:url(%1);
        background-repeat:no-repeat;
        background-position:center;
    })").arg(iconPath));

    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(26, 26);
    /*icon->setStyleSheet(QStringLiteral(
        "QLabel {"
        "   border-radius: 6px;"
        "   border: 1px solid rgba(75, 151, 255, 0.50);"
        "   color: #4f9bff;"
        "   font-size: 12px;"
        "   font-weight: 700;"
        "}"));*/
    layout->addWidget(icon);

    QWidget* textWrap = new QWidget(row);
    auto* textLayout = new QVBoxLayout(textWrap);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);
    QLabel* titleLabel = new QLabel(title, textWrap);
    titleLabel->setStyleSheet(QStringLiteral("QLabel { color: white; }"));
    applyWorkbenchFont(titleLabel, kFontSection);

    QLabel* descLabel = new QLabel(desc, textWrap);
    descLabel->setStyleSheet(QStringLiteral("QLabel { color: rgba(214, 230, 255, 0.62); }"));
    applyWorkbenchFont(descLabel, kFontCaption);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(descLabel);
    layout->addWidget(textWrap, 1);

    if (hasViewButton && viewSlot) {
        QPushButton* viewBtn = createTextButton(QString::fromUtf8(u8"查看"), row);
        viewBtn->setFixedSize(48, 30);
        viewBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #0683f5;
            border-radius: 10px;
            border: none;          
            color: white;         
        }
        QPushButton:hover {
            background-color: #0572d9; 
        }
        QPushButton:pressed {
            background-color: #0462bc; 
        }
        )");
        applyWorkbenchFont(viewBtn, kFontBody);

        connect(viewBtn, SIGNAL(clicked()), this, viewSlot);
        registerWorkbenchGateButton(viewBtn);
        layout->addWidget(viewBtn);
    }

    QPushButton* downloadBtn = createTextButton(QString::fromUtf8(u8"下载"), row);
    downloadBtn->setFixedSize(48, 30);
    downloadBtn->setStyleSheet(R"(
    QPushButton {
        background-color: #f8f9fe;
        border-radius: 10px;
        border: none;          
        color: #509af8;        
    }
    QPushButton:hover {
        background-color: #e9edfc; 
    }
    QPushButton:pressed {
        background-color: #dde4f8; 
    }
    )");
    applyWorkbenchFont(downloadBtn, kFontBody);

    connect(downloadBtn, SIGNAL(clicked()), this, downloadSlot);
    registerWorkbenchGateButton(downloadBtn);
    layout->addWidget(downloadBtn);
    return row;
}

QWidget* CaseWorkSessionPage::createNormalReportPanel()
{
    QWidget* panel = new QWidget(mWorkContentWidget);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QPushButton* reportBtn = createTextButton(QString::fromUtf8(u8"生成改造方案报告"), panel);
    reportBtn->setFixedSize(200, 30);
    reportBtn->setIcon(QIcon(QStringLiteral(":/icons/icons/report.png")));
    reportBtn->setIconSize(QSize(14, 14));
    //reportBtn->setMinimumHeight(34);
    reportBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 15px;"
        "   background-color: #0683f5;"
        "   color: white;"
        "}"
        "QPushButton:hover { background-color: #49a2ff; }"));
    applyWorkbenchFont(reportBtn, kFontBody, QFont::Bold);
    connect(reportBtn, &QPushButton::clicked, this, &CaseWorkSessionPage::onRenovationReportClicked);
    registerWorkbenchGateButton(reportBtn);
    layout->addWidget(reportBtn, 0, Qt::AlignHCenter);
    return panel;
}

void CaseWorkSessionPage::resetStateForCurrentCase()
{
    // 会话页模块在构造后固定，不清 mCurrentModule / mModuleSelected
    mModuleSelected = true;
    mGrpcCheckSummary.clear();
    mGrpcCheckPending = false;
    mGrpcCheckAllPassed = false;
    mActiveStepKey.clear();
    mNormalReviewCompleted = false;
    m_activeSuggestionType2.clear();
    ++mAlgorithmRunRequestSeq;
    mAlgorithmRunLoaded = false;
    mAlgorithmRunPassed = false;
    mAlgorithmStatusPendingApply = false;
    m_sseBusy = false;
    m_httpBusy = false;
    m_workbenchGateButtons.clear();
    mFlowSteps.clear();
}

void CaseWorkSessionPage::rebuildStepsForCurrentModule()
{
    mFlowSteps.clear();
    if (mCurrentCase.isBase) {
        FlowStep network;
        network.key = QStringLiteral("network");
        network.title = QString::fromUtf8(u8"路网现状分析");
        network.type2 = QStringLiteral("network");
        if (mCurrentModule == AnalysisModule::SlowTraffic) {
            network.results = {
                makeNetworkAnalysisResult(QString::fromUtf8(u8"机动车流量"),
                                          QStringLiteral("traffic_flow"),
                                          QStringLiteral("traffic_flow_table"),
                                          true,
                                          AnalysisModule::MotorVehicle),
                makeNetworkAnalysisResult(QString::fromUtf8(u8"机动车饱和度"),
                                          QStringLiteral("saturation"),
                                          QStringLiteral("saturation_table"),
                                          true,
                                          AnalysisModule::MotorVehicle),
                makeNetworkAnalysisResult(QString::fromUtf8(u8"慢行流量"),
                                          QStringLiteral("traffic_flow"),
                                          QStringLiteral("traffic_flow_table")),
                makeNetworkAnalysisResult(QString::fromUtf8(u8"慢行饱和度"),
                                          QStringLiteral("saturation"),
                                          QStringLiteral("saturation_table"))
            };
        } else {
            network.results = {
                {QString::fromUtf8(u8"路网流量分布"), QStringLiteral("traffic_flow"), QStringLiteral("traffic_flow_table")},
                {QString::fromUtf8(u8"交通饱和度"), QStringLiteral("saturation"), QStringLiteral("saturation_table")}
            };
        }

        FlowStep traffic;
        traffic.key = QStringLiteral("traffic");
        traffic.title = QString::fromUtf8(u8"交通问题诊断");
        traffic.type2 = QStringLiteral("traffic");
        traffic.state = StepState::Locked;
        {
            auto makeDiag = [](const QString& name,
                               const QString& layer,
                               const QString& table,
                               bool visible) {
                AnalysisResultItem item;
                item.name = name;
                item.renderLayer = layer;
                item.tableName = table;
                item.uiVisible = visible;
                return item;
            };
            traffic.results = {
                makeDiag(QString::fromUtf8(u8"路网流量分析"),
                         QStringLiteral("traffic_flow_diag"),
                         QStringLiteral("traffic_flow_table"),
                         true),
                makeDiag(QString::fromUtf8(u8"预设流量与现状差值"),
                         QStringLiteral("preset_flow_delta"),
                         QStringLiteral("preset_flow_delta_table"),
                         false),
                makeDiag(QString::fromUtf8(u8"预设饱和度与现状差值"),
                         QStringLiteral("preset_sat_delta"),
                         QStringLiteral("preset_sat_delta_table"),
                         false),
                makeDiag(QString::fromUtf8(u8"交通饱和度"),
                         QStringLiteral("saturation_diag"),
                         QStringLiteral("saturation_table"),
                         true),
                makeDiag(QString::fromUtf8(u8"OD分布"),
                         QStringLiteral("od_distribution_diag"),
                         QStringLiteral("od_distribution_table"),
                         false),
                makeDiag(QString::fromUtf8(u8"饱和OD差值"),
                         QStringLiteral("sat_od_delta"),
                         QStringLiteral("sat_od_delta_table"),
                         false)
            };
        }
        mFlowSteps = {network, traffic};
        return;
    }

    FlowStep suggestion;
    suggestion.key = QStringLiteral("suggestion");
    suggestion.title = QString::fromUtf8(u8"方案建议");
    suggestion.type2 = QStringLiteral("change");
    suggestion.state = StepState::Pending;

    FlowStep editLog;
    editLog.key = QStringLiteral("editLog");
    editLog.title = QString::fromUtf8(u8"修改方案记录");

    FlowStep review;
    review.key = QStringLiteral("review");
    review.title = QString::fromUtf8(u8"方案评审");
    review.type2 = QStringLiteral("review");
    review.state = StepState::Pending;
    review.results = {
        {QString::fromUtf8(u8"路网流量分布"), QStringLiteral("traffic_flow"), QStringLiteral("traffic_flow_table")},
        {QString::fromUtf8(u8"交通饱和度"), QStringLiteral("saturation"), QStringLiteral("saturation_table")}
    };
    mFlowSteps = {suggestion, editLog, review};
}

CaseWorkSessionPage::FlowStep* CaseWorkSessionPage::stepByKey(const QString& key)
{
    for (FlowStep& step : mFlowSteps) {
        if (step.key == key) {
            return &step;
        }
    }
    return nullptr;
}

const CaseWorkSessionPage::FlowStep* CaseWorkSessionPage::stepByKey(const QString& key) const
{
    for (const FlowStep& step : mFlowSteps) {
        if (step.key == key) {
            return &step;
        }
    }
    return nullptr;
}

void CaseWorkSessionPage::appendStepLog(const QString& stepKey, const QString& line)
{
    FlowStep* step = stepByKey(resolveSmartDialogKey(stepKey));
    if (!step || line.isEmpty()) {
        return;
    }

    AIChatMessage logMessage;
    logMessage.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    logMessage.role = AIMessageRole::Log;
    logMessage.content = line;
    logMessage.createdAt = QDateTime::currentDateTime();
    step->chatTimeline.append(logMessage);

    if (step->aiChatWidget) {
        step->aiChatWidget->appendTimelineMessage(logMessage, false);
    }
}

void CaseWorkSessionPage::appendActiveGrpcEvent(const GrpcStreamEventJson& ev)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    const QString title = ev.j_data().j_title();
    const QString msg = ev.j_message();
    const int pct = ev.j_data().j_percent();
    QString line = QStringLiteral("[%1] ").arg(ts);
    if (!msg.isEmpty()) {
        line += msg;
    }
    if (!title.isEmpty()) {
        if (!msg.isEmpty()) {
            line += QStringLiteral("：");
        }
        line += title;
    }
    if (pct > 0) {
        line += QStringLiteral("  %1%").arg(pct);
    }
    appendStepLog(mActiveStepKey, line);
}

QString CaseWorkSessionPage::moduleTitle(AnalysisModule module) const
{
    switch (module) {
    case AnalysisModule::SlowTraffic:
        return QString::fromUtf8(u8"慢行交通分析");
    case AnalysisModule::PublicTransport:
        return QString::fromUtf8(u8"公交运行分析");
    case AnalysisModule::MotorVehicle:
    default:
        return QString::fromUtf8(u8"机动车分析");
    }
}

QString CaseWorkSessionPage::analysisReportTitle() const
{
    switch (mCurrentModule) {
    case AnalysisModule::SlowTraffic:
        return QString::fromUtf8(u8"慢行交通分析报告");
    case AnalysisModule::PublicTransport:
        return QString::fromUtf8(u8"公交车分析报告");
    case AnalysisModule::MotorVehicle:
    default:
        return QString::fromUtf8(u8"机动车分析报告");
    }
}

QString CaseWorkSessionPage::grpcTransportType() const
{
    if (!mModuleSelected) {
        return QStringLiteral("motor");
    }
    switch (mCurrentModule) {
    case AnalysisModule::SlowTraffic:
        return QStringLiteral("slow");
    case AnalysisModule::PublicTransport:
        return QStringLiteral("bus");
    case AnalysisModule::MotorVehicle:
    default:
        return QStringLiteral("motor");
    }
}

QString CaseWorkSessionPage::authToken() const
{
    const auto login = UIG().loginInfo();
    return login.isNull() ? QString() : login->j_token().trimmed();
}

QString CaseWorkSessionPage::stepStateText(StepState state) const
{
    switch (state) {
    case StepState::Locked:
        return QString::fromUtf8(u8"待执行");
    case StepState::Running:
        return QString::fromUtf8(u8"运行中");
    case StepState::Finished:
        return QString::fromUtf8(u8"已完成");
    case StepState::Failed:
        return QString::fromUtf8(u8"失败");
    case StepState::Pending:
    default:
        return QString::fromUtf8(u8"等待执行");
    }
}

QString CaseWorkSessionPage::stepStateColor(StepState state) const
{
    switch (state) {
    case StepState::Running:
        return QStringLiteral("#4fb2ff");
    case StepState::Finished:
        return QStringLiteral("#27d17f");
    case StepState::Failed:
        return QStringLiteral("#ff6d6d");
    case StepState::Locked:
    case StepState::Pending:
    default:
        return QStringLiteral("#f4a43a");
    }
}

QString CaseWorkSessionPage::runningMoviePath() const
{
    return QString::fromUtf8(kRunningGifPath);
}

QString CaseWorkSessionPage::translateCheckField(const QString& key) const
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("way"), QString::fromUtf8(u8"路网数据")},
        {QStringLiteral("point"), QString::fromUtf8(u8"节点数据")},
        {QStringLiteral("community"), QString::fromUtf8(u8"小区数据")},
        {QStringLiteral("od"), QString::fromUtf8(u8"OD数据")}
    };
    return map.value(key, key);
}

void CaseWorkSessionPage::refreshGrpcCheck()
{
    const QString token = authToken();
    if (token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        mGrpcCheckPending = false;
        mGrpcCheckAllPassed = false;
        mGrpcCheckSummary = QString::fromUtf8(u8"缺少登录信息、项目或方案，无法检查数据支撑集。");
        if (!m_sseBusy) {
            refreshWorkPage();
        }
        return;
    }

    mGrpcCheckPending = true;
    mGrpcCheckAllPassed = false;
    mGrpcCheckSummary.clear();
    if (!m_sseBusy) {
        refreshWorkPage();
    }
    HttpApis::grpcCheck(token,
                        mProjectId,
                        mCurrentCase.id,
                        grpcTransportType(),
                        [this](bool suc, const QSharedPointer<GrpcCheckResultJson>& res, const QString& errMsg) {
                            applyGrpcCheckSummary(res.isNull() ? QJsonObject() : res->j_data(), suc, errMsg);
                        });
}

void CaseWorkSessionPage::applyGrpcCheckSummary(const QJsonObject& dataObj, bool httpOk, const QString& errMsg)
{
    mGrpcCheckPending = false;

    if (!httpOk) {
        mGrpcCheckAllPassed = false;
        mGrpcCheckSummary = QString::fromUtf8(u8"检查失败：%1").arg(errMsg.isEmpty() ? QString::fromUtf8(u8"未知错误") : errMsg);
    } else {
        QStringList lines;
        for (auto it = dataObj.constBegin(); it != dataObj.constEnd(); ++it) {
            const QString label = translateCheckField(it.key());
            const bool loaded = it.value().toInt(0) == 1;
            lines.append(QString::fromUtf8(u8"%1：%2")
                             .arg(label, loaded ? QString::fromUtf8(u8"已加载") : QString::fromUtf8(u8"数据错误")));
        }
        mGrpcCheckSummary = lines.isEmpty() ? QString::fromUtf8(u8"接口未返回明细字段。") : lines.join(QLatin1Char('\n'));
        mGrpcCheckAllPassed = isGrpcCheckDataAllPassed(dataObj);
    }

    if (!m_sseBusy) {
        refreshWorkPage();
    }
}

bool CaseWorkSessionPage::isGrpcCheckDataAllPassed(const QJsonObject& dataObj)
{
    if (dataObj.isEmpty()) {
        return false;
    }

    for (auto it = dataObj.constBegin(); it != dataObj.constEnd(); ++it) {
        if (it.value().toInt(0) != 1) {
            return false;
        }
    }
    return true;
}

void CaseWorkSessionPage::loadAlgorithmRunStatus()
{
    ++mAlgorithmRunRequestSeq;
    const int requestSeq = mAlgorithmRunRequestSeq;
    const int caseId = mCurrentCase.id;

    mAlgorithmRunLoaded = false;
    mAlgorithmRunPassed = false;
    mAlgorithmStatusPendingApply = false;

    const QString token = authToken();
    if (token.isEmpty() || mProjectId <= 0 || caseId <= 0) {
        mAlgorithmRunLoaded = true;
        return;
    }

    QPointer<CaseWorkSessionPage> self(this);
    HttpApis::getCaseDetail(token,
                            caseId,
                            [self, requestSeq, caseId](bool suc,
                                                       const QSharedPointer<CaseDetailJson>& res,
                                                       const QString&) {
                                if (!self || requestSeq != self->mAlgorithmRunRequestSeq
                                    || caseId != self->mCurrentCase.id) {
                                    return;
                                }
                                self->applyAlgorithmRunStatus(res.isNull() ? QJsonObject()
                                                                            : res->jsonObject(),
                                                              suc);
                            });
}

void CaseWorkSessionPage::applyAlgorithmRunStatus(const QJsonObject& dataObj, bool httpOk)
{
    mAlgorithmRunLoaded = true;
    mAlgorithmRunPassed = httpOk && isAlgorithmRunDataAllPassed(dataObj);

    if (!mAlgorithmRunPassed) {
        mAlgorithmStatusPendingApply = false;
        return;
    }

    if (isAnyAlgorithmRunStepRunning()) {
        mAlgorithmStatusPendingApply = true;
        return;
    }

    applyStoredAlgorithmRunStatus();
}

void CaseWorkSessionPage::applyStoredAlgorithmRunStatus()
{
    mAlgorithmStatusPendingApply = false;
    if (!mAlgorithmRunPassed || isAnyAlgorithmRunStepRunning()) {
        return;
    }

    for (const QString& key : algorithmRunStepKeys()) {
        if (FlowStep* step = stepByKey(key)) {
            step->state = StepState::Finished;
        }
    }
    refreshWorkPage();
}

bool CaseWorkSessionPage::isAlgorithmRunDataAllPassed(const QJsonObject& dataObj) const
{
    if (dataObj.isEmpty()) {
        return false;
    }

    const QString prefix = grpcTransportType();
    const int alg1 = dataObj.value(prefix + QStringLiteral("_alg1")).toInt(0);
    const int alg2 = dataObj.value(prefix + QStringLiteral("_alg2")).toInt(0);
    return alg1 == 1 && alg2 == 1;
}

bool CaseWorkSessionPage::isAnyAlgorithmRunStepRunning() const
{
    for (const QString& key : algorithmRunStepKeys()) {
        const FlowStep* step = stepByKey(key);
        if (step && step->state == StepState::Running) {
            return true;
        }
    }
    return false;
}

QStringList CaseWorkSessionPage::algorithmRunStepKeys() const
{
    if (mCurrentCase.isBase) {
        return {QStringLiteral("network"), QStringLiteral("traffic")};
    }
    return {QStringLiteral("suggestion"), QStringLiteral("review")};
}

bool CaseWorkSessionPage::canRunAlgorithms() const
{
    return mModuleSelected && !mGrpcCheckPending && mGrpcCheckAllPassed;
}

bool CaseWorkSessionPage::isStepStateRunnable(const FlowStep& step) const
{
    return step.state == StepState::Pending || step.state == StepState::Failed
           || step.state == StepState::Finished;
}

bool CaseWorkSessionPage::canRunStep(const FlowStep& step) const
{
    return canRunAlgorithms() && isStepStateRunnable(step) && !m_sseBusy && !m_httpBusy;
}

void CaseWorkSessionPage::appendFinishedAnalysisResultRows(QVBoxLayout* layout, const FlowStep& step, QWidget* card)
{
    if (!layout || step.state != StepState::Finished || step.key == QLatin1String("traffic")) {
        return;
    }

    bool hasVisibleResult = false;
    for (const AnalysisResultItem& result : step.results) {
        if (result.uiVisible) {
            hasVisibleResult = true;
            break;
        }
    }
    if (!hasVisibleResult) {
        return;
    }

    QLabel* resultTitle = new QLabel(QString::fromUtf8(u8"分析结果:"), card);
    resultTitle->setStyleSheet(QStringLiteral("QLabel { color: rgba(226,238,255,0.82); }"));
    applyWorkbenchFont(resultTitle, kFontBody);
    layout->addWidget(resultTitle);
    for (const AnalysisResultItem& result : step.results) {
        if (!result.uiVisible) {
            continue;
        }
        layout->addWidget(createResultRow(result));
    }
}

void CaseWorkSessionPage::showGrpcCheckRunBlockedMessage()
{
    if (mGrpcCheckPending) {
        TnUi::showWarning(QString::fromUtf8(u8"数据支撑集检查"),
                          QString::fromUtf8(u8"数据支撑集检查进行中，请稍候。"));
        return;
    }

    TnUi::showWarning(QString::fromUtf8(u8"数据支撑集检查"),
                      QString::fromUtf8(u8"数据支撑集检查未全部通过，无法运行算法。请完善数据后重试。"));
}

void CaseWorkSessionPage::registerWorkbenchGateButton(QPushButton* button)
{
    if (!button) {
        return;
    }
    m_workbenchGateButtons.append(button);
    updateWorkbenchGateButtons();
}

void CaseWorkSessionPage::updateWorkbenchGateButtons()
{
    const bool blocked = m_sseBusy || m_httpBusy;
    auto applyGate = [blocked](QPushButton* button) {
        if (!button) {
            return;
        }
        if (blocked) {
            if (!button->property("gateWasEnabled").isValid()) {
                button->setProperty("gateWasEnabled", button->isEnabled());
            }
            button->setEnabled(false);
        } else if (button->property("gateWasEnabled").isValid()) {
            button->setEnabled(button->property("gateWasEnabled").toBool());
            button->setProperty("gateWasEnabled", QVariant());
        }
    };

    for (QPushButton* button : m_workbenchGateButtons) {
        applyGate(button);
    }

    // 分析模块切换需始终可点，便于切到其它独立会话；busy 只门控运行/下载等
    auto keepModuleSwitchEnabled = [](QPushButton* button) {
        if (!button) {
            return;
        }
        button->setProperty("gateWasEnabled", QVariant());
        button->setEnabled(true);
    };
    keepModuleSwitchEnabled(mMotorButton);
    keepModuleSwitchEnabled(mSlowButton);
    keepModuleSwitchEnabled(mPublicButton);
}

void CaseWorkSessionPage::setHttpBusy(bool busy)
{
    if (m_httpBusy == busy) {
        return;
    }
    m_httpBusy = busy;
    updateWorkbenchGateButtons();
}

void CaseWorkSessionPage::setSseBusy(bool busy)
{
    if (m_sseBusy == busy) {
        return;
    }
    m_sseBusy = busy;
    updateWorkbenchGateButtons();
}

void CaseWorkSessionPage::downloadCaseFile(const QSharedPointer<DownLoadCaseJson>& response, const QString& failureTitle)
{
    if (response.isNull()) {
        setHttpBusy(false);
        return;
    }

    const QString downloadUrl = response->resolvedDownloadUrl();
    const QString fileName = response->resolvedFileName();
    if (downloadUrl.isEmpty()) {
        setHttpBusy(false);
        TnUi::showWarning(failureTitle, QString::fromUtf8(u8"接口未返回下载地址。"));
        return;
    }

    auto* downloader = new FileDownloader(this);
    QPointer<CaseWorkSessionPage> self(this);
    connect(downloader, &FileDownloader::finished, downloader,
        [self, downloader, failureTitle](bool success, const QString& filePath, const QString& errMsg) {
            if (self) {
                self->setHttpBusy(false);
            }
            if (success) {
                TnUi::showInformation(failureTitle,
                    QString::fromUtf8(u8"文件已保存至：\n%1").arg(filePath));
            } else {
                const QString message = errMsg.trimmed().isEmpty()
                    ? QString::fromUtf8(u8"下载失败")
                    : errMsg.trimmed();
                TnUi::showWarning(failureTitle, message);
            }
            downloader->deleteLater();
        });

    downloader->download(downloadUrl, QString(), fileName, window());
}

void CaseWorkSessionPage::downloadFromUrl(const QString& url, const QString& failureTitle)
{
    const QString downloadUrl = url.trimmed();
    if (downloadUrl.isEmpty()) {
        TnUi::showWarning(failureTitle, QString::fromUtf8(u8"下载地址无效。"));
        return;
    }
    if (m_httpBusy) {
        return;
    }

    setHttpBusy(true);
    const QString suggestedName = QFileInfo(QUrl(downloadUrl).path()).fileName();

    auto* downloader = new FileDownloader(this);
    QPointer<CaseWorkSessionPage> self(this);
    connect(downloader, &FileDownloader::finished, downloader,
            [self, downloader, failureTitle](bool success, const QString& filePath, const QString& errMsg) {
                if (self) {
                    self->setHttpBusy(false);
                }
                if (success) {
                    TnUi::showInformation(failureTitle,
                                            QString::fromUtf8(u8"文件已保存至：\n%1").arg(filePath));
                } else {
                    const QString message = errMsg.trimmed().isEmpty()
                                                ? QString::fromUtf8(u8"下载失败")
                                                : errMsg.trimmed();
                    TnUi::showWarning(failureTitle, message);
                }
                downloader->deleteLater();
            });

    downloader->download(downloadUrl, QString(), suggestedName, window());
}

void CaseWorkSessionPage::ensureStreamClient()
{
    if (!m_grpcStream) {
        m_grpcStream = HttpApis::createGrpcStream(this);
        connect(m_grpcStream, &GrpcStreamClient::progress, this, &CaseWorkSessionPage::onStreamProgress);
        connect(m_grpcStream, &GrpcStreamClient::finished, this, &CaseWorkSessionPage::onStreamFinished);
    }
}

void CaseWorkSessionPage::ensureAiClient()
{
    if (!m_grpcAiStream) {
        m_grpcAiStream = HttpApis::createGrpcAiStream(this);
        connect(m_grpcAiStream, &GrpcAiStreamClient::progress, this, &CaseWorkSessionPage::onStreamProgress);
        connect(m_grpcAiStream, &GrpcAiStreamClient::finished, this, &CaseWorkSessionPage::onAiFinished);
    }
}

void CaseWorkSessionPage::ensureTrafficMicroStreamClient()
{
    if (!m_trafficMicroStream) {
        m_trafficMicroStream = HttpApis::createGrpcStream(this);
        connect(m_trafficMicroStream, &GrpcStreamClient::progress, this,
                &CaseWorkSessionPage::onTrafficMicroStreamProgress);
        connect(m_trafficMicroStream, &GrpcStreamClient::finished, this,
                &CaseWorkSessionPage::onTrafficMicroStreamFinished);
    }
}

void CaseWorkSessionPage::appendTrafficMicroGrpcEvent(const GrpcStreamEventJson& ev)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    const QString title = ev.j_data().j_title();
    const QString msg = ev.j_message();
    const int pct = ev.j_data().j_percent();
    QString line = QStringLiteral("[%1] ").arg(ts);
    if (!msg.isEmpty()) {
        line += msg;
    }
    if (!title.isEmpty()) {
        if (!msg.isEmpty()) {
            line += QStringLiteral("：");
        }
        line += title;
    }
    if (pct > 0) {
        line += QStringLiteral("  %1%").arg(pct);
    }
    appendStepLog(smartDialogHostKey(), line);
}

void CaseWorkSessionPage::onTrafficMicroStreamProgress(const GrpcStreamEventJson& ev)
{
    appendTrafficMicroGrpcEvent(ev);
}

void CaseWorkSessionPage::onTrafficMicroStreamFinished(bool success, const QString& errMsg, const QJsonObject& result)
{
    Q_UNUSED(result);
    const QString hostKey = smartDialogHostKey();
    if (!success) {
        appendStepLog(hostKey,
                      QStringLiteral("[%1] %2")
                          .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                               errMsg.isEmpty() ? QString::fromUtf8(u8"中微观诊断失败") : errMsg));
        return;
    }

    appendStepLog(hostKey,
                  QStringLiteral("[%1] %2")
                      .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                           QString::fromUtf8(u8"中微观诊断完成")));
}

void CaseWorkSessionPage::startGrpcStreamForStep(const QString& stepKey, const QString& jsonPayload, const QString& ids)
{
    FlowStep* step = stepByKey(stepKey);
    const QString token = authToken();
    if (!step || token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"算法流"),
                          QString::fromUtf8(u8"登录状态或方案信息无效。"));
        return;
    }
    if (!canRunStep(*step)) {
        showGrpcCheckRunBlockedMessage();
        return;
    }

    ensureStreamClient();
    mActiveStepKey = stepKey;
    step->state = StepState::Running;
    ensureSmartDialogShownForRun(stepKey);
    refreshWorkPage();
    setSseBusy(true);
    appendStepLog(stepKey, QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                                       QString::fromUtf8(u8"启动运行：%1").arg(step->title)));

    m_grpcStream->start(mProjectId,
                        mCurrentCase.id,
                        mCurrentCase.isBase,
                        grpcTransportType(),
                        step->type2,
                        token,
                        ids,
                        jsonPayload);
}

void CaseWorkSessionPage::startGrpcRunAlgForSuggestion()
{
    FlowStep* step = stepByKey(QStringLiteral("suggestion"));
    const QString token = authToken();
    if (!step || token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"方案建议"),
                          QString::fromUtf8(u8"登录状态或方案信息无效。"));
        return;
    }
    if (!canRunStep(*step) || m_httpBusy) {
        showGrpcCheckRunBlockedMessage();
        return;
    }

    mActiveStepKey = step->key;
    step->state = StepState::Running;
    step->suggestionOutputVisible = true;
    step->suggestionRunText.clear();
    step->suggestionContentUrl.clear();
    refreshWorkPage();
    setHttpBusy(true);
    appendStepLog(step->key, QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                                         QString::fromUtf8(u8"启动运行：%1").arg(step->title)));

    QPointer<CaseWorkSessionPage> self(this);
    HttpApis::grpcRunAlg(token,
                         mProjectId,
                         mCurrentCase.id,
                         grpcTransportType(),
                         QStringLiteral("change"),
                         QString(),
                         [self](bool suc,
                                const QSharedPointer<GrpcRunAlgResultJson>& res,
                                const QString& errMsg) {
                             if (!self) {
                                 return;
                             }
                             self->setHttpBusy(false);
                             FlowStep* currentStep = self->stepByKey(QStringLiteral("suggestion"));
                             if (!currentStep) {
                                 return;
                             }

                             if (!suc || res.isNull()) {
                                 const QString message = errMsg.trimmed().isEmpty()
                                                             ? QString::fromUtf8(u8"运行失败")
                                                             : errMsg.trimmed();
                                 currentStep->suggestionRunText = message;
                                 currentStep->suggestionContentUrl.clear();
                                 self->appendStepLog(currentStep->key,
                                     QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                                                   message));
                                 currentStep->state = StepState::Pending;
                                 self->mAlgorithmStatusPendingApply = false;
                                 TnUi::showWarning(QString::fromUtf8(u8"方案建议"), message);
                                 self->refreshWorkPage();
                                 return;
                             }

                             QString content = runAlgDisplayText(*res);
                             if (content.isEmpty()) {
                                 content = QString::fromUtf8(u8"接口未返回内容。");
                             }
                             currentStep->suggestionRunText = content;
                             currentStep->suggestionContentUrl = runAlgContentUrl(*res);
                             currentStep->state = StepState::Finished;
                             self->appendStepLog(currentStep->key,
                                 QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                                               QString::fromUtf8(u8"运行完成")));
                             if (self->mAlgorithmStatusPendingApply) {
                                 self->applyStoredAlgorithmRunStatus();
                                 return;
                             }
                             self->refreshWorkPage();
                         });
}

void CaseWorkSessionPage::startAiStreamForSuggestion(const QString& type2, const QString& title)
{
    FlowStep* step = stepByKey(QStringLiteral("suggestion"));
    const QString token = authToken();
    if (!step || token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        TnUi::showWarning(QString::fromUtf8(u8"方案建议"),
                          QString::fromUtf8(u8"登录状态或方案信息无效。"));
        return;
    }
    if (!canRunAlgorithms() || step->state == StepState::Running || m_sseBusy) {
        showGrpcCheckRunBlockedMessage();
        return;
    }

    ensureAiClient();
    mActiveStepKey = QStringLiteral("suggestion");
    step->activeAiType2 = type2;
    m_activeSuggestionType2 = type2;
    step->state = StepState::Running;
    ensureSmartDialogShownForRun(QStringLiteral("suggestion"));
    refreshWorkPage();
    setSseBusy(true);
    appendStepLog(step->key, QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                                         QString::fromUtf8(u8"启动方案建议：%1").arg(title)));
    m_grpcAiStream->start(mProjectId, mCurrentCase.id, grpcTransportType(), type2, token, title);
}

QString CaseWorkSessionPage::aiType2ForStep(const FlowStep& step) const
{
    if (step.key == QLatin1String("suggestion")) {
        if (!step.activeAiType2.isEmpty()) {
            return step.activeAiType2;
        }
        if (!m_activeSuggestionType2.isEmpty()) {
            return m_activeSuggestionType2;
        }
        return QStringLiteral("way");
    }
    if (!step.type2.isEmpty()) {
        return step.type2;
    }
    return QStringLiteral("network");
}

void CaseWorkSessionPage::onAiChatPromptSubmitted(const QString& stepKey,
                                            const QString& prompt,
                                            int l1Index,
                                            int l2Index,
                                            const QString& commonQuestion)
{
    FlowStep* step = stepByKey(stepKey);
    const QString token = authToken();
    if (!step || !step->aiChatWidget || token.isEmpty() || mProjectId <= 0 || mCurrentCase.id <= 0) {
        if (step && step->aiChatWidget) {
            step->aiChatWidget->setInputBusy(false);
            step->aiChatWidget->appendSystemLine(QString::fromUtf8(u8"登录状态或方案信息无效。"));
        }
        return;
    }

    step->aiL1Index = l1Index;
    step->aiL2Index = l2Index;

    QPointer<AIChatWidget> chat = step->aiChatWidget;
    const QString type2 = aiType2ForStep(*step);
    const QString pendingId = chat->beginAssistantReply();

    const QString analysisObject = (m_aiConfig && m_aiConfig->isValid())
                                       ? m_aiConfig->optionLabel(AiConfigLevel::L1, l1Index, l2Index)
                                       : QString::number(l1Index);
    const QString engineerNotes = (m_aiConfig && m_aiConfig->isValid())
                                      ? m_aiConfig->optionLabel(AiConfigLevel::L2, l1Index, l2Index)
                                      : QString::number(l2Index);
    const QString ids = step->aiSelectedLinkIds.isEmpty()
                            ? QString()
                            : step->aiSelectedLinkIds.join(QLatin1Char(','));

    HttpApis::grpcAiApi(token,
                        mProjectId,
                        mCurrentCase.id,
                        grpcTransportType(),
                        type2,
                        prompt,
                        analysisObject,
                        engineerNotes,
                        commonQuestion,
                        QString(),
                        QString(),
                        ids,
                        [this, stepKey, pendingId](bool suc,
                                                   const QSharedPointer<GrpcAiApiResultJson>& res,
                                                   const QString& errMsg) {
                            auto applyReply = [&](const QString& content,
                                                  bool isError,
                                                  const QString& downloadUrl) {
                                FlowStep* step = stepByKey(stepKey);
                                if (!step) {
                                    return;
                                }
                                for (AIChatMessage& message : step->chatTimeline) {
                                    if (message.id != pendingId) {
                                        continue;
                                    }
                                    message.content = content;
                                    message.downloadUrl = downloadUrl;
                                    message.role = isError ? AIMessageRole::Error : AIMessageRole::Assistant;
                                    break;
                                }
                                if (step->aiChatWidget) {
                                    step->aiChatWidget->setInputBusy(false);
                                    step->aiChatWidget->finishAssistantReply(pendingId,
                                                                             content,
                                                                             isError,
                                                                             downloadUrl);
                                }
                            };

                            if (!suc || res.isNull()) {
                                applyReply(errMsg.isEmpty() ? QString::fromUtf8(u8"AI 请求失败") : errMsg,
                                           true,
                                           QString());
                                return;
                            }
                            QString content = res->j_content().trimmed();
                            if (content.isEmpty()) {
                                const QJsonObject dataObj = res->jsonObject();
                                content = dataObj.value(QStringLiteral("content")).toString().trimmed();
                            }
                            if (content.isEmpty()) {
                                applyReply(QString::fromUtf8(u8"AI 未返回内容。"), true, QString());
                                return;
                            }
                            QString downloadUrl = res->j_download_url().trimmed();
                            if (downloadUrl.isEmpty()) {
                                const QJsonObject dataObj = res->jsonObject();
                                downloadUrl = dataObj.value(QStringLiteral("download_url")).toString().trimmed();
                            }
                            applyReply(content, false, downloadUrl);
                        });
}

void CaseWorkSessionPage::markStepFinished(const QString& stepKey)
{
    FlowStep* step = stepByKey(stepKey);
    if (!step) {
        return;
    }
    step->state = StepState::Finished;

    if (stepKey == QLatin1String("network")) {
        if (FlowStep* traffic = stepByKey(QStringLiteral("traffic"));
            traffic && traffic->state != StepState::Finished) {
            traffic->state = StepState::Pending;
        }
        // 路网现状分析成功后通知外层自动刷新数据
        emit stepRunFinished(stepKey);
    } else if (stepKey == QLatin1String("review")) {
        mNormalReviewCompleted = true;
        // 方案评审成功后通知外层自动刷新数据
        emit stepRunFinished(stepKey);
    }
}

void CaseWorkSessionPage::markStepFailed(const QString& stepKey)
{
    FlowStep* step = stepByKey(stepKey);
    if (!step) {
        return;
    }
    if (stepKey == QLatin1String("suggestion")) {
        step->state = StepState::Pending;
        return;
    }
    step->state = StepState::Failed;
}

void CaseWorkSessionPage::onStreamProgress(const GrpcStreamEventJson& ev)
{
    appendActiveGrpcEvent(ev);
}

void CaseWorkSessionPage::onStreamFinished(bool success, const QString& errMsg, const QJsonObject& result)
{
    Q_UNUSED(result);
    setSseBusy(false);
    if (!success) {
        appendStepLog(mActiveStepKey, QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                                                  errMsg.isEmpty() ? QString::fromUtf8(u8"运行失败") : errMsg));
        markStepFailed(mActiveStepKey);
        mAlgorithmStatusPendingApply = false;
        refreshWorkPage();
        return;
    }

    appendStepLog(mActiveStepKey, QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                                              QString::fromUtf8(u8"运行完成")));
    markStepFinished(mActiveStepKey);
    if (mAlgorithmStatusPendingApply) {
        applyStoredAlgorithmRunStatus();
    } else {
        refreshWorkPage();
    }
}

void CaseWorkSessionPage::onAiFinished(bool success, const QString& errMsg, const QJsonObject& result)
{
    Q_UNUSED(result);
    setSseBusy(false);
    FlowStep* step = stepByKey(QStringLiteral("suggestion"));
    const QString activeType2 = m_activeSuggestionType2;
    if (!success) {
        appendStepLog(mActiveStepKey, QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                                                  errMsg.isEmpty() ? QString::fromUtf8(u8"运行失败") : errMsg));
        if (step) {
            step->state = StepState::Pending;
        }
        mAlgorithmStatusPendingApply = false;
        refreshWorkPage();
        return;
    }

    appendStepLog(mActiveStepKey, QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                                              QString::fromUtf8(u8"运行完成")));
    if (!activeType2.isEmpty()) {
        markSuggestionResultFinished(activeType2);
    }
    if (step) {
        step->state = StepState::Pending;
    }
    m_activeSuggestionType2.clear();
    if (mAlgorithmStatusPendingApply) {
        applyStoredAlgorithmRunStatus();
    } else {
        refreshWorkPage();
    }
}

void CaseWorkSessionPage::markSuggestionResultFinished(const QString& aiType2)
{
    if (AnalysisResultItem* item = suggestionResultByAiType2(aiType2)) {
        item->subState = StepState::Finished;
    }
}

CaseWorkSessionPage::AnalysisResultItem* CaseWorkSessionPage::suggestionResultByAiType2(const QString& aiType2)
{
    FlowStep* step = stepByKey(QStringLiteral("suggestion"));
    if (!step) {
        return nullptr;
    }
    for (AnalysisResultItem& item : step->results) {
        if (item.aiType2 == aiType2) {
            return &item;
        }
    }
    return nullptr;
}

const CaseWorkSessionPage::AnalysisResultItem* CaseWorkSessionPage::suggestionResultByAiType2(const QString& aiType2) const
{
    const FlowStep* step = stepByKey(QStringLiteral("suggestion"));
    if (!step) {
        return nullptr;
    }
    for (const AnalysisResultItem& item : step->results) {
        if (item.aiType2 == aiType2) {
            return &item;
        }
    }
    return nullptr;
}

bool CaseWorkSessionPage::canRunSuggestionItem(const FlowStep& step, const AnalysisResultItem& item) const
{
    return canRunAlgorithms() && step.state != StepState::Running && !m_sseBusy
           && item.subState == StepState::Pending;
}

