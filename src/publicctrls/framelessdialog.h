/*****************************************************************//**
 * \file   framelessdialog.h
 * \brief  无边框弹窗基类
 * \details 统一提供自定义标题栏、拖拽移动、最小化、最大化/还原和关闭能力
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef FRAMELESSDIALOG_H
#define FRAMELESSDIALOG_H

#include <QDialog>
#include <QIcon>
#include <QPoint>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;
class QByteArray;

class FramelessDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FramelessDialog(QWidget* parent = nullptr);
    ~FramelessDialog() override;

    void setTitleText(const QString& title);
    void setTitleIcon(const QIcon& icon);
    void setTitleIcon(const QString& resourcePath);
    void setMinimizeButtonVisible(bool visible);
    void setMaximizeButtonVisible(bool visible);
    QVBoxLayout* contentLayout() const;
    QWidget* titleBarWidget() const;

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onMinimizeClicked();
    void onMaximizeClicked();
    void onCloseClicked();

private:
    void initUi();
    void setDefaultTitleIcon();
    void updateMaximizeButtonIcon();
    QPushButton* createTitleButton(const QString& iconUrl) const;
    bool isTitleDragObject(QObject* object) const;
    Qt::Edges resizeEdgesAt(const QPoint& globalPos) const;
    void updateResizeCursor(QWidget* target, const QPoint& globalPos);
    void clearResizeCursor(QWidget* target);
    void startResize(const QPoint& globalPos);
    void performResize(const QPoint& globalPos);
    void finishResize();

private:
    QWidget* m_shellWidget;
    QWidget* m_titleBar;
    QLabel* m_iconLabel;
    QLabel* m_titleLabel;
    QPushButton* m_minimizeButton;
    QPushButton* m_maximizeButton;
    QPushButton* m_closeButton;
    QWidget* m_contentWidget;
    QVBoxLayout* m_contentLayout;
    bool m_dragging;
    QPoint m_dragPosition;
    bool m_resizing;
    Qt::Edges m_resizeEdges;
    QRect m_resizeStartGeometry;
    QPoint m_resizeStartPos;
};

#endif // FRAMELESSDIALOG_H
