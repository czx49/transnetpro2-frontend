/*****************************************************************//**
 * \file   casepopgenerationdialog.h
 * \brief  生成人口数据配置对话框（简化版分区统计）
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASEPOPGENERATIONDIALOG_H
#define CASEPOPGENERATIONDIALOG_H

#include "../publicctrls/framelessdialog.h"

#include "casepopzonalstathelper.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QgsVectorLayer;

/** @brief parent 必须为 nullptr（二期弹窗约定） */
class CasePopGenerationDialog : public FramelessDialog {
    Q_OBJECT

public:
    CasePopGenerationDialog(QgsVectorLayer* communityLayer,
                            const QString& layerDisplayName,
                            const QString& layerTypeName,
                            const QString& rasterPath,
                            int projectId,
                            const QString& authToken,
                            QWidget* parent = nullptr);
    ~CasePopGenerationDialog() override;

    QString successMessage() const { return m_successMessage; }

private slots:
    void onRunClicked();
    void onSubmitSucceeded();
    void onSubmitFailed();

private:
    void initUi();
    void updateRunEnabled();
    void showSubmitError(const QString& message);
    void setRunInProgress(bool inProgress);

    QgsVectorLayer* m_communityLayer = nullptr;
    QString m_layerDisplayName;
    QString m_layerTypeName;
    QString m_rasterPath;
    int m_projectId = 0;
    QString m_authToken;
    QString m_successMessage;
    QString m_pendingSubmitError;
    QLineEdit* m_inputLayerEdit = nullptr;
    QLineEdit* m_rasterBandEdit = nullptr;
    QPushButton* m_runButton = nullptr;
    QLabel* m_errorLabel = nullptr;
};

#endif // CASEPOPGENERATIONDIALOG_H
