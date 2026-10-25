/*****************************************************************//**
 * \file   myprojectsdialog.cpp
 * \brief  我的项目弹窗实现
 * \details 提供项目搜索、选择和打开能力
 *
 * \author zhangmg
 * \date   April 2026
 *********************************************************************/
#include "myprojectsdialog.h"

#include "../publicctrls/phase2dialogstyle.h"
#include "projectitemmodel.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include "../publicctrls/messagedialog.h"
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QPixmap scaledPixmapForBox(const QString& iconPath, const QSize& boxSize, qreal fillRatio)
{
    const QPixmap pixmap(iconPath);
    if (pixmap.isNull()) {
        return pixmap;
    }

    const int iconSize = qMax(1, qRound(qMin(boxSize.width(), boxSize.height()) * fillRatio));
    return pixmap.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QPushButton* createIconActionButton(const QString& iconPath, QWidget* parent)
{
    auto* button = new QPushButton(parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setFlat(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFixedSize(32, 32);
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(18, 18));
    button->setStyleSheet(
        "QPushButton {"
        "   border: none;"
        "   border-radius: 8px;"
        "   background-color: transparent;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(33, 136, 255, 0.12);"
        "}"
    );
    return button;
}
} // namespace

MyProjectsDialog::MyProjectsDialog(QWidget* parent)
    : FramelessDialog(parent)
    , m_model(nullptr)
    , m_searchEdit(nullptr)
    , m_listWidget(nullptr)
    , m_openButton(nullptr)
{
    setModal(true);
    setFixedSize(590, 405);
    setMinimizeButtonVisible(false);
    setMaximizeButtonVisible(false);
    setTitleText(u8"我的项目");

    setStyleSheet(Phase2Ui::dialogShellStyleSheet()
                  + QStringLiteral(
                      "QLineEdit {"
                      "   min-height: 38px;"
                      "   border: none;"
                      "   border-radius: 10px;"
                      "   background-color: white;"
                      "   color: #1f2430;"
                      "   padding: 0 12px;"
                      "   font-size: 14px;"
                      "}"
                      "QLineEdit:focus {"
                      "   border: 1px solid #2a8cff;"
                      "}"
                      "QListWidget {"
                      "   border: none;"
                      "   background: transparent;"
                      "   outline: none;"
                      "}"
                      "QListWidget::item {"
                      "   margin-bottom: 10px;"
                      "}"));

    QVBoxLayout* mainLayout = contentLayout();
    mainLayout->setContentsMargins(18, 18, 18, 20);
    mainLayout->setSpacing(14);

    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(10);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(u8"搜索项目名称...");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MyProjectsDialog::onKeywordChanged);
    toolbarLayout->addWidget(m_searchEdit, 1);

    QPushButton* newButton = new QPushButton(u8"新建项目", this);
    newButton->setCursor(Qt::PointingHandCursor);
    newButton->setFixedSize(96, 30);
    newButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    connect(newButton, &QPushButton::clicked, this, &MyProjectsDialog::onNewProjectClicked);
    toolbarLayout->addWidget(newButton);
    mainLayout->addLayout(toolbarLayout);

    m_listWidget = new QListWidget(this);
    m_listWidget->setSpacing(2);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &MyProjectsDialog::onItemActivated);
    connect(m_listWidget, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem*) {
        updateRowWidgets();
        if (current && m_openButton && !m_openButton->isEnabled()) {
            m_openButton->setEnabled(true);
        }
    });
    mainLayout->addWidget(m_listWidget, 1);

    QHBoxLayout* footerLayout = new QHBoxLayout();
    footerLayout->addStretch();

    m_openButton = new QPushButton(u8"打开项目", this);
    m_openButton->setCursor(Qt::PointingHandCursor);
    m_openButton->setFixedSize(96, 34);
    m_openButton->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    connect(m_openButton, &QPushButton::clicked, this, &MyProjectsDialog::onOpenClicked);
    footerLayout->addWidget(m_openButton);
    mainLayout->addLayout(footerLayout);
}

MyProjectsDialog::~MyProjectsDialog()
{
}

void MyProjectsDialog::setModel(ProjectItemModel* model)
{
    m_model = model;
    refresh();
}

void MyProjectsDialog::refresh()
{
    rebuildList();
}

int MyProjectsDialog::selectedProjectId() const
{
    if (!m_listWidget || !m_listWidget->currentItem()) {
        return -1;
    }

    return m_listWidget->currentItem()->data(Qt::UserRole).toInt();
}

void MyProjectsDialog::onKeywordChanged(const QString& text)
{
    Q_UNUSED(text);
    rebuildList();
}

void MyProjectsDialog::onItemActivated(QListWidgetItem* item)
{
    if (!item) {
        return;
    }

    const int projectId = item->data(Qt::UserRole).toInt();
    emit projectActivated(projectId);
    accept();
}

void MyProjectsDialog::onOpenClicked()
{
    const int projectId = selectedProjectId();
    if (projectId <= 0) {
        return;
    }

    emit projectActivated(projectId);
    accept();
}

void MyProjectsDialog::onNewProjectClicked()
{
    emit newProjectRequested();
    reject();
}

