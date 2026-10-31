#include "terminal/khostty_splitter.h"

#include <QApplication>
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

KhosttySplitterHandle::KhosttySplitterHandle(Qt::Orientation orientation, QSplitter* parent)
    : QSplitterHandle(orientation, parent)
{
    setAttribute(Qt::WA_Hover, true);
    setCursor(orientation == Qt::Horizontal ? Qt::SplitHCursor : Qt::SplitVCursor);
}

KhosttySplitterHandle::~KhosttySplitterHandle() {
    if (m_dragging) {
        qApp->removeEventFilter(this);
    }
}

QSize KhosttySplitterHandle::sizeHint() const {
    int w = splitter() ? splitter()->handleWidth() : 8;
    return (orientation() == Qt::Horizontal) ? QSize(w, 1) : QSize(1, w);
}

void KhosttySplitterHandle::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);

    QColor baseBg = palette().color(QPalette::Window);
    QColor midLine = palette().color(QPalette::Mid);
    QColor accent = palette().color(QPalette::Highlight);

    bool isHighlighted = underMouse() || m_dragging;

    if (isHighlighted) {
        QColor hoverBg = accent;
        hoverBg.setAlpha(45);
        painter.fillRect(rect(), hoverBg);
    } else {
        painter.fillRect(rect(), baseBg);
    }

    painter.setPen(isHighlighted ? accent : midLine);

    if (orientation() == Qt::Horizontal) {
        int x = width() / 2;
        painter.drawLine(x, 0, x, height());
    } else {
        int y = height() / 2;
        painter.drawLine(0, y, width(), y);
    }
}

void KhosttySplitterHandle::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && splitter()) {
        m_dragging = true;
        m_dragStartGlobalPos = event->globalPosition().toPoint();
        m_initialSizes = splitter()->sizes();

        // In QSplitter, handle(i) sits between widget(i-1) and widget(i)
        m_handleIndex = -1;
        for (int i = 1; i < splitter()->count(); ++i) {
            if (splitter()->handle(i) == this) {
                m_handleIndex = i;
                break;
            }
        }

        qApp->installEventFilter(this);
        event->accept();
        update();
        return;
    }
    QSplitterHandle::mousePressEvent(event);
}

bool KhosttySplitterHandle::eventFilter(QObject* watched, QEvent* event) {
    if (m_dragging) {
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            QPoint currentGlobalPos = me->globalPosition().toPoint();

            int delta = (orientation() == Qt::Horizontal)
                ? (currentGlobalPos.x() - m_dragStartGlobalPos.x())
                : (currentGlobalPos.y() - m_dragStartGlobalPos.y());

            if (splitter() && m_handleIndex > 0 && m_handleIndex < m_initialSizes.size()) {
                int prevIdx = m_handleIndex - 1;
                int nextIdx = m_handleIndex;

                int combined = m_initialSizes[prevIdx] + m_initialSizes[nextIdx];

                int minPrev = 50;
                int minNext = 50;
                if (QWidget* wPrev = splitter()->widget(prevIdx)) {
                    minPrev = (orientation() == Qt::Horizontal)
                        ? wPrev->minimumSize().width()
                        : wPrev->minimumSize().height();
                }
                if (QWidget* wNext = splitter()->widget(nextIdx)) {
                    minNext = (orientation() == Qt::Horizontal)
                        ? wNext->minimumSize().width()
                        : wNext->minimumSize().height();
                }

                minPrev = std::max(minPrev, 40);
                minNext = std::max(minNext, 40);

                int targetPrev = m_initialSizes[prevIdx] + delta;
                int targetNext = m_initialSizes[nextIdx] - delta;

                if (targetPrev < minPrev) {
                    targetPrev = minPrev;
                    targetNext = combined - targetPrev;
                } else if (targetNext < minNext) {
                    targetNext = minNext;
                    targetPrev = combined - targetNext;
                }

                QList<int> newSizes = m_initialSizes;
                newSizes[prevIdx] = targetPrev;
                newSizes[nextIdx] = targetNext;
                splitter()->setSizes(newSizes);
            }
            return true; // Absorb event so child terminal surfaces don't steal mouse moves
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragging = false;
                qApp->removeEventFilter(this);
                update();
                return true;
            }
        } else if (event->type() == QEvent::WindowDeactivate) {
            m_dragging = false;
            qApp->removeEventFilter(this);
            update();
        }
    }
    return QSplitterHandle::eventFilter(watched, event);
}

KhosttySplitter::KhosttySplitter(Qt::Orientation orientation, QWidget* parent)
    : QSplitter(orientation, parent)
{
    setHandleWidth(8);
    setChildrenCollapsible(false);
}

KhosttySplitter::KhosttySplitter(QWidget* parent)
    : QSplitter(parent)
{
    setHandleWidth(8);
    setChildrenCollapsible(false);
}

QSplitterHandle* KhosttySplitter::createHandle() {
    return new KhosttySplitterHandle(orientation(), this);
}
