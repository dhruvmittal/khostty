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

signals:
    void splitRightRequested();
    void splitDownRequested();
    void closeRequested();

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
    QToolButton* m_splitRightBtn = nullptr;
    QToolButton* m_splitDownBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;
    QString m_title;
    bool m_isActive = false;
};
