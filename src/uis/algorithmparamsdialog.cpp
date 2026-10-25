/*****************************************************************//**
 * \file   algorithmparamsdialog.cpp
 * \brief  算法参数配置对话框实现
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "algorithmparamsdialog.h"

#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"
#include "algorithmconfigjson.h"

#include <QCheckBox>
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
#include "../publicctrls/messagedialog.h"
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include "../jsonreader/algorithmconfigjson.h"

namespace {
constexpr char kConfigResource[] = ":/config/algorithmConfig.json";
}

AlgorithmParamsDialog::AlgorithmParamsDialog(QWidget* parent)
    : FramelessDialog(parent)
{
    setTitleText(QString::fromUtf8(u8"算法参数"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    resize(520, 420);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                  + QStringLiteral(
                      "#framelessDialogContent { background: transparent; }"
                      "#framelessDialogContent QWidget { background: transparent; }"));
}

bool AlgorithmParamsDialog::loadAlgorithm(const QString& algorithmName)
{
    m_algorithmName = algorithmName;
    clearForm();

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
        if (alg.j_algorithmName() == algorithmName) {
            return buildForm(alg);
        }
    }

    TnUi::showWarning(QString::fromUtf8(u8"算法配置"),
                         QString::fromUtf8(u8"未找到算法：%1").arg(algorithmName));
    return false;
}

void AlgorithmParamsDialog::clearForm()
{
    m_fieldEditors.clear();
    while (QLayoutItem* it = contentLayout()->takeAt(0)) {
        if (it->widget()) {
            it->widget()->deleteLater();
        }
        delete it;
    }
}

bool AlgorithmParamsDialog::buildForm(const AlgorithmNameJson& alg)
{
    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("algorithmParamsRoot"));
    root->setAttribute(Qt::WA_StyledBackground, true);
    root->setStyleSheet(
        QStringLiteral(
            "QWidget#algorithmParamsRoot {"
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
        + Phase2Ui::fieldStyleSheet(Phase2Ui::InputFieldTheme::LightCompact));
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(18, 12, 18, 12);
    rootLayout->setSpacing(12);

    QLabel* title = new QLabel(alg.j_displayName(), root);
    title->setStyleSheet(QStringLiteral("QLabel { font-weight: 700; font-size: 15px; color: #ffffff; }"));
    rootLayout->addWidget(title);

    auto* scroll = new QScrollArea(root);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* formHost = new QWidget(scroll);
    formHost->setObjectName(QStringLiteral("algorithmParamsFormHost"));
    formHost->setAttribute(Qt::WA_StyledBackground, true);
    formHost->setStyleSheet(QStringLiteral("QWidget#algorithmParamsFormHost { background-color: transparent; }"));
    auto* form = new QFormLayout(formHost);
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
            auto* cb = new QCheckBox(formHost);
            cb->setChecked(def.toBool());
            editor = cb;
        } else if (dt == QLatin1String("integer")) {
            auto* sp = new QSpinBox(formHost);
            sp->setRange(-2147483647, 2147483647);
            sp->setValue(def.toInt());
            editor = sp;
        } else if (dt == QLatin1String("float") || dt == QLatin1String("double")) {
            auto* dsp = new QDoubleSpinBox(formHost);
            dsp->setDecimals(8);
            dsp->setRange(-1e12, 1e12);
            dsp->setValue(def.toDouble());
            editor = dsp;
        } else {
            auto* le = new QLineEdit(formHost);
            le->setText(def.toString());
            editor = le;
        }

        const QString labelText = p.j_tranlateName().isEmpty() ? field : p.j_tranlateName();
        QString tip = p.j_desc();
        if (!p.j_remarks().isEmpty()) {
            tip += QStringLiteral("\n") + p.j_remarks();
        }
        auto* label = new QLabel(labelText + QStringLiteral(":"), formHost);
        label->setToolTip(tip);
        editor->setToolTip(tip);

        form->addRow(label, editor);
        m_fieldEditors.insert(field, editor);
    }

    scroll->setWidget(formHost);
    rootLayout->addWidget(scroll, 1);

    contentLayout()->addWidget(root);

    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(18, 0, 18, 12);
    btnRow->setSpacing(10);
    auto* okBtn = new QPushButton(QString::fromUtf8(u8"确定"), this);
    auto* cancelBtn = new QPushButton(QString::fromUtf8(u8"取消"), this);
    okBtn->setFixedHeight(32);
    cancelBtn->setFixedHeight(32);
    okBtn->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    cancelBtn->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    btnRow->addStretch();
    contentLayout()->addLayout(btnRow);

    return !m_fieldEditors.isEmpty();
}

QVariantMap AlgorithmParamsDialog::parameters() const
{
    QVariantMap map;
    for (auto it = m_fieldEditors.constBegin(); it != m_fieldEditors.constEnd(); ++it) {
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

QString AlgorithmParamsDialog::parametersJsonString() const
{
    const QVariantMap map = parameters();
    QJsonObject obj;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
