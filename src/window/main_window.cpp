#include "window/main_window.h"
#include "terminal/worklane_view.h"
#include "terminal/terminal_surface.h"
#include "core/ghostty_core.h"

#include <KActionCollection>
#include <KStandardAction>
#include <KCommandBar>
#include <KLocalizedString>
#include <KToolBar>
#include <KAcceleratorManager>
#include <QApplication>
#include <QClipboard>
#include <QKeySequence>
#include <QAction>
#include <QTabBar>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QFontDatabase>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KXMLGUIFactory>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>

MainWindow::MainWindow(QWidget* parent)
    : KXmlGuiWindow(parent)
{
    m_tabBar = new QTabBar(this);
    m_tabBar->setObjectName(QStringLiteral("khostty-tabs"));
    m_tabBar->setDocumentMode(true);
    m_tabBar->setMovable(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    KAcceleratorManager::setNoAccel(m_tabBar);

    m_stack = new QStackedWidget(this);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
    layout->addWidget(m_stack, 1);
    setCentralWidget(central);

    connect(m_tabBar, &QTabBar::currentChanged, m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_tabBar, &QTabBar::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        QWidget* page = m_stack->widget(from);
        if (!page) return;
        m_stack->removeWidget(page);
        m_stack->insertWidget(to, page);
        m_stack->setCurrentIndex(to);
    });
    connect(m_tabBar, &QTabBar::customContextMenuRequested, this, [this](const QPoint& position) {
        const int index = m_tabBar->tabAt(position);
        if (index < 0) return;
        QMenu menu(this);
        QAction* closeAction = menu.addAction(i18n("Close Tab"));
        if (menu.exec(m_tabBar->mapToGlobal(position)) == closeAction) {
            closeTab(index);
        }
    });

    createTab();

    setupActions();
    setupGUI(Keys | Save | Create, QStringLiteral("khosttyui.rc"));
    if (toolBar()) {
        toolBar()->hide();
    }

    connect(&GhosttyCore::instance(), &GhosttyCore::newWindowRequested, this, &MainWindow::newWindow);
    connect(&GhosttyCore::instance(), &GhosttyCore::newTabRequested, this, &MainWindow::newTab);
    connect(&GhosttyCore::instance(), &GhosttyCore::quitRequested, qApp, &QCoreApplication::quit);

    resize(1000, 700);
    setWindowTitle(QStringLiteral("Khostty"));
}

MainWindow::~MainWindow() = default;

void MainWindow::updateTabBarVisibility() {
    m_tabBar->setVisible(m_tabBar->count() > 1);
}

WorklaneView* MainWindow::createTab(const QString& title) {
    auto* worklane = new WorklaneView(m_stack);

    QString tabTitle = title.isEmpty() ? i18n("Terminal") : title;
    int idx = m_stack->addWidget(worklane);
    m_tabBar->insertTab(idx, tabTitle);
    m_tabBar->setTabToolTip(idx, tabTitle);
    m_tabBar->setCurrentIndex(idx);
    updateTabBarVisibility();

    connect(worklane, &WorklaneView::titleChanged, this, [this, worklane](const QString& newTitle) {
        int tabIdx = m_stack->indexOf(worklane);
        if (tabIdx != -1) {
            QString displayTitle = newTitle.isEmpty() ? i18n("Terminal") : newTitle;
            m_tabBar->setTabText(tabIdx, displayTitle);
            m_tabBar->setTabToolTip(tabIdx, displayTitle);
        }
        if (worklane == currentWorklane()) {
            updateTitle(newTitle);
        }
    });

    connect(worklane, &WorklaneView::empty, this, [this, worklane]() {
        int tabIdx = m_stack->indexOf(worklane);
        if (tabIdx != -1) {
            closeTab(tabIdx);
        }
    });

    auto wireSurface = [this](TerminalSurface* surface) {
        if (!surface) return;
        connect(surface, &TerminalSurface::gotoTabRequested, this, &MainWindow::gotoTab, Qt::UniqueConnection);
        connect(surface, &TerminalSurface::nextTabRequested, this, &MainWindow::nextTab, Qt::UniqueConnection);
        connect(surface, &TerminalSurface::previousTabRequested, this, &MainWindow::prevTab, Qt::UniqueConnection);
        connect(surface, &TerminalSurface::toggleCommandPaletteRequested, this, &MainWindow::showCommandBar, Qt::UniqueConnection);
    };

    connect(worklane, &WorklaneView::activeSurfaceChanged, this, wireSurface);
    if (worklane->activeSurface()) {
        wireSurface(worklane->activeSurface());
        worklane->activeSurface()->setFocus();
    }

    return worklane;
}

