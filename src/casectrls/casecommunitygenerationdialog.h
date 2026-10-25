/*****************************************************************//**
 * \file   casecommunitygenerationdialog.h
 * \brief  生成交通小区配置对话框
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASECOMMUNITYGENERATIONDIALOG_H
#define CASECOMMUNITYGENERATIONDIALOG_H

#include "../publicctrls/framelessdialog.h"

#include "casecommunitygenhelper.h"

#include <QFutureWatcher>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QgsVectorLayer;

/** @brief parent 必须为 nullptr（二期弹窗约定） */
class CaseCommunityGenerationDialog : public FramelessDialog {
    Q_OBJECT

public:
    CaseCommunityGenerationDialog(QgsVectorLayer* roadWayLayer,
                                  const QString& projectName,
                                  const QString& roadWayDisplayName,
                                  const QString& roadWayTypeName,
                                  const QString& targetTableDisplayName,
                                  const QString& rasterPath,
                                  int projectId,
                                  const QString& authToken,
                                  QWidget* parent = nullptr);
    ~CaseCommunityGenerationDialog() override;

    QString successMessage() const { return m_successMessage; }

private slots:
    void onRunClicked();
    void onComputeFinished();
    void onSubmitSucceeded();
    void onSubmitFailed();

private:
    void initUi();
    void updateRunEnabled();
    void showSubmitError(const QString& message);
    void setRunInProgress(bool inProgress);
    void setStatusText(const QString& text);
    void setRunProgressPercent(int percent);
    CaseCommunityGenParams collectParams() const;

    QgsVectorLayer* m_roadWayLayer = nullptr;
    QString m_projectName;
    QString m_roadWayDisplayName;
    QString m_roadWayTypeName;
    QString m_targetTableDisplayName;
    QString m_rasterPath;
    int m_projectId = 0;
    QString m_authToken;
    QString m_successMessage;
    QString m_pendingSubmitError;
    CaseCommunityGenResult m_lastResult;

    QLineEdit* m_projectEdit = nullptr;
    QLineEdit* m_rasterBandEdit = nullptr;
    QDoubleSpinBox* m_gridCellSpin = nullptr;
    QDoubleSpinBox* m_bufferHalfSpin = nullptr;
    QDoubleSpinBox* m_targetPopSpin = nullptr;
    QDoubleSpinBox* m_popCapSpin = nullptr;
    QCheckBox* m_dryRunCheck = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_previewLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_runButton = nullptr;
    QLabel* m_errorLabel = nullptr;

    QFutureWatcher<CaseCommunityGenResult>* m_computeWatcher = nullptr;
};

#endif // CASECOMMUNITYGENERATIONDIALOG_H
