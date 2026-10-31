#pragma once

#include <QWidget>

class WorklaneView;

class WorklaneSeparator : public QWidget {
    Q_OBJECT

public:
    WorklaneSeparator(QWidget* leftCol, WorklaneView* worklane, QWidget* parent = nullptr);
    ~WorklaneSeparator() override;

    QWidget* leftColumn() const { return m_leftCol; }
    void setLeftColumn(QWidget* col) { m_leftCol = col; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* m_leftCol = nullptr;
    WorklaneView* m_worklane = nullptr;
    bool m_dragging = false;
    int m_startGlobalX = 0;
    int m_startLeftWidth = 0;
};
