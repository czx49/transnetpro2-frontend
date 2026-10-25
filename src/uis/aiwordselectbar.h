/*****************************************************************//**
 * \file   aiwordselectbar.h
 * \brief  智能对话单行 L1/L2 选词条（Chip + Menu）
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef AIWORDSELECTBAR_H
#define AIWORDSELECTBAR_H

#include "aiconfigmodel.h"

#include <QWidget>

class QMenu;
class QPushButton;
class QToolButton;

/**
 * \brief 单行 L1/L2 Chip 选词 + 数据选择占位按钮；发送后保持选择，可随时改选
 */
class AiWordSelectBar : public QWidget {
    Q_OBJECT

public:
    explicit AiWordSelectBar(QWidget* parent = nullptr);

    void setConfig(const AiConfigModel* config);
    const AiConfigModel* config() const;

    int l1Index() const;
    int l2Index() const;
    void setSelection(int l1Index, int l2Index);

    void setInteractionEnabled(bool enabled);
    void setSelectedCount(int count);
    void setDataSelectChecked(bool checked);
    bool isDataSelectChecked() const;

signals:
    void selectionChanged(int l1Index, int l2Index);
    void dataSelectToggled(bool checked);

private slots:
    void showL1Menu();
    void showL2Menu();

private:
    void rebuildL2Menu();
    void updateChipLabels();
    void updateDataSelectButton();
    void emitSelection();
    QToolButton* createChip(const QString& objectName);
    QString chipText(const QString& optionName) const;
    QString chipToolTip(AiConfigLevel level, const QString& optionName) const;

    const AiConfigModel* m_config = nullptr;
    QToolButton* m_l1Chip = nullptr;
    QToolButton* m_l2Chip = nullptr;
    QPushButton* m_dataSelectButton = nullptr;
    class QMenu* m_l1Menu = nullptr;
    class QMenu* m_l2Menu = nullptr;
    int m_l1Index = 0;
    int m_l2Index = 0;
    int m_selectedCount = 0;
    bool m_interactionEnabled = true;
};

#endif // AIWORDSELECTBAR_H