WorklaneView* MainWindow::currentWorklane() const {
    return qobject_cast<WorklaneView*>(m_stack->currentWidget());
}

TerminalSurface* MainWindow::activeSurface() const {
    auto* wl = currentWorklane();
    return wl ? wl->activeSurface() : nullptr;
}

void MainWindow::setupActions() {
    KActionCollection* ac = actionCollection();

    // Standard Window & Tab Actions (Matching Ghostty Defaults)
    auto* newTabAction = new QAction(i18n("New Tab"), this);
    connect(newTabAction, &QAction::triggered, this, &MainWindow::newTab);
    ac->addAction(QStringLiteral("new_tab"), newTabAction);
    ac->setDefaultShortcut(newTabAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));

    auto* newWinAction = new QAction(i18n("New Window"), this);
    connect(newWinAction, &QAction::triggered, this, &MainWindow::newWindow);
    ac->addAction(QStringLiteral("new_window"), newWinAction);
    ac->setDefaultShortcut(newWinAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));

    auto* closeTabAction = new QAction(i18n("Close Tab"), this);
    connect(closeTabAction, &QAction::triggered, this, &MainWindow::closeCurrentTab);
    ac->addAction(QStringLiteral("close_tab"), closeTabAction);
    ac->setDefaultShortcut(closeTabAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));

    auto* closeWinAction = new QAction(i18n("Close Window"), this);
    connect(closeWinAction, &QAction::triggered, this, &MainWindow::close);
    ac->addAction(QStringLiteral("close_window"), closeWinAction);
    ac->setDefaultShortcut(closeWinAction, QKeySequence(Qt::ALT | Qt::Key_F4));

    auto* quitAction = new QAction(i18n("Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    ac->addAction(QStringLiteral("file_quit"), quitAction);
    ac->setDefaultShortcut(quitAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q));

    auto* copyAction = KStandardAction::copy(this, &MainWindow::copyText, ac);
    copyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    ac->setDefaultShortcut(copyAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));

    auto* pasteAction = KStandardAction::paste(this, &MainWindow::pasteText, ac);
    pasteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    ac->setDefaultShortcut(pasteAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));

    auto* clearScrollbackAction = new QAction(i18n("Clear Scrollback"), this);
    connect(clearScrollbackAction, &QAction::triggered, this, [this]() {
        TerminalSurface* surface = activeSurface();
        if (surface && surface->surface()) {
            ghostty_surface_binding_action(surface->surface(), "clear_screen", 12);
        }
    });
    ac->addAction(QStringLiteral("clear_scrollback"), clearScrollbackAction);
    clearScrollbackAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    ac->setDefaultShortcut(clearScrollbackAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));

    // Tab Navigation (Matching Ghostty Defaults: Ctrl+Shift+Left/Right, Ctrl+PageUp/Down)
    auto* nextTabAction = new QAction(i18n("Next Tab"), this);
    connect(nextTabAction, &QAction::triggered, this, &MainWindow::nextTab);
    ac->addAction(QStringLiteral("next_tab"), nextTabAction);
    ac->setDefaultShortcuts(nextTabAction, {
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Right),
        QKeySequence(Qt::CTRL | Qt::Key_PageDown)
    });

    auto* prevTabAction = new QAction(i18n("Previous Tab"), this);
    connect(prevTabAction, &QAction::triggered, this, &MainWindow::prevTab);
    ac->addAction(QStringLiteral("prev_tab"), prevTabAction);
    ac->setDefaultShortcuts(prevTabAction, {
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Left),
        QKeySequence(Qt::CTRL | Qt::Key_PageUp)
    });

    // Tab Selection Alt+1..8, Alt+9 for last tab
    for (int i = 1; i <= 8; ++i) {
        auto* gotoTabAction = new QAction(i18n("Go to Tab %1", i), this);
        connect(gotoTabAction, &QAction::triggered, this, [this, i]() {
            if (i <= m_tabBar->count()) {
                m_tabBar->setCurrentIndex(i - 1);
            }
        });
        ac->addAction(QStringLiteral("goto_tab_%1").arg(i), gotoTabAction);
        ac->setDefaultShortcut(gotoTabAction, QKeySequence(Qt::ALT | (Qt::Key_0 + i)));
    }

    auto* gotoLastTabAction = new QAction(i18n("Go to Last Tab"), this);
    connect(gotoLastTabAction, &QAction::triggered, this, [this]() {
        if (m_tabBar->count() > 0) {
            m_tabBar->setCurrentIndex(m_tabBar->count() - 1);
        }
    });
    ac->addAction(QStringLiteral("goto_tab_last"), gotoLastTabAction);
    ac->setDefaultShortcut(gotoLastTabAction, QKeySequence(Qt::ALT | Qt::Key_9));

    // Command Bar
    auto* cmdBarAction = actionCollection()->action(QStringLiteral("open_kcommand_bar"));
    if (!cmdBarAction) {
        cmdBarAction = new QAction(i18n("Open Command Bar"), this);
        connect(cmdBarAction, &QAction::triggered, this, &MainWindow::showCommandBar);
        ac->addAction(QStringLiteral("open_kcommand_bar"), cmdBarAction);
    }
    cmdBarAction->setShortcuts({
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P),
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I)
    });
    ac->setDefaultShortcuts(cmdBarAction, {
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P),
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I)
    });
    addAction(cmdBarAction);

    // Worklane Split Actions (Matching Ghostty Defaults: Ctrl+Shift+O Right, Ctrl+Shift+E Down)
    auto* newColAction = new QAction(i18n("Split Right"), this);
    connect(newColAction, &QAction::triggered, this, &MainWindow::newColumn);
    ac->addAction(QStringLiteral("new_split_right"), newColAction);
    ac->addAction(QStringLiteral("new_column"), newColAction);
    ac->setDefaultShortcut(newColAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));

    auto* splitVertAction = new QAction(i18n("Split Down"), this);
    connect(splitVertAction, &QAction::triggered, this, &MainWindow::splitVertical);
    ac->addAction(QStringLiteral("new_split_down"), splitVertAction);
    ac->addAction(QStringLiteral("split_vertical"), splitVertAction);
    ac->setDefaultShortcut(splitVertAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));

    auto* closeActiveAction = new QAction(i18n("Close Active Terminal"), this);
    connect(closeActiveAction, &QAction::triggered, this, &MainWindow::closeActive);
    ac->addAction(QStringLiteral("close_active"), closeActiveAction);

    // Split Zoom & Equalize (Matching Ghostty: Ctrl+Shift+Enter Zoom, Ctrl+Alt+= Equalize)
    auto* zoomSplitAction = new QAction(i18n("Toggle Split Zoom"), this);
    connect(zoomSplitAction, &QAction::triggered, this, &MainWindow::toggleSplitZoom);
    ac->addAction(QStringLiteral("toggle_split_zoom"), zoomSplitAction);
    ac->setDefaultShortcut(zoomSplitAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Return));

    auto* eqSplitAction = new QAction(i18n("Equalize Splits"), this);
    connect(eqSplitAction, &QAction::triggered, this, &MainWindow::equalizeSplits);
    ac->addAction(QStringLiteral("equalize_splits"), eqSplitAction);
    ac->setDefaultShortcut(eqSplitAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Equal));

    // Split Navigation (Matching Ghostty Defaults: Ctrl+Alt+Arrow keys)
    auto* focusLeftAction = new QAction(i18n("Focus Left Split"), this);
    connect(focusLeftAction, &QAction::triggered, this, [this]() {
        if (auto* wl = currentWorklane()) wl->focusLeftColumn();
    });
    ac->addAction(QStringLiteral("focus_prev_column"), focusLeftAction);
    ac->addAction(QStringLiteral("focus_left"), focusLeftAction);
    ac->setDefaultShortcut(focusLeftAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left));

    auto* focusRightAction = new QAction(i18n("Focus Right Split"), this);
    connect(focusRightAction, &QAction::triggered, this, [this]() {
        if (auto* wl = currentWorklane()) wl->focusRightColumn();
    });
    ac->addAction(QStringLiteral("focus_next_column"), focusRightAction);
    ac->addAction(QStringLiteral("focus_right"), focusRightAction);
    ac->setDefaultShortcut(focusRightAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Right));

    auto* focusUpAction = new QAction(i18n("Focus Up Split"), this);
    connect(focusUpAction, &QAction::triggered, this, [this]() {
        if (auto* wl = currentWorklane()) wl->focusUpSplit();
    });
    ac->addAction(QStringLiteral("focus_up"), focusUpAction);
    ac->setDefaultShortcut(focusUpAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Up));

    auto* focusDownAction = new QAction(i18n("Focus Down Split"), this);
    connect(focusDownAction, &QAction::triggered, this, [this]() {
        if (auto* wl = currentWorklane()) wl->focusDownSplit();
    });
    ac->addAction(QStringLiteral("focus_down"), focusDownAction);
    ac->setDefaultShortcut(focusDownAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Down));

    // Settings
    KStandardAction::preferences(this, &MainWindow::showPreferencesDialog, ac);
    KStandardAction::keyBindings(guiFactory(), &KXMLGUIFactory::showConfigureShortcutsDialog, ac);
}

