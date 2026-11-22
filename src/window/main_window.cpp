#include "window/main_window.h"
#include "core/ghostty_core.h"
#include "terminal/terminal_surface.h"
#include "terminal/worklane_view.h"
#include "window/worklane_sidebar.h"

#include <KAcceleratorManager>
#include <KActionCollection>
#include <KColorButton>
#include <KCommandBar>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNotification>
#include <KSharedConfig>
#include <KStandardAction>
#include <KToolBar>
#include <KXMLGUIFactory>
#ifdef KHOSTTY_HAS_KWINDOWSYSTEM
#include <KWindowEffects>
#endif
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabBar>
#include <QTextStream>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

MainWindow::MainWindow(QWidget *parent) : KXmlGuiWindow(parent) {
  setAttribute(Qt::WA_TranslucentBackground, true);

  m_tabBar = new QTabBar(this);
  m_tabBar->setObjectName(QStringLiteral("khostty-tabs"));
  m_tabBar->setDocumentMode(true);
  m_tabBar->setAutoFillBackground(true);
  m_tabBar->setMovable(true);
  m_tabBar->setTabsClosable(true);
  m_tabBar->setExpanding(false);
  m_tabBar->setElideMode(Qt::ElideRight);
  m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
  m_tabBar->setFocusPolicy(Qt::NoFocus);
  KAcceleratorManager::setNoAccel(m_tabBar);
  KAcceleratorManager::setNoAccel(this);

  m_stack = new QStackedWidget(this);

  m_sidebar = new WorklaneSidebar(this, this);
  m_sidebar->setVisible(false);
  connect(m_sidebar, &WorklaneSidebar::newWorklaneRequested, this,
          &MainWindow::newTab);

  // Main horizontal splitter holding [ WorklaneSidebar | ContentArea ]
  m_mainSplitter = new QSplitter(Qt::Horizontal, this);
  m_mainSplitter->setHandleWidth(1);
  m_mainSplitter->setChildrenCollapsible(false);
  m_mainSplitter->addWidget(m_sidebar);

  auto *rightArea = new QWidget(m_mainSplitter);
  auto *rightLayout = new QVBoxLayout(rightArea);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(0);
  rightLayout->addWidget(m_tabBar);
  rightLayout->addWidget(m_stack, 1);
  m_mainSplitter->addWidget(rightArea);

  m_mainSplitter->setStretchFactor(0, 0);
  m_mainSplitter->setStretchFactor(1, 1);

  setCentralWidget(m_mainSplitter);

  connect(m_tabBar, &QTabBar::currentChanged, m_stack,
          &QStackedWidget::setCurrentIndex);
  connect(m_tabBar, &QTabBar::currentChanged, this, &MainWindow::onTabChanged);
  connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::closeTab);
  connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
    QWidget *page = m_stack->widget(from);
    if (!page)
      return;
    m_stack->removeWidget(page);
    m_stack->insertWidget(to, page);
    m_stack->setCurrentIndex(m_tabBar->currentIndex());
  });
  connect(m_tabBar, &QTabBar::customContextMenuRequested, this,
          [this](const QPoint &position) {
            const int index = m_tabBar->tabAt(position);
            if (index < 0)
              return;
            QMenu menu(this);
            QAction *closeAction = menu.addAction(i18n("Close Tab"));
            if (menu.exec(m_tabBar->mapToGlobal(position)) == closeAction) {
              closeTab(index);
            }
          });

  createTab();

  setupActions();
  setupGUI(Keys | Save | Create, QStringLiteral("khosttyui.rc"));
  if (menuBar()) {
    menuBar()->setAutoFillBackground(true);
    KAcceleratorManager::setNoAccel(menuBar());
  }
  if (toolBar()) {
    toolBar()->hide();
  }

  connect(&GhosttyCore::instance(), &GhosttyCore::configChanged, this,
          &MainWindow::syncAppearance);

  syncColorScheme();
  syncAppearance();

  resize(1000, 700);
  setWindowTitle(QStringLiteral("Khostty"));
}

MainWindow::~MainWindow() = default;

int MainWindow::tabCount() const { return m_stack ? m_stack->count() : 0; }

WorklaneView *MainWindow::worklaneAt(int index) const {
  if (!m_stack || index < 0 || index >= m_stack->count())
    return nullptr;
  return qobject_cast<WorklaneView *>(m_stack->widget(index));
}

void MainWindow::setCurrentTabIndex(int index) {
  if (m_tabBar && index >= 0 && index < m_tabBar->count()) {
    m_tabBar->setCurrentIndex(index);
  }
}

void MainWindow::moveTab(int fromIndex, int toIndex) {
  if (fromIndex == toIndex || fromIndex < 0 || toIndex < 0)
    return;
  if (fromIndex >= m_tabBar->count() || toIndex >= m_tabBar->count())
    return;
  m_tabBar->moveTab(fromIndex, toIndex);
  if (m_sidebar && m_sidebarVisible) {
    m_sidebar->refreshTree();
  }
}

