/*****************************************************************//**
 * \file   tnlayerfilterclausewidget.cpp
 * \brief  单条属性过滤子句行实现
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "tnlayerfilterclausewidget.h"

#include "../publicctrls/phase2comboboxutil.h"
#include "tnlayerfiltersqlbuilder.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QStackedWidget>
#include <QtConcurrent>

#include <algorithm>

#include <qgsvectorlayer.h>

namespace TNLayerFilter {

namespace {

QComboBox* createStyledCombo(QWidget* parent)
{
    auto* combo = new QComboBox(parent);
    combo->setMinimumHeight(30);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    Phase2Ui::setupThemedCombo(combo, Phase2Ui::InputFieldTheme::LightCompact);
    return combo;
}

QString removeButtonStyleSheet()
{
    return QStringLiteral(
        "QPushButton {"
        "   border: 1px solid #b8c7da;"
        "   border-radius: 4px;"
        "   background-color: #ffffff;"
        "   color: #64748b;"
        "   font-size: 16px;"
        "   font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "   background-color: #fef2f2;"
        "   border-color: #f87171;"
        "   color: #b91c1c;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #fee2e2;"
        "}");
}

QString uniqueValuesLoadingText()
{
    return QString::fromUtf8(u8"正在加载...");
}

bool isUniqueValuesLoadingText(const QString& text)
{
    const QString t = text.trimmed();
    return t == uniqueValuesLoadingText() || t == QString::fromUtf8(u8"正在加载…");
}

} // namespace

ClauseWidget::ClauseWidget(QgsVectorLayer* layer, bool showJoin, QWidget* parent)
    : QWidget(parent)
    , m_layer(layer)
    , m_showJoin(showJoin)
    , m_blockChanges(false)
    , m_joinCombo(nullptr)
    , m_fieldCombo(createStyledCombo(this))
    , m_operatorCombo(createStyledCombo(this))
    , m_valueStack(new QStackedWidget(this))
    , m_valueEmptyPage(new QWidget(this))
    , m_valueSinglePage(new QWidget(this))
    , m_valueBetweenPage(new QWidget(this))
    , m_valueCombo(createStyledCombo(m_valueSinglePage))
    , m_valueBetweenCombo1(createStyledCombo(m_valueBetweenPage))
    , m_valueBetweenCombo2(createStyledCombo(m_valueBetweenPage))
    , m_dateEdit1(nullptr)
    , m_dateEdit2(nullptr)
    , m_spin1(nullptr)
    , m_spin2(nullptr)
    , m_wildcardBtn(nullptr)
    , m_removeBtn(new QPushButton(QStringLiteral("×"), this))
    , m_uniqueWatcher(nullptr)
    , m_uniqueFieldIndex(-1)
{
    setMinimumHeight(40);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_valueCombo->setEditable(true);
    m_valueBetweenCombo1->setEditable(true);
    m_valueBetweenCombo2->setEditable(true);

    m_valueStack->setMinimumWidth(140);
    m_valueStack->setMinimumHeight(32);

    if (showJoin) {
        m_joinCombo = createStyledCombo(this);
        m_joinCombo->setMinimumWidth(96);
        m_joinCombo->addItem(QString::fromUtf8(u8"并且 (AND)"), static_cast<int>(JoinType::And));
        m_joinCombo->addItem(QString::fromUtf8(u8"或者 (OR)"), static_cast<int>(JoinType::Or));
    }

    m_removeBtn->setFixedSize(32, 32);
    m_removeBtn->setToolTip(QString::fromUtf8(u8"删除此子句"));
    m_removeBtn->setCursor(Qt::PointingHandCursor);
    m_removeBtn->setCheckable(false);
    m_removeBtn->setStyleSheet(removeButtonStyleSheet());

    auto* singleLay = new QHBoxLayout(m_valueSinglePage);
    singleLay->setContentsMargins(0, 0, 0, 0);
    singleLay->setSpacing(4);
    singleLay->addWidget(m_valueCombo, 1);

    m_wildcardBtn = new QPushButton(QStringLiteral("%"), m_valueSinglePage);
    m_wildcardBtn->setFixedSize(32, 32);
    m_wildcardBtn->setCheckable(false);
    m_wildcardBtn->setToolTip(QString::fromUtf8(u8"插入通配符 %"));
    m_wildcardBtn->setVisible(false);
    m_wildcardBtn->setStyleSheet(removeButtonStyleSheet());
    singleLay->addWidget(m_wildcardBtn);

    auto* betweenLay = new QHBoxLayout(m_valueBetweenPage);
    betweenLay->setContentsMargins(0, 0, 0, 0);
    betweenLay->setSpacing(4);
    betweenLay->addWidget(m_valueBetweenCombo1, 1);
    auto* betweenLabel = new QLabel(QString::fromUtf8(u8"至"), m_valueBetweenPage);
    betweenLabel->setStyleSheet(QStringLiteral("QLabel { color: #64748b; font-size: 13px; }"));
    betweenLay->addWidget(betweenLabel);
    betweenLay->addWidget(m_valueBetweenCombo2, 1);

    m_valueStack->addWidget(m_valueEmptyPage);
    m_valueStack->addWidget(m_valueSinglePage);
    m_valueStack->addWidget(m_valueBetweenPage);
    m_valueStack->setCurrentWidget(m_valueSinglePage);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);
    row->setAlignment(Qt::AlignVCenter);
    if (m_joinCombo) {
        row->addWidget(m_joinCombo);
    }
    m_fieldCombo->setMinimumWidth(100);
    m_operatorCombo->setMinimumWidth(96);
    row->addWidget(m_fieldCombo, 2);
    row->addWidget(m_operatorCombo, 2);
    row->addWidget(m_valueStack, 3);
    row->addWidget(m_removeBtn);

    connect(m_fieldCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ClauseWidget::onFieldChanged);
    connect(m_operatorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ClauseWidget::onOperatorChanged);
    connect(m_valueCombo, &QComboBox::editTextChanged, this, &ClauseWidget::onValueEdited);
    connect(m_valueBetweenCombo1, &QComboBox::editTextChanged, this, &ClauseWidget::onValueEdited);
    connect(m_valueBetweenCombo2, &QComboBox::editTextChanged, this, &ClauseWidget::onValueEdited);
    connect(m_removeBtn, &QPushButton::clicked, this, &ClauseWidget::removeRequested);
    connect(m_wildcardBtn, &QPushButton::clicked, this, &ClauseWidget::onWildcardClicked);

    setLayer(layer);
}

ClauseWidget::~ClauseWidget()
{
    if (m_uniqueWatcher) {
        m_uniqueWatcher->cancel();
        m_uniqueWatcher->waitForFinished();
        delete m_uniqueWatcher;
        m_uniqueWatcher = nullptr;
    }
}

QComboBox* ClauseWidget::activeValueCombo1() const
{
    if (m_valueStack->currentWidget() == m_valueBetweenPage) {
        return m_valueBetweenCombo1;
    }
    if (m_valueStack->currentWidget() == m_valueSinglePage) {
        return m_valueCombo;
    }
    return nullptr;
}

QComboBox* ClauseWidget::activeValueCombo2() const
{
    if (m_valueStack->currentWidget() == m_valueBetweenPage) {
        return m_valueBetweenCombo2;
    }
    return nullptr;
}

QList<QComboBox*> ClauseWidget::activeValueCombosForLoad() const
{
    QList<QComboBox*> list;
    if (m_valueStack->currentWidget() == m_valueBetweenPage) {
        list << m_valueBetweenCombo1 << m_valueBetweenCombo2;
    } else if (m_valueStack->currentWidget() == m_valueSinglePage) {
        list << m_valueCombo;
    }
    return list;
}

void ClauseWidget::setLayer(QgsVectorLayer* layer)
{
    m_layer = layer;
    m_blockChanges = true;
    m_fieldCombo->clear();
    if (m_layer) {
        const QgsFields fields = m_layer->fields();
        for (const QgsField& f : fields) {
            m_fieldCombo->addItem(f.displayName(), f.name());
        }
    }
    m_blockChanges = false;
    if (m_fieldCombo->count() > 0) {
        onFieldChanged(0);
    } else {
        rebuildOperatorCombo();
        rebuildValueWidgets();
    }
}

void ClauseWidget::setShowJoin(bool showJoin)
{
    m_showJoin = showJoin;
    if (m_joinCombo) {
        m_joinCombo->setVisible(showJoin);
    }
}

FieldKind ClauseWidget::currentFieldKind() const
{
    return fieldKindForLayerField(m_layer, currentFieldName());
}

QString ClauseWidget::currentFieldName() const
{
    return m_fieldCombo->currentData().toString();
}

Operator ClauseWidget::currentOperator() const
{
    return static_cast<Operator>(m_operatorCombo->currentData().toInt());
}

void ClauseWidget::rebuildOperatorCombo()
{
    m_blockChanges = true;
    const FieldKind kind = currentFieldKind();
    const Operator prev = m_operatorCombo->count() > 0 ? currentOperator() : Operator::Eq;

    m_operatorCombo->clear();
    const QStringList codes = operatorCodesForFieldKind(kind);
    for (const QString& code : codes) {
        const Operator op = operatorFromCode(code);
        m_operatorCombo->addItem(operatorDisplayName(op, kind), static_cast<int>(op));
    }

    int idx = 0;
    for (int i = 0; i < m_operatorCombo->count(); ++i) {
        if (static_cast<Operator>(m_operatorCombo->itemData(i).toInt()) == prev) {
            idx = i;
            break;
        }
    }
    m_operatorCombo->setCurrentIndex(idx);
    m_blockChanges = false;
    applyOperatorVisibility();
}

void ClauseWidget::rebuildValueWidgets()
{
    if (m_dateEdit1) {
        m_dateEdit1->deleteLater();
        m_dateEdit1 = nullptr;
    }
    if (m_dateEdit2) {
        m_dateEdit2->deleteLater();
        m_dateEdit2 = nullptr;
    }
    if (m_spin1) {
        m_spin1->deleteLater();
        m_spin1 = nullptr;
    }
    if (m_spin2) {
        m_spin2->deleteLater();
        m_spin2 = nullptr;
    }

    const FieldKind kind = currentFieldKind();
    const Operator op = currentOperator();

    if (!SqlBuilder::operatorNeedsValue(op)) {
        m_valueStack->setCurrentWidget(m_valueEmptyPage);
        m_wildcardBtn->setVisible(false);
        return;
    }

    if (SqlBuilder::operatorNeedsTwoValues(op)) {
        m_valueStack->setCurrentWidget(m_valueBetweenPage);
        m_wildcardBtn->setVisible(false);
    } else {
        m_valueStack->setCurrentWidget(m_valueSinglePage);
        const bool likeOp = op == Operator::LikeContains || op == Operator::LikeStarts || op == Operator::LikeEnds;
        m_wildcardBtn->setVisible(likeOp && kind == FieldKind::String);
    }

    if (kind == FieldKind::Boolean) {
        m_blockChanges = true;
        m_valueCombo->clear();
        m_valueCombo->addItem(QStringLiteral("true"), true);
        m_valueCombo->addItem(QStringLiteral("false"), false);
        m_valueCombo->setEditable(false);
        m_blockChanges = false;
        return;
    }

    m_valueCombo->setEditable(true);
    m_valueBetweenCombo1->setEditable(true);
    m_valueBetweenCombo2->setEditable(true);
    startLoadUniqueValues();
}

void ClauseWidget::setValueCombosLoading(bool loading)
{
    const QString loadingText = uniqueValuesLoadingText();
    m_blockChanges = true;
    for (QComboBox* combo : activeValueCombosForLoad()) {
        combo->blockSignals(true);
        if (loading) {
            combo->clear();
            combo->addItem(loadingText);
            combo->setCurrentIndex(0);
            combo->setEditable(false);
            combo->setEnabled(false);
        } else {
            combo->setEnabled(true);
        }
        combo->blockSignals(false);
    }
    m_blockChanges = false;
}

void ClauseWidget::clearValueComboToEmpty(QComboBox* combo)
{
    if (!combo) {
        return;
    }
    combo->blockSignals(true);
    combo->setCurrentIndex(-1);
    if (combo->lineEdit()) {
        combo->lineEdit()->clear();
    }
    combo->blockSignals(false);
}

void ClauseWidget::startLoadUniqueValues()
{
    if (!m_layer) {
        return;
    }
    const QString fieldName = currentFieldName();
    const int fieldIdx = m_layer->fields().indexFromName(fieldName);
    if (fieldIdx < 0) {
        return;
    }

    const FieldKind kind = currentFieldKind();
    if (kind == FieldKind::Boolean) {
        return;
    }

    if (m_uniqueWatcher) {
        m_uniqueWatcher->cancel();
        m_uniqueWatcher->waitForFinished();
        delete m_uniqueWatcher;
        m_uniqueWatcher = nullptr;
    }

    m_uniqueFieldIndex = fieldIdx;
    setValueCombosLoading(true);

    QgsVectorLayer* layer = m_layer;
    const int idx = fieldIdx;

    m_uniqueWatcher = new QFutureWatcher<QSet<QVariant>>(this);
    connect(m_uniqueWatcher, &QFutureWatcher<QSet<QVariant>>::finished, this, &ClauseWidget::onUniqueValuesFinished);

    m_uniqueWatcher->setFuture(QtConcurrent::run([layer, idx]() -> QSet<QVariant> {
        if (!layer) {
            return {};
        }
        return layer->uniqueValues(idx, kMaxUniqueValues);
    }));
}

void ClauseWidget::onUniqueValuesFinished()
{
    if (!m_uniqueWatcher) {
        return;
    }

    const int requestedIdx = m_uniqueFieldIndex;
    m_uniqueFieldIndex = -1;

    const bool wasCanceled = m_uniqueWatcher->isCanceled();
    QSet<QVariant> values;
    if (!wasCanceled) {
        values = m_uniqueWatcher->result();
    }
    delete m_uniqueWatcher;
    m_uniqueWatcher = nullptr;

    if (!m_layer || m_layer->fields().indexFromName(currentFieldName()) != requestedIdx) {
        return;
    }

    if (wasCanceled) {
        for (QComboBox* combo : activeValueCombosForLoad()) {
            combo->blockSignals(true);
            combo->clear();
            combo->setEditable(currentFieldKind() != FieldKind::Boolean);
            combo->setEnabled(true);
            clearValueComboToEmpty(combo);
            combo->blockSignals(false);
        }
        return;
    }

    const FieldKind kind = currentFieldKind();
    const bool editable = kind != FieldKind::Boolean;

    auto fillCombo = [&](QComboBox* combo) {
        combo->blockSignals(true);
        combo->clear();
        combo->setEditable(editable);
        combo->setEnabled(true);

        QList<QVariant> sorted = values.values();
        std::sort(sorted.begin(), sorted.end(), [](const QVariant& a, const QVariant& b) {
            return a.toString() < b.toString();
        });
        int count = 0;
        for (const QVariant& v : sorted) {
            if (v.isNull()) {
                combo->addItem(QString::fromUtf8(u8"(空)"), QString());
            } else {
                combo->addItem(v.toString(), v);
            }
            ++count;
            if (count >= kMaxUniqueValues) {
                break;
            }
        }
        if (values.size() >= kMaxUniqueValues) {
            combo->addItem(QString::fromUtf8(u8"（仅显示前 %1 个唯一值）").arg(kMaxUniqueValues));
            combo->setItemData(combo->count() - 1, QVariant(), Qt::UserRole - 1);
        }

        clearValueComboToEmpty(combo);
        combo->blockSignals(false);
    };

    m_blockChanges = true;
    for (QComboBox* combo : activeValueCombosForLoad()) {
        fillCombo(combo);
    }

    if (m_hasRestoreValue1) {
        setValueOnCombo(activeValueCombo1(), m_restoreValue1);
        m_hasRestoreValue1 = false;
    }
    if (m_hasRestoreValue2) {
        setValueOnCombo(activeValueCombo2(), m_restoreValue2);
        m_hasRestoreValue2 = false;
    }

    m_blockChanges = false;
}

void ClauseWidget::applyOperatorVisibility()
{
    rebuildValueWidgets();
}

void ClauseWidget::onFieldChanged(int index)
{
    Q_UNUSED(index)
    if (m_blockChanges) {
        return;
    }
    m_hasRestoreValue1 = false;
    m_hasRestoreValue2 = false;
    rebuildOperatorCombo();
    emit clauseChanged();
}

void ClauseWidget::onOperatorChanged(int index)
{
    Q_UNUSED(index)
    if (m_blockChanges) {
        return;
    }
    applyOperatorVisibility();
    emit clauseChanged();
}

void ClauseWidget::onValueEdited()
{
    if (m_blockChanges) {
        return;
    }
    emit clauseChanged();
}

void ClauseWidget::onWildcardClicked()
{
    if (QComboBox* combo = activeValueCombo1()) {
        combo->setEditText(combo->currentText() + QLatin1Char('%'));
        emit clauseChanged();
    }
}

QVariant ClauseWidget::valueFromCombo(const QComboBox* combo) const
{
    if (!combo) {
        return QVariant();
    }
    const FieldKind kind = currentFieldKind();
    const QString text = combo->currentText().trimmed();
    if (!combo->isEnabled() || isUniqueValuesLoadingText(text)) {
        return QVariant();
    }
    if (text == QString::fromUtf8(u8"(空)")) {
        return QString();
    }
    if (kind == FieldKind::Boolean && !combo->isEditable()) {
        return combo->currentData();
    }
    if (kind == FieldKind::Numeric) {
        bool ok = false;
        const double d = text.toDouble(&ok);
        if (ok) {
            return d;
        }
    }
    if (combo->currentData().isValid() && !combo->currentData().toString().isEmpty()) {
        return combo->currentData();
    }
    return text;
}

QVariant ClauseWidget::value1() const
{
    return valueFromCombo(activeValueCombo1());
}

QVariant ClauseWidget::value2() const
{
    return valueFromCombo(activeValueCombo2());
}

void ClauseWidget::setValueOnCombo(QComboBox* combo, const QVariant& v)
{
    if (!combo || !v.isValid()) {
        return;
    }
    const int idx = combo->findData(v);
    if (idx >= 0) {
        combo->setCurrentIndex(idx);
    } else {
        combo->setEditText(v.toString());
    }
}

void ClauseWidget::setValue1(const QVariant& v)
{
    setValueOnCombo(activeValueCombo1(), v);
    if (m_valueStack->currentWidget() == m_valueSinglePage) {
        setValueOnCombo(m_valueCombo, v);
    }
}

void ClauseWidget::setValue2(const QVariant& v)
{
    setValueOnCombo(activeValueCombo2(), v);
}

Clause ClauseWidget::clause() const
{
    Clause c;
    if (m_joinCombo) {
        c.join = static_cast<JoinType>(m_joinCombo->currentData().toInt());
    }
    c.fieldName = currentFieldName();
    c.op = currentOperator();
    if (SqlBuilder::operatorNeedsValue(c.op)) {
        c.value1 = value1();
        if (SqlBuilder::operatorNeedsTwoValues(c.op)) {
            c.value2 = value2();
        }
    }
    return c;
}

void ClauseWidget::setClause(const Clause& clause)
{
    m_blockChanges = true;

    if (m_joinCombo && clause.join != JoinType::None) {
        const int jidx = m_joinCombo->findData(static_cast<int>(clause.join));
        if (jidx >= 0) {
            m_joinCombo->setCurrentIndex(jidx);
        }
    }

    const int fidx = m_fieldCombo->findData(clause.fieldName);
    if (fidx >= 0) {
        m_fieldCombo->setCurrentIndex(fidx);
    }

    rebuildOperatorCombo();

    const int opIdx = m_operatorCombo->findData(static_cast<int>(clause.op));
    if (opIdx >= 0) {
        m_operatorCombo->setCurrentIndex(opIdx);
    }

    m_hasRestoreValue1 = false;
    m_hasRestoreValue2 = false;
    if (SqlBuilder::operatorNeedsValue(clause.op)) {
        m_hasRestoreValue1 = true;
        m_restoreValue1 = clause.value1;
        if (SqlBuilder::operatorNeedsTwoValues(clause.op)) {
            m_hasRestoreValue2 = true;
            m_restoreValue2 = clause.value2;
        }
    }

    applyOperatorVisibility();

    const FieldKind kind = fieldKindForLayerField(m_layer, clause.fieldName);
    if (kind == FieldKind::Boolean && SqlBuilder::operatorNeedsValue(clause.op)) {
        setValue1(clause.value1);
        if (SqlBuilder::operatorNeedsTwoValues(clause.op)) {
            setValue2(clause.value2);
        }
        m_hasRestoreValue1 = false;
        m_hasRestoreValue2 = false;
    } else if (!m_uniqueWatcher && SqlBuilder::operatorNeedsValue(clause.op)) {
        if (m_hasRestoreValue1) {
            setValue1(m_restoreValue1);
            m_hasRestoreValue1 = false;
        }
        if (m_hasRestoreValue2) {
            setValue2(m_restoreValue2);
            m_hasRestoreValue2 = false;
        }
    }

    m_blockChanges = false;
}

bool ClauseWidget::isComplete() const
{
    const Clause c = clause();
    if (!c.isValid()) {
        return false;
    }
    if (!SqlBuilder::operatorNeedsValue(c.op)) {
        return true;
    }
    if (SqlBuilder::operatorNeedsTwoValues(c.op)) {
        return c.value1.isValid() && c.value2.isValid();
    }
    return c.value1.isValid();
}

} // namespace TNLayerFilter