void MainWindow::newWindow() {
    auto* win = new MainWindow();
    win->show();
}

void MainWindow::newTab() {
    createTab();
}

void MainWindow::closeCurrentTab() {
    closeTab(m_tabBar->currentIndex());
}

void MainWindow::closeTab(int index) {
    if (index < 0 || index >= m_stack->count()) {
        return;
    }

    QWidget* page = m_stack->widget(index);
    m_stack->removeWidget(page);
    m_tabBar->removeTab(index);
    updateTabBarVisibility();
    page->deleteLater();

    if (m_tabBar->count() == 0) {
        close();
    } else {
        auto* wl = currentWorklane();
        if (wl && wl->activeSurface()) {
            wl->activeSurface()->setFocus();
            updateTitle(wl->activeTitle());
        }
    }
}

void MainWindow::gotoTab(int tabIdx) {
    if (tabIdx >= 1 && tabIdx <= m_tabBar->count()) {
        m_tabBar->setCurrentIndex(tabIdx - 1);
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
        int prev = (m_tabBar->currentIndex() - 1 + m_tabBar->count()) % m_tabBar->count();
        m_tabBar->setCurrentIndex(prev);
    }
}

void MainWindow::onTabChanged(int index) {
    if (index >= 0 && index < m_stack->count()) {
        auto* wl = qobject_cast<WorklaneView*>(m_stack->widget(index));
        if (wl) {
            if (wl->activeSurface()) {
                wl->activeSurface()->setFocus();
            }
            updateTitle(wl->activeTitle());
        }
    }
}

