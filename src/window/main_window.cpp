#include "window/main_window.h"
#include "terminal/worklane_view.h"
#include "terminal/terminal_surface.h"
#include "core/ghostty_core.h"

#include <KActionCollection>
#include <KStandardAction>
#include <KCommandBar>
#include <KLocalizedString>
#include <QApplication>
#include <QClipboard>
#include <QKeySequence>
#include <QAction>

MainWindow::MainWindow(QWidget* parent)
    : KXmlGuiWindow(parent)
{
    m_worklane = new WorklaneView(this);
    setCentralWidget(m_worklane);

    connect(m_worklane, &WorklaneView::titleChanged, this, &MainWindow::updateTitle);

    setupActions();
    setupGUI(Default, QStringLiteral("khosttyui.rc"));

    connect(&GhosttyCore::instance(), &GhosttyCore::newWindowRequested, this, &MainWindow::newWindow);
    connect(&GhosttyCore::instance(), &GhosttyCore::quitRequested, qApp, &QCoreApplication::quit);

    resize(1000, 700);
    setWindowTitle(QStringLiteral("Khostty"));
}

MainWindow::~MainWindow() = default;

void MainWindow::setupActions() {
    KActionCollection* ac = actionCollection();

    KStandardAction::openNew(this, &MainWindow::newWindow, ac);
    KStandardAction::quit(qApp, &QCoreApplication::quit, ac);
    KStandardAction::copy(this, &MainWindow::copyText, ac);
    KStandardAction::paste(this, &MainWindow::pasteText, ac);

    auto* cmdBarAction = actionCollection()->action(QStringLiteral("open_kcommand_bar"));
    if (!cmdBarAction) {
        cmdBarAction = new QAction(i18n("Open Command Bar"), this);
        connect(cmdBarAction, &QAction::triggered, this, &MainWindow::showCommandBar);
        ac->addAction(QStringLiteral("open_kcommand_bar"), cmdBarAction);
        ac->setDefaultShortcut(cmdBarAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I));
    }

    // Terminal Worklane Actions
    auto* newColAction = new QAction(QStringLiteral("New Column"), this);
    connect(newColAction, &QAction::triggered, this, &MainWindow::newColumn);
    ac->addAction(QStringLiteral("new_column"), newColAction);
    ac->setDefaultShortcut(newColAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));

    auto* splitVertAction = new QAction(QStringLiteral("Split Vertical"), this);
    connect(splitVertAction, &QAction::triggered, this, &MainWindow::splitVertical);
    ac->addAction(QStringLiteral("split_vertical"), splitVertAction);
    ac->setDefaultShortcut(splitVertAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));

    auto* closeActiveAction = new QAction(QStringLiteral("Close Active Terminal"), this);
    connect(closeActiveAction, &QAction::triggered, this, &MainWindow::closeActive);
    ac->addAction(QStringLiteral("close_active"), closeActiveAction);
    ac->setDefaultShortcut(closeActiveAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));

    // Navigation Actions
    auto* focusLeftAction = new QAction(QStringLiteral("Focus Left Column"), this);
    connect(focusLeftAction, &QAction::triggered, m_worklane, &WorklaneView::focusLeftColumn);
    ac->addAction(QStringLiteral("focus_left"), focusLeftAction);
    ac->setDefaultShortcut(focusLeftAction, QKeySequence(Qt::ALT | Qt::Key_Left));

    auto* focusRightAction = new QAction(QStringLiteral("Focus Right Column"), this);
    connect(focusRightAction, &QAction::triggered, m_worklane, &WorklaneView::focusRightColumn);
    ac->addAction(QStringLiteral("focus_right"), focusRightAction);
    ac->setDefaultShortcut(focusRightAction, QKeySequence(Qt::ALT | Qt::Key_Right));

    auto* focusUpAction = new QAction(QStringLiteral("Focus Up Split"), this);
    connect(focusUpAction, &QAction::triggered, m_worklane, &WorklaneView::focusUpSplit);
    ac->addAction(QStringLiteral("focus_up"), focusUpAction);
    ac->setDefaultShortcut(focusUpAction, QKeySequence(Qt::ALT | Qt::Key_Up));

    auto* focusDownAction = new QAction(QStringLiteral("Focus Down Split"), this);
    connect(focusDownAction, &QAction::triggered, m_worklane, &WorklaneView::focusDownSplit);
    ac->addAction(QStringLiteral("focus_down"), focusDownAction);
    ac->setDefaultShortcut(focusDownAction, QKeySequence(Qt::ALT | Qt::Key_Down));
}

void MainWindow::newWindow() {
    auto* win = new MainWindow();
    win->show();
}

void MainWindow::newColumn() {
    m_worklane->addColumn();
}

void MainWindow::splitVertical() {
    m_worklane->splitCurrentVertical();
}

void MainWindow::closeActive() {
    m_worklane->closeActiveSurface();
}

void MainWindow::copyText() {
    TerminalSurface* surface = m_worklane->activeSurface();
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
    TerminalSurface* surface = m_worklane->activeSurface();
    if (!surface) return;

    QString text = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    if (!text.isEmpty()) {
        surface->pasteText(text);
    }
}

void MainWindow::showCommandBar() {
    auto* bar = new KCommandBar(this);
    KCommandBar::ActionGroup group;
    group.name = QStringLiteral("Actions");
    group.actions = actionCollection()->actions();
    bar->setActions({group});
    bar->show();
}

void MainWindow::updateTitle(const QString& title) {
    if (title.isEmpty()) {
        setWindowTitle(QStringLiteral("Khostty"));
    } else {
        setWindowTitle(QStringLiteral("%1 — Khostty").arg(title));
    }
}
