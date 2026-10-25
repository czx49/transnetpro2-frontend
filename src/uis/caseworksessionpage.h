/*****************************************************************//**
 * \file   caseworksessionpage.h
 * \brief  方案工作台单会话页（一个 case + 分析模块的工作流状态）
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef CASEWORKSESSIONPAGE_H
#define CASEWORKSESSIONPAGE_H

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "aiclient/aichatsession.h"
#include "aiclient/aimessage.h"
#include "../casectrls/caseitem.h"
#include "caseworkpanel.h"

class GrpcAiStreamClient;
class GrpcStreamClient;
class GrpcStreamEventJson;
class DownLoadCaseJson;
class QPushButton;
class QVBoxLayout;
class AIChatWidget;
class AiConfigModel;

/**
 * \brief  单个 (caseId, isBase, module) 的工作台会话页
 * \details 持有流程步骤、SSE 客户端、智能对话与报告状态；由 CaseWorkPanel 壳层缓存与切换。
 */
class CaseWorkSessionPage : public QWidget {
    Q_OBJECT

public:
    using AnalysisModule = CaseWorkPanel::AnalysisModule;
    using StepState = CaseWorkPanel::StepState;
    using AnalysisResultItem = CaseWorkPanel::AnalysisResultItem;

    explicit CaseWorkSessionPage(const CaseItem& caseItem,
                                 AnalysisModule module,
                                 QSharedPointer<AiConfigModel> aiConfig,
                                 QHash<QString, QStringList>* commonQuestionCache,
                                 QWidget* parent = nullptr);
    ~CaseWorkSessionPage() override;

    int caseId() const { return mCurrentCase.id; }
    bool isBaseCase() const { return mCurrentCase.isBase == 1; }
    AnalysisModule analysisModule() const { return mCurrentModule; }

    void setCaseInfo(const CaseItem& caseItem);
    void resetWorkflow();

    void setAiSelectedLinkIds(const QString& stepKey, const QStringList& linkIds);
    void setAiDataSelectChecked(const QString& stepKey, bool checked);
    void finishAiDataSelect(const QString& stepKey, const QStringList& linkIds);
    void startTrafficMicroDiagnosis(const QString& ids);
    void prepareForShutdown();
    /** @brief 壳层析构前断开对共享缓存的引用 */
    void detachSharedCaches();
    /** @brief 会话即将显示：按固定模块同步三按钮 checked/图标 */
    void prepareForDisplay();

    QString grpcTransportType() const;
    bool isSseBusy() const { return m_sseBusy; }

signals:
    void analysisDataRequested(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                               const QString& tableName, const QString& resultName);
    void analysisResultViewed(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                              const QString& resultName);
    void deriveCaseFromBaseRequested();
    void stepRunFinished(const QString& stepKey);
    void aiDataSelectRequested(CaseWorkPanel::AnalysisModule module, const QString& stepKey, bool activate);
    void moduleSwitchRequested(CaseWorkPanel::AnalysisModule module);

private slots:
    void onModuleClicked();
    void onStepAiAssistantClicked();
    void onStepRunClicked();
    void onSuggestionButtonClicked();
    void onEditLogClicked();
    void onCostBenefitClicked();
    void onRenovationReportClicked();
    void onGenerateAnalysisReportClicked();
    void onAnalysisReportDownloadClicked();
    void onReportViewClicked();
    void onOsmDownloadClicked();
    void onSimulateDownloadClicked();
    void onDeriveCaseClicked();
    void onResultDataClicked();
    void onResultViewClicked();

private:
    struct FlowStep {
        QString key;
        QString title;
        QString type2;
        QString activeAiType2;
        QString description;
        QList<AnalysisResultItem> results;
        StepState state = StepState::Pending;
        bool logVisible = false;
        bool logCollapsed = true;
        QString suggestionRunText;
        QString suggestionContentUrl;
        bool suggestionOutputVisible = false;
        QString editLogOutputText;
        bool editLogOutputVisible = false;
        int editLogPage = 1;
        int editLogLimit = 100;
        QList<AIChatMessage> chatTimeline;
        QSharedPointer<AIChatSession> aiSession;
        AIChatWidget* aiChatWidget = nullptr;
        int aiL1Index = 0;
        int aiL2Index = 0;
        QStringList aiSelectedLinkIds;
    };

    void initUi();
    /** @brief 会话首次创建时初始化步骤与数据检查（模块在构造后固定） */
    void initializeWorkflow();
    void rebuildWorkflow();
    void refreshWorkPage();
    void clearWorkContentLayout();
    void syncModuleButtonsToCurrentModule();
    QWidget* createWorkPage();
    QWidget* createAnalysisModulePanel();
    QPushButton* createStepAiAssistantButton(QWidget* parent, const QString& stepKey);
    void attachStepAiChatWidget(QVBoxLayout* layout, FlowStep& step, QWidget* card);
    QString smartDialogHostKey() const;
    QString resolveSmartDialogKey(const QString& stepKey) const;
    bool shouldAttachSmartDialog(const QString& stepKey) const;
    void setStepSmartDialogVisible(const QString& stepKey, bool visible, bool expand = false);
    void ensureSmartDialogShownForRun(const QString& stepKey);
    QWidget* createDataCheckPanel();
    QWidget* createBaseFlowPanel();
    QWidget* createNormalFlowPanel();
    QWidget* createStepCard(int stepIndex);
    QWidget* createSuggestionStepCard();
    QWidget* createEditLogStepCard();
    QWidget* createNormalReviewStepCard();
    QWidget* createResultRow(const AnalysisResultItem& resultItem);
    QWidget* createBottomReportPanel();
    QWidget* createFinalOutputPanel();
    QWidget* createFinalOutputRow(const QString& iconText,
                                  const QString& iconPath,
                                  const QString& title,
                                  const QString& desc,
                                  bool hasViewButton,
                                  const char* viewSlot,
                                  const char* downloadSlot);
    QWidget* createNormalReportPanel();

