/*****************************************************************//**
 * \file   newprojectdialog.h
 * \brief  新建项目对话框
 * \details 对齐首页设计图的小型新建项目弹窗，支持项目名称与省市选择
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#ifndef NEWPROJECTDIALOG_H
#define NEWPROJECTDIALOG_H

#include "framelessdialog.h"
#include "httpapijson.h"
#include "projectitemmodel.h"

class QComboBox;
class QLineEdit;
class QPushButton;

class NewProjectDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget* parent = nullptr);
    ~NewProjectDialog() override;

    QString projectName() const;
    QString projectDescription() const;
    QString projectPath() const;
    int provinceId() const;
    QString provinceName() const;
    int cityId() const;
    QString cityName() const;
    bool isEditMode() const;
    int editProjectId() const;
    void reset();
    void prepareForEdit(const ProjectItem& item);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onNameTextChanged(const QString& text);
    void onProvinceIndexChanged(int index);
    void onCityIndexChanged(int index);
    void onConfirmClicked();

private:
    void initUi();
    void loadProvincesIfNeeded();
    void populateProvinceCombo();
    void loadCitiesForProvince(int provinceId);
    void populateCityCombo();
    void resetCityCombo(const QString& placeholder, bool enabled);
    void applyPendingProvinceSelection();
    void applyPendingCitySelection();
    void updateConfirmEnabled();
    bool validateInput() const;

private:
    QLineEdit* m_nameEdit;
    QComboBox* m_provinceCombo;
    QComboBox* m_cityCombo;
    QPushButton* m_confirmButton;
    QString m_projectName;
    int m_provinceId = 0;
    QString m_provinceName;
    int m_cityId = 0;
    QString m_cityName;
    QList<RegionItemJson> m_provinces;
    QList<RegionItemJson> m_cities;
    bool m_provincesLoaded = false;
    bool m_provincesLoading = false;
    bool m_provincesAvailable = false;
    bool m_citiesLoading = false;
    int m_loadingCitiesForProvinceId = 0;
    bool m_isEditMode = false;
    int m_editProjectId = 0;
    int m_pendingProvinceId = 0;
    int m_pendingCityId = 0;
};

#endif // NEWPROJECTDIALOG_H