void MainWindow::movePane(TerminalSurface *surface, int targetTabIndex,
                          TerminalSurface *targetSurface, bool insertAfter,
                          bool stackUnder) {
  if (!surface)
    return;

  // Locate source tab / WorklaneView
  WorklaneView *sourceWl = nullptr;
  int sourceTabIndex = -1;
  for (int i = 0; i < tabCount(); ++i) {
    WorklaneView *wl = worklaneAt(i);
    if (wl && wl->columnIndexOf(surface) != -1) {
      sourceWl = wl;
      sourceTabIndex = i;
      break;
    }
  }
  if (!sourceWl || sourceTabIndex < 0)
    return;

  WorklaneView *targetWl = worklaneAt(targetTabIndex);
  if (!targetWl)
    return;

  // Check host / session compatibility (e.g. SSH / remote boundaries)
  if (!targetWl->canAcceptPaneFrom(sourceWl))
    return;

  // Guard no-op moves
  if (sourceWl == targetWl) {
    if (sourceWl->paneCount() <= 1) {
      return;
    }
    if (targetSurface == surface) {
      return;
    }
    int srcCol = sourceWl->columnIndexOf(surface);
    int srcRow = sourceWl->rowIndexOf(surface);
    if (targetSurface) {
      int tgtCol = targetWl->columnIndexOf(targetSurface);
      int tgtRow = targetWl->rowIndexOf(targetSurface);
      if (!stackUnder) {
        if (sourceWl->rowCount(srcCol) == 1 && srcCol == tgtCol) {
          return;
        }
        if (srcCol == tgtCol - 1 && !insertAfter)
          return;
        if (srcCol == tgtCol + 1 && insertAfter)
          return;
      } else {
        if (srcCol == tgtCol && srcRow == tgtRow + 1) {
          return;
        }
      }
    }
  }

  TerminalPane *pane = sourceWl->detachSurface(surface);
  if (!pane)
    return;

  if (stackUnder) {
    if (targetSurface) {
      int destCol = targetWl->columnIndexOf(targetSurface);
      int destRow = targetWl->rowIndexOf(targetSurface);
      if (destCol != -1 && destRow != -1) {
        targetWl->attachPaneToColumn(pane, destCol, destRow + 1);
      } else {
        int lastCol = std::max(0, targetWl->columnCount() - 1);
        targetWl->attachPaneToColumn(pane, lastCol,
                                     targetWl->rowCount(lastCol));
      }
    } else {
      int lastCol = std::max(0, targetWl->columnCount() - 1);
      targetWl->attachPaneToColumn(pane, lastCol, targetWl->rowCount(lastCol));
    }
  } else {
    if (targetSurface) {
      int destCol = targetWl->columnIndexOf(targetSurface);
      int insertCol = (destCol != -1) ? (insertAfter ? destCol + 1 : destCol)
                                      : targetWl->columnCount();
      targetWl->attachPaneAsColumn(pane, insertCol);
    } else {
      targetWl->attachPaneAsColumn(pane, targetWl->columnCount());
    }
  }

  int currentTargetIdx = m_stack->indexOf(targetWl);
  if (currentTargetIdx != -1) {
    setCurrentTabIndex(currentTargetIdx);
    updateTabTitleWithSplitCount(currentTargetIdx);
  }
  if (sourceTabIndex >= 0 && sourceTabIndex < m_stack->count()) {
    updateTabTitleWithSplitCount(sourceTabIndex);
  }
  if (m_sidebar && m_sidebarVisible) {
    m_sidebar->refreshTree();
  }
}

void MainWindow::toggleSidebar() {
  m_sidebarVisible = !m_sidebarVisible;
  if (m_toggleSidebarAction &&
      m_toggleSidebarAction->isChecked() != m_sidebarVisible) {
    m_toggleSidebarAction->setChecked(m_sidebarVisible);
  }
  if (m_sidebar) {
    if (!m_sidebarVisible) {
      // Save width before hiding
      QList<int> currentSizes = m_mainSplitter->sizes();
      if (!currentSizes.isEmpty() && currentSizes[0] >= 120) {
        m_sidebarSavedWidth = currentSizes[0];
      }
    }
    m_sidebar->setVisible(m_sidebarVisible);
    if (m_sidebarVisible) {
      m_sidebar->refreshTree();
      QList<int> sizes = m_mainSplitter->sizes();
      if (sizes.size() >= 2) {
        int targetW = std::clamp(m_sidebarSavedWidth, 180, 450);
        sizes[0] = targetW;
        sizes[1] = std::max(300, sizes[1] - targetW);
        m_mainSplitter->setSizes(sizes);
      }
    }
  }
  updateTabBarVisibility();
}

void MainWindow::updateTabBarVisibility() {
  if (m_sidebarVisible) {
    m_tabBar->hide();
  } else {
    bool multiTab = m_tabBar->count() > 1;
    m_tabBar->setVisible(multiTab);
  }
  bool multiTab = m_tabBar->count() > 1;
  for (auto *action : m_tabSwitchActions) {
    if (action) {
      action->setEnabled(multiTab);
    }
  }
}

void MainWindow::updateTabTitleWithSplitCount(int index) {
  if (index < 0 || index >= m_stack->count())
    return;
  auto *worklane = qobject_cast<WorklaneView *>(m_stack->widget(index));
  if (!worklane)
    return;

  QString baseTitle = worklane->activeTitle();
  if (baseTitle.isEmpty())
    baseTitle = i18n("Terminal");

  int pCount = worklane->paneCount();
  QString displayTitle;
  if (pCount > 1) {
    displayTitle = QStringLiteral("%1 · %2").arg(baseTitle).arg(pCount);
  } else {
    displayTitle = baseTitle;
  }

  m_tabBar->setTabText(index, displayTitle);
  m_tabBar->setTabToolTip(index, displayTitle);

  if (worklane == currentWorklane()) {
    updateTitle(baseTitle);
  }

  if (m_sidebar && m_sidebarVisible) {
    m_sidebar->scheduleRefresh();
  }
}

