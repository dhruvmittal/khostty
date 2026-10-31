#pragma once

#include <QSplitter>
#include <QSplitterHandle>
#include <QPoint>
#include <QList>

class KhosttySplitterHandle : public QSplitterHandle {
    Q_OBJECT

public:
    KhosttySplitterHandle(Qt::Orientation orientation, QSplitter* parent);
    ~KhosttySplitterHandle() override;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool m_dragging = false;
    QPoint m_dragStartGlobalPos;
    QList<int> m_initialSizes;
    int m_handleIndex = -1;
};

class KhosttySplitter : public QSplitter {
    Q_OBJECT

public:
    explicit KhosttySplitter(Qt::Orientation orientation, QWidget* parent = nullptr);
    explicit KhosttySplitter(QWidget* parent = nullptr);
    ~KhosttySplitter() override = default;

protected:
    QSplitterHandle* createHandle() override;
};