void MainWindow::newColumn() {
    auto* wl = currentWorklane();
    if (wl) {
        wl->addColumn();
    }
}

void MainWindow::splitVertical() {
    auto* wl = currentWorklane();
    if (wl) {
        wl->splitCurrentVertical();
    }
}

void MainWindow::closeActive() {
    auto* wl = currentWorklane();
    if (wl) {
        wl->closeActiveSurface();
    }
}

void MainWindow::toggleSplitZoom() {
    auto* wl = currentWorklane();
    if (wl) {
        wl->toggleSplitZoom();
    }
}

void MainWindow::equalizeSplits() {
    auto* wl = currentWorklane();
    if (wl) {
        wl->equalizeSplits();
    }
}

void MainWindow::copyText() {
    TerminalSurface* surface = activeSurface();
    if (!surface || !surface->surface()) return;

    ghostty_text_s text = {};
    if (ghostty_surface_read_selection(surface->surface(), &text)) {
        if (text.text && text.text_len > 0) {
            QString selected = QString::fromUtf8(text.text, static_cast<qsizetype>(text.text_len));
            QGuiApplication::clipboard()->setText(selected, QClipboard::Clipboard);
        }
        ghostty_surface_free_text(surface->surface(), &text);
    }
}

void MainWindow::pasteText() {
    TerminalSurface* surface = activeSurface();
    if (!surface) return;

    QString text = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    if (!text.isEmpty()) {
        surface->pasteText(text);
    }
}