WorklaneView *MainWindow::createTab(const QString &title,
                                    const QString &initialDir) {
  QString dir = initialDir.trimmed();
  if (dir.isEmpty()) {
    dir = QDir::homePath();
  }
  auto *worklane = new WorklaneView(m_stack, dir);

  QString tabTitle = title.isEmpty() ? i18n("Terminal") : title;
  int idx = m_stack->addWidget(worklane);
  m_tabBar->insertTab(idx, tabTitle);
  m_tabBar->setTabToolTip(idx, tabTitle);
  m_tabBar->setCurrentIndex(idx);
  updateTabBarVisibility();

  connect(worklane, &WorklaneView::titleChanged, this,
          [this, worklane](const QString &) {
            int tabIdx = m_stack->indexOf(worklane);
            if (tabIdx != -1) {
              updateTabTitleWithSplitCount(tabIdx);
            }
          });

  connect(worklane, &WorklaneView::layoutChanged, this, [this, worklane]() {
    int tabIdx = m_stack->indexOf(worklane);
    if (tabIdx != -1) {
      updateTabTitleWithSplitCount(tabIdx);
    }
  });

  connect(worklane, &WorklaneView::empty, this, [this, worklane]() {
    int tabIdx = m_stack->indexOf(worklane);
    if (tabIdx != -1) {
      closeTab(tabIdx);
    }
  });

  auto wireSurface = [this](TerminalSurface *surface) {
    if (!surface)
      return;
    connect(surface, &TerminalSurface::newTabRequested, this,
            &MainWindow::newTab, Qt::UniqueConnection);
    connect(surface, &TerminalSurface::contextMenuRequested, this,
            &MainWindow::showSurfaceContextMenu, Qt::UniqueConnection);
    connect(surface, &TerminalSurface::pwdChanged, m_sidebar,
            &WorklaneSidebar::scheduleRefresh, Qt::UniqueConnection);
    connect(surface, &TerminalSurface::gotoTabRequested, this,
            &MainWindow::gotoTab, Qt::UniqueConnection);
    connect(surface, &TerminalSurface::nextTabRequested, this,
            &MainWindow::nextTab, Qt::UniqueConnection);
    connect(surface, &TerminalSurface::previousTabRequested, this,
            &MainWindow::prevTab, Qt::UniqueConnection);
    connect(surface, &TerminalSurface::toggleCommandPaletteRequested, this,
            &MainWindow::showCommandBar, Qt::UniqueConnection);
    connect(surface, &TerminalSurface::bell, this, &MainWindow::handleBell,
            Qt::UniqueConnection);
    connect(surface, &TerminalSurface::desktopNotification, this,
            &MainWindow::handleDesktopNotification, Qt::UniqueConnection);
  };

  connect(worklane, &WorklaneView::activeSurfaceChanged, this,
          [this](TerminalSurface *surface) {
            if (surface && m_sidebar) {
              m_sidebar->clearAlert(surface);
              if (m_sidebarVisible) {
                m_sidebar->scheduleRefresh();
              }
            }
          });
  connect(worklane, &WorklaneView::activeSurfaceChanged, this, wireSurface);
  if (worklane->activeSurface()) {
    wireSurface(worklane->activeSurface());
    worklane->activeSurface()->setFocus();
  }

  return worklane;
}

WorklaneView *MainWindow::currentWorklane() const {
  return qobject_cast<WorklaneView *>(m_stack->currentWidget());
}

TerminalSurface *MainWindow::activeSurface() const {
  auto *wl = currentWorklane();
  return wl ? wl->activeSurface() : nullptr;
}

