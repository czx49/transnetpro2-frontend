#include "offlineapi.h"
/*****************************************************************//**
 * \file   odestimationdialog.cpp
 * \brief  OD反推工具弹窗实现
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "odestimationdialog.h"

#include "../casectrls/casewfsmapcontroller.h"
#include "../publicctrls/messagedialog.h"
#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"
#include "algorithmconfigjson.h"
#include "appsession.h"
#include "databrowserdialog.h"
#include "grpcstreamclient.h"
#include "httpapis.h"
#include "httpapijson.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include <qgsvectorlayer.h>

namespace {
constexpr char kConfigResource[] = ":/config/algorithmConfig.json";
constexpr char kAlgorithmName[] = "odEstimateAlgorithm";
constexpr char kStreamType[] = "tool";
constexpr char kStreamType2[] = "estimation";
constexpr char kOdTableScheme[] = "odtable";
constexpr char kOdEstimationIntro[] =
    u8"OD矩阵校准,根据路段观测流量与初始 OD需求，在机动车路网上迭代调整各交通小区之间的出行量，使分配流量尽量拟合观测值;\n"
    u8"输入:路网路段表、交通小区表、路网节点表、OD需求、路段观测流量表(约束路段与观测流量;缺则失败)。";

struct OdResultTableLink {
    const char* suffix;
    const char* title;
};

constexpr OdResultTableLink kOdResultTableLinks[] = {
    { "od_estimation_results", u8"OD估计结果表" },
    { "link_flow_results", u8"路段流量对比表" },
    { "iteration_record", u8"双层规划迭代记录表" },
};

/** field_chinese 未覆盖的 OD 结果表专用字段 → 中文表头 */
QMap<QString, QString> odResultTableHeaderAliasMap(const QString& tableSuffix)
{
    QMap<QString, QString> map;
    if (tableSuffix == QLatin1String("od_estimation_results")) {
        map.insert(QStringLiteral("origin_id"), QString::fromUtf8(u8"起点小区ID"));
        map.insert(QStringLiteral("dest_id"), QString::fromUtf8(u8"终点小区ID"));
        map.insert(QStringLiteral("estimated_demand"), QString::fromUtf8(u8"估计需求"));
        map.insert(QStringLiteral("initial_demand"), QString::fromUtf8(u8"初始需求"));
    } else if (tableSuffix == QLatin1String("link_flow_results")) {
        map.insert(QStringLiteral("from_node"), QString::fromUtf8(u8"起点节点"));
        map.insert(QStringLiteral("to_node"), QString::fromUtf8(u8"终点节点"));
        map.insert(QStringLiteral("estimated_flow"), QString::fromUtf8(u8"估计流量"));
        map.insert(QStringLiteral("observed_flow"), QString::fromUtf8(u8"观测流量"));
        map.insert(QStringLiteral("cost"), QString::fromUtf8(u8"阻抗"));
        map.insert(QStringLiteral("ratio"), QString::fromUtf8(u8"观测估计比"));
        map.insert(QStringLiteral("capacity"), QString::fromUtf8(u8"通行能力"));
        map.insert(QStringLiteral("vc_ratio"), QString::fromUtf8(u8"饱和度"));
        map.insert(QStringLiteral("observed_over_estimated"), QString::fromUtf8(u8"观测估计比"));
    } else if (tableSuffix == QLatin1String("iteration_record")) {
        map.insert(QStringLiteral("iteration"), QString::fromUtf8(u8"迭代次数"));
        map.insert(QStringLiteral("upper_objective"), QString::fromUtf8(u8"上层目标值"));
        map.insert(QStringLiteral("rmse"), QStringLiteral("RMSE"));
        map.insert(QStringLiteral("rgp"), QString::fromUtf8(u8"相对间隙"));
    }
    return map;
}

