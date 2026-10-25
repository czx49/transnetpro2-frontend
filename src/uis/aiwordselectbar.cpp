/*****************************************************************//**
 * \file   aiwordselectbar.cpp
 * \brief  智能对话单行 L1/L2 选词条实现
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "aiwordselectbar.h"

#include <QAction>
#include <QFont>
#include <QHBoxLayout>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QToolButton>

extern QStringList g_fontfamilies;

namespace {

constexpr int kFontChatBody = 13;

QFont aiWordFont()
{
    QFont font;
    if (!g_fontfamilies.isEmpty()) {
        font.setFamily(g_fontfamilies.first());
    }
    font.setPixelSize(kFontChatBody);
    font.setWeight(QFont::Normal);
    return font;
}

QString aiWordMenuStyleSheet()
{
    return QStringLiteral(
        "QMenu {"
        "   background-color: #1e52b4;"
        "   border: 1px solid rgba(255, 255, 255, 0.18);"
        "   border-radius: 8px;"
        "   padding: 4px 4px;"
        "}"
        "QMenu::item {"
        "   padding: 6px 24px 6px 10px;"
        "   color: rgba(255, 255, 255, 0.95);"
        "   border-radius: 4px;"
        "   font-size: %1px;"
        "}"
        "QMenu::item:selected {"
        "   background-color: rgba(255, 255, 255, 0.14);"
        "}").arg(kFontChatBody);
}

QString aiWordChipStyleSheet()
{
    return QStringLiteral(
        "QToolButton#aiWordChip {"
        "   border: 1px solid rgba(47, 132, 227, 0.45);"
        "   border-radius: 6px;"
        "   background-color: #071b35;"
        "   color: rgba(110, 184, 255, 0.95);"
        "   padding: 0 6px;"
        "   min-height: 20px;"
        "   max-height: 20px;"
        "}"
        "QToolButton#aiWordChip:hover:enabled {"
        "   border-color: #66b5ff;"
        "   color: white;"
        "}"
        "QToolButton#aiWordChip:disabled {"
        "   border-color: rgba(72, 112, 164, 0.28);"
        "   color: rgba(175, 199, 230, 0.40);"
        "}"
        "QToolButton#aiWordChip::menu-indicator {"
        "   image: none;"
        "   width: 0;"
        "}");
}

QString aiDataSelectButtonStyleSheet()
{
    return QStringLiteral(
        "QPushButton#aiWordDataSelect {"
        "   border: 1px solid rgba(47, 132, 227, 0.45);"
        "   border-radius: 6px;"
        "   background-color: #071b35;"
        "   color: rgba(110, 184, 255, 0.95);"
        "   padding: 0 6px;"
        "   min-height: 20px;"
        "   max-height: 20px;"
        "}"
        "QPushButton#aiWordDataSelect:hover:enabled {"
        "   border-color: #66b5ff;"
        "   color: white;"
        "}"
        "QPushButton#aiWordDataSelect:checked {"
        "   border-color: #66b5ff;"
        "   color: white;"
        "   background-color: rgba(47, 132, 227, 0.28);"
        "}"
        "QPushButton#aiWordDataSelect:disabled {"
        "   border-color: rgba(72, 112, 164, 0.28);"
        "   color: rgba(175, 199, 230, 0.40);"
        "}");
}

} // namespace

AiWordSelectBar::AiWordSelectBar(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFixedHeight(22);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 2);
    layout->setSpacing(4);

    m_l1Chip = createChip(QStringLiteral("aiWordChipL1"));
    m_l2Chip = createChip(QStringLiteral("aiWordChipL2"));
    m_dataSelectButton = new QPushButton(QString::fromUtf8(u8"数据选择"), this);
    m_dataSelectButton->setObjectName(QStringLiteral("aiWordDataSelect"));
    m_dataSelectButton->setCheckable(true);
    m_dataSelectButton->setCursor(Qt::PointingHandCursor);
    m_dataSelectButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_dataSelectButton->setStyleSheet(aiDataSelectButtonStyleSheet());
    m_dataSelectButton->setFont(aiWordFont());
    layout->addWidget(m_l1Chip, 1);
    layout->addWidget(m_l2Chip, 1);
    layout->addWidget(m_dataSelectButton, 1);

    m_l1Menu = new QMenu(this);
    m_l2Menu = new QMenu(this);
    const QString menuStyle = aiWordMenuStyleSheet();
    m_l1Menu->setStyleSheet(menuStyle);
    m_l2Menu->setStyleSheet(menuStyle);

    connect(m_l1Chip, &QToolButton::clicked, this, &AiWordSelectBar::showL1Menu);
    connect(m_l2Chip, &QToolButton::clicked, this, &AiWordSelectBar::showL2Menu);
    connect(m_dataSelectButton, &QPushButton::toggled, this, &AiWordSelectBar::dataSelectToggled);

    updateChipLabels();
    updateDataSelectButton();
}

void AiWordSelectBar::setConfig(const AiConfigModel* config)
{
    m_config = config;
    if (m_config && m_config->isValid()) {
        m_config->clampSelection(m_l1Index, m_l2Index);
    }
    rebuildL2Menu();
    updateChipLabels();
    updateDataSelectButton();
}

const AiConfigModel* AiWordSelectBar::config() const
{
    return m_config;
}

int AiWordSelectBar::l1Index() const
{
    return m_l1Index;
}

int AiWordSelectBar::l2Index() const
{
    return m_l2Index;
}

void AiWordSelectBar::setSelection(int l1Index, int l2Index)
{
    m_l1Index = l1Index;
    m_l2Index = l2Index;
    if (m_config && m_config->isValid()) {
        m_config->clampSelection(m_l1Index, m_l2Index);
    }
    rebuildL2Menu();
    updateChipLabels();
    updateDataSelectButton();
}

void AiWordSelectBar::setInteractionEnabled(bool enabled)
{
    m_interactionEnabled = enabled;
    if (m_l1Chip) {
        m_l1Chip->setEnabled(enabled && (m_config != nullptr) && m_config->isValid());
    }
    if (m_l2Chip) {
        m_l2Chip->setEnabled(enabled && (m_config != nullptr) && m_config->isValid());
    }
    updateDataSelectButton();
}

void AiWordSelectBar::setSelectedCount(int count)
{
    m_selectedCount = qMax(0, count);
    updateDataSelectButton();
}

void AiWordSelectBar::setDataSelectChecked(bool checked)
{
    if (!m_dataSelectButton) {
        return;
    }
    if (m_dataSelectButton->isChecked() == checked) {
        updateDataSelectButton();
        return;
    }
    QSignalBlocker blocker(m_dataSelectButton);
    m_dataSelectButton->setChecked(checked);
    updateDataSelectButton();
}

bool AiWordSelectBar::isDataSelectChecked() const
{
    return m_dataSelectButton && m_dataSelectButton->isChecked();
}

void AiWordSelectBar::updateDataSelectButton()
{
    if (!m_dataSelectButton) {
        return;
    }
    const bool hasSelection = m_selectedCount > 0;
    const bool picking = m_dataSelectButton->isChecked();
    if (hasSelection) {
        m_dataSelectButton->setText(
            QString::fromUtf8(u8"数据选择(%1)").arg(m_selectedCount));
    } else {
        m_dataSelectButton->setText(QString::fromUtf8(u8"数据选择"));
    }
    if (picking) {
        m_dataSelectButton->setToolTip(QString::fromUtf8(u8"再次点击取消框选"));
    } else if (hasSelection) {
        m_dataSelectButton->setToolTip(
            QString::fromUtf8(u8"已选中 %1 个路段，点击可重新框选").arg(m_selectedCount));
    } else {
        m_dataSelectButton->setToolTip(QString::fromUtf8(u8"在地图上框选路段"));
    }
    bool enabled = false;
    if (m_interactionEnabled && m_config && m_config->isValid()) {
        enabled = m_config->hasBoxSelectL1Option()
                      ? m_config->isBoxSelectL1(m_l1Index, m_l2Index)
                      : true;
    }
    if (!enabled && m_dataSelectButton->isChecked()) {
        emit dataSelectToggled(false);
        QSignalBlocker blocker(m_dataSelectButton);
        m_dataSelectButton->setChecked(false);
    }
    m_dataSelectButton->setEnabled(enabled);
}

void AiWordSelectBar::showL1Menu()
{
    if (!m_interactionEnabled || !m_config || !m_config->isValid() || !m_l1Menu || !m_l1Chip) {
        return;
    }

    m_l1Menu->clear();
    const QStringList options = m_config->l1Options();
    for (int i = 0; i < options.size(); ++i) {
        QAction* action = m_l1Menu->addAction(options.at(i));
        connect(action, &QAction::triggered, this, [this, i]() {
            if (m_l1Index == i) {
                return;
            }
            m_l1Index = i;
            if (m_config) {
                m_config->clampSelection(m_l1Index, m_l2Index);
            }
            rebuildL2Menu();
            updateChipLabels();
            updateDataSelectButton();
            emitSelection();
        });
    }
    m_l1Menu->popup(m_l1Chip->mapToGlobal(QPoint(0, m_l1Chip->height())));
}

void AiWordSelectBar::showL2Menu()
{
    if (!m_interactionEnabled || !m_config || !m_config->isValid() || !m_l2Menu || !m_l2Chip) {
        return;
    }

    rebuildL2Menu();
    m_l2Menu->popup(m_l2Chip->mapToGlobal(QPoint(0, m_l2Chip->height())));
}

void AiWordSelectBar::rebuildL2Menu()
{
    if (!m_l2Menu || !m_config) {
        return;
    }

    m_l2Menu->clear();
    if (!m_config->isValid()) {
        return;
    }

    const QStringList options = m_config->l2Options(m_l1Index);
    for (int i = 0; i < options.size(); ++i) {
        QAction* action = m_l2Menu->addAction(options.at(i));
        connect(action, &QAction::triggered, this, [this, i]() {
            if (m_l2Index == i) {
                return;
            }
            m_l2Index = i;
            updateChipLabels();
            emitSelection();
        });
    }
}

void AiWordSelectBar::updateChipLabels()
{
    const bool configReady = m_config && m_config->isValid();
    const QString loading = QString::fromUtf8(u8"加载中");
    const QString unavailable = QString::fromUtf8(u8"—");

    auto setChip = [this, configReady, loading, unavailable](QToolButton* chip, AiConfigLevel level) {
        if (!chip) {
            return;
        }
        if (!configReady) {
            chip->setText(chipText(loading));
            chip->setToolTip(QString::fromUtf8(u8"选词配置加载中"));
            chip->setEnabled(false);
            return;
        }
        QString label;
        switch (level) {
        case AiConfigLevel::L1:
            label = m_config->optionLabel(AiConfigLevel::L1, m_l1Index, m_l2Index);
            break;
        case AiConfigLevel::L2:
            label = m_config->optionLabel(AiConfigLevel::L2, m_l1Index, m_l2Index);
            break;
        }
        if (label.isEmpty()) {
            label = unavailable;
        }
        chip->setText(chipText(label));
        chip->setToolTip(chipToolTip(level, label));
        chip->setEnabled(m_interactionEnabled);
    };

    setChip(m_l1Chip, AiConfigLevel::L1);
    setChip(m_l2Chip, AiConfigLevel::L2);
}

void AiWordSelectBar::emitSelection()
{
    emit selectionChanged(m_l1Index, m_l2Index);
}

QToolButton* AiWordSelectBar::createChip(const QString& objectName)
{
    auto* chip = new QToolButton(this);
    chip->setObjectName(QStringLiteral("aiWordChip"));
    chip->setProperty("chipRole", objectName);
    chip->setToolButtonStyle(Qt::ToolButtonTextOnly);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    chip->setStyleSheet(aiWordChipStyleSheet());
    chip->setFont(aiWordFont());
    return chip;
}

QString AiWordSelectBar::chipText(const QString& optionName) const
{
    return optionName + QChar(0x25BE);
}

QString AiWordSelectBar::chipToolTip(AiConfigLevel level, const QString& optionName) const
{
    if (!m_config) {
        return optionName;
    }
    return QStringLiteral("%1：%2").arg(m_config->levelTitle(level), optionName);
}