void MainWindow::setupActions() {
  KActionCollection *ac = actionCollection();

  auto addAct = [this, ac](const QString &name, const QString &text, auto slot,
                           std::initializer_list<QKeySequence> shortcuts,
                           bool checkable = false) -> QAction * {
    auto *act = new QAction(text, this);
    act->setCheckable(checkable);
    connect(act, &QAction::triggered, this, std::move(slot));
    ac->addAction(name, act);
    if (shortcuts.size() == 1) {
      ac->setDefaultShortcut(act, *shortcuts.begin());
    } else if (shortcuts.size() > 1) {
      ac->setDefaultShortcuts(act, QList<QKeySequence>(shortcuts));
    }
    return act;
  };

  // Standard Window & Tab Actions (Matching Ghostty Defaults)
  addAct(QStringLiteral("new_tab"), i18n("New Tab"), &MainWindow::newTab,
         {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T)});
  addAct(QStringLiteral("new_window"), i18n("New Window"),
         &MainWindow::newWindow,
         {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N)});
  addAct(QStringLiteral("close_active"), i18n("Close Active Terminal"),
         &MainWindow::closeActive,
         {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W)});
  addAct(QStringLiteral("close_tab"), i18n("Close Tab"),
         &MainWindow::closeCurrentTab,
         {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_W)});
  addAct(QStringLiteral("close_window"), i18n("Close Window"),
         &MainWindow::close, {QKeySequence(Qt::ALT | Qt::Key_F4)});
  addAct(
      QStringLiteral("file_quit"), i18n("Quit"),
      []() { QCoreApplication::quit(); },
      {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q)});

  auto *copyAction = KStandardAction::copy(this, &MainWindow::copyText, ac);
  ac->setDefaultShortcut(copyAction,
                         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));

  auto *pasteAction = KStandardAction::paste(this, &MainWindow::pasteText, ac);
  ac->setDefaultShortcut(pasteAction,
                         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));

  auto *findAction = KStandardAction::find(this, &MainWindow::findText, ac);
  ac->setDefaultShortcut(findAction,
                         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));

  addAct(
      QStringLiteral("clear_scrollback"), i18n("Clear Scrollback"),
      [this]() {
        if (auto *surface = activeSurface()) {
          surface->performBindingAction("clear_screen");
        }
      },
      {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K)});

  // View Zoom Actions
  auto *zoomInAction = KStandardAction::zoomIn(this, &MainWindow::zoomIn, ac);
  ac->setDefaultShortcuts(zoomInAction,
                          {QKeySequence(Qt::CTRL | Qt::Key_Plus),
                           QKeySequence(Qt::CTRL | Qt::Key_Equal)});

  auto *zoomOutAction =
      KStandardAction::zoomOut(this, &MainWindow::zoomOut, ac);
  ac->setDefaultShortcut(zoomOutAction, QKeySequence(Qt::CTRL | Qt::Key_Minus));

  addAct(QStringLiteral("view_zoom_reset"), i18n("Reset Zoom"),
         &MainWindow::zoomReset, {QKeySequence(Qt::CTRL | Qt::Key_0)});

  // Tab Navigation (Matching Ghostty Defaults: Ctrl+Shift+Left/Right,
  // Ctrl+PageUp/Down)
  addAct(QStringLiteral("next_tab"), i18n("Next Tab"), &MainWindow::nextTab,
         {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Right),
          QKeySequence(Qt::CTRL | Qt::Key_PageDown)});
  addAct(QStringLiteral("prev_tab"), i18n("Previous Tab"), &MainWindow::prevTab,
         {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Left),
          QKeySequence(Qt::CTRL | Qt::Key_PageUp)});

  // Tab Selection Alt+1..8, Alt+9 for last tab
  m_tabSwitchActions.clear();
  for (int i = 1; i <= 8; ++i) {
    m_tabSwitchActions.append(
        addAct(QStringLiteral("goto_tab_%1").arg(i), i18n("Go to Tab %1", i),
               [this, i]() { gotoTab(i); },
               {QKeySequence(Qt::ALT | (Qt::Key_0 + i))}));
  }
  m_tabSwitchActions.append(
      addAct(QStringLiteral("goto_tab_last"), i18n("Go to Last Tab"),
             [this]() { gotoTab(GHOSTTY_GOTO_TAB_LAST); },
             {QKeySequence(Qt::ALT | Qt::Key_9)}));

  // Command Bar (created by KXmlGuiWindow constructor; only adjust defaults)
  if (auto *cmdBarAction = ac->action(QStringLiteral("open_kcommand_bar"))) {
    ac->setDefaultShortcuts(cmdBarAction,
                            {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P),
                             QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I)});
  }

  // Sidebar Toggle Action (Ctrl+Shift+B)
  m_toggleSidebarAction = addAct(
      QStringLiteral("toggle_sidebar"), i18n("Toggle Worklane Sidebar"),
      &MainWindow::toggleSidebar,
      {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B)}, true);
  m_toggleSidebarAction->setChecked(m_sidebarVisible);

  // Worklane Split Actions (Matching Ghostty Defaults: Ctrl+Shift+O Right,
  // Ctrl+Shift+E Down)
  addAct(QStringLiteral("new_split_right"), i18n("Split Right"),
         &MainWindow::newColumn,
         {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O)});
  addAct(QStringLiteral("new_split_down"), i18n("Split Down"),
         &MainWindow::splitVertical,
         {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E)});

  // Split Zoom & Equalize (Matching Ghostty: Ctrl+Shift+Enter Zoom, Ctrl+Alt+=
  // Equalize)
  addAct(QStringLiteral("toggle_split_zoom"), i18n("Toggle Split Zoom"),
         &MainWindow::toggleSplitZoom,
         {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Return)});
  addAct(QStringLiteral("equalize_splits"), i18n("Equalize Splits"),
         &MainWindow::equalizeSplits,
         {QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Equal)});

  // Split Navigation (Matching Ghostty Defaults: Ctrl+Alt+Arrow keys)
  addAct(
      QStringLiteral("focus_prev_column"), i18n("Focus Left Split"),
      [this]() {
        if (auto *wl = currentWorklane())
          wl->focusLeftColumn();
      },
      {QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left)});
  addAct(
      QStringLiteral("focus_next_column"), i18n("Focus Right Split"),
      [this]() {
        if (auto *wl = currentWorklane())
          wl->focusRightColumn();
      },
      {QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Right)});
  addAct(
      QStringLiteral("focus_up"), i18n("Focus Up Split"),
      [this]() {
        if (auto *wl = currentWorklane())
          wl->focusUpSplit();
      },
      {QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Up)});
  addAct(
      QStringLiteral("focus_down"), i18n("Focus Down Split"),
      [this]() {
        if (auto *wl = currentWorklane())
          wl->focusDownSplit();
      },
      {QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Down)});

  // Settings
  KStandardAction::preferences(this, &MainWindow::showPreferencesDialog, ac);
  KStandardAction::keyBindings(
      guiFactory(), &KXMLGUIFactory::showConfigureShortcutsDialog, ac);
}

void MainWindow::newWindow() {
  auto *win = new MainWindow();
  win->show();
}

void MainWindow::newTab() {
  QString initialDir;
  KConfigGroup genCfg(KSharedConfig::openConfig(QStringLiteral("khosttyrc")),
                      QStringLiteral("General"));
  bool inherit = genCfg.readEntry("NewTabInheritsPwd", false);
  if (inherit) {
    auto *curWl = currentWorklane();
    if (curWl && curWl->activeSurface()) {
      initialDir = curWl->activeSurface()->currentWorkingDirectory();
    }
  }
  if (initialDir.isEmpty()) {
    initialDir = QDir::homePath();
  }
  createTab(QString(), initialDir);
}

