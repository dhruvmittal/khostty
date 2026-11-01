#include "terminal/worklane_view.h"
#include "terminal/terminal_surface.h"
#include "terminal/terminal_pane.h"
#include "core/ghostty_core.h"

#include <KSharedConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <QScrollBar>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QDir>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QIcon>
#include <algorithm>

WorklaneView::WorklaneView(QWidget* parent, const QString& initialDir)
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

    // Build Search Bar overlay
    m_searchBar = new QWidget(viewport());
    m_searchBar->setObjectName(QStringLiteral("khostty_search_bar"));
    m_searchBar->setVisible(false);
    auto* sbLayout = new QHBoxLayout(m_searchBar);
    sbLayout->setContentsMargins(6, 4, 6, 4);
    sbLayout->setSpacing(4);

    m_searchEdit = new QLineEdit(m_searchBar);
    m_searchEdit->setPlaceholderText(i18n("Search terminal..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(200);
    m_searchEdit->installEventFilter(this);

    auto* prevBtn = new QToolButton(m_searchBar);
    prevBtn->setFocusPolicy(Qt::NoFocus);
    prevBtn->setIcon(QIcon::fromTheme(QStringLiteral("go-up"), QIcon::fromTheme(QStringLiteral("arrow-up"))));
    prevBtn->setToolTip(i18n("Previous Match (Shift+Enter)"));
    prevBtn->setAutoRaise(true);

    auto* nextBtn = new QToolButton(m_searchBar);
    nextBtn->setFocusPolicy(Qt::NoFocus);
    nextBtn->setIcon(QIcon::fromTheme(QStringLiteral("go-down"), QIcon::fromTheme(QStringLiteral("arrow-down"))));
    nextBtn->setToolTip(i18n("Next Match (Enter)"));
    nextBtn->setAutoRaise(true);

    m_searchCountLabel = new QLabel(m_searchBar);
    m_searchCountLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));

    auto* closeBtn = new QToolButton(m_searchBar);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setIcon(QIcon::fromTheme(QStringLiteral("window-close"), QIcon::fromTheme(QStringLiteral("dialog-close"))));
    closeBtn->setToolTip(i18n("Close Search (Escape)"));
    closeBtn->setAutoRaise(true);

    sbLayout->addWidget(m_searchEdit);
    sbLayout->addWidget(prevBtn);
    sbLayout->addWidget(nextBtn);
    sbLayout->addWidget(m_searchCountLabel);
    sbLayout->addWidget(closeBtn);

    QColor winColor = palette().color(QPalette::Window);
    QColor midColor = palette().color(QPalette::Mid);
    m_searchBar->setStyleSheet(QStringLiteral(
        "QWidget#khostty_search_bar { background-color: %1; border: 1px solid %2; border-radius: 4px; }"
    ).arg(winColor.name(), midColor.name()));

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_activeSurface) {
            m_activeSurface->startSearch(text);
        }
    });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        if (m_activeSurface) {
            m_activeSurface->navigateSearch(true);
        }
    });
    connect(nextBtn, &QToolButton::clicked, this, [this]() {
        if (m_activeSurface) {
            m_activeSurface->navigateSearch(true);
        }
    });
    connect(prevBtn, &QToolButton::clicked, this, [this]() {
        if (m_activeSurface) {
            m_activeSurface->navigateSearch(false);
        }
    });
    connect(closeBtn, &QToolButton::clicked, this, &WorklaneView::hideSearchBar);

    // Initial column
    addColumn(initialDir);
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

void WorklaneView::updatePaneDecorations() {
    KConfigGroup appearanceConfig(KSharedConfig::openConfig(QStringLiteral("khosttyrc")), QStringLiteral("Appearance"));
    bool showHeaderOnlyOnSplit = appearanceConfig.readEntry("ShowHeaderOnlyOnSplit", true);
    bool split = hasAnySplit();
    bool showHeader = !showHeaderOnlyOnSplit || split;

    for (auto* col : m_columns) {
        for (int i = 0; i < col->count(); ++i) {
            if (auto* pane = qobject_cast<TerminalPane*>(col->widget(i))) {
                pane->setHeaderVisible(showHeader);
                pane->setBorderVisible(split);
            }
        }
    }
}

void WorklaneView::rebuildCanvasLayout() {
    int savedScroll = horizontalScrollBar()->value();

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
            col->setMinimumWidth(200);
            col->setMaximumWidth(QWIDGETSIZE_MAX);
            m_canvasLayout->addWidget(col, 1);
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
    updatePaneDecorations();
    horizontalScrollBar()->setValue(savedScroll);
}

