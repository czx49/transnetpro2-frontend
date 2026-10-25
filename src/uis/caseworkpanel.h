/*****************************************************************//**
 * \file   caseworkpanel.h
 * \brief  方案详情业务工作台壳层（折叠 + 会话缓存）
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef CASEWORKPANEL_H
#define CASEWORKPANEL_H

#include <QEasingCurve>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QPropertyAnimation>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "../casectrls/caseitem.h"

class AiConfigModel;
class CaseWorkSessionPage;
class QLabel;
class QPushButton;
class QHideEvent;
class QResizeEvent;
class QShowEvent;
class QStackedWidget;

/**
 * \brief  工作台壳层：标题、折叠、按 (caseId,isBase,module) 缓存 CaseWorkSessionPage
 */
class CaseWorkPanel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int currentWidth READ currentWidth WRITE setCurrentWidth)

public:
    enum class AnalysisModule {
        MotorVehicle = 0,
        SlowTraffic,
        PublicTransport
    };

    enum class StepState {
        Locked = 0,
        Pending,
        Running,
        Finished,
        Failed
    };

    struct AnalysisResultItem {
        QString name;
        QString renderLayer;
        QString tableName;
        QString aiType2;
        bool uiVisible = true;
        StepState subState = StepState::Pending;
        bool overrideModule = false;
        AnalysisModule targetModule = AnalysisModule::MotorVehicle;
    };

    explicit CaseWorkPanel(QWidget* parent = nullptr);
    ~CaseWorkPanel() override;

    int currentWidth() const;
    void setCurrentWidth(int width);
    void setCurrentCase(const CaseItem& caseItem);
    void setAnalysisModule(AnalysisModule module);
    void resetWorkflow();

    bool isExpanded() const;
    void setExpanded(bool expand);
    bool isAnalysisModuleSelected() const;

    void startTrafficMicroDiagnosis(const QString& ids);
    void setAiSelectedLinkIds(const QString& stepKey, const QStringList& linkIds);
    void setAiDataSelectChecked(const QString& stepKey, bool checked);
    void finishAiDataSelect(const QString& stepKey, const QStringList& linkIds);
    void prepareForShutdown();

    /** @brief 删除方案时清理该 caseId 的全部会话 */
    void clearSessionsForCase(int caseId);

    QString grpcTransportType() const;

signals:
    void analysisDataRequested(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                               const QString& tableName, const QString& resultName);
    void analysisResultViewed(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                              const QString& resultName);
    void analysisModuleChanged(CaseWorkPanel::AnalysisModule module);
    void deriveCaseFromBaseRequested();
    void stepRunFinished(const QString& stepKey);
    void workbenchExpandedChanged(bool expanded);
    void aiDataSelectRequested(CaseWorkPanel::AnalysisModule module, const QString& stepKey, bool activate);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void togglePanel();
    void onChooserModuleClicked();
    void onSessionModuleSwitchRequested(CaseWorkPanel::AnalysisModule module);
    void onSessionAnalysisResultViewed(CaseWorkPanel::AnalysisModule module, const QString& resultKey,
                                       const QString& resultName);

private:
    struct PendingResultView {
        int caseId = 0;
        AnalysisModule module = AnalysisModule::MotorVehicle;
        QString resultKey;
        QString resultName;
    };

    QString caseKey(int caseId, bool isBase) const;
    QString sessionKey(int caseId, bool isBase, AnalysisModule module) const;
    QString chooserKey(int caseId, bool isBase) const;

    void initUi();
    void updatePosition();
    void updateToggleGeometry();
    void scheduleToggleGeometryUpdate();
    QWidget* createHeader();
    QWidget* createChooserPage();
    void refreshHeader();
    void connectSessionSignals(CaseWorkSessionPage* session, const QString& key);
    CaseWorkSessionPage* ensureSession(AnalysisModule module);
    QWidget* ensureChooserPage();
    void showSession(CaseWorkSessionPage* session);
    void showChooser();
    CaseWorkSessionPage* currentSession() const;
    void flushPendingAnalysisResultViewed();

    QPropertyAnimation* mAnim;
    int mCurrentWidth;
    int mExpandWidth;
    int mCollapseWidth;
    bool mIsCollapsed;

    CaseItem mCurrentCase;
    AnalysisModule mCurrentModule;
    bool mModuleSelected = false;
    QHash<QString, AnalysisModule> mLastModuleByCaseKey;

    QStackedWidget* mSessionStack = nullptr;
    QHash<QString, CaseWorkSessionPage*> mSessions;
    QHash<QString, QWidget*> mChoosers;

    QList<PendingResultView> mPendingResultViews;
    QString mActiveAiSelectSessionKey;

    QSharedPointer<AiConfigModel> m_aiConfig;
    QHash<QString, QStringList> m_commonQuestionCache;

    QWidget* mContentWidget;
    QLabel* mTitleLabel;
    QPointer<QPushButton> mToggleButton;
};

#endif // CASEWORKPANEL_H