void MainWindow::closeCurrentTab() { closeTab(m_tabBar->currentIndex()); }

void MainWindow::closeTab(int index) {
  if (index < 0 || index >= m_stack->count()) {
    return;
  }

  QWidget *page = m_stack->widget(index);
  m_stack->removeWidget(page);
  m_tabBar->removeTab(index);
  updateTabBarVisibility();
  page->deleteLater();

  if (m_sidebar && m_sidebarVisible) {
    m_sidebar->refreshTree();
  }

  if (m_tabBar->count() == 0) {
    close();
  } else {
    auto *wl = currentWorklane();
    if (wl && wl->activeSurface()) {
      wl->activeSurface()->setFocus();
      updateTitle(wl->activeTitle());
    }
  }
}

void MainWindow::gotoTab(int tabIdx) {
  if (tabIdx == GHOSTTY_GOTO_TAB_PREVIOUS) {
    prevTab();
  } else if (tabIdx == GHOSTTY_GOTO_TAB_NEXT) {
    nextTab();
  } else if (tabIdx == GHOSTTY_GOTO_TAB_LAST) {
    if (m_tabBar->count() > 0) {
      m_tabBar->setCurrentIndex(m_tabBar->count() - 1);
    }
  } else if (tabIdx >= 1 && m_tabBar->count() > 0) {
    m_tabBar->setCurrentIndex(std::min(tabIdx, m_tabBar->count()) - 1);
  }
}

void MainWindow::nextTab() {
  if (m_tabBar->count() > 1) {
    int next = (m_tabBar->currentIndex() + 1) % m_tabBar->count();
    m_tabBar->setCurrentIndex(next);
  }
}

void MainWindow::prevTab() {
  if (m_tabBar->count() > 1) {
    int prev =
        (m_tabBar->currentIndex() - 1 + m_tabBar->count()) % m_tabBar->count();
    m_tabBar->setCurrentIndex(prev);
  }
}

void MainWindow::onTabChanged(int index) {
  if (index >= 0 && index < m_stack->count()) {
    auto *wl = qobject_cast<WorklaneView *>(m_stack->widget(index));
    if (wl) {
      if (wl->activeSurface()) {
        wl->activeSurface()->setFocus();
        if (m_sidebar) {
          m_sidebar->clearAlert(wl->activeSurface());
        }
      }
      updateTitle(wl->activeTitle());
    }
  }
  if (m_sidebar && m_sidebarVisible) {
    m_sidebar->refreshTree();
  }
}

void MainWindow::newColumn() {
  auto *wl = currentWorklane();
  if (wl) {
    wl->addColumn();
  }
}

void MainWindow::splitVertical() {
  auto *wl = currentWorklane();
  if (wl) {
    wl->splitCurrentVertical();
  }
}

void MainWindow::closeActive() {
  auto *wl = currentWorklane();
  if (!wl)
    return;
  auto *s = wl->activeSurface();
  if (s && s->surface() && ghostty_surface_needs_confirm_quit(s->surface())) {
    auto res = KMessageBox::warningContinueCancel(
        this,
        i18n("A process is running in this terminal. Closing it may terminate "
             "the process. Do you want to continue?"),
        i18n("Confirm Close"), KStandardGuiItem::close());
    if (res != KMessageBox::Continue) {
      return;
    }
  }
  wl->closeActiveSurface();
}

void MainWindow::findText() {
  if (auto *wl = currentWorklane()) {
    wl->toggleSearchBar();
  }
}

void MainWindow::zoomIn() {
  if (TerminalSurface *surface = activeSurface()) {
    surface->performBindingAction("increase_font_size:1");
  }
}

void MainWindow::zoomOut() {
  if (TerminalSurface *surface = activeSurface()) {
    surface->performBindingAction("decrease_font_size:1");
  }
}

void MainWindow::zoomReset() {
  if (TerminalSurface *surface = activeSurface()) {
    surface->performBindingAction("reset_font_size");
  }
}

void MainWindow::toggleSplitZoom() {
  auto *wl = currentWorklane();
  if (wl) {
    wl->toggleSplitZoom();
  }
}

void MainWindow::equalizeSplits() {
  auto *wl = currentWorklane();
  if (wl) {
    wl->equalizeSplits();
  }
}

void MainWindow::copyText() {
  if (TerminalSurface *surface = activeSurface()) {
    surface->performBindingAction("copy_to_clipboard");
  }
}

void MainWindow::pasteText() {
  if (TerminalSurface *surface = activeSurface()) {
    surface->performBindingAction("paste_from_clipboard");
  }
}

