#include "terminal/worklane_view.h"
#include "terminal/terminal_surface.h"
#include "terminal/terminal_pane.h"

#include <QScrollBar>
#include <QFocusEvent>
#include <QWheelEvent>
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
    m_columnsLayout->setContentsMargins(4, 4, 4, 4);
    m_columnsLayout->setSpacing(6);

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
        TerminalPane* pane = findPaneForSurface(surface);
        QSplitter* splitter = findSplitterForSurface(surface);
        if (splitter && pane) {
            delete pane;
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
                auto* nextPane = qobject_cast<TerminalPane*>(lastSplitter->widget(0));
                if (nextPane && nextPane->surface()) {
                    setActiveSurface(nextPane->surface());
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

void WorklaneView::setupPane(TerminalPane* pane) {
    if (!pane) return;
    connect(pane, &TerminalPane::splitRightRequested, this, [this]() {
        addColumn();
    });
    connect(pane, &TerminalPane::splitDownRequested, this, [this]() {
        splitCurrentVertical();
    });
    connect(pane, &TerminalPane::closeRequested, this, [this, pane]() {
        if (pane->surface()) {
            pane->surface()->handleProcessExited(true);
        }
    });
}

TerminalSurface* WorklaneView::addColumn() {
    auto* splitter = new QSplitter(Qt::Vertical, m_container);
    splitter->setChildrenCollapsible(false);
    splitter->setMinimumWidth(400);

    auto* surface = new TerminalSurface(splitter);
    setupSurface(surface);

    auto* pane = new TerminalPane(surface, splitter);
    setupPane(pane);
    splitter->addWidget(pane);

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

    auto* pane = new TerminalPane(surface, splitter);
    setupPane(pane);

    TerminalPane* currentPane = findPaneForSurface(m_activeSurface);
    int idx = currentPane ? splitter->indexOf(currentPane) : -1;
    if (idx != -1) {
        splitter->insertWidget(idx + 1, pane);
    } else {
        splitter->addWidget(pane);
    }

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
            auto* targetPane = qobject_cast<TerminalPane*>(targetSplitter->widget(0));
            if (targetPane && targetPane->surface()) {
                setActiveSurface(targetPane->surface());
            }
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
            auto* targetPane = qobject_cast<TerminalPane*>(targetSplitter->widget(0));
            if (targetPane && targetPane->surface()) {
                setActiveSurface(targetPane->surface());
            }
        }
    }
}

void WorklaneView::focusUpSplit() {
    if (!m_activeSurface) return;
    QSplitter* splitter = findSplitterForSurface(m_activeSurface);
    TerminalPane* currentPane = findPaneForSurface(m_activeSurface);
    if (!splitter || !currentPane) return;
    int idx = splitter->indexOf(currentPane);
    if (idx > 0) {
        auto* targetPane = qobject_cast<TerminalPane*>(splitter->widget(idx - 1));
        if (targetPane && targetPane->surface()) {
            setActiveSurface(targetPane->surface());
        }
    }
}

void WorklaneView::focusDownSplit() {
    if (!m_activeSurface) return;
    QSplitter* splitter = findSplitterForSurface(m_activeSurface);
    TerminalPane* currentPane = findPaneForSurface(m_activeSurface);
    if (!splitter || !currentPane) return;
    int idx = splitter->indexOf(currentPane);
    if (idx >= 0 && idx < splitter->count() - 1) {
        auto* targetPane = qobject_cast<TerminalPane*>(splitter->widget(idx + 1));
        if (targetPane && targetPane->surface()) {
            setActiveSurface(targetPane->surface());
        }
    }
}

void WorklaneView::setActiveSurface(TerminalSurface* surface) {
    if (m_activeSurface == surface && surface != nullptr) {
        if (auto* pane = findPaneForSurface(surface)) {
            pane->setActive(true);
        }
        return;
    }

    if (m_activeSurface) {
        if (auto* oldPane = findPaneForSurface(m_activeSurface)) {
            oldPane->setActive(false);
        }
    }

    m_activeSurface = surface;
    if (m_activeSurface) {
        if (auto* newPane = findPaneForSurface(m_activeSurface)) {
            newPane->setActive(true);
        }
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
    TerminalPane* currentPane = findPaneForSurface(m_activeSurface);
    if (!splitter || !currentPane || splitter->count() <= 1) return;

    if (m_isZoomed) {
        for (int i = 0; i < splitter->count(); ++i) {
            splitter->widget(i)->show();
        }
        m_isZoomed = false;
    } else {
        for (int i = 0; i < splitter->count(); ++i) {
            QWidget* w = splitter->widget(i);
            if (w != currentPane) {
                w->hide();
            }
        }
        m_isZoomed = true;
    }
}

void WorklaneView::ensureSurfaceVisible(TerminalSurface* surface) {
    if (!surface) return;
    QWidget* target = surface;
    if (auto* pane = findPaneForSurface(surface)) {
        target = pane;
    }
    ensureWidgetVisible(target, 50, 0);
}

TerminalPane* WorklaneView::findPaneForSurface(TerminalSurface* surface) const {
    if (!surface) return nullptr;
    return qobject_cast<TerminalPane*>(surface->parentWidget());
}

QSplitter* WorklaneView::findSplitterForSurface(TerminalSurface* surface) const {
    TerminalPane* pane = findPaneForSurface(surface);
    if (!pane) return nullptr;
    return qobject_cast<QSplitter*>(pane->parentWidget());
}

bool WorklaneView::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::FocusIn) {
        auto* surface = qobject_cast<TerminalSurface*>(watched);
        if (surface && surface != m_activeSurface) {
            setActiveSurface(surface);
        }
    }
    return QScrollArea::eventFilter(watched, event);
}

void WorklaneView::wheelEvent(QWheelEvent* event) {
    int delta = 0;
    if (!event->pixelDelta().isNull() && event->pixelDelta().x() != 0) {
        delta = event->pixelDelta().x();
    } else if (event->angleDelta().x() != 0) {
        delta = event->angleDelta().x();
    } else if (event->angleDelta().y() != 0 && (event->modifiers() & (Qt::ShiftModifier | Qt::AltModifier))) {
        delta = event->angleDelta().y();
    }

    if (delta != 0) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta);
        event->accept();
        return;
    }

    QScrollArea::wheelEvent(event);
}