// 浅色滚动条样式，与 Phase2Ui 浅色弹层滚动条保持一致
QString scrollBarStyleSheet()
{
    return QStringLiteral(
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: #cbd5e1; border-radius: 4px; min-height: 24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QScrollBar:horizontal { background: transparent; height: 8px; margin: 2px; }"
        "QScrollBar::handle:horizontal { background: #cbd5e1; border-radius: 4px; min-width: 24px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }");
}

}

OdEstimationDialog::OdEstimationDialog(int projectId, const QString& token, QWidget* parent)
    : FramelessDialog(parent)
    , mProjectId(projectId)
    , mToken(token)
    , mTabWidget(nullptr)
    , mFormHost(nullptr)
    , mObsStatusLabel(nullptr)
    , mOutputEdit(nullptr)
    , mRunButton(nullptr)
    , mCancelButton(nullptr)
    , mStreamClient(nullptr)
    , mRunning(false)
    , mObsReady(false)
    , mFieldEditors()
{
    setTitleText(QString::fromUtf8(u8"OD反推"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    resize(560, 520);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                  + QStringLiteral(
                      "#framelessDialogContent { background: transparent; }"
                      "#framelessDialogContent QWidget { background: transparent; }"));
    buildUi();
    requestObsCheck();
}

OdEstimationDialog::~OdEstimationDialog()
{
    if (mStreamClient) {
        mStreamClient->stop();
    }
}

void OdEstimationDialog::buildUi()
{
    mTabWidget = new QTabWidget(this);
    mTabWidget->setObjectName(QStringLiteral("odEstimationTab"));
    mTabWidget->setStyleSheet(Phase2Ui::dialogTabStyleSheet(QStringLiteral("odEstimationTab")));
    mTabWidget->addTab(buildParamPage(), QString::fromUtf8(u8"参数配置"));
    mTabWidget->addTab(buildOutputPage(), QString::fromUtf8(u8"运行输出"));
    connect(mTabWidget, &QTabWidget::currentChanged, this, [this](int) {
        updateCancelButtonVisible();
    });
    contentLayout()->addWidget(mTabWidget);
}

QWidget* OdEstimationDialog::buildParamPage()
{
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("odEstimationParamPage"));
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setStyleSheet(
        QStringLiteral(
            "QWidget#odEstimationParamPage {"
            "   background-color: transparent;"
            "}"
            "QLabel {"
            "   background-color: transparent;"
            "   color: #ffffff;"
            "   font-size: 13px;"
            "}"
            "QScrollArea {"
            "   border: none;"
            "   background-color: transparent;"
            "}"
            "QScrollArea > QWidget > QWidget {"
            "   background-color: transparent;"
            "}"
            "QCheckBox {"
            "   background-color: transparent;"
            "   color: #ffffff;"
            "}")
        + Phase2Ui::fieldStyleSheet(Phase2Ui::InputFieldTheme::LightCompact)
        + scrollBarStyleSheet());

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(12);

    auto* introLabel = new QLabel(QString::fromUtf8(kOdEstimationIntro), page);
    introLabel->setObjectName(QStringLiteral("odEstimationIntroLabel"));
    introLabel->setWordWrap(true);
    introLabel->setStyleSheet(
        QStringLiteral(
            "QLabel#odEstimationIntroLabel {"
            "   background-color: rgba(33, 136, 255, 0.15);"
            "   border: 1px solid rgba(255, 255, 255, 0.18);"
            "   border-radius: 6px;"
            "   color: #ffffff;"
            "   font-size: 13px;"
            "   padding: 10px 12px;"
            "}"));
    layout->addWidget(introLabel);

    mObsStatusLabel = new QLabel(page);
    mObsStatusLabel->setObjectName(QStringLiteral("odEstimationObsStatusLabel"));
    mObsStatusLabel->setWordWrap(true);
    mObsStatusLabel->setStyleSheet(
        QStringLiteral(
            "QLabel#odEstimationObsStatusLabel {"
            "   background-color: transparent;"
            "   color: #fbbf24;"
            "   font-size: 12px;"
            "}"));
    mObsStatusLabel->hide();
    layout->addWidget(mObsStatusLabel);

    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    mFormHost = new QWidget(scroll);
    mFormHost->setObjectName(QStringLiteral("odEstimationFormHost"));
    mFormHost->setAttribute(Qt::WA_StyledBackground, true);
    mFormHost->setStyleSheet(QStringLiteral("QWidget#odEstimationFormHost { background-color: transparent; }"));
    scroll->setWidget(mFormHost);
    layout->addWidget(scroll, 1);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    mRunButton = new QPushButton(QString::fromUtf8(u8"运行"), page);
    mRunButton->setFixedHeight(32);
    mRunButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    mRunButton->setEnabled(false);
    connect(mRunButton, &QPushButton::clicked, this, &OdEstimationDialog::onRunClicked);
    btnRow->addStretch();
    btnRow->addWidget(mRunButton);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    return page;
}

