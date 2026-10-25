#include "tnselectionsetdialog.h"

#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

TNSelectionSetDialog::TNSelectionSetDialog(QWidget* parent)
    : FramelessDialog(parent)
{
    setTitleText(QString::fromUtf8(u8"选择集"));
    setMinimumSize(800, 500);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet());

    auto mainLayout = contentLayout();
    mainLayout->setContentsMargins(16, 12, 16, 16);
    mainLayout->setSpacing(12);

    auto topLayout = new QHBoxLayout();
    auto lbl = new QLabel(QString::fromUtf8(u8"图层："), this);
    lbl->setStyleSheet(QStringLiteral("QLabel { color: #ffffff; font-weight: 600; background: transparent; }"));
    mLayerCombo = new QComboBox(this);
    mLayerCombo->setEditable(false);
    mLayerCombo->setMinimumHeight(30);
    Phase2Ui::setupThemedCombo(mLayerCombo, Phase2Ui::InputFieldTheme::LightCompactSoft);
    connect(mLayerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TNSelectionSetDialog::onLayerComboChanged);
    topLayout->addWidget(lbl);
    topLayout->addWidget(mLayerCombo, 1);
    mainLayout->addLayout(topLayout);

    auto* tableFrame = new QFrame(this);
    tableFrame->setObjectName(QStringLiteral("tnSelectionAttrFrame"));
    tableFrame->setStyleSheet(QStringLiteral(
        "QFrame#tnSelectionAttrFrame { background: #ffffff; border: 1px solid #cbd5e1; border-radius: 8px; }"));
    auto* frameLay = new QVBoxLayout(tableFrame);
    frameLay->setContentsMargins(8, 8, 8, 8);
    mAttrWidget = new TNAttributeWidget(tableFrame);
    mAttrWidget->setPhase2ChromeEnabled(true);
    frameLay->addWidget(mAttrWidget);
    mainLayout->addWidget(tableFrame, 1);
}

TNSelectionSetDialog::~TNSelectionSetDialog() = default;

void TNSelectionSetDialog::setCanvas(QgsMapCanvas* canvas)
{
    mCanvas = canvas;
    if (mAttrWidget) {
        mAttrWidget->setCanvas(mCanvas);
    }
}

void TNSelectionSetDialog::addOrRefreshLayer(QgsVectorLayer* layer, const QgsFeatureIds& currentSelection)
{
    if (!layer) {
        return;
    }
    const QString id = layer->id();
    const QString name = layer->name();

    bool exists = mLayers.contains(id);
    mLayers[id] = {layer, currentSelection};

    if (!exists)
    {
        mLayerCombo->addItem(name, id);
    }
    else
    {
        for (int i = 0; i < mLayerCombo->count(); ++i)
        {
            if (mLayerCombo->itemData(i).toString() == id)
            {
                if (mLayerCombo->itemText(i) != name) {
                    mLayerCombo->setItemText(i, name);
                }
                break;
            }
        }
    }
}

void TNSelectionSetDialog::setCurrentLayer(QgsVectorLayer* layer)
{
    if (!layer) {
        return;
    }
    const QString id = layer->id();
    for (int i = 0; i < mLayerCombo->count(); ++i)
    {
        if (mLayerCombo->itemData(i).toString() == id)
        {
            mLayerCombo->setCurrentIndex(i);
            break;
        }
    }
    updateAttributeViewForLayer(layer);
}

void TNSelectionSetDialog::onLayerComboChanged(int index)
{
    if (index < 0) {
        return;
    }
    const QString id = mLayerCombo->itemData(index).toString();
    if (!mLayers.contains(id)) {
        return;
    }
    auto entry = mLayers.value(id);
    updateAttributeViewForLayer(entry.layer);
}

void TNSelectionSetDialog::updateAttributeViewForLayer(QgsVectorLayer* layer)
{
    if (!layer || !mAttrWidget) {
        return;
    }
    QgsFeatureIds selection = layer->selectedFeatureIds();
    mAttrWidget->setCurrentLayer(layer);
    mAttrWidget->clearFilter();
    mAttrWidget->setFilteredFeatures(selection);
}

QString TNSelectionSetDialog::buildSelectionFilter(const QgsFeatureIds& ids) const
{
    if (ids.isEmpty())
    {
        return QStringLiteral("FALSE");
    }
    QStringList list;
    list.reserve(ids.size());
    for (const auto& fid : ids)
    {
        list << QString::number(static_cast<qlonglong>(fid));
    }
    return QStringLiteral("$id IN (%1)").arg(list.join(QLatin1Char(',')));
}
