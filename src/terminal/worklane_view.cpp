#include "terminal/worklane_view.h"
#include "terminal/terminal_surface.h"
#include "terminal/terminal_pane.h"
#include "core/ghostty_core.h"

#include <QScrollBar>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QTimer>
#include <algorithm>

WorklaneView::WorklaneView(QWidget* parent)
    : QScrollArea(parent)
{
    setWidgetResizable(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);

    m_canvas = new QWidget(viewport());
    m_canvasLayout = new QHBoxLayout(m_canvas);
    m_canvasLayout->setContentsMargins(0, 0, 0, 0);
    m_canvasLayout->setSpacing(0);

    setWidget(m_canvas);

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

bool WorklaneView::hasAnySplit() const {
    if (m_columns.size() > 1) return true;
    if (m_columns.size() == 1 && m_columns[0]->count() > 1) return true;
    return false;
}

void WorklaneView::rebuildCanvasLayout() {
    for (auto* sep : m_separators) {
        delete sep;
    }
    m_separators.clear();

    QLayoutItem* item;
    while ((item = m_canvasLayout->takeAt(0)) != nullptr) {
        delete item;
    }

    if (m_columns.isEmpty()) {
        updateCanvasLayout();
        return;
    }

    int viewportW = viewport()->width();
    bool splitPresent = hasAnySplit();
    int maxAllowed = splitPresent ? std::max(280, static_cast<int>(viewportW * 0.90)) : viewportW;

    if (m_columns.size() == 1) {
        auto* col = m_columns[0];
        if (splitPresent) {
            if (col->width() > maxAllowed) {
                col->setFixedWidth(maxAllowed);
            }
            m_canvasLayout->addWidget(col);

            auto* sep = new WorklaneSeparator(col, this, m_canvas);
            m_separators.append(sep);
            m_canvasLayout->addWidget(sep);
        } else {
            col->setFixedWidth(std::max(viewportW, 400));
            m_canvasLayout->addWidget(col);
        }
    } else {
        for (int i = 0; i < m_columns.size(); ++i) {
            auto* col = m_columns[i];
            if (col->width() > maxAllowed) {
                col->setFixedWidth(maxAllowed);
            }
            m_canvasLayout->addWidget(col);

            auto* sep = new WorklaneSeparator(col, this, m_canvas);
            m_separators.append(sep);
            m_canvasLayout->addWidget(sep);
        }
    }

    updateCanvasLayout();
}

void WorklaneView::updateCanvasLayout() {
    int viewportW = viewport()->width();
    int viewportH = viewport()->height();

    int totalW = 0;
    bool splitPresent = hasAnySplit();
    int maxAllowed = splitPresent ? std::max(280, static_cast<int>(viewportW * 0.90)) : viewportW;

    for (auto* col : m_columns) {
        if (splitPresent && col->width() > maxAllowed) {
            col->setFixedWidth(maxAllowed);
        }
        col->setFixedHeight(viewportH);
        totalW += col->width();
    }
    for (auto* sep : m_separators) {
        sep->setFixedHeight(viewportH);
        totalW += sep->width();
    }

    int finalW = std::max(totalW, viewportW);
    m_canvas->setFixedSize(finalW, viewportH);
}

void WorklaneView::resizeEvent(QResizeEvent* event) {
    QScrollArea::resizeEvent(event);
    updateCanvasLayout();
}

void WorklaneView::setupSurface(TerminalSurface* surface) {
    surface->installEventFilter(this);

    connect(surface, &TerminalSurface::titleChanged, this, [this, surface](const QString& title) {
        if (surface == m_activeSurface) {
            m_activeTitle = title;
            emit titleChanged(title);
        }
    });

    connect(surface, &TerminalSurface::closeRequested, this, [this, surface]() {
        closeSurface(surface);
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
            closeSurface(pane->surface());
        }
    });
}

