#pragma once

#include <QWidget>

class KhosttySplitter;
class WorklaneView;

class WorklaneSeparator : public QWidget {
    Q_OBJECT

public:
    WorklaneSeparator(KhosttySplitter* leftCol, WorklaneView* worklane, QWidget* parent = nullptr);
    ~WorklaneSeparator() override;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    KhosttySplitter* m_leftCol = nullptr;
    WorklaneView* m_worklane = nullptr;
    bool m_dragging = false;
    int m_startGlobalX = 0;
    int m_startLeftWidth = 0;
};