void MainWindow::showSurfaceContextMenu(const QPoint &globalPos,
                                        const QString &hoveredUrl) {
  auto *surface = qobject_cast<TerminalSurface *>(sender());
  if (!surface)
    return;

  for (int i = 0; i < m_stack->count(); ++i) {
    if (auto *wl = worklaneAt(i)) {
      if (wl->columnIndexOf(surface) != -1) {
        setCurrentTabIndex(i);
        wl->setActiveSurface(surface);
        break;
      }
    }
  }
  surface->setFocus();

  QPointer<TerminalSurface> safeSurface(surface);
  QMenu menu(this);

  if (!hoveredUrl.isEmpty()) {
    const QString capturedUrl = hoveredUrl;
    QAction *openLink = menu.addAction(
        QIcon::fromTheme(QStringLiteral("internet-services")), i18n("Open Link"));
    connect(openLink, &QAction::triggered, &menu,
            [capturedUrl]() { TerminalSurface::openExternalUrl(capturedUrl); });

    QAction *copyLink = menu.addAction(
        QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy Link Address"));
    connect(copyLink, &QAction::triggered, &menu, [capturedUrl]() {
      QGuiApplication::clipboard()->setText(capturedUrl);
    });
    menu.addSeparator();
  }

  KActionCollection *ac = actionCollection();
  if (QAction *sharedCopy = ac->action(QStringLiteral("edit_copy"))) {
    auto *copyAct = menu.addAction(sharedCopy->icon(), sharedCopy->text());
    copyAct->setShortcut(sharedCopy->shortcut());
    const bool hasSel =
        surface->surface() && ghostty_surface_has_selection(surface->surface());
    copyAct->setEnabled(hasSel);
    connect(copyAct, &QAction::triggered, &menu, [safeSurface]() {
      if (safeSurface)
        safeSurface->performBindingAction("copy_to_clipboard");
    });
  }
  if (QAction *pasteAct = ac->action(QStringLiteral("edit_paste"))) {
    menu.addAction(pasteAct);
  }
  menu.addSeparator();

  const QString cwd = surface->currentWorkingDirectory();
  QAction *openFolder = menu.addAction(
      QIcon::fromTheme(QStringLiteral("system-file-manager")),
      i18n("Open File Manager"));
  openFolder->setToolTip(cwd);
  connect(openFolder, &QAction::triggered, &menu,
          [cwd]() { QDesktopServices::openUrl(QUrl::fromLocalFile(cwd)); });

  menu.addSeparator();
  if (QAction *clearAct = ac->action(QStringLiteral("clear_scrollback"))) {
    menu.addAction(clearAct);
  }

  menu.addSeparator();
  QMenu *splitMenu = menu.addMenu(
      QIcon::fromTheme(QStringLiteral("view-split-left-right")),
      i18n("Split View"));
  auto addSplitAct = [ac, splitMenu](const char *name) {
    if (QAction *a = ac->action(QLatin1String(name)))
      splitMenu->addAction(a);
  };
  addSplitAct("new_split_right");
  addSplitAct("new_split_down");
  splitMenu->addSeparator();
  addSplitAct("toggle_split_zoom");
  addSplitAct("equalize_splits");
  splitMenu->addSeparator();
  addSplitAct("close_active");

  for (const char *name : {"new_tab", "new_window"}) {
    if (QAction *a = ac->action(QLatin1String(name)))
      menu.addAction(a);
  }
  menu.addSeparator();
  if (QAction *cfgAct = ac->action(QStringLiteral("options_configure"))) {
    menu.addAction(cfgAct);
  }

  menu.exec(globalPos);
}

void MainWindow::showPreferencesDialog() {
  QDialog dialog(this);
  dialog.setWindowTitle(i18n("Configure — Khostty"));
  dialog.setMinimumWidth(440);

  auto *layout = new QVBoxLayout(&dialog);
  KSharedConfig::Ptr cfg =
      KSharedConfig::openConfig(QStringLiteral("khosttyrc"));

  // Appearance & Layout Settings
  auto *appearanceGroup = new QGroupBox(i18n("Appearance & Layout"), &dialog);
  auto *appearanceLayout = new QVBoxLayout(appearanceGroup);

  KConfigGroup appCfg(cfg, QStringLiteral("Appearance"));
  bool showHeaderOnlyOnSplit = appCfg.readEntry("ShowHeaderOnlyOnSplit", true);
  bool showWorklaneScrollbar = appCfg.readEntry("ShowWorklaneScrollbar", false);

  auto *hideSingleHeaderCheck = new QCheckBox(
      i18n("Hide pane header when only one terminal is open"), appearanceGroup);
  hideSingleHeaderCheck->setChecked(showHeaderOnlyOnSplit);
  appearanceLayout->addWidget(hideSingleHeaderCheck);

  auto *showScrollbarCheck = new QCheckBox(
      i18n("Show horizontal scrollbar on worklane canvas when overflowed"),
      appearanceGroup);
  showScrollbarCheck->setChecked(showWorklaneScrollbar);
  appearanceLayout->addWidget(showScrollbarCheck);

  // Sidebar Accent Color Override
  QString customAccentStr = appCfg.readEntry("CustomAccentColor", QString());
  bool useCustomAccent = !customAccentStr.isEmpty();
  QColor initialAccent =
      useCustomAccent ? QColor(customAccentStr)
                      : (m_sidebar ? m_sidebar->accentColor()
                                   : palette().color(QPalette::Highlight));

  auto *accentLayout = new QHBoxLayout();
  auto *customAccentCheck =
      new QCheckBox(i18n("Override sidebar accent color:"), appearanceGroup);
  customAccentCheck->setChecked(useCustomAccent);

  auto *colorBtn = new KColorButton(initialAccent, appearanceGroup);
  colorBtn->setEnabled(useCustomAccent);
  colorBtn->setDefaultColor(m_sidebar ? m_sidebar->accentColor()
                                      : palette().color(QPalette::Highlight));

  connect(customAccentCheck, &QCheckBox::toggled, colorBtn,
          &QWidget::setEnabled);

  accentLayout->addWidget(customAccentCheck);
  accentLayout->addWidget(colorBtn);
  accentLayout->addStretch();
  appearanceLayout->addLayout(accentLayout);

  layout->addWidget(appearanceGroup);

  // Working Directory (CWD) Settings
  auto *cwdGroup = new QGroupBox(i18n("Working Directory"), &dialog);
  auto *cwdLayout = new QVBoxLayout(cwdGroup);

  KConfigGroup genCfg(cfg, QStringLiteral("General"));
  bool newTabInherit = genCfg.readEntry("NewTabInheritsPwd", false);
  bool newSplitInherit = genCfg.readEntry("NewSplitInheritsPwd", true);

  auto *newTabInheritCheck = new QCheckBox(
      i18n("New tabs inherit working directory from active terminal"),
      cwdGroup);
  newTabInheritCheck->setChecked(newTabInherit);
  newTabInheritCheck->setToolTip(
      i18n("When unchecked, new tabs start in your home directory ($HOME)."));
  cwdLayout->addWidget(newTabInheritCheck);

  auto *newSplitInheritCheck = new QCheckBox(
      i18n("New splits inherit working directory from active terminal"),
      cwdGroup);
  newSplitInheritCheck->setChecked(newSplitInherit);
  newSplitInheritCheck->setToolTip(
      i18n("When unchecked, new splits start in your home directory ($HOME)."));
  cwdLayout->addWidget(newSplitInheritCheck);

  layout->addWidget(cwdGroup);

  // Font Settings
  auto *fontGroup = new QGroupBox(i18n("Font Settings"), &dialog);
  auto *fontLayout = new QVBoxLayout(fontGroup);

  QFont sysFixed = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  float sysPt = static_cast<float>(sysFixed.pointSizeF());
  if (sysPt <= 0.0f)
    sysPt = static_cast<float>(sysFixed.pointSize());
  if (sysPt <= 0.0f)
    sysPt = 10.0f;

  KConfigGroup fontCfg(cfg, QStringLiteral("Font"));
  bool useSys = fontCfg.readEntry("UseSystemFixedFont", true);
  float currentCustom = fontCfg.readEntry("FontSize", 0.0f);
  if (currentCustom <= 0.0f)
    currentCustom = sysPt;

  auto *useSysCheck = new QCheckBox(
      i18n("Use KDE system fixed font (%1, %2 pt)", sysFixed.family(), sysPt),
      fontGroup);
  useSysCheck->setChecked(useSys);
  fontLayout->addWidget(useSysCheck);

  auto *customSizeLayout = new QHBoxLayout();
  auto *customSizeLabel = new QLabel(i18n("Custom font size (pt):"), fontGroup);
  auto *customSizeSpin = new QDoubleSpinBox(fontGroup);
  customSizeSpin->setRange(6.0, 72.0);
  customSizeSpin->setSingleStep(0.5);
  customSizeSpin->setValue(currentCustom);
  customSizeSpin->setEnabled(!useSys);

  customSizeLayout->addWidget(customSizeLabel);
  customSizeLayout->addWidget(customSizeSpin);
  fontLayout->addLayout(customSizeLayout);

  connect(
      useSysCheck, &QCheckBox::toggled, customSizeSpin,
      [customSizeSpin](bool checked) { customSizeSpin->setEnabled(!checked); });

  layout->addWidget(fontGroup);

  // Ghostty Terminal Engine (Power Users)
  auto *engineGroup = new QGroupBox(i18n("Ghostty Terminal Engine"), &dialog);
  auto *engineLayout = new QVBoxLayout(engineGroup);

  QString ghosttyConfigDir = QStandardPaths::writableLocation(
                                 QStandardPaths::GenericConfigLocation) +
                             QStringLiteral("/ghostty");
  QString modernPath = ghosttyConfigDir + QStringLiteral("/config.ghostty");
  QString legacyPath = ghosttyConfigDir + QStringLiteral("/config");
  bool hasModern = QFile::exists(modernPath);
  bool hasLegacy = QFile::exists(legacyPath);
  QString displayPath = hasModern ? modernPath : (hasLegacy ? legacyPath : modernPath);
  QString engineDesc =
      i18n("Terminal engine features (cursor shape, scrollback, "
           "themes, keybindings) are powered by Ghostty and "
           "configured in %1.",
           displayPath);
  if (hasModern && hasLegacy) {
    engineDesc += QStringLiteral("\n") +
                  i18n("Note: Both config and config.ghostty exist; "
                       "config.ghostty takes precedence.");
  }
  auto *engineDescLabel = new QLabel(engineDesc, engineGroup);
  engineDescLabel->setWordWrap(true);
  engineLayout->addWidget(engineDescLabel);

  auto *engineBtnLayout = new QHBoxLayout();
  auto *editConfigBtn =
      new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")),
                      i18n("Edit Ghostty Config..."), engineGroup);
  auto *reloadConfigBtn =
      new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")),
                      i18n("Reload Ghostty Config"), engineGroup);

  connect(editConfigBtn, &QPushButton::clicked, &dialog, []() {
    QString configPath = GhosttyCore::instance().openConfigPath();
    if (configPath.isEmpty()) {
      return;
    }
    QFile configFile(configPath);
    if (configFile.exists() && configFile.size() == 0) {
      if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&configFile);
        out << "# Ghostty Terminal Configuration\n";
        out << "# Refer to https://ghostty.org/docs/config for available "
               "options.\n";
        configFile.close();
      }
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(configPath));
  });

  connect(reloadConfigBtn, &QPushButton::clicked, &dialog,
          []() { GhosttyCore::instance().reloadConfig(); });

  engineBtnLayout->addWidget(editConfigBtn);
  engineBtnLayout->addWidget(reloadConfigBtn);
  engineLayout->addLayout(engineBtnLayout);

  layout->addWidget(engineGroup);

  // Dialog Buttons
  auto *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
      Qt::Horizontal, &dialog);
  layout->addWidget(buttonBox);

  auto applyChanges = [=, this]() {
    KConfigGroup genGroup(cfg, QStringLiteral("General"));
    genGroup.writeEntry("NewTabInheritsPwd", newTabInheritCheck->isChecked());
    genGroup.writeEntry("NewSplitInheritsPwd",
                        newSplitInheritCheck->isChecked());

    KConfigGroup appGroup(cfg, QStringLiteral("Appearance"));
    appGroup.writeEntry("ShowHeaderOnlyOnSplit",
                        hideSingleHeaderCheck->isChecked());
    appGroup.writeEntry("ShowWorklaneScrollbar",
                        showScrollbarCheck->isChecked());
    if (customAccentCheck->isChecked()) {
      appGroup.writeEntry("CustomAccentColor", colorBtn->color().name());
    } else {
      appGroup.deleteEntry("CustomAccentColor");
    }

    KConfigGroup fontGroup(cfg, QStringLiteral("Font"));
    const bool oldUseSys = fontGroup.readEntry("UseSystemFixedFont", true);
    const double oldCustom = fontGroup.readEntry("FontSize", 0.0);
    const bool newUseSys = useSysCheck->isChecked();
    const double newCustom = customSizeSpin->value();
    const bool fontChanged =
        (newUseSys != oldUseSys) ||
        (!newUseSys && std::abs(newCustom - oldCustom) > 0.01);

    fontGroup.writeEntry("UseSystemFixedFont", newUseSys);
    fontGroup.writeEntry("FontSize", newCustom);

    cfg->sync();
    GhosttyCore::instance().reloadConfig();
    if (fontChanged) {
      GhosttyCore::instance().resetAllFontSizes();
    }
  };

  connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
          applyChanges);
  connect(buttonBox, &QDialogButtonBox::accepted, [&dialog, applyChanges]() {
    applyChanges();
    dialog.accept();
  });
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  dialog.exec();
}