TerminalSurface* WorklaneView::addColumn() {
    int viewportW = viewport()->width();
    int viewportH = viewport()->height();

    auto* colSplitter = new KhosttySplitter(Qt::Vertical, m_canvas);
    colSplitter->setMinimumHeight(std::max(100, viewportH));

    auto* surface = new TerminalSurface(colSplitter);
    setupSurface(surface);

    auto* pane = new TerminalPane(surface, colSplitter);
    setupPane(pane);
    colSplitter->addWidget(pane);

    if (m_columns.isEmpty()) {
        colSplitter->setFixedWidth(std::max(400, viewportW));
    } else {
        int maxAllowed = std::max(280, static_cast<int>(viewportW * 0.90));
        int colW = std::clamp(static_cast<int>(viewportW * 0.50), 350, maxAllowed);
        colSplitter->setFixedWidth(colW);
    }

    m_columns.append(colSplitter);
    rebuildCanvasLayout();

    setActiveSurface(surface);
    ensureSurfaceVisible(surface);
    return surface;
}

TerminalSurface* WorklaneView::splitCurrentVertical() {
    if (!m_activeSurface) {
        return addColumn();
    }

    KhosttySplitter* splitter = findSplitterForSurface(m_activeSurface);
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

    // Equalize vertical splits within this column
    int count = splitter->count();
    if (count > 1) {
        int totalHeight = splitter->height();
        int part = std::max(1, totalHeight / count);
        QList<int> sizes;
        for (int i = 0; i < count; ++i) sizes << part;
        splitter->setSizes(sizes);
    }

    if (m_columns.size() == 1) {
        rebuildCanvasLayout();
    } else {
        updateCanvasLayout();
    }

    setActiveSurface(surface);
    ensureSurfaceVisible(surface);
    return surface;
}

void WorklaneView::closeActiveSurface() {
    if (m_activeSurface) {
        closeSurface(m_activeSurface);
    }
}

void WorklaneView::closeSurface(TerminalSurface* surface) {
    if (!surface) return;

    TerminalPane* pane = findPaneForSurface(surface);
    KhosttySplitter* colSplitter = findSplitterForSurface(surface);
    if (!pane || !colSplitter) return;

    bool wasActive = (m_activeSurface == surface);
    if (wasActive) {
        m_activeSurface = nullptr;
    }

    // Safely remove pane from splitter before scheduling deletion
    pane->hide();
    pane->setParent(nullptr);
    pane->deleteLater();

    if (colSplitter->count() == 0) {
        m_columns.removeAll(colSplitter);
        colSplitter->hide();
        colSplitter->setParent(nullptr);
        colSplitter->deleteLater();
        rebuildCanvasLayout();
    } else {
        int count = colSplitter->count();
        if (count > 0) {
            int totalH = colSplitter->height();
            int part = std::max(1, totalH / count);
            QList<int> sizes;
            for (int i = 0; i < count; ++i) sizes << part;
            colSplitter->setSizes(sizes);
        }
        updateCanvasLayout();
    }

    if (m_columns.isEmpty()) {
        m_activeSurface = nullptr;
        QTimer::singleShot(0, this, [this]() {
            emit empty();
        });
    } else if (wasActive) {
        // Pick next surface to focus
        TerminalSurface* nextSurface = nullptr;
        for (auto* col : m_columns) {
            for (int i = 0; i < col->count(); ++i) {
                if (auto* p = qobject_cast<TerminalPane*>(col->widget(i))) {
                    if (p->surface()) {
                        nextSurface = p->surface();
                        break;
                    }
                }
            }
            if (nextSurface) break;
        }

        if (nextSurface) {
            setActiveSurface(nextSurface);
            nextSurface->setFocus();
            ensureSurfaceVisible(nextSurface);
        }
    }
}

void WorklaneView::focusLeftColumn() {
    if (m_columns.size() <= 1 || !m_activeSurface) return;
    KhosttySplitter* current = findSplitterForSurface(m_activeSurface);
    int idx = m_columns.indexOf(current);
    if (idx > 0) {
        KhosttySplitter* prev = m_columns[idx - 1];
        if (prev->count() > 0) {
            auto* p = qobject_cast<TerminalPane*>(prev->widget(0));
            if (p && p->surface()) {
                p->surface()->setFocus();
                ensureSurfaceVisible(p->surface());
            }
        }
    }
}

