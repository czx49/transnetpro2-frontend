#include "framelessdialog.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QRect>
#include <QSize>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace
{
const char* MINIMIZE_ICON = ":/icons/icons/minisize.png";
const char* MAXIMIZE_ICON = ":/icons/icons/maxsize.png";
const char* NORMAL_ICON = ":/icons/icons/normalsize.png";
const char* CLOSE_ICON = ":/icons/icons/closebtn_1.png";

constexpr int RESIZE_BORDER_WIDTH = 6;
}

FramelessDialog::FramelessDialog(QWidget* parent)
    : QDialog(parent)
    , m_shellWidget(nullptr)
    , m_titleBar(nullptr)
    , m_iconLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_minimizeButton(nullptr)
    , m_maximizeButton(nullptr)
    , m_closeButton(nullptr)
    , m_contentWidget(nullptr)
    , m_contentLayout(nullptr)
    , m_dragging(false)
    , m_resizing(false)
{
    setWindowFlags(windowFlags() | Qt::Dialog | Qt::FramelessWindowHint);
    // 顶层透明；实色圆角由内层 #framelessDialogShell 绘制（勿在顶层 QSS 填实色）
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_StyledBackground, true);
    if (qApp != nullptr)
    {
        qApp->installEventFilter(this);
    }
    initUi();
    setDefaultTitleIcon();
}

FramelessDialog::~FramelessDialog()
{
    if (qApp != nullptr)
    {
        qApp->removeEventFilter(this);
    }
}

void FramelessDialog::setTitleText(const QString& title)
{
    setWindowTitle(title);
    m_titleLabel->setText(title);
}

void FramelessDialog::setTitleIcon(const QIcon& icon)
{
    m_iconLabel->setPixmap(icon.pixmap(18, 18));
    //m_iconLabel->setVisible(!icon.isNull());
    m_iconLabel->setVisible(false);
}

void FramelessDialog::setTitleIcon(const QString& resourcePath)
{
    setTitleIcon(QIcon(resourcePath));
}

void FramelessDialog::setMinimizeButtonVisible(bool visible)
{
    m_minimizeButton->setVisible(visible);
}

void FramelessDialog::setMaximizeButtonVisible(bool visible)
{
    m_maximizeButton->setVisible(visible);
}

QVBoxLayout* FramelessDialog::contentLayout() const
{
    return m_contentLayout;
}

QWidget* FramelessDialog::titleBarWidget() const
{
    return m_titleBar;
}

bool FramelessDialog::event(QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && !isMaximized())
        {
            m_resizeEdges = resizeEdgesAt(mouseEvent->globalPos());
            if (m_resizeEdges != Qt::Edges())
            {
                startResize(mouseEvent->globalPos());
                return true;
            }
        }
    }
    else if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (m_resizing && (mouseEvent->buttons() & Qt::LeftButton))
        {
            performResize(mouseEvent->globalPos());
            return true;
        }
        if (!m_dragging && !isMaximized())
        {
            updateResizeCursor(this, mouseEvent->globalPos());
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        if (m_resizing)
        {
            finishResize();
            return true;
        }
    }
    else if (event->type() == QEvent::Leave && !m_resizing)
    {
        clearResizeCursor(this);
    }

    return QDialog::event(event);
}

