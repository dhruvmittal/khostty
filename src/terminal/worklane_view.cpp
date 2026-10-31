#include "terminal/worklane_view.h"
#include "terminal/terminal_surface.h"

#include <QScrollBar>
#include <QFocusEvent>
#include <QDebug>

WorklaneView::WorklaneView(QWidget* parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);

    m_container = new QWidget(this);
    m_columnsLayout = new QHBoxLayout(m_container);
    m_columnsLayout->setContentsMargins(0, 0, 0, 0);
    m_columnsLayout->setSpacing(4);

    setWidget(m_container);

    // Initial column
    addColumn();
}

WorklaneView::~WorklaneView() = default;

TerminalSurface* WorklaneView::activeSurface() const {
    return m_activeSurface;
}

int WorklaneView::columnCount() const {
    return m_columns.size();
}

void WorklaneView::setupSurface(TerminalSurface* surface) {
    if (!surface) return;
    surface->installEventFilter(this);

    connect(surface, &TerminalSurface::titleChanged, this, [this, surface](const QString& title) {
        if (surface == m_activeSurface) {
            m_activeTitle = title;
            emit titleChanged(title);
        }
    });

    connect(surface, &TerminalSurface::closeRequested, this, [this, surface]() {
        QSplitter* splitter = findSplitterForSurface(surface);
        if (splitter) {
            delete surface;
            if (splitter->count() == 0) {
                m_columns.removeAll(splitter);
                m_columnsLayout->removeWidget(splitter);
                delete splitter;
            }
        }

        if (m_columns.isEmpty()) {
            m_activeSurface = nullptr;
            emit empty();
        } else {
            // Focus first available surface in last column
            QSplitter* lastSplitter = m_columns.last();
            if (lastSplitter->count() > 0) {
                auto* nextSurface = qobject_cast<TerminalSurface*>(lastSplitter->widget(0));
                if (nextSurface) {
                    setActiveSurface(nextSurface);
                }
            }
        }
    });

    connect(surface, &TerminalSurface::newSplitRequested, this, [this](ghostty_action_split_direction_e dir) {
        if (dir == GHOSTTY_SPLIT_DIRECTION_DOWN || dir == GHOSTTY_SPLIT_DIRECTION_UP) {
            splitCurrentVertical();
        } else {
            addColumn();
        }
    });

    connect(surface, &TerminalSurface::gotoSplitRequested, this, [this](ghostty_action_goto_split_e dir) {
        switch (dir) {
            case GHOSTTY_GOTO_SPLIT_LEFT:
            case GHOSTTY_GOTO_SPLIT_PREVIOUS:
                focusLeftColumn();
                break;
            case GHOSTTY_GOTO_SPLIT_RIGHT:
            case GHOSTTY_GOTO_SPLIT_NEXT:
                focusRightColumn();
                break;
            case GHOSTTY_GOTO_SPLIT_UP:
                focusUpSplit();
                break;
            case GHOSTTY_GOTO_SPLIT_DOWN:
                focusDownSplit();
                break;
        }
    });
}

TerminalSurface* WorklaneView::addColumn() {
    auto* splitter = new QSplitter(Qt::Vertical, m_container);
    splitter->setChildrenCollapsible(false);
    splitter->setMinimumWidth(400);

    auto* surface = new TerminalSurface(splitter);
    setupSurface(surface);
    splitter->addWidget(surface);

    m_columns.append(splitter);
    m_columnsLayout->addWidget(splitter);

    setActiveSurface(surface);
    return surface;
}

TerminalSurface* WorklaneView::splitCurrentVertical() {
    if (!m_activeSurface) {
        return addColumn();
    }

    QSplitter* splitter = findSplitterForSurface(m_activeSurface);
    if (!splitter) {
        return addColumn();
    }

    auto* surface = new TerminalSurface(splitter);
    setupSurface(surface);

    int idx = splitter->indexOf(m_activeSurface);
    splitter->insertWidget(idx + 1, surface);

    setActiveSurface(surface);
    return surface;
}

void WorklaneView::closeActiveSurface() {
    if (m_activeSurface) {
        m_activeSurface->handleProcessExited(true);
    }
}