QWidget* OdEstimationDialog::buildOutputPage()
{
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("odEstimationOutputPage"));
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setStyleSheet(QStringLiteral("QWidget#odEstimationOutputPage { background-color: transparent; }"));

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(12);

    mOutputEdit = new QTextBrowser(page);
    mOutputEdit->setReadOnly(true);
    mOutputEdit->setOpenLinks(false);
    mOutputEdit->setOpenExternalLinks(false);
    mOutputEdit->setStyleSheet(
        QStringLiteral(
            "QTextBrowser {"
            "   background-color: #ffffff;"
            "   border: 1px solid #cdd9e8;"
            "   border-radius: 6px;"
            "   color: #1f2937;"
            "   font-size: 13px;"
            "}"
            "QTextBrowser a {"
            "   color: #1f6fd6;"
            "   text-decoration: underline;"
            "}")
        + scrollBarStyleSheet());
    connect(mOutputEdit, &QTextBrowser::anchorClicked, this, &OdEstimationDialog::onResultTableLinkClicked);
    layout->addWidget(mOutputEdit, 1);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    mCancelButton = new QPushButton(QString::fromUtf8(u8"取消"), page);
    mCancelButton->setFixedHeight(32);
    mCancelButton->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    mCancelButton->setVisible(false);
    connect(mCancelButton, &QPushButton::clicked, this, &OdEstimationDialog::onCancelClicked);
    btnRow->addStretch();
    btnRow->addWidget(mCancelButton);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    return page;
}

bool OdEstimationDialog::loadAlgorithm()
{
    QFile f(QString::fromUtf8(kConfigResource));
    if (!f.open(QIODevice::ReadOnly)) {
        TnUi::showWarning(QString::fromUtf8(u8"算法配置"),
                             QString::fromUtf8(u8"无法打开内置算法配置文件。"));
        return false;
    }

    const QByteArray bytes = f.readAll();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        TnUi::showWarning(QString::fromUtf8(u8"算法配置"),
                             QString::fromUtf8(u8"算法配置文件格式错误。"));
        return false;
    }

    const QJsonArray arr = doc.array();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) {
            continue;
        }
        AlgorithmNameJson alg(v);
        if (alg.j_algorithmName() == QString::fromUtf8(kAlgorithmName)) {
            return buildParamForm(alg);
        }
    }

    TnUi::showWarning(QString::fromUtf8(u8"算法配置"),
                         QString::fromUtf8(u8"未找到算法：%1").arg(QString::fromUtf8(kAlgorithmName)));
    return false;
}

