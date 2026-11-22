#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QSurfaceFormat>
#include <KAboutData>
#include <KLocalizedString>
#include "core/ghostty_core.h"
#include "window/main_window.h"

namespace {
MainWindow* resolveTargetWindow() {
    QWidget* active = QApplication::activeWindow();
    while (active) {
        if (auto* mw = qobject_cast<MainWindow*>(active)) {
            return mw;
        }
        active = active->parentWidget() ? active->parentWidget()->window() : nullptr;
    }
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (auto* mw = qobject_cast<MainWindow*>(top)) {
            return mw;
        }
    }
    return nullptr;
}
} // namespace

int main(int argc, char* argv[]) {
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("khostty");

    KAboutData aboutData(
        QStringLiteral("khostty"),
        i18n("Khostty"),
        QStringLiteral("0.1.2"),
        i18n("A native KDE Plasma 6 terminal emulator powered by libghostty"),
        KAboutLicense::MIT,
        i18n("(c) 2026 Khostty Developers")
    );
    aboutData.setDesktopFileName(QStringLiteral("com.github.khostty"));
    KAboutData::setApplicationData(aboutData);
    QApplication::setDesktopFileName(QStringLiteral("com.github.khostty"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    if (!GhosttyCore::instance().initialize(argc, argv)) {
        qCritical("Failed to initialize libghostty core engine.");
        return 1;
    }

    QObject::connect(&GhosttyCore::instance(), &GhosttyCore::newWindowRequested, &app, []() {
        auto* win = new MainWindow();
        win->show();
    });
    QObject::connect(&GhosttyCore::instance(), &GhosttyCore::newTabRequested, &app, []() {
        if (MainWindow* win = resolveTargetWindow()) {
            win->newTab();
        } else {
            auto* created = new MainWindow();
            created->show();
        }
    });
    QObject::connect(&GhosttyCore::instance(), &GhosttyCore::quitRequested,
                     &app, &QCoreApplication::quit);

    auto* mainWindow = new MainWindow();
    mainWindow->show();

    int exitCode = app.exec();

    GhosttyCore::instance().shutdown();
    return exitCode;
}
