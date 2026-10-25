/*****************************************************************//**
 * \file   casepopgenerationdialog.cpp
 * \brief  生成人口数据配置对话框
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "casepopgenerationdialog.h"

#include "../httpapis/httpapis.h"
#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <qgsvectorlayer.h>

CasePopGenerationDialog::CasePopGenerationDialog(QgsVectorLayer* communityLayer,
                                                 const QString& layerDisplayName,
                                                 const QString& layerTypeName,
                                                 const QString& rasterPath,
                                                 int projectId,
                                                 const QString& authToken,
                                                 QWidget* parent)
    : FramelessDialog(parent)
    , m_communityLayer(communityLayer)
    , m_layerDisplayName(layerDisplayName)
    , m_layerTypeName(layerTypeName)
    , m_rasterPath(rasterPath)
    , m_projectId(projectId)
    , m_authToken(authToken.trimmed())
{
    setTitleText(QString::fromUtf8(u8"生成人口数据"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    resize(560, 240);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet());
    initUi();
}

CasePopGenerationDialog::~CasePopGenerationDialog() = default;

void CasePopGenerationDialog::initUi()
{
    QVBoxLayout* root = contentLayout();
    root->setSpacing(12);
    root->setContentsMargins(16, 12, 16, 16);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #c0392b;"));
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);

    m_inputLayerEdit = new QLineEdit(QString::fromUtf8(u8"小区图层"), this);
    m_inputLayerEdit->setReadOnly(true);
    Phase2Ui::applyFieldStyle(m_inputLayerEdit, Phase2Ui::InputFieldTheme::LightCompact);

    m_rasterBandEdit = new QLineEdit(this);
    m_rasterBandEdit->setReadOnly(true);
    Phase2Ui::applyFieldStyle(m_rasterBandEdit, Phase2Ui::InputFieldTheme::LightCompact);
    m_rasterBandEdit->setText(CasePopZonalStatHelper::defaultPopRasterDisplayLabel());

    auto addRow = [this, root](const QString& labelText, QWidget* widget) {
        auto* row = new QHBoxLayout();
        auto* label = new QLabel(labelText, this);
        label->setMinimumWidth(88);
        row->addWidget(label);
        row->addWidget(widget, 1);
        root->addLayout(row);
    };

    addRow(QString::fromUtf8(u8"小区图层"), m_inputLayerEdit);
    addRow(QString::fromUtf8(u8"人口数据"), m_rasterBandEdit);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_runButton = new QPushButton(QString::fromUtf8(u8"运行"), this);
    QPushButton* cancelBtn = new QPushButton(QString::fromUtf8(u8"取消"), this);
    m_runButton->setCursor(Qt::PointingHandCursor);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    m_runButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    cancelBtn->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(m_runButton, &QPushButton::clicked, this, &CasePopGenerationDialog::onRunClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &CasePopGenerationDialog::reject);
    btnRow->addWidget(m_runButton);
    btnRow->addWidget(cancelBtn);
    root->addLayout(btnRow);

    updateRunEnabled();
}

void CasePopGenerationDialog::showSubmitError(const QString& message)
{
    if (!m_errorLabel) {
        return;
    }
    m_errorLabel->setText(message);
    m_errorLabel->show();
}

void CasePopGenerationDialog::setRunInProgress(bool inProgress)
{
    if (!m_runButton) {
        return;
    }
    if (inProgress) {
        m_runButton->setEnabled(false);
        m_runButton->setText(QString::fromUtf8(u8"计算中…"));
        return;
    }

    m_runButton->setText(QString::fromUtf8(u8"运行"));
    const QFileInfo rasterInfo(m_rasterPath);
    const bool canRun = rasterInfo.exists()
        && CasePopZonalStatHelper::hasPopIdField(m_communityLayer)
        && m_communityLayer && m_communityLayer->isValid()
        && m_projectId > 0 && !m_authToken.isEmpty();
    m_runButton->setEnabled(canRun);
}

void CasePopGenerationDialog::updateRunEnabled()
{
    const QFileInfo rasterInfo(m_rasterPath);
    const bool rasterOk = rasterInfo.exists();
    const bool hasAreaIdField = CasePopZonalStatHelper::hasPopIdField(m_communityLayer);
    const bool layerOk = m_communityLayer && m_communityLayer->isValid();
    const bool projectOk = m_projectId > 0 && !m_authToken.isEmpty();

    if (m_errorLabel) {
        if (!projectOk) {
            m_errorLabel->setText(QString::fromUtf8(u8"登录信息或项目无效，请重新登录后重试。"));
            m_errorLabel->show();
        } else if (!rasterOk) {
            m_errorLabel->setText(QString::fromUtf8(u8"未找到人口栅格文件：%1").arg(m_rasterPath));
            m_errorLabel->show();
        } else if (!hasAreaIdField) {
            m_errorLabel->setText(QString::fromUtf8(u8"小区图层缺少 area_id 字段。"));
            m_errorLabel->show();
        } else if (!layerOk) {
            m_errorLabel->setText(QString::fromUtf8(u8"小区图层无效，请重新加载后重试。"));
            m_errorLabel->show();
        } else {
            m_errorLabel->hide();
        }
    }

    if (m_runButton) {
        m_runButton->setEnabled(rasterOk && hasAreaIdField && layerOk && projectOk);
    }
}

void CasePopGenerationDialog::onSubmitSucceeded()
{
    accept();
}

void CasePopGenerationDialog::onSubmitFailed()
{
    setRunInProgress(false);
    showSubmitError(m_pendingSubmitError);
}

void CasePopGenerationDialog::onRunClicked()
{
    if (!m_communityLayer || m_projectId <= 0 || m_authToken.isEmpty()) {
        return;
    }

    setRunInProgress(true);

    const CasePopZonalStatResult result = CasePopZonalStatHelper::run(m_communityLayer, m_rasterPath);

    if (!result.errorMessage.isEmpty()) {
        setRunInProgress(false);
        showSubmitError(result.errorMessage);
        return;
    }

    if (m_runButton) {
        m_runButton->setText(QString::fromUtf8(u8"提交中…"));
    }

    const QVariantList dataList = CasePopZonalStatHelper::buildPopDataRequestArray(result.records);
    QPointer<CasePopGenerationDialog> self(this);

    HttpApis::projectPopData(m_authToken,
                             m_projectId,
                             dataList,
                             [self](bool suc,
                                    const QSharedPointer<ProjectPopDataResultJson>& response,
                                    const QString& errMsg) {
                                 if (!self) {
                                     return;
                                 }
                                 if (!suc) {
                                     self->m_pendingSubmitError = errMsg.trimmed().isEmpty()
                                         ? QString::fromUtf8(u8"提交失败")
                                         : errMsg.trimmed();
                                     QTimer::singleShot(0, self, &CasePopGenerationDialog::onSubmitFailed);
                                     return;
                                 }

                                 const QString dataMessage = response.isNull()
                                     ? QString()
                                     : response->j_message().trimmed();
                                 self->m_successMessage = dataMessage.isEmpty()
                                     ? QString::fromUtf8(u8"操作成功")
                                     : dataMessage;
                                 QTimer::singleShot(0, self, &CasePopGenerationDialog::onSubmitSucceeded);
                             });
}