bool OdEstimationDialog::buildParamForm(const AlgorithmNameJson& alg)
{
    mFieldEditors.clear();

    auto* form = new QFormLayout(mFormHost);
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);

    const QList<ParameterJson> params = alg.j_parameters();
    for (const ParameterJson& p : params) {
        const QString field = p.j_fieldName();
        const QString dt = p.j_dataType().toLower();
        const QVariant def = p.j_default();
        QWidget* editor = nullptr;

        if (dt == QLatin1String("bool")) {
            auto* cb = new QCheckBox(mFormHost);
            cb->setChecked(def.toBool());
            editor = cb;
        } else if (dt == QLatin1String("integer")) {
            auto* sp = new QSpinBox(mFormHost);
            sp->setRange(-2147483647, 2147483647);
            sp->setValue(def.toInt());
            editor = sp;
        } else if (dt == QLatin1String("float") || dt == QLatin1String("double")) {
            auto* dsp = new QDoubleSpinBox(mFormHost);
            dsp->setDecimals(8);
            dsp->setRange(-1e12, 1e12);
            dsp->setValue(def.toDouble());
            editor = dsp;
        } else {
            auto* le = new QLineEdit(mFormHost);
            le->setText(def.toString());
            editor = le;
        }

        const QString labelText = p.j_tranlateName().isEmpty() ? field : p.j_tranlateName();
        QString tip = p.j_desc();
        if (!p.j_remarks().isEmpty()) {
            tip += QStringLiteral("\n") + p.j_remarks();
        }
        auto* label = new QLabel(labelText + QStringLiteral(":"), mFormHost);
        label->setToolTip(tip);
        editor->setToolTip(tip);

        form->addRow(label, editor);
        mFieldEditors.insert(field, editor);
    }

    return !mFieldEditors.isEmpty();
}

QVariantMap OdEstimationDialog::collectParameters() const
{
    QVariantMap map;
    for (auto it = mFieldEditors.constBegin(); it != mFieldEditors.constEnd(); ++it) {
        const QString key = it.key();
        QWidget* w = it.value();
        if (auto* cb = qobject_cast<QCheckBox*>(w)) {
            map.insert(key, cb->isChecked());
        } else if (auto* sp = qobject_cast<QSpinBox*>(w)) {
            map.insert(key, sp->value());
        } else if (auto* dsp = qobject_cast<QDoubleSpinBox*>(w)) {
            map.insert(key, dsp->value());
        } else if (auto* le = qobject_cast<QLineEdit*>(w)) {
            map.insert(key, le->text());
        }
    }
    return map;
}

QString OdEstimationDialog::parametersJsonString() const
{
    const QVariantMap map = collectParameters();
    QJsonObject obj;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void OdEstimationDialog::appendOutputLine(const QString& text)
{
    if (mOutputEdit) {
        mOutputEdit->append(text.toHtmlEscaped());
    }
}

void OdEstimationDialog::appendResultTableLinks()
{
    if (!mOutputEdit) {
        return;
    }

    const int userId = AppSession::instance().userInfo().j_user_id();
    if (mProjectId <= 0 || userId <= 0) {
        appendOutputLine(QString::fromUtf8(u8"无法生成结果表链接：项目或用户信息无效。"));
        return;
    }

    mOutputEdit->append(QString::fromUtf8(u8"结果表：").toHtmlEscaped());
    for (const OdResultTableLink& item : kOdResultTableLinks) {
        const QString title = QString::fromUtf8(item.title);
        const QString href = QStringLiteral("%1:%2").arg(QString::fromUtf8(kOdTableScheme),
                                                         QString::fromUtf8(item.suffix));
        mOutputEdit->append(QStringLiteral("<a href=\"%1\">%2</a>")
                                .arg(href.toHtmlEscaped(), title.toHtmlEscaped()));
    }
}

void OdEstimationDialog::openResultTable(const QString& tableSuffix, const QString& title)
{
    const int userId = AppSession::instance().userInfo().j_user_id();
    if (mProjectId <= 0 || userId <= 0 || tableSuffix.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"结果表"),
                          QString::fromUtf8(u8"项目或用户信息无效，无法打开结果表。"));
        return;
    }

    const QString typeName = QStringLiteral("project%1_user%2_%3")
                                 .arg(mProjectId)
                                 .arg(userId)
                                 .arg(tableSuffix);
    const QString uri = CaseWfsMap::buildWfsDataSourceUri(typeName);
    if (uri.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"结果表"),
                          QString::fromUtf8(u8"WFS 数据源无效，无法打开结果表。"));
        return;
    }

    CaseWfsMap::waitForCapabilitiesReady();
    QgsVectorLayer layer(uri, title, OfflineApi::enabled()?QStringLiteral("postgres"):QStringLiteral("WFS"));
    if (!layer.isValid()) {
        const QString err = layer.error().message().trimmed();
        TnUi::showWarning(QString::fromUtf8(u8"结果表"),
                          err.isEmpty() ? QString::fromUtf8(u8"结果表加载失败，请确认表已生成。")
                                        : QString::fromUtf8(u8"结果表加载失败：%1").arg(err));
        return;
    }

    DataBrowserDialog dialog(this);
    dialog.setTitleText(title);
    dialog.setHeaderAliasMap(odResultTableHeaderAliasMap(tableSuffix));
    dialog.loadVectorLayer(&layer);
    dialog.moveToRightTop(this);
    dialog.exec();
}