void MainWindow::showCommandBar() {
  if (m_commandBar) {
    m_commandBar->close();
    return;
  }

  auto *bar = new KCommandBar(this);
  bar->setAttribute(Qt::WA_DeleteOnClose);
  m_commandBar = bar;

  KCommandBar::ActionGroup group;
  group.name = QStringLiteral("Actions");
  group.actions = actionCollection()->actions();
  bar->setActions({group});
  bar->show();
  bar->raise();
}

void MainWindow::updateTitle(const QString &title) {
  if (title.isEmpty()) {
    setWindowTitle(QStringLiteral("Khostty"));
  } else {
    setWindowTitle(QStringLiteral("%1 — Khostty").arg(title));
  }
}

void MainWindow::changeEvent(QEvent *event) {
  KXmlGuiWindow::changeEvent(event);
  if (event->type() == QEvent::ApplicationPaletteChange ||
      event->type() == QEvent::PaletteChange) {
    syncColorScheme();
  }
  if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
    ghostty_app_set_focus(GhosttyCore::instance().app(), true);
    auto *wl = currentWorklane();
    if (wl && wl->activeSurface()) {
      wl->activeSurface()->setFocus();
    }
  }
}

void MainWindow::showEvent(QShowEvent *event) {
  KXmlGuiWindow::showEvent(event);
  syncAppearance();
  auto *wl = currentWorklane();
  if (wl && wl->activeSurface()) {
    wl->activeSurface()->setFocus();
  }
}