bool FramelessDialog::eventFilter(QObject* watched, QEvent* event)
{
    QWidget* watchedWidget = qobject_cast<QWidget*>(watched);
    if (watchedWidget != nullptr && isAncestorOf(watchedWidget))
    {
        if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (m_resizing && (mouseEvent->buttons() & Qt::LeftButton))
            {
                performResize(mouseEvent->globalPos());
                return true;
            }
            if (!m_dragging && !isMaximized())
            {
                updateResizeCursor(watchedWidget, mouseEvent->globalPos());
            }
        }
        else if (event->type() == QEvent::Leave && !m_resizing)
        {
            clearResizeCursor(watchedWidget);
        }
    }

    if (watched == qApp)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && !isMaximized())
            {
                m_resizeEdges = resizeEdgesAt(mouseEvent->globalPos());
                if (m_resizeEdges != Qt::Edges())
                {
                    startResize(mouseEvent->globalPos());
                    return true;
                }
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (m_resizing && (mouseEvent->buttons() & Qt::LeftButton))
            {
                performResize(mouseEvent->globalPos());
                return true;
            }
            if (isActiveWindow() && !m_dragging && !isMaximized())
            {
                QWidget* target = QApplication::widgetAt(mouseEvent->globalPos());
                if (target != nullptr && isAncestorOf(target))
                {
                    updateResizeCursor(target, mouseEvent->globalPos());
                }
                else
                {
                    clearResizeCursor(this);
                }
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            if (m_resizing)
            {
                finishResize();
                return true;
            }
        }

        return QDialog::eventFilter(watched, event);
    }

    if (!isTitleDragObject(watched))
    {
        return QDialog::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && !isMaximized()
            && resizeEdgesAt(mouseEvent->globalPos()) == Qt::Edges())
        {
            m_dragging = true;
            m_dragPosition = mouseEvent->globalPos() - frameGeometry().topLeft();
            return true;
        }
    }
    else if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (m_dragging && (mouseEvent->buttons() & Qt::LeftButton))
        {
            move(mouseEvent->globalPos() - m_dragPosition);
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        m_dragging = false;
        return true;
    }

    return QDialog::eventFilter(watched, event);
}

bool FramelessDialog::nativeEvent(const QByteArray& eventType, void* message, long* result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType)
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message != WM_NCHITTEST || isMaximized())
    {
        return QDialog::nativeEvent(eventType, message, result);
    }

    const Qt::Edges edges = resizeEdgesAt(QPoint(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)));
    if (edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::LeftEdge))
    {
        *result = HTTOPLEFT;
        return true;
    }
    if (edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::RightEdge))
    {
        *result = HTTOPRIGHT;
        return true;
    }
    if (edges.testFlag(Qt::BottomEdge) && edges.testFlag(Qt::LeftEdge))
    {
        *result = HTBOTTOMLEFT;
        return true;
    }
    if (edges.testFlag(Qt::BottomEdge) && edges.testFlag(Qt::RightEdge))
    {
        *result = HTBOTTOMRIGHT;
        return true;
    }
    if (edges.testFlag(Qt::LeftEdge))
    {
        *result = HTLEFT;
        return true;
    }
    if (edges.testFlag(Qt::RightEdge))
    {
        *result = HTRIGHT;
        return true;
    }
    if (edges.testFlag(Qt::TopEdge))
    {
        *result = HTTOP;
        return true;
    }
    if (edges.testFlag(Qt::BottomEdge))
    {
        *result = HTBOTTOM;
        return true;
    }
#endif

    return QDialog::nativeEvent(eventType, message, result);
}

void FramelessDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::WindowTitleChange)
    {
        m_titleLabel->setText(windowTitle());
    }
    else if (event->type() == QEvent::WindowIconChange)
    {
        setDefaultTitleIcon();
    }
    else if (event->type() == QEvent::WindowStateChange)
    {
        updateMaximizeButtonIcon();
    }
}

void FramelessDialog::onMinimizeClicked()
{
    showMinimized();
}

void FramelessDialog::onMaximizeClicked()
{
    if (isMaximized())
    {
        showNormal();
    }
    else
    {
        showMaximized();
    }
    updateMaximizeButtonIcon();
}

void FramelessDialog::onCloseClicked()
{
    reject();
}