void OdEstimationDialog::onResultTableLinkClicked(const QUrl& url)
{
    if (url.scheme() != QLatin1String(kOdTableScheme)) {
        return;
    }

    for (const OdResultTableLink& item : kOdResultTableLinks) {
        const QString expected = QStringLiteral("%1:%2").arg(QString::fromUtf8(kOdTableScheme),
                                                             QString::fromUtf8(item.suffix));
        if (url.toString() == expected) {
            openResultTable(QString::fromUtf8(item.suffix), QString::fromUtf8(item.title));
            return;
        }
    }
}

void OdEstimationDialog::requestObsCheck()
{
    mObsReady = false;
    updateRunButtonEnabled();
    if (mObsStatusLabel) {
        mObsStatusLabel->setText(QString::fromUtf8(u8"正在校验路段观测流量数据…"));
        mObsStatusLabel->show();
    }

    if (mProjectId <= 0 || mToken.isEmpty()) {
        applyObsCheckResult(false, QString::fromUtf8(u8"登录状态或项目信息无效。"), 0);
        return;
    }

    QPointer<OdEstimationDialog> self(this);
    HttpApis::checkProjectObs(mToken,
                              mProjectId,
                              [self](bool suc,
                                     const QSharedPointer<ProjectCheckObsResultJson>& response,
                                     const QString& errMsg) {
                                  if (!self) {
                                      return;
                                  }
                                  if (!suc || response.isNull()) {
                                      self->applyObsCheckResult(
                                          false,
                                          errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"请求失败")
                                                                     : errMsg.trimmed(),
                                          0);
                                      return;
                                  }
                                  self->applyObsCheckResult(true, QString(), response->j_obs());
                              });
}

void OdEstimationDialog::applyObsCheckResult(bool ok, const QString& err, int obs)
{
    mObsReady = ok && obs == 1;
    updateRunButtonEnabled();

    if (!mObsStatusLabel) {
        return;
    }

    if (mObsReady) {
        mObsStatusLabel->clear();
        mObsStatusLabel->hide();
        return;
    }

    if (!ok) {
        mObsStatusLabel->setText(QString::fromUtf8(u8"校验路段观测流量数据失败：") + err);
    } else {
        mObsStatusLabel->setText(
            QString::fromUtf8(u8"当前项目缺少路段观测流量数据，无法运行 OD 校准工具。"));
    }
    mObsStatusLabel->show();
}

void OdEstimationDialog::updateRunButtonEnabled()
{
    if (!mRunButton) {
        return;
    }
    mRunButton->setEnabled(mObsReady && !mRunning);
}

