#include "terminal/worklane_separator.h"
#include "terminal/worklane_view.h"

#include <QApplication>
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

WorklaneSeparator::WorklaneSeparator(QWidget* leftCol, WorklaneView* worklane, QWidget* parent)
    : QWidget(parent)
    , m_leftCol(leftCol)
    , m_worklane(worklane)
{
    setFixedWidth(8);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::SplitHCursor);
}

WorklaneSeparator::~WorklaneSeparator() {
    if (m_dragging) {
        qApp->removeEventFilter(this);
    }
}

QSize WorklaneSeparator::sizeHint() const {
    return QSize(8, 0);
}

void WorklaneSeparator::paintEvent(QPaintEvent* event) {
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
    int x = width() / 2;
    painter.drawLine(x, 0, x, height());
}

void WorklaneSeparator::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_leftCol && m_worklane) {
        m_dragging = true;
        m_startGlobalX = event->globalPosition().toPoint().x();
        m_startLeftWidth = m_leftCol->width();

        qApp->installEventFilter(this);
        event->accept();
        update();
        return;
    }
    QWidget::mousePressEvent(event);
}

bool WorklaneSeparator::eventFilter(QObject* watched, QEvent* event) {
    if (m_dragging) {
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            int delta = me->globalPosition().toPoint().x() - m_startGlobalX;

            if (m_leftCol && m_worklane) {
                int viewportW = m_worklane->viewport()->width();
                // "Any individual terminal should not be allowed to be wider than 90% of the window (when a split is present)"
                int maxW = std::max(280, static_cast<int>(viewportW * 0.90));
                int minW = 280;
                int newW = std::clamp(m_startLeftWidth + delta, minW, maxW);

                m_leftCol->setFixedWidth(newW);
                m_worklane->updateCanvasLayout();
            }
            return true; // Absorb event so terminal doesn't steal mouse moves
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
    return QWidget::eventFilter(watched, event);
}
