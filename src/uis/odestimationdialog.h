/*****************************************************************//**
 * \file   odestimationdialog.h
 * \brief  OD反推工具弹窗
 * \details 第一页动态生成 OD反推算法参数表单，第二页展示 /api/grpc/stream 回传的 SSE 输出
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef OD_ESTIMATION_DIALOG_H
#define OD_ESTIMATION_DIALOG_H

#include "framelessdialog.h"

#include <QHash>
#include <QString>
#include <QVariantMap>

class QLabel;
class QPushButton;
class QTabWidget;
class QTextBrowser;
class QUrl;
class QWidget;
class GrpcStreamClient;
class GrpcStreamEventJson;

/**
 * \brief  OD反推工具弹窗
 * \details 以 TabWidget 承载参数配置与运行输出两页，运行时调用 GrpcStreamClient
 *          订阅 /api/grpc/stream，并将 SSE 文本追加到输出页。
 * \author zhangmg
 * \date   June 2026
 */
class OdEstimationDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit OdEstimationDialog(int projectId, const QString& token, QWidget* parent = nullptr);
    ~OdEstimationDialog() override;

    /**
     * \brief 载入 OD反推算法参数模板并构建表单
     *
     * \return 模板存在且至少含一个参数时返回 true
     */
    bool loadAlgorithm();

    /** @brief 向「运行输出」页追加一行文本 */
    void appendOutputLine(const QString& text);

signals:
    void runSucceeded();

protected:
    void reject() override;

private slots:
    void onRunClicked();
    void onCancelClicked();
    void onStreamProgress(const GrpcStreamEventJson& event);
    void onStreamFinished(bool success, const QString& errMsg);
    void onResultTableLinkClicked(const QUrl& url);

private:
    void buildUi();
    QWidget* buildParamPage();
    QWidget* buildOutputPage();
    bool buildParamForm(const class AlgorithmNameJson& alg);
    QVariantMap collectParameters() const;
    QString parametersJsonString() const;
    void updateCancelButtonVisible();
    void cancelRunningStream(const QString& logText);
    void appendResultTableLinks();
    void openResultTable(const QString& tableSuffix, const QString& title);
    void requestObsCheck();
    void applyObsCheckResult(bool ok, const QString& err, int obs);
    void updateRunButtonEnabled();

    /**
     * \brief 弹出与项目其它位置一致的「是/否」二次确认框
     *
     * \param title   标题
     * \param message 提示内容
     * \return 用户选择「是」返回 true
     */
    bool confirmYesNo(const QString& title, const QString& message);

private:
    int mProjectId;
    QString mToken;
    QTabWidget* mTabWidget;
    QWidget* mFormHost;
    QLabel* mObsStatusLabel;
    QTextBrowser* mOutputEdit;
    QPushButton* mRunButton;
    QPushButton* mCancelButton;
    GrpcStreamClient* mStreamClient;
    bool mRunning;
    bool mObsReady;
    QHash<QString, QWidget*> mFieldEditors;
};

#endif // OD_ESTIMATION_DIALOG_H
