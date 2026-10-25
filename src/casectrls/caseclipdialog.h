/*****************************************************************//**
 * \file   caseclipdialog.h
 * \brief  方案/基础数据地图裁剪对话框（FramelessDialog）
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef CASECLIPDIALOG_H
#define CASECLIPDIALOG_H

#include "../publicctrls/framelessdialog.h"

class QComboBox;
class QLineEdit;
class TNMapWidget;

/** @brief parent 必须为 nullptr（二期弹窗约定）；地图与工程上下文由参数传入 */
class CaseClipLayerDialog : public FramelessDialog {
    Q_OBJECT

public:
    CaseClipLayerDialog(TNMapWidget* mapWidget, int projectId, int caseId, const QString& authToken, QWidget* parent = nullptr);
    ~CaseClipLayerDialog() override;

private slots:
    void onOkClicked();

private:
    void initUi();

    TNMapWidget* m_mapWidget;
    int m_projectId;
    int m_caseId;
    QString m_authToken;
    QComboBox* m_modeCombo;
    QComboBox* m_communityCombo;
    QLineEdit* m_clipNameEdit;
};

#endif // CASECLIPDIALOG_H