void MainWindow::syncColorScheme() {
  bool isDark = (qApp->palette().color(QPalette::Window).lightness() < 128);
  GhosttyCore::instance().setColorScheme(isDark);
  for (int i = 0; i < m_stack->count(); ++i) {
    if (auto *wl = worklaneAt(i)) {
      for (auto *surface : wl->allSurfaces()) {
        if (surface) {
          surface->update();
        }
      }
    }
  }
}

void MainWindow::syncAppearance() {
#ifdef KHOSTTY_HAS_KWINDOWSYSTEM
  if (QWindow *win = windowHandle()) {
    const double opacity = GhosttyCore::instance().backgroundOpacity();
    const bool blur = GhosttyCore::instance().backgroundBlur() && (opacity < 1.0);
    KWindowEffects::enableBlurBehind(win, blur);
  }
#endif
  if (m_sidebar) {
    m_sidebar->updateAccentColor();
  }
  for (int i = 0; i < m_stack->count(); ++i) {
    if (auto *wl = worklaneAt(i)) {
      wl->updatePaneDecorations();
      for (auto *surface : wl->allSurfaces()) {
        if (surface) {
          surface->update();
        }
      }
    }
  }
  update();
}

void MainWindow::handleBell() {
  auto *s = qobject_cast<TerminalSurface *>(sender());
  if (s && s != activeSurface() && m_sidebar) {
    m_sidebar->markSurfaceAlert(s, true);
  }
  KNotification::event(QStringLiteral("bell"), i18n("Bell"), QString(),
                       QStringLiteral("utilities-terminal"));
}

void MainWindow::handleDesktopNotification(const QString &title,
                                           const QString &body) {
  auto *s = qobject_cast<TerminalSurface *>(sender());
  if (s && s != activeSurface() && m_sidebar) {
    m_sidebar->markSurfaceAlert(s, true);
  }
  KNotification::event(QStringLiteral("command_finished"), title, body,
                       QStringLiteral("utilities-terminal"));
}