void MainWindow::showPreferencesDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(i18n("Configure — Khostty"));
    dialog.setMinimumWidth(440);

    auto* layout = new QVBoxLayout(&dialog);

    // Appearance & Layout Settings
    auto* appearanceGroup = new QGroupBox(i18n("Appearance & Layout"), &dialog);
    auto* appearanceLayout = new QVBoxLayout(appearanceGroup);

    KConfigGroup appCfg(KSharedConfig::openConfig(QStringLiteral("khosttyrc")), QStringLiteral("Appearance"));
    bool showHeaderOnlyOnSplit = appCfg.readEntry("ShowHeaderOnlyOnSplit", true);

    auto* hideSingleHeaderCheck = new QCheckBox(
        i18n("Hide pane header when only one terminal is open"),
        appearanceGroup
    );
    hideSingleHeaderCheck->setChecked(showHeaderOnlyOnSplit);
    appearanceLayout->addWidget(hideSingleHeaderCheck);

    layout->addWidget(appearanceGroup);

    // Font Settings
    auto* fontGroup = new QGroupBox(i18n("Font Settings"), &dialog);
    auto* fontLayout = new QVBoxLayout(fontGroup);

    QFont sysFixed = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    float sysPt = static_cast<float>(sysFixed.pointSizeF());
    if (sysPt <= 0.0f) sysPt = static_cast<float>(sysFixed.pointSize());
    if (sysPt <= 0.0f) sysPt = 10.0f;

    KConfigGroup fontCfg(KSharedConfig::openConfig(QStringLiteral("khosttyrc")), QStringLiteral("Font"));
    bool useSys = fontCfg.readEntry("UseSystemFixedFont", true);
    float currentCustom = fontCfg.readEntry("FontSize", 0.0f);
    if (currentCustom <= 0.0f) currentCustom = sysPt;

    auto* useSysCheck = new QCheckBox(
        i18n("Use KDE system fixed font (%1, %2 pt)", sysFixed.family(), sysPt),
        fontGroup
    );
    useSysCheck->setChecked(useSys);
    fontLayout->addWidget(useSysCheck);

    auto* customSizeLayout = new QHBoxLayout();
    auto* customSizeLabel = new QLabel(i18n("Custom font size (pt):"), fontGroup);
    auto* customSizeSpin = new QDoubleSpinBox(fontGroup);
    customSizeSpin->setRange(6.0, 72.0);
    customSizeSpin->setSingleStep(0.5);
    customSizeSpin->setValue(currentCustom);
    customSizeSpin->setEnabled(!useSys);

    customSizeLayout->addWidget(customSizeLabel);
    customSizeLayout->addWidget(customSizeSpin);
    fontLayout->addLayout(customSizeLayout);

    connect(useSysCheck, &QCheckBox::toggled, customSizeSpin, [customSizeSpin](bool checked) {
        customSizeSpin->setEnabled(!checked);
    });

    layout->addWidget(fontGroup);

    // Ghostty Terminal Engine (Power Users)
    auto* engineGroup = new QGroupBox(i18n("Ghostty Terminal Engine"), &dialog);
    auto* engineLayout = new QVBoxLayout(engineGroup);

    auto* engineDescLabel = new QLabel(
        i18n("Terminal engine features (cursor shape, scrollback, themes, keybindings) are powered by Ghostty and configured in ~/.config/ghostty/config."),
        engineGroup
    );
    engineDescLabel->setWordWrap(true);
    engineLayout->addWidget(engineDescLabel);

    auto* engineBtnLayout = new QHBoxLayout();
    auto* editConfigBtn = new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")), i18n("Edit Ghostty Config..."), engineGroup);
    auto* reloadConfigBtn = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Reload Ghostty Config"), engineGroup);

    connect(editConfigBtn, &QPushButton::clicked, &dialog, []() {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/ghostty");
        QDir().mkpath(configDir);
        QString configPath = configDir + QStringLiteral("/config");
        QFile configFile(configPath);
        if (!configFile.exists()) {
            if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&configFile);
                out << "# Ghostty Terminal Configuration\n";
                out << "# Refer to https://ghostty.org/docs/config for available options.\n";
                configFile.close();
            }
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(configPath));
    });

    connect(reloadConfigBtn, &QPushButton::clicked, &dialog, []() {
        GhosttyCore::instance().reloadConfig();
    });

    engineBtnLayout->addWidget(editConfigBtn);
    engineBtnLayout->addWidget(reloadConfigBtn);
    engineLayout->addLayout(engineBtnLayout);

    layout->addWidget(engineGroup);

    // Dialog Buttons
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
        Qt::Horizontal,
        &dialog
    );
    layout->addWidget(buttonBox);

    auto applyChanges = [=, this]() {
        KConfigGroup appGroup(KSharedConfig::openConfig(QStringLiteral("khosttyrc")), QStringLiteral("Appearance"));
        appGroup.writeEntry("ShowHeaderOnlyOnSplit", hideSingleHeaderCheck->isChecked());
        appGroup.sync();

        KConfigGroup fontGroup(KSharedConfig::openConfig(QStringLiteral("khosttyrc")), QStringLiteral("Font"));
        fontGroup.writeEntry("UseSystemFixedFont", useSysCheck->isChecked());
        fontGroup.writeEntry("FontSize", customSizeSpin->value());
        fontGroup.sync();

        for (auto* surface : GhosttyCore::instance().allSurfaces()) {
            if (surface) {
                surface->applyFontConfig();
            }
        }

        for (int i = 0; i < m_stack->count(); ++i) {
            if (auto* wl = qobject_cast<WorklaneView*>(m_stack->widget(i))) {
                wl->updatePaneDecorations();
            }
        }
    };

    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, applyChanges);
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

    auto* bar = new KCommandBar(this);
    bar->setAttribute(Qt::WA_DeleteOnClose);
    m_commandBar = bar;

    KCommandBar::ActionGroup group;
    group.name = QStringLiteral("Actions");
    group.actions = actionCollection()->actions();
    bar->setActions({group});
    bar->show();
    bar->raise();
}

void MainWindow::updateTitle(const QString& title) {
    if (title.isEmpty()) {
        setWindowTitle(QStringLiteral("Khostty"));
    } else {
        setWindowTitle(QStringLiteral("%1 — Khostty").arg(title));
    }
}

void MainWindow::changeEvent(QEvent* event) {
    KXmlGuiWindow::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        ghostty_app_set_focus(GhosttyCore::instance().app(), true);
        auto* wl = currentWorklane();
        if (wl && wl->activeSurface()) {
            wl->activeSurface()->setFocus();
        }
    }
}

void MainWindow::showEvent(QShowEvent* event) {
    KXmlGuiWindow::showEvent(event);
    auto* wl = currentWorklane();
    if (wl && wl->activeSurface()) {
        wl->activeSurface()->setFocus();
    }
}