void WorklaneView::focusLeftColumn() {
    if (!m_activeSurface || m_columns.size() <= 1) return;
    QSplitter* currentSplitter = findSplitterForSurface(m_activeSurface);
    int idx = m_columns.indexOf(currentSplitter);
    if (idx > 0) {
        QSplitter* targetSplitter = m_columns[idx - 1];
        if (targetSplitter->count() > 0) {
            auto* target = qobject_cast<TerminalSurface*>(targetSplitter->widget(0));
            if (target) setActiveSurface(target);
        }
    }
}

void WorklaneView::focusRightColumn() {
    if (!m_activeSurface || m_columns.size() <= 1) return;
    QSplitter* currentSplitter = findSplitterForSurface(m_activeSurface);
    int idx = m_columns.indexOf(currentSplitter);
    if (idx >= 0 && idx < m_columns.size() - 1) {
        QSplitter* targetSplitter = m_columns[idx + 1];
        if (targetSplitter->count() > 0) {
            auto* target = qobject_cast<TerminalSurface*>(targetSplitter->widget(0));
            if (target) setActiveSurface(target);
        }
    }
}

void WorklaneView::focusUpSplit() {
    if (!m_activeSurface) return;
    QSplitter* splitter = findSplitterForSurface(m_activeSurface);
    if (!splitter) return;
    int idx = splitter->indexOf(m_activeSurface);
    if (idx > 0) {
        auto* target = qobject_cast<TerminalSurface*>(splitter->widget(idx - 1));
        if (target) setActiveSurface(target);
    }
}

void WorklaneView::focusDownSplit() {
    if (!m_activeSurface) return;
    QSplitter* splitter = findSplitterForSurface(m_activeSurface);
    if (!splitter) return;
    int idx = splitter->indexOf(m_activeSurface);
    if (idx >= 0 && idx < splitter->count() - 1) {
        auto* target = qobject_cast<TerminalSurface*>(splitter->widget(idx + 1));
        if (target) setActiveSurface(target);
    }
}

void WorklaneView::setActiveSurface(TerminalSurface* surface) {
    if (m_activeSurface == surface) return;
    m_activeSurface = surface;
    if (m_activeSurface) {
        m_activeSurface->setFocus();
        ensureSurfaceVisible(m_activeSurface);
        emit activeSurfaceChanged(m_activeSurface);
        emit titleChanged(m_activeTitle);
    }
}

void WorklaneView::equalizeSplits() {
    if (m_columns.isEmpty()) return;
    for (auto* splitter : m_columns) {
        int count = splitter->count();
        if (count > 0) {
            QList<int> sizes;
            int total = splitter->height();
            int part = std::max(1, total / count);
            for (int i = 0; i < count; ++i) sizes << part;
            splitter->setSizes(sizes);
        }
    }
}

void WorklaneView::toggleSplitZoom() {
    if (!m_activeSurface) return;
    QSplitter* splitter = findSplitterForSurface(m_activeSurface);
    if (!splitter || splitter->count() <= 1) return;

    if (m_isZoomed) {
        for (int i = 0; i < splitter->count(); ++i) {
            splitter->widget(i)->show();
        }
        m_isZoomed = false;
    } else {
        for (int i = 0; i < splitter->count(); ++i) {
            QWidget* w = splitter->widget(i);
            if (w != m_activeSurface) {
                w->hide();
            }
        }
        m_isZoomed = true;
    }
}

void WorklaneView::ensureSurfaceVisible(TerminalSurface* surface) {
    if (!surface) return;
    ensureWidgetVisible(surface, 50, 0);
}

QSplitter* WorklaneView::findSplitterForSurface(TerminalSurface* surface) const {
    if (!surface) return nullptr;
    return qobject_cast<QSplitter*>(surface->parentWidget());
}

bool WorklaneView::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::FocusIn) {
        auto* surface = qobject_cast<TerminalSurface*>(watched);
        if (surface && surface != m_activeSurface) {
            m_activeSurface = surface;
            ensureSurfaceVisible(m_activeSurface);
            emit activeSurfaceChanged(m_activeSurface);
        }
    }
    return QScrollArea::eventFilter(watched, event);
}
