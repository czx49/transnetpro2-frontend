/*****************************************************************//**
 * \file   newprojectdialog.cpp
 * \brief  新建项目对话框实现
 * \details 对齐设计稿中的轻量级新建项目弹窗
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "newprojectdialog.h"

#include "../httpapis/httpapis.h"
#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include "../publicctrls/messagedialog.h"
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

NewProjectDialog::NewProjectDialog(QWidget* parent)
    : FramelessDialog(parent)
    , m_nameEdit(nullptr)
    , m_provinceCombo(nullptr)
    , m_cityCombo(nullptr)
    , m_confirmButton(nullptr)
{
    setModal(true);
    setFixedSize(400, 320);
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);

    initUi();
}

NewProjectDialog::~NewProjectDialog()
{
}

QString NewProjectDialog::projectName() const
{
    return m_projectName;
}

QString NewProjectDialog::projectDescription() const
{
    return QString();
}

QString NewProjectDialog::projectPath() const
{
    return QString();
}

int NewProjectDialog::provinceId() const
{
    return m_provinceId;
}

QString NewProjectDialog::provinceName() const
{
    return m_provinceName;
}

int NewProjectDialog::cityId() const
{
    return m_cityId;
}

QString NewProjectDialog::cityName() const
{
    return m_cityName;
}

bool NewProjectDialog::isEditMode() const
{
    return m_isEditMode;
}

int NewProjectDialog::editProjectId() const
{
    return m_editProjectId;
}

void NewProjectDialog::prepareForEdit(const ProjectItem& item)
{
    m_isEditMode = true;
    m_editProjectId = item.id;
    m_pendingProvinceId = item.provinceId;
    m_pendingCityId = item.cityId;
    setTitleText(u8"编辑项目");

    if (m_nameEdit) {
        m_nameEdit->setText(item.name);
    }

    if (m_provincesLoaded) {
        populateProvinceCombo();
    }
    updateConfirmEnabled();
}

void NewProjectDialog::reset()
{
    m_isEditMode = false;
    m_editProjectId = 0;
    m_pendingProvinceId = 0;
    m_pendingCityId = 0;
    setTitleText(u8"新建项目");
    m_projectName.clear();
    m_provinceId = 0;
    m_provinceName.clear();
    m_cityId = 0;
    m_cityName.clear();
    m_cities.clear();
    m_citiesLoading = false;
    m_loadingCitiesForProvinceId = 0;

    if (m_nameEdit) {
        m_nameEdit->clear();
    }
    if (m_provinceCombo) {
        if (m_provincesLoaded && m_provincesAvailable && !m_provinces.isEmpty()) {
            m_provinceCombo->setCurrentIndex(0);
            loadCitiesForProvince(m_provinces.first().j_id());
        } else if (m_provincesLoaded && !m_provincesAvailable) {
            m_provinceCombo->clear();
            m_provinceCombo->addItem(u8"暂无数据");
            m_provinceCombo->setEnabled(false);
            resetCityCombo(u8"暂无数据", false);
        } else {
            m_provinceCombo->clear();
            m_provinceCombo->addItem(u8"加载中…");
            m_provinceCombo->setEnabled(false);
            resetCityCombo(u8"请先选择省", false);
        }
    }
    updateConfirmEnabled();
}

void NewProjectDialog::showEvent(QShowEvent* event)
{
    FramelessDialog::showEvent(event);
    loadProvincesIfNeeded();
    if (m_nameEdit) {
        m_nameEdit->setFocus();
    }
}

void NewProjectDialog::onNameTextChanged(const QString& text)
{
    Q_UNUSED(text)
    updateConfirmEnabled();
}

void NewProjectDialog::onProvinceIndexChanged(int index)
{
    Q_UNUSED(index)
    if (!m_provinceCombo || !m_provincesAvailable) {
        updateConfirmEnabled();
        return;
    }

    const int provinceId = m_provinceCombo->currentData().toInt();
    if (provinceId <= 0) {
        resetCityCombo(u8"请先选择省", false);
        updateConfirmEnabled();
        return;
    }

    loadCitiesForProvince(provinceId);
}

void NewProjectDialog::onCityIndexChanged(int index)
{
    Q_UNUSED(index)
    updateConfirmEnabled();
}

void NewProjectDialog::onConfirmClicked()
{
    if (!validateInput()) {
        return;
    }

    m_projectName = m_nameEdit->text().trimmed();
    if (m_provincesAvailable && m_provinceCombo && m_provinceCombo->currentData().toInt() > 0) {
        m_provinceId = m_provinceCombo->currentData().toInt();
        m_provinceName = m_provinceCombo->currentText().trimmed();
    } else {
        m_provinceId = 0;
        m_provinceName.clear();
    }

    if (m_provincesAvailable && m_cityCombo && m_cityCombo->currentData().toInt() > 0) {
        m_cityId = m_cityCombo->currentData().toInt();
        m_cityName = m_cityCombo->currentText().trimmed();
    } else {
        m_cityId = 0;
        m_cityName.clear();
    }
    accept();
}

void NewProjectDialog::initUi()
{
    setTitleText(u8"新建项目");
    setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                  + Phase2Ui::fieldStyleSheet(Phase2Ui::InputFieldTheme::LightDialogRounded));

    QVBoxLayout* mainLayout = contentLayout();
    mainLayout->setContentsMargins(20, 18, 20, 18);
    mainLayout->setSpacing(10);

    QLabel* nameLabel = new QLabel(u8"*项目名称", this);
    nameLabel->setStyleSheet(QStringLiteral("QLabel { color: #ffffff; font-size: 14px; font-weight: 600; }"));
    mainLayout->addWidget(nameLabel);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(u8"搜索项目名称...");
    connect(m_nameEdit, &QLineEdit::textChanged, this, &NewProjectDialog::onNameTextChanged);
    mainLayout->addWidget(m_nameEdit);

    QLabel* provinceLabel = new QLabel(u8"*省", this);
    provinceLabel->setStyleSheet(QStringLiteral("QLabel { color: #ffffff; font-size: 14px; font-weight: 600; }"));
    mainLayout->addWidget(provinceLabel);

    m_provinceCombo = new QComboBox(this);
    m_provinceCombo->setCursor(Qt::PointingHandCursor);
    m_provinceCombo->addItem(u8"加载中…");
    m_provinceCombo->setEnabled(false);
    Phase2Ui::setupThemedCombo(m_provinceCombo, Phase2Ui::InputFieldTheme::LightDialogRounded);
    connect(m_provinceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NewProjectDialog::onProvinceIndexChanged);
    mainLayout->addWidget(m_provinceCombo);

    QLabel* cityLabel = new QLabel(u8"*市", this);
    cityLabel->setStyleSheet(QStringLiteral("QLabel { color: #ffffff; font-size: 14px; font-weight: 600; }"));
    mainLayout->addWidget(cityLabel);

    m_cityCombo = new QComboBox(this);
    m_cityCombo->setCursor(Qt::PointingHandCursor);
    m_cityCombo->addItem(u8"请先选择省");
    m_cityCombo->setEnabled(false);
    Phase2Ui::setupThemedCombo(m_cityCombo, Phase2Ui::InputFieldTheme::LightDialogRounded);
    connect(m_cityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NewProjectDialog::onCityIndexChanged);
    mainLayout->addWidget(m_cityCombo);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 8, 0, 0);
    buttonLayout->setSpacing(16);
    buttonLayout->addStretch();

    QPushButton* cancelButton = new QPushButton(u8"取消", this);
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setFixedSize(82, 32);
    cancelButton->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(cancelButton, &QPushButton::clicked, this, &FramelessDialog::reject);

    m_confirmButton = new QPushButton(u8"确定", this);
    m_confirmButton->setCursor(Qt::PointingHandCursor);
    m_confirmButton->setFixedSize(82, 32);
    m_confirmButton->setEnabled(false);
    m_confirmButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    connect(m_confirmButton, &QPushButton::clicked, this, &NewProjectDialog::onConfirmClicked);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(m_confirmButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);
}

void NewProjectDialog::loadProvincesIfNeeded()
{
    if (m_provincesLoaded || m_provincesLoading) {
        if (m_provincesLoaded) {
            populateProvinceCombo();
        }
        return;
    }

    m_provincesLoading = true;
    if (m_provinceCombo) {
        m_provinceCombo->clear();
        m_provinceCombo->addItem(u8"加载中…");
        m_provinceCombo->setEnabled(false);
    }
    resetCityCombo(u8"加载中…", false);
    updateConfirmEnabled();

    QPointer<NewProjectDialog> self(this);
    HttpApis::getRegions(0, [self](bool suc, const QSharedPointer<RegionListJson>& response, const QString& /*errMsg*/) {
        if (!self) {
            return;
        }

        self->m_provincesLoading = false;
        if (!suc || response.isNull()) {
            self->m_provincesLoaded = true;
            self->m_provincesAvailable = false;
            if (self->m_provinceCombo) {
                self->m_provinceCombo->clear();
                self->m_provinceCombo->addItem(u8"暂无数据");
                self->m_provinceCombo->setEnabled(false);
            }
            self->resetCityCombo(u8"暂无数据", false);
            self->updateConfirmEnabled();
            return;
        }

        self->m_provinces = response->j_data();
        self->m_provincesLoaded = true;
        self->populateProvinceCombo();
    });
}