void OdEstimationDialog::onRunClicked()
{
    if (!mObsReady) {
        return;
    }
    if (mProjectId <= 0 || mToken.isEmpty()) {
        TnUi::showWarning(QString::fromUtf8(u8"OD反推"),
                             QString::fromUtf8(u8"登录状态或项目信息无效。"));
        return;
    }
    if (mStreamClient && mStreamClient->isRunning()) {
        return;
    }

    if (!mStreamClient) {
        mStreamClient = new GrpcStreamClient(this);
        connect(mStreamClient, &GrpcStreamClient::progress, this, &OdEstimationDialog::onStreamProgress);
        connect(mStreamClient,
                &GrpcStreamClient::finished,
                this,
                [this](bool success, const QString& errMsg, const QJsonObject& /*result*/) {
                    onStreamFinished(success, errMsg);
                });
    }

    const QString json = parametersJsonString();
    mRunning = true;
    if (mTabWidget) {
        mTabWidget->setCurrentIndex(1);
    }
    updateRunButtonEnabled();
    updateCancelButtonVisible();

    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    appendOutputLine(QStringLiteral("[%1] %2").arg(ts, QString::fromUtf8(u8"启动运行：OD反推")));

    mStreamClient->start(mProjectId,
                         0,
                         1,
                         QString::fromUtf8(kStreamType),
                         QString::fromUtf8(kStreamType2),
                         mToken,
                         QString(),
                         json);
}

void OdEstimationDialog::onCancelClicked()
{
    if (!mRunning) {
        return;
    }
    if (!confirmYesNo(QString::fromUtf8(u8"OD反推"), QString::fromUtf8(u8"确定取消 OD反推算法运行？"))) {
        return;
    }
    cancelRunningStream(QString::fromUtf8(u8"取消OD反推算法运行"));
}

void OdEstimationDialog::cancelRunningStream(const QString& logText)
{
    if (mStreamClient) {
        mStreamClient->stop();
    }
    mRunning = false;
    if (!logText.isEmpty()) {
        const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
        appendOutputLine(QStringLiteral("[%1] %2").arg(ts, logText));
    }
    updateRunButtonEnabled();
    updateCancelButtonVisible();
}

void OdEstimationDialog::updateCancelButtonVisible()
{
    if (!mCancelButton) {
        return;
    }
    const bool onOutputPage = mTabWidget && mTabWidget->currentIndex() == 1;
    mCancelButton->setVisible(mRunning && onOutputPage);
}

bool OdEstimationDialog::confirmYesNo(const QString& title, const QString& message)
{
    return TnUi::confirmYesNo(title, message);
}

void OdEstimationDialog::reject()
{
    if (mRunning) {
        if (!confirmYesNo(QString::fromUtf8(u8"OD反推"),
                          QString::fromUtf8(u8"当前正在运行 OD反推算法，是否取消运行并关闭？"))) {
            return;
        }
        if (mStreamClient) {
            mStreamClient->stop();
        }
        mRunning = false;
    }
    FramelessDialog::reject();
}

void OdEstimationDialog::onStreamProgress(const GrpcStreamEventJson& event)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    const QString title = event.j_data().j_title();
    const QString msg = event.j_message();
    const int pct = event.j_data().j_percent();
    QString line = QStringLiteral("[%1] ").arg(ts);
    if (!msg.isEmpty()) {
        line += msg;
    }
    if (!title.isEmpty()) {
        if (!msg.isEmpty()) {
            line += QString::fromUtf8(u8"：");
        }
        line += title;
    }
    if (pct > 0) {
        line += QStringLiteral("  %1%").arg(pct);
    }
    appendOutputLine(line);
}

void OdEstimationDialog::onStreamFinished(bool success, const QString& errMsg)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    if (success) {
        appendOutputLine(QStringLiteral("[%1] %2").arg(ts, QString::fromUtf8(u8"运行完成")));
        appendResultTableLinks();
        emit runSucceeded();
    } else {
        const QString message = errMsg.trimmed().isEmpty() ? QString::fromUtf8(u8"运行失败") : errMsg.trimmed();
        appendOutputLine(QStringLiteral("[%1] %2").arg(ts, message));
    }
    mRunning = false;
    updateRunButtonEnabled();
    updateCancelButtonVisible();
}
