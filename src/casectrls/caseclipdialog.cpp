/*****************************************************************//**
 * \file   caseclipdialog.cpp
 * \brief  地图裁剪对话框
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "caseclipdialog.h"

#include "../publicctrls/phase2comboboxutil.h"
#include "../publicctrls/phase2dialogstyle.h"
#include "../httpapis/httpapis.h"
#include "../jsonreader/httpapijson.h"
#include "../tnmapwidget/tnmapwidget.h"
#include "../uis/uiglobalstate.h"

#include <QDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <qgsdatasourceuri.h>
#include <qgsmapcanvas.h>
#include <qgscoordinatetransform.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

namespace {

QString currentWayTableName(QgsVectorLayer* layer)
{
    if (!layer || !layer->dataProvider()) {
        return {};
    }
    QgsDataSourceUri uri(layer->dataProvider()->dataSourceUri());
    QString t = uri.table();
    if (!t.isEmpty()) {
        return t;
    }
    return uri.param(QStringLiteral("typename"));
}

void appendCommunityItems(QComboBox* combo)
{
    combo->clear();
    const QSharedPointer<ProjectDetailJson> detail = UIG().currentProjectDetail();
    if (detail.isNull()) {
        return;
    }
    for (const MenuJson& menuItem : detail->j_menu()) {
        const QList<MenuChildrenJson> children = menuItem.j_children();
        if (children.isEmpty()) {
            continue;
        }
        for (const MenuChildrenJson& child : children) {
            const QString val = child.j_val().trimmed();
            if (!val.endsWith(QStringLiteral("_community"))) {
                continue;
            }
            const QString valueKey = child.j_value().trimmed();
            if (valueKey.isEmpty()) {
                continue;
            }
            const QString display = child.j_name().trimmed();
            combo->addItem(display.isEmpty() ? valueKey : display, valueKey);
        }
    }
}

} // namespace

CaseClipLayerDialog::CaseClipLayerDialog(TNMapWidget* mapWidget, int projectId, int caseId, const QString& authToken, QWidget* parent)
    : FramelessDialog(parent)
    , m_mapWidget(mapWidget)
    , m_projectId(projectId)
    , m_caseId(caseId)
    , m_authToken(authToken)
    , m_modeCombo(nullptr)
    , m_communityCombo(nullptr)
    , m_clipNameEdit(nullptr)
{
    setTitleText(QString::fromUtf8(u8"图层裁剪"));
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    resize(480, 260);
    setStyleSheet(Phase2Ui::dialogShellStyleSheet());
    initUi();
}

CaseClipLayerDialog::~CaseClipLayerDialog() = default;

void CaseClipLayerDialog::initUi()
{
    QVBoxLayout* root = contentLayout();
    root->setSpacing(12);
    root->setContentsMargins(16, 12, 16, 16);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QString::fromUtf8(u8"按面图层裁剪"), 0);
    m_modeCombo->addItem(QString::fromUtf8(u8"按当前地图可视范围裁剪"), 1);
    Phase2Ui::setupThemedCombo(m_modeCombo, Phase2Ui::InputFieldTheme::LightCompact);

    m_communityCombo = new QComboBox(this);
    appendCommunityItems(m_communityCombo);
    Phase2Ui::setupThemedCombo(m_communityCombo, Phase2Ui::InputFieldTheme::LightCompact);

    m_clipNameEdit = new QLineEdit(this);
    m_clipNameEdit->setPlaceholderText(QString::fromUtf8(u8"裁剪结果图层名称（可选）"));
    Phase2Ui::applyFieldStyle(m_clipNameEdit, Phase2Ui::InputFieldTheme::LightCompact);

    auto* modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel(QString::fromUtf8(u8"裁剪方式："), this));
    modeRow->addWidget(m_modeCombo, 1);
    root->addLayout(modeRow);

    auto* commRow = new QHBoxLayout();
    commRow->addWidget(new QLabel(QString::fromUtf8(u8"面图层："), this));
    commRow->addWidget(m_communityCombo, 1);
    root->addLayout(commRow);

    auto* nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel(QString::fromUtf8(u8"名称："), this));
    nameRow->addWidget(m_clipNameEdit, 1);
    root->addLayout(nameRow);

    connect(m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        const bool extentMode = m_modeCombo->currentData().toInt() == 1;
        m_communityCombo->setEnabled(!extentMode);
    });
    m_communityCombo->setEnabled(m_modeCombo->currentData().toInt() != 1);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    QPushButton* okBtn = new QPushButton(QString::fromUtf8(u8"确定"), this);
    QPushButton* cancelBtn = new QPushButton(QString::fromUtf8(u8"取消"), this);
    okBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    cancelBtn->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());
    connect(okBtn, &QPushButton::clicked, this, &CaseClipLayerDialog::onOkClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &CaseClipLayerDialog::reject);
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    root->addLayout(btnRow);
}

void CaseClipLayerDialog::onOkClicked()
{
    if (!m_mapWidget || m_projectId <= 0) {
        reject();
        return;
    }

    QgsVectorLayer* wayLayer = qobject_cast<QgsVectorLayer*>(m_mapWidget->selectedLayer());
    if (!wayLayer) {
        wayLayer = qobject_cast<QgsVectorLayer*>(m_mapWidget->currentLayer());
    }
    if (!wayLayer) {
        return;
    }

    const QString currentTableName = currentWayTableName(wayLayer);
    if (currentTableName.isEmpty()) {
        return;
    }

    const QString clipName = m_clipNameEdit ? m_clipNameEdit->text().trimmed() : QString();
    const bool extentMode = m_modeCombo && m_modeCombo->currentData().toInt() == 1;

    if (extentMode) {
        QgsRectangle viewExtent = m_mapWidget->canvas()->extent();
        QgsCoordinateReferenceSystem canvasCrs = m_mapWidget->canvas()->mapSettings().destinationCrs();
        QgsCoordinateReferenceSystem wgs84Crs = QgsCoordinateReferenceSystem::fromEpsgId(4326);
        QgsCoordinateTransform transform(canvasCrs, wgs84Crs, QgsProject::instance());
        try {
            viewExtent = transform.transform(viewExtent);
        } catch (const QgsCsException&) {
            reject();
            return;
        }

        const double xMin = viewExtent.xMinimum();
        const double xMax = viewExtent.xMaximum();
        const double yMin = viewExtent.yMinimum();
        const double yMax = viewExtent.yMaximum();
        const QString wkt = QStringLiteral("POLYGON((%1 %2, %3 %2, %3 %4, %1 %4, %1 %2))")
                                .arg(xMin, 0, 'f', 6)
                                .arg(yMin, 0, 'f', 6)
                                .arg(xMax, 0, 'f', 6)
                                .arg(yMax, 0, 'f', 6);

        HttpApis::clipPart(m_authToken, m_projectId, m_caseId, currentTableName, clipName, wkt,
            [this](bool suc, const QSharedPointer<ResponseJson>& response, const QString& errMsg) {
                Q_UNUSED(response)
                if (suc) {
                    accept();
                } else {
                    Q_UNUSED(errMsg)
                    reject();
                }
            });
        return;
    }

    if (!m_communityCombo || m_communityCombo->currentIndex() < 0) {
        reject();
        return;
    }
    const QString communityTable = m_communityCombo->currentData().toString();
    if (communityTable.isEmpty()) {
        reject();
        return;
    }

    HttpApis::clipLayer(m_authToken, m_projectId, m_caseId, communityTable, currentTableName, clipName,
        [this](bool suc, const QSharedPointer<ResponseJson>& response, const QString& errMsg) {
            Q_UNUSED(response)
            if (suc) {
                accept();
            } else {
                Q_UNUSED(errMsg)
                reject();
            }
        });
}