QWidget* MyProjectsDialog::createRowWidget(const ProjectItem& item, bool selected)
{
    QWidget* row = new QWidget();
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setStyleSheet(QString(
        "QWidget {"
        "   background-color: %1;"
        "   border-radius: 10px;"
        "}"
    ).arg(selected ? QStringLiteral("#2188ff") : QStringLiteral("white")));

    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 12, 12, 12);
    layout->setSpacing(12);

    QLabel* iconBox = new QLabel(row);
    iconBox->setFixedSize(44, 44);
    iconBox->setAlignment(Qt::AlignCenter);
    iconBox->setStyleSheet(
        "QLabel {"
        "   border-radius: 12px;"
        "   background-color: rgba(33, 136, 255, 0.12);"
        "}"
    );
    iconBox->setPixmap(scaledPixmapForBox(QStringLiteral(":/icons/icons/folder_open@2x.png"), iconBox->size(), 0.78));
    layout->addWidget(iconBox);

    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);

    const QString primaryColor = selected ? QStringLiteral("white") : QStringLiteral("#1f2430");
    const QString secondaryColor =
        selected ? QStringLiteral("rgba(255,255,255,0.92)") : QStringLiteral("#8f97a7");

    QLabel* titleLabel = new QLabel(row);
    titleLabel->setStyleSheet(QString(
        "QLabel {"
        "   color: %1;"
        "   font-size: 15px;"
        "   font-weight: 700;"
        "}"
    ).arg(primaryColor));

    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    const QFontMetrics titleMetrics(titleFont);
    const int titleAvailableWidth = 590 - 18 - 18 - 16 - 12 - 44 - 12 - 12 - 32 - 8;
    titleLabel->setText(titleMetrics.elidedText(item.name, Qt::ElideRight, qMax(120, titleAvailableWidth)));

    QLabel* regionLabel = new QLabel(formatProjectRegionText(item), row);
    regionLabel->setStyleSheet(QString(
        "QLabel {"
        "   color: %1;"
        "   font-size: 12px;"
        "}"
    ).arg(secondaryColor));

    const QString meta = QString(u8"方案数：%1    %2")
                             .arg(item.caseCount)
                             .arg(item.createTime.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
    QLabel* metaLabel = new QLabel(meta, row);
    metaLabel->setStyleSheet(QString(
        "QLabel {"
        "   color: %1;"
        "   font-size: 12px;"
        "}"
    ).arg(secondaryColor));

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(regionLabel);
    textLayout->addWidget(metaLabel);
    layout->addLayout(textLayout, 1);

    QVBoxLayout* actionLayout = new QVBoxLayout();
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(4);
    actionLayout->setAlignment(Qt::AlignVCenter);

    QPushButton* editButton = createIconActionButton(QStringLiteral(":/icons/icons/edit_project.png"), row);
    if (selected) {
        editButton->setStyleSheet(
            "QPushButton {"
            "   border: none;"
            "   border-radius: 8px;"
            "   background-color: transparent;"
            "}"
            "QPushButton:hover {"
            "   background-color: rgba(255, 255, 255, 0.18);"
            "}"
        );
    }
    connect(editButton, &QPushButton::clicked, this, [this, projectId = item.id]() {
        emit projectEditRequested(projectId);
    });

    QPushButton* deleteButton = createIconActionButton(QStringLiteral(":/icons/icons/delete_project.png"), row);
    if (selected) {
        deleteButton->setStyleSheet(editButton->styleSheet());
    }
    connect(deleteButton, &QPushButton::clicked, this, [this, projectId = item.id, projectName = item.name]() {
        if (TnUi::confirmYesNo(u8"确认删除",
                                QString(u8"确定要删除项目“%1”吗？").arg(projectName))) {
            emit projectDeleteRequested(projectId);
        }
    });

    actionLayout->addWidget(editButton);
    actionLayout->addWidget(deleteButton);
    layout->addLayout(actionLayout);

    return row;
}

void MyProjectsDialog::rebuildList()
{
    if (!m_listWidget) {
        return;
    }

    m_listWidget->clear();

    if (!m_model) {
        if (m_openButton) {
            m_openButton->setEnabled(false);
        }
        return;
    }

    const QString keyword = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    const QList<ProjectItem> items = m_model->datas();

    for (const ProjectItem& item : items) {
        if (!keyword.isEmpty() && !item.name.contains(keyword, Qt::CaseInsensitive)) {
            continue;
        }

        QListWidgetItem* rowItem = new QListWidgetItem();
        rowItem->setData(Qt::UserRole, item.id);
        rowItem->setSizeHint(QSize(0, 104));
        rowItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        m_listWidget->addItem(rowItem);
    }

    m_listWidget->clearSelection();
    updateRowWidgets();

    if (m_openButton) {
        m_openButton->setEnabled(false);
    }
}

void MyProjectsDialog::updateRowWidgets()
{
    if (!m_listWidget || !m_model) {
        return;
    }

    for (int row = 0; row < m_listWidget->count(); ++row) {
        QListWidgetItem* listItem = m_listWidget->item(row);
        ProjectItem currentItem;
        if (!m_model->projectById(listItem->data(Qt::UserRole).toInt(), currentItem)) {
            continue;
        }

        m_listWidget->setItemWidget(
            listItem,
            createRowWidget(currentItem, listItem == m_listWidget->currentItem()));
    }
}