void FramelessDialog::initUi()
{
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_shellWidget = new QWidget(this);
    m_shellWidget->setObjectName(QStringLiteral("framelessDialogShell"));
    m_shellWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_shellWidget->setAutoFillBackground(false);

    QVBoxLayout* shellLayout = new QVBoxLayout(m_shellWidget);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    m_titleBar = new QWidget(m_shellWidget);
    m_titleBar->setObjectName("framelessDialogTitleBar");
    m_titleBar->setAttribute(Qt::WA_StyledBackground, true);
    m_titleBar->setAutoFillBackground(false);
    m_titleBar->setFixedHeight(40);
    m_titleBar->installEventFilter(this);

    QHBoxLayout* titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(16, 0, 12, 0);
    titleLayout->setSpacing(6);
    titleLayout->setAlignment(Qt::AlignVCenter);

    m_iconLabel = new QLabel(m_titleBar);
    m_iconLabel->setFixedSize(18, 18);
    m_iconLabel->setVisible(false);
    m_iconLabel->installEventFilter(this);

    m_titleLabel = new QLabel(m_titleBar);
    m_titleLabel->setObjectName("framelessDialogTitleLabel");
    m_titleLabel->installEventFilter(this);

    m_minimizeButton = createTitleButton(QString::fromLatin1(MINIMIZE_ICON));
    m_maximizeButton = createTitleButton(QString::fromLatin1(MAXIMIZE_ICON));
    m_closeButton = createTitleButton(QString::fromLatin1(CLOSE_ICON));

    titleLayout->addWidget(m_iconLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_minimizeButton);
    titleLayout->addWidget(m_maximizeButton);
    titleLayout->addWidget(m_closeButton);

    m_contentWidget = new QWidget(m_shellWidget);
    m_contentWidget->setObjectName("framelessDialogContent");
    m_contentWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_contentWidget->setAutoFillBackground(false);
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);

    shellLayout->addWidget(m_titleBar);
    shellLayout->addWidget(m_contentWidget, 1);
    rootLayout->addWidget(m_shellWidget);

    connect(m_minimizeButton, &QPushButton::clicked, this, &FramelessDialog::onMinimizeClicked);
    connect(m_maximizeButton, &QPushButton::clicked, this, &FramelessDialog::onMaximizeClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &FramelessDialog::onCloseClicked);

    setStyleSheet(
        "FramelessDialog { background-color: transparent; }"
        "#framelessDialogShell {"
        "   background-color: #051733;"
        "   border-radius: 16px;"
        "   border: 1px solid rgba(255,255,255,0.18);"
        "}"
        "#framelessDialogTitleBar {"
        "   background-color: #083777;"
        "   border-top-left-radius: 16px;"
        "   border-top-right-radius: 16px;"
        "}"
        "#framelessDialogTitleLabel { color: #ffffff; font-size: 16px; font-weight: 600; }"
        "#framelessDialogContent { background-color: transparent; }"
        "#framelessDialogContent QLabel { background: transparent; color: #ffffff; }"
        "#framelessDialogContent QCheckBox { background: transparent; color: #ffffff; spacing: 6px; }"
        "#framelessDialogContent QRadioButton { background: transparent; color: #ffffff; spacing: 6px; }"
        "#framelessDialogContent QGroupBox {"
        "   background: transparent; color: #ffffff; font-weight: 700;"
        "   border: 1px solid rgba(255,255,255,0.28); border-radius: 10px;"
        "   margin-top: 10px; padding: 10px;"
        "}"
        "#framelessDialogContent QGroupBox::title {"
        "   subcontrol-origin: margin; left: 12px; padding: 0 4px; color: #ffffff;"
        "}"
    );
}

void FramelessDialog::setDefaultTitleIcon()
{
    QIcon icon = windowIcon();
    if (icon.isNull() && qApp != nullptr)
    {
        icon = qApp->windowIcon();
    }
    if (icon.isNull())
    {
        icon = QIcon(":/icons/icon.ico");
    }
    setTitleIcon(icon);
}

void FramelessDialog::updateMaximizeButtonIcon()
{
    const QString iconUrl = QString::fromLatin1(isMaximized() ? NORMAL_ICON : MAXIMIZE_ICON);
    m_maximizeButton->setIcon(QIcon(iconUrl));
    m_maximizeButton->setIconSize(QSize(14, 14));
}

QPushButton* FramelessDialog::createTitleButton(const QString& iconUrl) const
{
    QPushButton* button = new QPushButton(m_titleBar);
    button->setFixedSize(20, 20);
    button->setIcon(QIcon(iconUrl));
    button->setIconSize(QSize(14, 14));
    button->setFlat(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   border: none;"
        "   background: transparent;"
        "   padding: 0;"
        "}"
        "QPushButton:hover {"
        "   background: rgba(255, 255, 255, 0.12);"
        "   border-radius: 4px;"
        "}"));
    return button;
}

bool FramelessDialog::isTitleDragObject(QObject* object) const
{
    return object == m_titleBar || object == m_iconLabel || object == m_titleLabel;
}

