/*****************************************************************//**
 * \file   casecommunitygenerationdialog.cpp
 * \brief  生成交通小区配置对话框
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "casecommunitygenerationdialog.h"

#include "casepopzonalstathelper.h"
#include "../httpapis/httpapis.h"
#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <qgsvectorlayer.h>

#include <QtConcurrent>

CaseCommunityGenerationDialog::CaseCommunityGenerationDialog(QgsVectorLayer* roadWayLayer,
                                                             const QString& projectName,
                                                             const QString& roadWayDisplayName,
                                                             const QString& roadWayTypeName,
                                                             const QString& targetTableDisplayName,
                                                             const QString& rasterPath,
                                                             int projectId,
                                                             const QString& authToken,
                                                             QWidget* parent)
    : FramelessDialog(parent)
    , m_roadWayLayer(roadWayLayer)
    , m_projectName(projectName)
    , m_roadWayDisplayName(roadWayDisplayName)
    , m_roadWayTypeName(roadWayTypeName)
    , m_targetTableDisplayName(targetTableDisplayName)
    , m_rasterPath(rasterPath)
    , m_projectId(projectId)
    , m_authToken(authToken.trimmed())
{
    setTitleText(QString::fromUtf8(u8"生成交通小区"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    resize(580, 340);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet());
    initUi();
}

CaseCommunityGenerationDialog::~CaseCommunityGenerationDialog()
{
    if (m_computeWatcher) {
        m_computeWatcher->cancel();
        m_computeWatcher->waitForFinished();
    }
}

void CaseCommunityGenerationDialog::initUi()
{
    QVBoxLayout* root = contentLayout();
    root->setSpacing(10);
    root->setContentsMargins(16, 12, 16, 16);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #c0392b;"));
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #cbd5e1;"));
    m_statusLabel->hide();
    root->addWidget(m_statusLabel);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setWordWrap(true);
    m_previewLabel->hide();
    root->addWidget(m_previewLabel);

    auto addRow = [this, root](const QString& labelText, QWidget* widget) {
        auto* row = new QHBoxLayout();
        auto* label = new QLabel(labelText, this);
        label->setMinimumWidth(108);
        row->addWidget(label);
        row->addWidget(widget, 1);
        root->addLayout(row);
    };

    m_projectEdit = new QLineEdit(m_projectName, this);
    m_projectEdit->setReadOnly(true);
    Phase2Ui::applyFieldStyle(m_projectEdit, Phase2Ui::InputFieldTheme::LightCompact);

    m_rasterBandEdit = new QLineEdit(this);
    m_rasterBandEdit->setReadOnly(true);
    Phase2Ui::applyFieldStyle(m_rasterBandEdit, Phase2Ui::InputFieldTheme::LightCompact);
    m_rasterBandEdit->setText(CasePopZonalStatHelper::defaultPopRasterDisplayLabel());

    addRow(QString::fromUtf8(u8"当前项目"), m_projectEdit);
    addRow(QString::fromUtf8(u8"人口数据"), m_rasterBandEdit);

    auto makeSpin = [this](double value, double minValue, double maxValue, int decimals) {
        auto* spin = new QDoubleSpinBox(this);
        spin->setRange(minValue, maxValue);
        spin->setDecimals(decimals);
        spin->setValue(value);
        spin->setSingleStep(value >= 1000.0 ? 1000.0 : 1.0);
        Phase2Ui::applyFieldStyle(spin, Phase2Ui::InputFieldTheme::LightCompact);
        return spin;
    };

    m_gridCellSpin = makeSpin(500.0, 1.0, 100000.0, 0);
    m_bufferHalfSpin = makeSpin(50.0, 1.0, 100000.0, 0);
    m_targetPopSpin = makeSpin(50000.0, 1.0, 1e12, 0);
    m_popCapSpin = makeSpin(1.3, 1.0, 10.0, 2);

    addRow(QString::fromUtf8(u8"格网边长(m)"), m_gridCellSpin);
    addRow(QString::fromUtf8(u8"缓冲半宽(m)"), m_bufferHalfSpin);
    addRow(QString::fromUtf8(u8"目标人口"), m_targetPopSpin);
    addRow(QString::fromUtf8(u8"人口上限系数"), m_popCapSpin);

    m_dryRunCheck = new QCheckBox(
        QString::fromUtf8(u8"是否覆盖现有小区图层"), this);
    m_dryRunCheck->setChecked(true);
    m_dryRunCheck->setStyleSheet(QStringLiteral("QCheckBox { color: #ffffff; background: transparent; }"));
    root->addWidget(m_dryRunCheck);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(QStringLiteral("%p%"));
    m_progressBar->setFixedWidth(170);
    m_progressBar->setFixedHeight(22);
    m_progressBar->setStyleSheet(Phase2Ui::dialogProgressBarStyleSheet());
    m_progressBar->hide();
    m_runButton = new QPushButton(QString::fromUtf8(u8"运行"), this);
    QPushButton* cancelBtn = new QPushButton(QString::fromUtf8(u8"取消"), this);
    m_runButton->setCursor(Qt::PointingHandCursor);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    m_runButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    cancelBtn->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(m_runButton, &QPushButton::clicked, this, &CaseCommunityGenerationDialog::onRunClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &CaseCommunityGenerationDialog::reject);
    btnRow->addWidget(m_progressBar);
    btnRow->addWidget(m_runButton);
    btnRow->addWidget(cancelBtn);
    root->addLayout(btnRow);

    connect(m_gridCellSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &CaseCommunityGenerationDialog::updateRunEnabled);
    connect(m_bufferHalfSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &CaseCommunityGenerationDialog::updateRunEnabled);
    connect(m_targetPopSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &CaseCommunityGenerationDialog::updateRunEnabled);
    connect(m_popCapSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &CaseCommunityGenerationDialog::updateRunEnabled);

    updateRunEnabled();
}

void CaseCommunityGenerationDialog::showSubmitError(const QString& message)
{
    if (!m_errorLabel) {
        return;
    }
    m_errorLabel->setText(message);
    m_errorLabel->show();
}

void CaseCommunityGenerationDialog::setStatusText(const QString& text)
{
    if (!m_statusLabel) {
        return;
    }
    if (text.trimmed().isEmpty()) {
        m_statusLabel->hide();
        return;
    }
    m_statusLabel->setText(text);
    m_statusLabel->show();
}

void CaseCommunityGenerationDialog::setRunProgressPercent(int percent)
{
    if (!m_progressBar) {
        return;
    }
    const int clamped = qBound(0, percent, 100);
    m_progressBar->setValue(clamped);
}

void CaseCommunityGenerationDialog::setRunInProgress(bool inProgress)
{
    if (m_gridCellSpin) {
        m_gridCellSpin->setEnabled(!inProgress);
    }
    if (m_bufferHalfSpin) {
        m_bufferHalfSpin->setEnabled(!inProgress);
    }
    if (m_targetPopSpin) {
        m_targetPopSpin->setEnabled(!inProgress);
    }
    if (m_popCapSpin) {
        m_popCapSpin->setEnabled(!inProgress);
    }
    if (m_dryRunCheck) {
        m_dryRunCheck->setEnabled(!inProgress);
    }

    if (m_progressBar) {
        if (inProgress) {
            m_progressBar->setValue(0);
            m_progressBar->show();
        } else {
            m_progressBar->hide();
            m_progressBar->setValue(0);
        }
    }

    if (!m_runButton) {
        return;
    }
    m_runButton->setEnabled(!inProgress);
    if (inProgress) {
        m_runButton->setText(QString::fromUtf8(u8"运行中…"));
        if (m_errorLabel) {
            m_errorLabel->hide();
        }
        if (m_previewLabel) {
            m_previewLabel->hide();
        }
        return;
    }

    m_runButton->setText(QString::fromUtf8(u8"运行"));
    updateRunEnabled();
}

CaseCommunityGenParams CaseCommunityGenerationDialog::collectParams() const
{
    CaseCommunityGenParams params;
    params.gridCellSizeM = m_gridCellSpin ? m_gridCellSpin->value() : 500.0;
    params.bufferHalfWidthM = m_bufferHalfSpin ? m_bufferHalfSpin->value() : 50.0;
    params.targetPopulation = m_targetPopSpin ? m_targetPopSpin->value() : 50000.0;
    params.populationCapFactor = m_popCapSpin ? m_popCapSpin->value() : 1.3;
    params.dryRun = !(m_dryRunCheck && m_dryRunCheck->isChecked());
    return params;
}

void CaseCommunityGenerationDialog::updateRunEnabled()
{
    const CaseCommunityGenParams params = collectParams();
    const QString validationError =
        CaseCommunityGenHelper::validateInputs(m_roadWayLayer, m_rasterPath, params);
    const bool projectOk = m_projectId > 0 && !m_authToken.isEmpty();
    const bool layerOk = m_roadWayLayer && m_roadWayLayer->isValid();

    if (m_errorLabel) {
        if (!projectOk) {
            m_errorLabel->setText(QString::fromUtf8(u8"登录信息或项目无效，请重新登录后重试。"));
            m_errorLabel->show();
        } else if (!layerOk) {
            m_errorLabel->setText(QString::fromUtf8(u8"road_way 图层无效，请重新加载后重试。"));
            m_errorLabel->show();
        } else if (!validationError.isEmpty()) {
            m_errorLabel->setText(validationError);
            m_errorLabel->show();
        } else {
            m_errorLabel->hide();
        }
    }

    if (m_runButton && !m_computeWatcher) {
        m_runButton->setEnabled(projectOk && layerOk && validationError.isEmpty());
    }
}

void CaseCommunityGenerationDialog::onRunClicked()
{
    if (!m_roadWayLayer || m_projectId <= 0 || m_authToken.isEmpty()) {
        return;
    }

    const CaseCommunityGenParams params = collectParams();
    const QString validationError =
        CaseCommunityGenHelper::validateInputs(m_roadWayLayer, m_rasterPath, params);
    if (!validationError.isEmpty()) {
        showSubmitError(validationError);
        return;
    }

    setRunInProgress(true);
    setStatusText(CaseCommunityGenHelper::stepDisplayMessage(CaseCommunityGenStep::Validate));
    setRunProgressPercent(CaseCommunityGenHelper::stepStartPercent(CaseCommunityGenStep::Validate));

    if (m_computeWatcher) {
        m_computeWatcher->deleteLater();
        m_computeWatcher = nullptr;
    }

    QgsVectorLayer* roadWayLayer = m_roadWayLayer;
    const QString rasterPath = m_rasterPath;
    QPointer<CaseCommunityGenerationDialog> self(this);

    m_computeWatcher = new QFutureWatcher<CaseCommunityGenResult>(this);
    connect(m_computeWatcher, &QFutureWatcher<CaseCommunityGenResult>::finished, this,
            &CaseCommunityGenerationDialog::onComputeFinished);

    m_computeWatcher->setFuture(QtConcurrent::run([roadWayLayer, rasterPath, params, self]() {
        return CaseCommunityGenHelper::run(
            roadWayLayer, rasterPath, params,
            [self](CaseCommunityGenStep step, const QString& message, int percent) {
                if (!self) {
                    return;
                }
                const QString text = message.trimmed().isEmpty()
                    ? CaseCommunityGenHelper::stepDisplayMessage(step)
                    : message;
                QMetaObject::invokeMethod(self, [self, text, percent]() {
                    self->setStatusText(text);
                    self->setRunProgressPercent(percent);
                }, Qt::QueuedConnection);
            });
    }));
}

void CaseCommunityGenerationDialog::onComputeFinished()
{
    if (!m_computeWatcher) {
        return;
    }

    const CaseCommunityGenResult result = m_computeWatcher->result();
    m_computeWatcher->deleteLater();
    m_computeWatcher = nullptr;

    if (!result.errorMessage.isEmpty()) {
        setRunInProgress(false);
        setStatusText(QString());
        showSubmitError(result.errorMessage);
        return;
    }

    m_lastResult = result;
    if (m_previewLabel) {
        m_previewLabel->setText(QString::fromUtf8(u8"预估：格网 %1 个，交通小区 %2 个，人口合计 %3")
                                    .arg(result.gridCount)
                                    .arg(result.tazCount)
                                    .arg(result.totalPopulation, 0, 'f', 1));
        m_previewLabel->show();
    }

    const CaseCommunityGenParams params = collectParams();
    if (params.dryRun) {
        setRunProgressPercent(100);
        setRunInProgress(false);
        setStatusText(QString::fromUtf8(u8"预估完成（未提交服务端）。"));
        return;
    }

    setStatusText(CaseCommunityGenHelper::stepDisplayMessage(CaseCommunityGenStep::Submit));
    setRunProgressPercent(CaseCommunityGenHelper::stepStartPercent(CaseCommunityGenStep::Submit));
    if (m_runButton) {
        m_runButton->setText(QString::fromUtf8(u8"提交中…"));
    }

    const QVariantList dataList = CaseCommunityGenHelper::buildGenCommunityRequestArray(result.records);
    QPointer<CaseCommunityGenerationDialog> self(this);

    HttpApis::projectGenCommunity(m_authToken,
                                  m_projectId,
                                  dataList,
                                  [self](bool suc,
                                         const QSharedPointer<ProjectGenCommunityResultJson>& response,
                                         const QString& errMsg) {
                                      if (!self) {
                                          return;
                                      }
                                      if (!suc) {
                                          self->m_pendingSubmitError = errMsg.trimmed().isEmpty()
                                              ? QString::fromUtf8(u8"提交失败")
                                              : errMsg.trimmed();
                                          QTimer::singleShot(0, self, &CaseCommunityGenerationDialog::onSubmitFailed);
                                          return;
                                      }

                                      const QString dataMessage = response.isNull()
                                          ? QString()
                                          : response->j_message().trimmed();
                                      self->m_successMessage = dataMessage.isEmpty()
                                          ? QString::fromUtf8(u8"操作成功")
                                          : dataMessage;
                                      QTimer::singleShot(0, self, &CaseCommunityGenerationDialog::onSubmitSucceeded);
                                  });
}

void CaseCommunityGenerationDialog::onSubmitSucceeded()
{
    setRunProgressPercent(100);
    accept();
}

void CaseCommunityGenerationDialog::onSubmitFailed()
{
    setRunInProgress(false);
    setStatusText(QString());
    showSubmitError(m_pendingSubmitError);
}