void NewProjectDialog::populateProvinceCombo()
{
    if (!m_provinceCombo) {
        return;
    }

    m_provinceCombo->blockSignals(true);
    m_provinceCombo->clear();
    if (m_provinces.isEmpty()) {
        m_provincesAvailable = false;
        m_provinceCombo->addItem(u8"暂无数据");
        m_provinceCombo->setEnabled(false);
        resetCityCombo(u8"暂无数据", false);
    } else {
        m_provincesAvailable = true;
        for (const RegionItemJson& region : m_provinces) {
            m_provinceCombo->addItem(region.j_name(), region.j_id());
        }
        m_provinceCombo->setEnabled(true);
        m_provinceCombo->setCurrentIndex(0);
    }
    m_provinceCombo->blockSignals(false);

    applyPendingProvinceSelection();
}

void NewProjectDialog::applyPendingProvinceSelection()
{
    if (!m_provinceCombo || !m_provincesAvailable) {
        updateConfirmEnabled();
        return;
    }

    int provinceId = 0;
    if (m_pendingProvinceId > 0) {
        for (int i = 0; i < m_provinceCombo->count(); ++i) {
            if (m_provinceCombo->itemData(i).toInt() == m_pendingProvinceId) {
                m_provinceCombo->blockSignals(true);
                m_provinceCombo->setCurrentIndex(i);
                m_provinceCombo->blockSignals(false);
                provinceId = m_pendingProvinceId;
                break;
            }
        }
    }

    if (provinceId <= 0 && !m_provinces.isEmpty()) {
        m_provinceCombo->blockSignals(true);
        m_provinceCombo->setCurrentIndex(0);
        m_provinceCombo->blockSignals(false);
        provinceId = m_provinces.first().j_id();
    }

    if (provinceId > 0) {
        loadCitiesForProvince(provinceId);
    } else {
        updateConfirmEnabled();
    }
}

