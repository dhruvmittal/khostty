#pragma once

#include <QFrame>
#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

class TerminalSurface;

class TerminalPane : public QFrame {
    Q_OBJECT

public:
    explicit TerminalPane(TerminalSurface* surface, QWidget* parent = nullptr);
    ~TerminalPane() override;

    TerminalSurface* surface() const { return m_surface; }
    bool isActive() const { return m_isActive; }
    void setActive(bool active);
    void setTitle(const QString& title);
    void setPositionText(const QString& text);
    void setHeaderVisible(bool visible);
    void setBorderVisible(bool visible);
    void setZoomed(bool zoomed);
    bool isZoomed() const { return m_isZoomed; }

signals:
    void splitRightRequested();
    void splitDownRequested();
    void closeRequested();
    void unzoomRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateHeaderStyle();

    TerminalSurface* m_surface = nullptr;
    QWidget* m_header = nullptr;
    QLabel* m_dotIndicator = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_positionLabel = nullptr;
    QToolButton* m_zoomBadgeBtn = nullptr;
    QToolButton* m_splitRightBtn = nullptr;
    QToolButton* m_splitDownBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;
    QString m_title;
    bool m_isActive = false;
    bool m_borderVisible = true;
    bool m_isZoomed = false;
};