Qt::Edges FramelessDialog::resizeEdgesAt(const QPoint& globalPos) const
{
    if (minimumWidth() == maximumWidth() && minimumHeight() == maximumHeight())
    {
        return Qt::Edges();
    }

    const QRect frame = frameGeometry();
    Qt::Edges edges;
    if (minimumWidth() != maximumWidth())
    {
        if (globalPos.x() >= frame.left() && globalPos.x() < frame.left() + RESIZE_BORDER_WIDTH)
        {
            edges |= Qt::LeftEdge;
        }
        else if (globalPos.x() <= frame.right() && globalPos.x() > frame.right() - RESIZE_BORDER_WIDTH)
        {
            edges |= Qt::RightEdge;
        }
    }
    if (minimumHeight() != maximumHeight())
    {
        if (globalPos.y() >= frame.top() && globalPos.y() < frame.top() + RESIZE_BORDER_WIDTH)
        {
            edges |= Qt::TopEdge;
        }
        else if (globalPos.y() <= frame.bottom() && globalPos.y() > frame.bottom() - RESIZE_BORDER_WIDTH)
        {
            edges |= Qt::BottomEdge;
        }
    }
    return edges;
}

void FramelessDialog::updateResizeCursor(QWidget* target, const QPoint& globalPos)
{
    if (target == nullptr)
    {
        return;
    }

    const Qt::Edges edges = resizeEdgesAt(globalPos);
    if ((edges.testFlag(Qt::LeftEdge) && edges.testFlag(Qt::TopEdge))
        || (edges.testFlag(Qt::RightEdge) && edges.testFlag(Qt::BottomEdge)))
    {
        target->setCursor(Qt::SizeFDiagCursor);
    }
    else if ((edges.testFlag(Qt::RightEdge) && edges.testFlag(Qt::TopEdge))
        || (edges.testFlag(Qt::LeftEdge) && edges.testFlag(Qt::BottomEdge)))
    {
        target->setCursor(Qt::SizeBDiagCursor);
    }
    else if (edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge))
    {
        target->setCursor(Qt::SizeHorCursor);
    }
    else if (edges.testFlag(Qt::TopEdge) || edges.testFlag(Qt::BottomEdge))
    {
        target->setCursor(Qt::SizeVerCursor);
    }
    else
    {
        clearResizeCursor(target);
    }
}

void FramelessDialog::clearResizeCursor(QWidget* target)
{
    if (target != nullptr)
    {
        target->unsetCursor();
    }
}

void FramelessDialog::startResize(const QPoint& globalPos)
{
    m_resizing = true;
    m_resizeStartPos = globalPos;
    m_resizeStartGeometry = geometry();
    grabMouse();
}

void FramelessDialog::performResize(const QPoint& globalPos)
{
    const QPoint delta = globalPos - m_resizeStartPos;
    QRect newGeometry = m_resizeStartGeometry;

    if (m_resizeEdges.testFlag(Qt::LeftEdge))
    {
        newGeometry.setLeft(std::min(newGeometry.right() - minimumWidth() + 1,
                                     m_resizeStartGeometry.left() + delta.x()));
    }
    if (m_resizeEdges.testFlag(Qt::RightEdge))
    {
        newGeometry.setRight(std::max(newGeometry.left() + minimumWidth() - 1,
                                      m_resizeStartGeometry.right() + delta.x()));
    }
    if (m_resizeEdges.testFlag(Qt::TopEdge))
    {
        newGeometry.setTop(std::min(newGeometry.bottom() - minimumHeight() + 1,
                                    m_resizeStartGeometry.top() + delta.y()));
    }
    if (m_resizeEdges.testFlag(Qt::BottomEdge))
    {
        newGeometry.setBottom(std::max(newGeometry.top() + minimumHeight() - 1,
                                       m_resizeStartGeometry.bottom() + delta.y()));
    }

    if (maximumWidth() < QWIDGETSIZE_MAX)
    {
        newGeometry.setWidth(std::min(newGeometry.width(), maximumWidth()));
    }
    if (maximumHeight() < QWIDGETSIZE_MAX)
    {
        newGeometry.setHeight(std::min(newGeometry.height(), maximumHeight()));
    }

    setGeometry(newGeometry);
}

void FramelessDialog::finishResize()
{
    m_resizing = false;
    m_resizeEdges = Qt::Edges();
    releaseMouse();
    QWidget* target = QApplication::widgetAt(QCursor::pos());
    if (target != nullptr && isAncestorOf(target))
    {
        updateResizeCursor(target, QCursor::pos());
    }
    else
    {
        clearResizeCursor(this);
    }
}
