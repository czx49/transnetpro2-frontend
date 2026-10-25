/*****************************************************************//**
 * \file   casesymbolstyledialog.h
 * \brief  矢量图层字段分级符号化设置（FramelessDialog）
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef CASESYMBOLSTYLEDIALOG_H
#define CASESYMBOLSTYLEDIALOG_H

#include "../publicctrls/framelessdialog.h"
#include "../tnmapwidget/layerstylepersistence.h"

class TNMapWidget;
class QgsVectorLayer;
class QLabel;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QListWidget;
class QPushButton;

/** @brief 字段分级符号化设置；parent 建议为 nullptr */
class CaseSymbolStyleDialog : public FramelessDialog {
    Q_OBJECT

public:
    CaseSymbolStyleDialog(TNMapWidget* mapWidget, int projectId, int caseId, QWidget* parent = nullptr);
    ~CaseSymbolStyleDialog() override;

private slots:
    void onLayerChanged(int index);
    void onFieldChanged(int index);
    void onClassLevelChanged(int index);
    void onColorRampChanged(int index);
    void onRampInvertedChanged(int state);
    void onColorItemClicked();
    void onApplyFieldRuleClicked();
    void onSaveFieldRuleClicked();
    void onLoadFieldRuleClicked();
    void onClearFieldRuleClicked();
    void onEditSymbolClicked();

private:
    void initUi();
    void refreshLayerCombo();
    void refreshFieldCombo();
    void populateColorRampCombo();
    void syncColorRampControlsFromRule();
    void loadCurrentFieldRule(bool applyAfterLoad);
    bool generateFieldColorRule();
    bool applyCurrentFieldRule();
    void syncLegendFromAppliedRule();
    void updateColorList();
    QString currentFieldName() const;
    QString currentLayerKey() const;
    QString currentColorRampName() const;
    bool currentRampInverted() const;
    QList<double> numericFieldValues(const QString& fieldName) const;
    QgsVectorLayer* vectorLayerById(const QString& layerId) const;
    static QString layerStorageKey(QgsVectorLayer* layer);
    static QPixmap colorRampPreviewPixmap(const QString& colorRampName, bool inverted, int width = 72, int height = 16);

    TNMapWidget* m_mapWidget;
    int m_projectId;
    int m_caseId;
    QgsVectorLayer* m_layer;
    QComboBox* m_layerCombo;
    QComboBox* m_fieldCombo;
    QComboBox* m_classCombo;
    QComboBox* m_colorRampCombo;
    QCheckBox* m_rampInvertCheck;
    QDoubleSpinBox* m_opacitySpin;
    QDoubleSpinBox* m_lineWidthSpin;
    QListWidget* m_colorList;
    QLabel* m_statusLabel;
    FieldColorRule m_fieldRule;
};

#endif // CASESYMBOLSTYLEDIALOG_H