    void resetStateForCurrentCase();
    void rebuildStepsForCurrentModule();
    FlowStep* stepByKey(const QString& key);
    const FlowStep* stepByKey(const QString& key) const;
    void appendStepLog(const QString& stepKey, const QString& line);
    void appendActiveGrpcEvent(const GrpcStreamEventJson& ev);

    QString moduleTitle(AnalysisModule module) const;
    QString analysisReportTitle() const;
    QString authToken() const;
    QString stepStateText(StepState state) const;
    QString stepStateColor(StepState state) const;
    QString runningMoviePath() const;
    QString translateCheckField(const QString& key) const;

    void refreshGrpcCheck();
    void applyGrpcCheckSummary(const QJsonObject& dataObj, bool httpOk, const QString& errMsg);
    void loadAlgorithmRunStatus();
    void applyAlgorithmRunStatus(const QJsonObject& dataObj, bool httpOk);
    void applyStoredAlgorithmRunStatus();
    bool isAlgorithmRunDataAllPassed(const QJsonObject& dataObj) const;
    bool isAnyAlgorithmRunStepRunning() const;
    QStringList algorithmRunStepKeys() const;
    void setSseBusy(bool busy);
    void ensureStreamClient();
    void ensureAiClient();
    void ensureTrafficMicroStreamClient();
    void startGrpcStreamForStep(const QString& stepKey, const QString& jsonPayload = QString(), const QString& ids = QString());
    void appendTrafficMicroGrpcEvent(const GrpcStreamEventJson& ev);
    void startGrpcRunAlgForSuggestion();
    void startAiStreamForSuggestion(const QString& type2, const QString& title);
    void onAiChatPromptSubmitted(const QString& stepKey,
                                 const QString& prompt,
                                 int l1Index,
                                 int l2Index,
                                 const QString& commonQuestion);
    void ensureAiConfigLoaded();
    void applyAiConfigToChatWidgets();
    void ensureCommonQuestionsLoaded();
    void applyCommonQuestionsToChatWidgets();
    QString commonQuestionCacheKey() const;
    QString aiType2ForStep(const FlowStep& step) const;
    void markStepFinished(const QString& stepKey);
    void markStepFailed(const QString& stepKey);
    void markSuggestionResultFinished(const QString& aiType2);
    AnalysisResultItem* suggestionResultByAiType2(const QString& aiType2);
    const AnalysisResultItem* suggestionResultByAiType2(const QString& aiType2) const;
    bool canRunSuggestionItem(const FlowStep& step, const AnalysisResultItem& item) const;

    void onStreamProgress(const GrpcStreamEventJson& ev);
    void onStreamFinished(bool success, const QString& errMsg, const QJsonObject& result);
    void onAiFinished(bool success, const QString& errMsg, const QJsonObject& result);
    void onTrafficMicroStreamProgress(const GrpcStreamEventJson& ev);
    void onTrafficMicroStreamFinished(bool success, const QString& errMsg, const QJsonObject& result);

    void showAnalysisReportDialog(const QString& title, const QString& html);
    void registerWorkbenchGateButton(QPushButton* button);
    void updateWorkbenchGateButtons();
    void setHttpBusy(bool busy);
    void downloadCaseFile(const QSharedPointer<DownLoadCaseJson>& response, const QString& failureTitle);
    void downloadFromUrl(const QString& url, const QString& failureTitle);

    bool canRunAlgorithms() const;
    bool isStepStateRunnable(const FlowStep& step) const;
    bool canRunStep(const FlowStep& step) const;
    void appendFinishedAnalysisResultRows(QVBoxLayout* layout, const FlowStep& step, QWidget* card);
    void showGrpcCheckRunBlockedMessage();
    static bool isGrpcCheckDataAllPassed(const QJsonObject& dataObj);

private:
    CaseItem mCurrentCase;
    int mProjectId = -1;
    AnalysisModule mCurrentModule;
    bool mModuleSelected = true;
    QList<FlowStep> mFlowSteps;
    QString mGrpcCheckSummary;
    bool mGrpcCheckPending = false;
    bool mGrpcCheckAllPassed = false;
    bool mAlgorithmRunLoaded = false;
    bool mAlgorithmRunPassed = false;
    bool mAlgorithmStatusPendingApply = false;
    int mAlgorithmRunRequestSeq = 0;
    QString mActiveStepKey;
    bool mNormalReviewCompleted = false;
    QString m_activeSuggestionType2;

    GrpcStreamClient* m_grpcStream = nullptr;
    GrpcAiStreamClient* m_grpcAiStream = nullptr;
    GrpcStreamClient* m_trafficMicroStream = nullptr;
    QSharedPointer<AiConfigModel> m_aiConfig;
    bool m_aiConfigRequested = false;
    bool m_aiConfigLoadNotified = false;
    QHash<QString, QStringList>* m_commonQuestionCache = nullptr;
    QStringList m_commonQuestionRequests;
    QList<QPushButton*> m_workbenchGateButtons;
    bool m_sseBusy = false;
    bool m_httpBusy = false;

    QPushButton* mMotorButton = nullptr;
    QPushButton* mSlowButton = nullptr;
    QPushButton* mPublicButton = nullptr;
    QWidget* mWorkContentWidget = nullptr;
    QVBoxLayout* mWorkContentLayout = nullptr;
};

#endif // CASEWORKSESSIONPAGE_H
