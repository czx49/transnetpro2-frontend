/*****************************************************************//**
 * \file   featurecardwidget.h
 * \brief  首页功能卡片控件
 * \details 展示首页五个业务入口卡片，支持悬停与选中态
 *
 * \author zhangmg
 * \date   April 2026
 * \History 首页重构版本
 *********************************************************************/
#ifndef FEATURECARDWIDGET_H
#define FEATURECARDWIDGET_H

#include <QLabel>
#include <QMouseEvent>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

class QResizeEvent;

/**
 * \brief  功能卡片类型枚举
 */
enum class FeatureCardType {
    Planning,      ///< 城市规划
    DataManage,    ///< 交通规划
    Evaluation,    ///< 交通管控
    Simulation,    ///< 交通政策
    Publish        ///< 公共交通
};

/**
 * \brief  功能卡片控件
 * \details 采用深色半透明风格，匹配首页设计稿
 */
class FeatureCardWidget : public QWidget {
    Q_OBJECT

public:
    explicit FeatureCardWidget(FeatureCardType type, QWidget* parent = nullptr);
    ~FeatureCardWidget() override;

    FeatureCardType cardType() const;
    void setSelected(bool selected);
    bool isSelected() const;

    /** 覆盖卡片主标题（如 `/api/config/base` 的 `titlist[].title`），图标仍由类型决定 */
    void setDisplayTitle(const QString& text);

signals:
    void cardClicked(FeatureCardType type);

protected:
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void initUi();
    void getCardInfo(FeatureCardType type, QString& outTitle, QString& outIcon) const;
    void refreshIcon();
    void updateChildGeometry();

private:
    FeatureCardType m_cardType;
    bool m_isSelected;
    bool m_isHovered;

    QLabel* m_titleLabel;
    QVBoxLayout* m_layout;
    QString m_iconPath;
};

#endif // FEATURECARDWIDGET_H