void NewProjectDialog::loadCitiesForProvince(int provinceId)
{
    if (provinceId <= 0) {
        resetCityCombo(u8"请先选择省", false);
        updateConfirmEnabled();
        return;
    }

    m_citiesLoading = true;
    m_loadingCitiesForProvinceId = provinceId;
    resetCityCombo(u8"加载中…", false);
    updateConfirmEnabled();

    QPointer<NewProjectDialog> self(this);
    HttpApis::getRegions(provinceId, [self, provinceId](bool suc,
                                                        const QSharedPointer<RegionListJson>& response,
                                                        const QString& /*errMsg*/) {
        if (!self) {
            return;
        }

        if (self->m_loadingCitiesForProvinceId != provinceId) {
            return;
        }

        self->m_citiesLoading = false;
        self->m_loadingCitiesForProvinceId = 0;

        if (!suc || response.isNull() || response->j_data().isEmpty()) {
            self->m_cities.clear();
            self->resetCityCombo(u8"暂无数据", false);
            self->updateConfirmEnabled();
            return;
        }

        self->m_cities = response->j_data();
        self->populateCityCombo();
    });
}

void NewProjectDialog::populateCityCombo()
{
    if (!m_cityCombo) {
        return;
    }

    m_cityCombo->blockSignals(true);
    m_cityCombo->clear();
    if (m_cities.isEmpty()) {
        m_cityCombo->addItem(u8"暂无数据");
        m_cityCombo->setEnabled(false);
    } else {
        for (const RegionItemJson& city : m_cities) {
            m_cityCombo->addItem(city.j_name(), city.j_id());
        }
        m_cityCombo->setEnabled(true);
        m_cityCombo->setCurrentIndex(0);
    }
    m_cityCombo->blockSignals(false);
    applyPendingCitySelection();
    updateConfirmEnabled();
}

void NewProjectDialog::applyPendingCitySelection()
{
    if (!m_cityCombo || m_pendingCityId <= 0) {
        return;
    }

    for (int i = 0; i < m_cityCombo->count(); ++i) {
        if (m_cityCombo->itemData(i).toInt() == m_pendingCityId) {
            m_cityCombo->blockSignals(true);
            m_cityCombo->setCurrentIndex(i);
            m_cityCombo->blockSignals(false);
            break;
        }
    }
    m_pendingCityId = 0;
}

void NewProjectDialog::resetCityCombo(const QString& placeholder, bool enabled)
{
    if (!m_cityCombo) {
        return;
    }

    m_cityCombo->blockSignals(true);
    m_cityCombo->clear();
    m_cityCombo->addItem(placeholder);
    m_cityCombo->setEnabled(enabled);
    m_cityCombo->blockSignals(false);
}

void NewProjectDialog::updateConfirmEnabled()
{
    if (!m_confirmButton) {
        return;
    }

    const bool hasName = m_nameEdit && !m_nameEdit->text().trimmed().isEmpty();
    const bool loadingDone = m_provincesLoaded && !m_provincesLoading && !m_citiesLoading;
    const bool provinceOk = !m_provincesAvailable
                            || (m_provinceCombo && m_provinceCombo->currentData().toInt() > 0);
    const bool cityOk = !m_provincesAvailable
                        || (m_cityCombo && m_cityCombo->currentData().toInt() > 0);
    m_confirmButton->setEnabled(hasName && loadingDone && provinceOk && cityOk);
}

bool NewProjectDialog::validateInput() const
{
    if (!m_nameEdit || m_nameEdit->text().trimmed().isEmpty()) {
        TnUi::showWarning(u8"提示", u8"请输入项目名称。");
        return false;
    }

    if (m_provincesAvailable) {
        if (!m_provinceCombo || m_provinceCombo->currentData().toInt() <= 0) {
            TnUi::showWarning(u8"提示", u8"请选择省份。");
            return false;
        }
        if (!m_cityCombo || m_cityCombo->currentData().toInt() <= 0) {
            TnUi::showWarning(u8"提示", u8"请选择城市。");
            return false;
        }
    }

    return true;
}
