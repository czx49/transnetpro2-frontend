/*****************************************************************//**
 * \file   tnlayerfilterclausewidget.h
 * \brief  单条属性过滤子句行
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef TNLAYERFILTERCLAUSEWIDGET_H
#define TNLAYERFILTERCLAUSEWIDGET_H

#include "tnlayerfiltertypes.h"

#include <QSet>
#include <QVariant>
#include <QWidget>

#include <QFutureWatcher>

class QComboBox;
class QDateTimeEdit;
class QDoubleSpinBox;
class QHBoxLayout;
class QPushButton;
class QStackedWidget;
class QgsVectorLayer;

namespace TNLayerFilter {

QString filterComboStyleSheet();

class ClauseWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ClauseWidget(QgsVectorLayer* layer, bool showJoin, QWidget* parent = nullptr);
    ~ClauseWidget() override;

    void setLayer(QgsVectorLayer* layer);
    void setShowJoin(bool showJoin);

    Clause clause() const;
    void setClause(const Clause& clause);

    bool isComplete() const;

signals:
    void clauseChanged();
    void removeRequested();

private slots:
    void onFieldChanged(int index);
    void onOperatorChanged(int index);
    void onValueEdited();
    void onUniqueValuesFinished();
    void onWildcardClicked();

private:
    void rebuildOperatorCombo();
    void rebuildValueWidgets();
    void startLoadUniqueValues();
    void setValueCombosLoading(bool loading);
    void clearValueComboToEmpty(QComboBox* combo);
    void applyOperatorVisibility();
    FieldKind currentFieldKind() const;
    QString currentFieldName() const;
    Operator currentOperator() const;

    QComboBox* activeValueCombo1() const;
    QComboBox* activeValueCombo2() const;
    QList<QComboBox*> activeValueCombosForLoad() const;

    void setValue1(const QVariant& v);
    void setValue2(const QVariant& v);
    QVariant value1() const;
    QVariant value2() const;
    QVariant valueFromCombo(const QComboBox* combo) const;
    void setValueOnCombo(QComboBox* combo, const QVariant& v);

    QgsVectorLayer* m_layer;
    bool m_showJoin;
    bool m_blockChanges;

    QComboBox* m_joinCombo;
    QComboBox* m_fieldCombo;
    QComboBox* m_operatorCombo;
    QStackedWidget* m_valueStack;
    QWidget* m_valueEmptyPage;
    QWidget* m_valueSinglePage;
    QWidget* m_valueBetweenPage;
    QComboBox* m_valueCombo;
    QComboBox* m_valueBetweenCombo1;
    QComboBox* m_valueBetweenCombo2;
    QDateTimeEdit* m_dateEdit1;
    QDateTimeEdit* m_dateEdit2;
    QDoubleSpinBox* m_spin1;
    QDoubleSpinBox* m_spin2;
    QPushButton* m_wildcardBtn;
    QPushButton* m_removeBtn;

    QFutureWatcher<QSet<QVariant>>* m_uniqueWatcher;
    int m_uniqueFieldIndex;

    bool m_hasRestoreValue1 = false;
    bool m_hasRestoreValue2 = false;
    QVariant m_restoreValue1;
    QVariant m_restoreValue2;
};

} // namespace TNLayerFilter

#endif // TNLAYERFILTERCLAUSEWIDGET_H
