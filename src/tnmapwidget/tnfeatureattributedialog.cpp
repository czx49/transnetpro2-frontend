#include "tnfeatureattributedialog.h"

#include "../publicctrls/numberdisplayutil.h"
#include "../publicctrls/phase2dialogstyle.h"

#include <QDate>
#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QMetaType>
#include <QPushButton>

namespace {

const char* kTableStyle =
    "QTableWidget { background: #ffffff; color: #1e293b; gridline-color: #e2e8f0; "
    "alternate-background-color: #f8fafc; border: 1px solid #cbd5e1; border-radius: 0; }"
    "QTableWidget::item:selected { background: #dbeafe; color: #0f172a; }"
    "QHeaderView::section { background: #f1f5f9; color: #0f172a; padding: 8px 10px; border: none; "
    "border-right: 1px solid #e2e8f0; border-bottom: 1px solid #cbd5e1; font-weight: 600; }";

} // namespace

TNFeatureAttributeDialog::TNFeatureAttributeDialog(QgsVectorLayer* layer,
                                                   const QgsFeature& feature,
                                                   QWidget* parent,
                                                   const QString& titleText,
                                                   const QStringList& readOnlyFieldNames)
    : FramelessDialog(parent)
    , mLayer(layer)
    , mFeature(feature)
    , mReadOnlyFieldNames(readOnlyFieldNames.begin(), readOnlyFieldNames.end())
{
    setTitleText(titleText.isEmpty() ? QString::fromUtf8(u8"编辑要素属性") : titleText);
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    setMinimumSize(600, 400);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet());
    setupUI();
    loadFeatureAttributes();
}

TNFeatureAttributeDialog::~TNFeatureAttributeDialog() = default;

void TNFeatureAttributeDialog::setupUI()
{
    QVBoxLayout* mainLayout = contentLayout();
    mainLayout->setContentsMargins(16, 12, 16, 16);
    mainLayout->setSpacing(12);

    mAttributeTable = new QTableWidget(this);
    mAttributeTable->setObjectName(QStringLiteral("tnFeatureAttrTable"));
    mAttributeTable->setColumnCount(3);
    mAttributeTable->setHorizontalHeaderLabels(QStringList()
                                               << QString::fromUtf8(u8"字段") << QString::fromUtf8(u8"值")
                                               << QString::fromUtf8(u8"类型"));
    mAttributeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    mAttributeTable->verticalHeader()->setVisible(false);
    mAttributeTable->setAlternatingRowColors(true);
    mAttributeTable->setStyleSheet(QString::fromLatin1(kTableStyle));
    mainLayout->addWidget(mAttributeTable);

    QPushButton* okBtn = new QPushButton(QString::fromUtf8(u8"确定"), this);
    QPushButton* cancelBtn = new QPushButton(QString::fromUtf8(u8"取消"), this);
    okBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    cancelBtn->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(okBtn, &QPushButton::clicked, this, &TNFeatureAttributeDialog::onAccepted);
    connect(cancelBtn, &QPushButton::clicked, this, &TNFeatureAttributeDialog::reject);
    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);
}

void TNFeatureAttributeDialog::loadFeatureAttributes()
{
    if (!mLayer || !mFeature.isValid()) {
        return;
    }

    const QgsFields& fields = mLayer->fields();
    mAttributeTable->setRowCount(fields.count());

    for (int i = 0; i < fields.count(); ++i)
    {
        const QgsField& field = fields.at(i);

        QTableWidgetItem* nameItem = new QTableWidgetItem(field.displayName());
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        mAttributeTable->setItem(i, 0, nameItem);

        QTableWidgetItem* valueItem = new QTableWidgetItem(
            NumberDisplayUtil::formatAttributeDisplay(mFeature.attribute(i), static_cast<int>(field.type())));
        if (field.isReadOnly() || mReadOnlyFieldNames.contains(field.name()))
        {
            valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        }
        mAttributeTable->setItem(i, 1, valueItem);

        QTableWidgetItem* typeItem = new QTableWidgetItem(field.friendlyTypeString());
        typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
        mAttributeTable->setItem(i, 2, typeItem);
    }
}

void TNFeatureAttributeDialog::onAccepted()
{
    mEditedAttributes.clear();

    for (int i = 0; i < mAttributeTable->rowCount(); ++i)
    {
        const QgsField& field = mLayer->fields().at(i);
        if (field.isReadOnly() || mReadOnlyFieldNames.contains(field.name())) {
            continue;
        }

        QTableWidgetItem* valueItem = mAttributeTable->item(i, 1);
        if (!valueItem) {
            continue;
        }

        const QVariant original = mFeature.attribute(i);
        const QVariant::Type type = static_cast<QVariant::Type>(field.type());
        bool changed = false;
        if (NumberDisplayUtil::isFloatingPointType(static_cast<int>(type))) {
            const QString originalDisplay = NumberDisplayUtil::formatAttributeDisplay(original, static_cast<int>(type));
            changed = (valueItem->text() != originalDisplay);
        } else {
            changed = (valueItem->text() != original.toString());
        }

        if (changed)
        {
            QVariant value = valueItem->text();

            if (type == QVariant::Int) {
                value = value.toInt();
            }
            else if (type == QVariant::Double || NumberDisplayUtil::isFloatingPointType(static_cast<int>(type))) {
                value = value.toDouble();
            }
            else if (type == QVariant::Date) {
                value = QDate::fromString(value.toString(), Qt::ISODate);
            }
            else if (type == QVariant::DateTime) {
                value = QDateTime::fromString(value.toString(), Qt::ISODate);
            }

            mEditedAttributes.insert(i, value);
        }
    }

    accept();
}
