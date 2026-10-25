/*****************************************************************//**
 * \file   featurecardwidget.cpp
 * \brief  首页功能卡片控件实现
 * \details 实现首页业务卡片的视觉与交互
 *
 * \author zhangmg
 * \date   April 2026
 * \History 首页重构版本
 *********************************************************************/
#include "featurecardwidget.h"

#include <QCursor>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

FeatureCardWidget::FeatureCardWidget(FeatureCardType type, QWidget* parent)
    : QWidget(parent)
    , m_cardType(type)
    , m_isSelected(false)
    , m_isHovered(false)
    , m_titleLabel(nullptr)
    , m_layout(nullptr)
{
    initUi();
}

FeatureCardWidget::~FeatureCardWidget()
{
}

FeatureCardType FeatureCardWidget::cardType() const
{
    return m_cardType;
}

void FeatureCardWidget::setSelected(bool selected)
{
    if (m_isSelected == selected) {
        return;
    }

    m_isSelected = selected;
    update();
}

bool FeatureCardWidget::isSelected() const
{
    return m_isSelected;
}

void FeatureCardWidget::setDisplayTitle(const QString& text)
{
    if (m_titleLabel) {
        m_titleLabel->setText(text);
    }
}

void FeatureCardWidget::resizeEvent(QResizeEvent* event)
{
    updateChildGeometry();
    refreshIcon();
    QWidget::resizeEvent(event);
}

void FeatureCardWidget::enterEvent(QEvent* event)
{
    m_isHovered = true;
    setCursor(Qt::PointingHandCursor);
    update();
    QWidget::enterEvent(event);
}

void FeatureCardWidget::leaveEvent(QEvent* event)
{
    m_isHovered = false;
    unsetCursor();
    update();
    QWidget::leaveEvent(event);
}

void FeatureCardWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit cardClicked(m_cardType);
    }

    QWidget::mousePressEvent(event);
}

void FeatureCardWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF cardRect = rect().adjusted(1.0, 1.0, -1.0, -1.0);
    const qreal radius = 18.0;

    QLinearGradient background(cardRect.topLeft(), cardRect.bottomLeft());
    if (m_isSelected || m_isHovered) {
        if (m_isHovered) {
            background.setColorAt(0.0, QColor(14, 46, 108, 224));
            background.setColorAt(1.0, QColor(10, 29, 71, 224));
        }
        else {
            background.setColorAt(0.0, QColor(19, 61, 136, 236));
            background.setColorAt(1.0, QColor(8, 31, 77, 236));
        }

        QColor borderColor(42, 163, 255, m_isSelected ? 188 : (m_isHovered ? 116 : 62));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(borderColor, m_isSelected ? 2.0 : 1.0));
        painter.drawRoundedRect(cardRect, radius, radius);
    } 

    //painter.setPen(Qt::NoPen);
    //painter.setBrush(background);
    //painter.drawRoundedRect(cardRect, radius, radius);

    /*QLinearGradient glow(cardRect.left(), cardRect.bottom(), cardRect.right(), cardRect.top());
    glow.setColorAt(0.0, QColor(0, 210, 255, 70));
    glow.setColorAt(0.7, QColor(0, 210, 255, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(glow);
    painter.drawRoundedRect(cardRect.adjusted(0.0, cardRect.height() * 0.56, 0.0, 0.0), radius, radius);*/
}

void FeatureCardWidget::initUi()
{
    setFixedSize(154, 176);
    setAttribute(Qt::WA_StyledBackground, true);
    setMouseTracking(true);

    m_layout = nullptr;

    QString title;
    QString iconPath;
    getCardInfo(m_cardType, title, iconPath);
    m_iconPath = iconPath;

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(
	R"(
        QLabel {
        color: #fcf5ff;
        min-height: 16px;
        font: normal 16px "Source Han Sans CN";
		font-weight: 600;
        background: transparent;
        qproperty-alignment: AlignCenter;
        }
    )");

    updateChildGeometry();
    refreshIcon();
 
    m_titleLabel->raise();
}

void FeatureCardWidget::refreshIcon()
{
	if (m_iconPath.isEmpty()) {
        return;
    }

    const QPixmap icon(m_iconPath);
    if (icon.isNull()) {
        return;
        /*m_iconLabel->setText(u8"●");
        m_iconLabel->setStyleSheet(
            "QLabel {"
            "   color: #21d4fd;"
            "   font-size: 42px;"
            "   background: transparent;"
            "}"
        );*/
    }
	
	setAutoFillBackground(true);
    QPalette palette = this->palette();
    palette.setBrush(QPalette::Window, QBrush(QPixmap(m_iconPath)));
    this->setPalette(palette);
}

void FeatureCardWidget::updateChildGeometry()
{
    if (!m_titleLabel) {
        return;
    }

    const QRect contentRect = rect().adjusted(8, 8, -8, -8);
    const int titleHeight = 28;
    m_titleLabel->setGeometry(65, 15, 70, titleHeight);
    m_titleLabel->raise();
}

void FeatureCardWidget::getCardInfo(FeatureCardType type, QString& outTitle, QString& outIcon) const
{
    switch (type) {
    case FeatureCardType::Planning:
        outTitle = u8"城市规划";
        outIcon = QStringLiteral(":/icons/icons/urban_planning@2x.png");
        break;
    case FeatureCardType::DataManage:
        outTitle = u8"交通规划";
        outIcon = QStringLiteral(":/icons/icons/traffic_planning@2x.png");
        break;
    case FeatureCardType::Evaluation:
        outTitle = u8"交通管控";
        outIcon = QStringLiteral(":/icons/icons/traffic_control@2x.png");
        break;
    case FeatureCardType::Simulation:
        outTitle = u8"交通政策";
        outIcon = QStringLiteral(":/icons/icons/traffic_policy@2x.png");
        break;
    case FeatureCardType::Publish:
        outTitle = u8"公共交通";
        outIcon = QStringLiteral(":/icons/icons/public_transport@2x.png");
        break;
    default:
        outTitle = u8"未知模块";
        outIcon.clear();
        break;
    }
}