void WorklaneView::focusRightColumn() {
    if (m_columns.size() <= 1 || !m_activeSurface) return;
    KhosttySplitter* current = findSplitterForSurface(m_activeSurface);
    int idx = m_columns.indexOf(current);
    if (idx >= 0 && idx < m_columns.size() - 1) {
        KhosttySplitter* next = m_columns[idx + 1];
        if (next->count() > 0) {
            auto* p = qobject_cast<TerminalPane*>(next->widget(0));
            if (p && p->surface()) {
                p->surface()->setFocus();
                ensureSurfaceVisible(p->surface());
            }
        }
    }
}

void WorklaneView::focusUpSplit() {
    if (!m_activeSurface) return;
    KhosttySplitter* splitter = findSplitterForSurface(m_activeSurface);
    if (!splitter || splitter->count() <= 1) return;
    TerminalPane* currentPane = findPaneForSurface(m_activeSurface);
    int idx = splitter->indexOf(currentPane);
    if (idx > 0) {
        auto* p = qobject_cast<TerminalPane*>(splitter->widget(idx - 1));
        if (p && p->surface()) {
            p->surface()->setFocus();
            ensureSurfaceVisible(p->surface());
        }
    }
}

void WorklaneView::focusDownSplit() {
    if (!m_activeSurface) return;
    KhosttySplitter* splitter = findSplitterForSurface(m_activeSurface);
    if (!splitter || splitter->count() <= 1) return;
    TerminalPane* currentPane = findPaneForSurface(m_activeSurface);
    int idx = splitter->indexOf(currentPane);
    if (idx >= 0 && idx < splitter->count() - 1) {
        auto* p = qobject_cast<TerminalPane*>(splitter->widget(idx + 1));
        if (p && p->surface()) {
            p->surface()->setFocus();
            ensureSurfaceVisible(p->surface());
        }
    }
}

void WorklaneView::setActiveSurface(TerminalSurface* surface) {
    if (m_activeSurface == surface) return;

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
        m_activeTitle = m_activeSurface->title();
        emit activeSurfaceChanged(m_activeSurface);
        emit titleChanged(m_activeTitle);
        ensureSurfaceVisible(m_activeSurface);
    }
}

void WorklaneView::equalizeSplits() {
    if (m_columns.isEmpty()) return;

    int viewportW = viewport()->width();
    bool splitPresent = hasAnySplit();
    int maxAllowed = splitPresent ? std::max(280, static_cast<int>(viewportW * 0.90)) : viewportW;

    if (m_columns.size() == 1) {
        m_columns[0]->setFixedWidth(splitPresent ? maxAllowed : viewportW);
    } else {
        int count = static_cast<int>(m_columns.size());
        int available = (viewportW - 8 * count) / count;
        int equalW = std::min(maxAllowed, std::max(350, available));
        for (auto* col : m_columns) {
            col->setFixedWidth(equalW);
        }
    }

    // Equalize vertical splits within each column
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

    updateCanvasLayout();
}

void WorklaneView::toggleSplitZoom() {
    if (!m_activeSurface) return;
    KhosttySplitter* splitter = findSplitterForSurface(m_activeSurface);
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

KhosttySplitter* WorklaneView::findSplitterForSurface(TerminalSurface* surface) const {
    TerminalPane* pane = findPaneForSurface(surface);
    if (!pane) return nullptr;
    return qobject_cast<KhosttySplitter*>(pane->parentWidget());
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
    // Fast scroll multiplier: 2.5x base, 5x hyper-scroll if Ctrl is held
    double multiplier = (event->modifiers() & Qt::ControlModifier) ? 5.0 : 2.5;

    if (!event->pixelDelta().isNull() && event->pixelDelta().x() != 0) {
        delta = static_cast<int>(event->pixelDelta().x() * multiplier);
    } else if (event->angleDelta().x() != 0) {
        delta = static_cast<int>(event->angleDelta().x() * multiplier);
    } else if (event->angleDelta().y() != 0 && (event->modifiers() & (Qt::ShiftModifier | Qt::AltModifier))) {
        delta = static_cast<int>(event->angleDelta().y() * multiplier);
    }

    if (delta != 0) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta);
        event->accept();
    } else {
        QScrollArea::wheelEvent(event);
    }
}