void WorklaneView::updateCanvasLayout() {
    int viewportW = viewport()->width();
    int viewportH = viewport()->height();

    int totalW = 0;
    bool splitPresent = hasAnySplit();
    int maxAllowed = splitPresent ? std::max(280, static_cast<int>(viewportW * 0.90)) : viewportW;

    if (!splitPresent && m_columns.size() == 1) {
        auto* col = m_columns[0];
        col->setMinimumWidth(200);
        col->setMaximumWidth(QWIDGETSIZE_MAX);
        col->setFixedHeight(viewportH);
        totalW = viewportW;
    } else {
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
    }

    int finalW = std::max(totalW, viewportW);
    m_canvas->setFixedSize(finalW, viewportH);
}

void WorklaneView::resizeEvent(QResizeEvent* event) {
    QScrollArea::resizeEvent(event);
    updateCanvasLayout();
    if (m_searchBar && m_searchBar->isVisible()) {
        m_searchBar->adjustSize();
        int x = std::max(10, viewport()->width() - m_searchBar->width() - 20);
        m_searchBar->move(x, 10);
        m_searchBar->raise();
    }
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

    connect(surface, &TerminalSurface::splitRequested, this, [this, surface](Qt::Orientation ori) {
        if (ori == Qt::Horizontal) {
            KhosttySplitter* col = findSplitterForSurface(surface);
            addColumnAfter(col);
        } else {
            splitVertical(surface);
        }
    });

    connect(surface, &TerminalSurface::searchStatsChanged, this, [this, surface](int total, int selected) {
        if (surface == m_activeSurface && m_searchCountLabel) {
            if (total >= 0) m_searchTotal = total;
            if (selected >= 0) m_searchSelected = selected;
            if (m_searchTotal <= 0) {
                m_searchCountLabel->setText(m_searchEdit && m_searchEdit->text().isEmpty() ? QString() : i18n("No matches"));
            } else {
                m_searchCountLabel->setText(i18n("%1 of %2", m_searchSelected + 1, m_searchTotal));
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
    connect(pane, &TerminalPane::splitRightRequested, this, [this, pane]() {
        KhosttySplitter* col = findSplitterForSurface(pane->surface());
        addColumnAfter(col);
    });
    connect(pane, &TerminalPane::splitDownRequested, this, [this, pane]() {
        splitVertical(pane->surface());
    });
    connect(pane, &TerminalPane::closeRequested, this, [this, pane]() {
        if (pane->surface()) {
            closeSurface(pane->surface());
        }
    });
    connect(pane, &TerminalPane::unzoomRequested, this, &WorklaneView::toggleSplitZoom);
}

TerminalSurface* WorklaneView::addColumn(const QString& initialDir) {
    KhosttySplitter* current = findSplitterForSurface(m_activeSurface);
    return addColumnAfter(current, initialDir);
}

TerminalSurface* WorklaneView::addColumnAfter(KhosttySplitter* targetCol, const QString& initialDir) {
    int viewportW = viewport()->width();
    int viewportH = viewport()->height();

    auto* colSplitter = new KhosttySplitter(Qt::Vertical, m_canvas);
    colSplitter->setMinimumHeight(std::max(100, viewportH));

    QString dir = initialDir.trimmed();
    if (dir.isEmpty()) {
        if (!m_columns.isEmpty()) {
            KConfigGroup genCfg(KSharedConfig::openConfig(QStringLiteral("khosttyrc")), QStringLiteral("General"));
            bool inherit = genCfg.readEntry("NewSplitInheritsPwd", true);
            if (inherit && m_activeSurface) {
                dir = m_activeSurface->currentWorkingDirectory();
            }
        }
        if (dir.isEmpty()) {
            dir = QDir::homePath();
        }
    }

    auto* surface = new TerminalSurface(colSplitter, dir);
    setupSurface(surface);

    auto* pane = new TerminalPane(surface, colSplitter);
    setupPane(pane);
    colSplitter->addWidget(pane);

    if (m_columns.isEmpty()) {
        colSplitter->setFixedWidth(std::max(400, viewportW));
        m_columns.append(colSplitter);
    } else {
        int maxAllowed = std::max(280, static_cast<int>(viewportW * 0.90));
        int colW = std::clamp(static_cast<int>(viewportW * 0.50), 350, maxAllowed);
        colSplitter->setFixedWidth(colW);

        int insertIdx = m_columns.size();
        if (targetCol) {
            int idx = m_columns.indexOf(targetCol);
            if (idx != -1) {
                insertIdx = idx + 1;
            }
        }
        m_columns.insert(insertIdx, colSplitter);
    }

    rebuildCanvasLayout();

    setActiveSurface(surface);
    surface->setFocus();
    ensureSurfaceVisible(surface);
    return surface;
}

TerminalSurface* WorklaneView::splitCurrentVertical() {
    return splitVertical(m_activeSurface);
}

TerminalSurface* WorklaneView::splitVertical(TerminalSurface* target, const QString& initialDir) {
    if (!target) {
        target = m_activeSurface;
    }
    if (!target) {
        return addColumn(initialDir);
    }

    KhosttySplitter* splitter = findSplitterForSurface(target);
    if (!splitter) {
        return addColumn(initialDir);
    }

    QString dir = initialDir.trimmed();
    if (dir.isEmpty()) {
        KConfigGroup genCfg(KSharedConfig::openConfig(QStringLiteral("khosttyrc")), QStringLiteral("General"));
        bool inherit = genCfg.readEntry("NewSplitInheritsPwd", true);
        if (inherit) {
            TerminalSurface* baseSurface = target ? target : m_activeSurface;
            if (baseSurface) {
                dir = baseSurface->currentWorkingDirectory();
            }
        }
        if (dir.isEmpty()) {
            dir = QDir::homePath();
        }
    }

    auto* surface = new TerminalSurface(splitter, dir);
    setupSurface(surface);

    auto* pane = new TerminalPane(surface, splitter);
    setupPane(pane);

    TerminalPane* currentPane = findPaneForSurface(target);
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
    updatePaneDecorations();

    setActiveSurface(surface);
    surface->setFocus();
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

    int savedScroll = horizontalScrollBar()->value();
    bool wasActive = (m_activeSurface == surface);
    if (wasActive) {
        m_activeSurface = nullptr;
    }

    // Determine neighbor to focus if wasActive
    TerminalSurface* nextSurface = nullptr;
    int colIdx = m_columns.indexOf(colSplitter);
    int paneIdx = colSplitter->indexOf(pane);

    if (colSplitter->count() > 1) {
        // Another pane in the same column
        int nextPaneIdx = (paneIdx + 1 < colSplitter->count()) ? (paneIdx + 1) : (paneIdx - 1);
        if (nextPaneIdx >= 0 && nextPaneIdx < colSplitter->count()) {
            if (auto* p = qobject_cast<TerminalPane*>(colSplitter->widget(nextPaneIdx))) {
                nextSurface = p->surface();
            }
        }
    } else if (m_columns.size() > 1) {
        // Entire column is closing; pick adjacent column
        int nextColIdx = (colIdx + 1 < m_columns.size()) ? (colIdx + 1) : (colIdx - 1);
        if (nextColIdx >= 0 && nextColIdx < m_columns.size()) {
            KhosttySplitter* nextCol = m_columns[nextColIdx];
            if (nextCol->count() > 0) {
                if (auto* p = qobject_cast<TerminalPane*>(nextCol->widget(0))) {
                    nextSurface = p->surface();
                }
            }
        }
    }

    // Safely remove pane from splitter before scheduling deletion
    pane->hide();
    pane->setParent(nullptr);
    pane->deleteLater();

    if (colSplitter->count() == 0) {
        m_columns.removeOne(colSplitter);
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
    updatePaneDecorations();

    // Restore scroll position
    horizontalScrollBar()->setValue(savedScroll);

    if (m_columns.isEmpty()) {
        m_activeSurface = nullptr;
        QTimer::singleShot(0, this, [this]() {
            emit empty();
        });
    } else if (wasActive) {
        if (nextSurface) {
            setActiveSurface(nextSurface);
            nextSurface->setFocus();
            ensureSurfaceVisible(nextSurface);
        }
    } else {
        if (m_activeSurface) {
            ensureSurfaceVisible(m_activeSurface);
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
            if (auto* pane = qobject_cast<TerminalPane*>(splitter->widget(i))) {
                pane->setZoomed(false);
            }
        }
        m_isZoomed = false;
    } else {
        for (int i = 0; i < splitter->count(); ++i) {
            QWidget* w = splitter->widget(i);
            if (w != currentPane) {
                w->hide();
            }
        }
        currentPane->setZoomed(true);
        m_isZoomed = true;
    }
}

void WorklaneView::toggleSearchBar() {
    if (m_searchBar && m_searchBar->isVisible()) {
        hideSearchBar();
    } else {
        showSearchBar();
    }
}

void WorklaneView::showSearchBar() {
    if (!m_searchBar) return;
    m_searchBar->adjustSize();
    int x = std::max(10, viewport()->width() - m_searchBar->width() - 20);
    m_searchBar->move(x, 10);
    m_searchBar->show();
    m_searchBar->raise();
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
    if (!m_searchEdit->text().isEmpty() && m_activeSurface) {
        m_activeSurface->startSearch(m_searchEdit->text());
    }
}

void WorklaneView::hideSearchBar() {
    if (!m_searchBar) return;
    m_searchBar->hide();
    if (m_activeSurface) {
        m_activeSurface->endSearch();
        m_activeSurface->setFocus();
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
    if (watched == m_searchEdit && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            hideSearchBar();
            return true;
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (ke->modifiers() & Qt::ShiftModifier) {
                if (m_activeSurface) m_activeSurface->navigateSearch(false);
                return true;
            }
        }
    }

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
