#include <QApplication>
#include <QCommandLineParser>
#include <KAboutData>
#include <KLocalizedString>
#include "core/ghostty_core.h"
#include "window/main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("khostty");

    KAboutData aboutData(
        QStringLiteral("khostty"),
        i18n("Khostty"),
        QStringLiteral("0.1.0"),
        i18n("A native KDE Plasma 6 terminal emulator powered by libghostty"),
        KAboutLicense::MIT,
        i18n("(c) 2026 Khostty Developers")
    );
    KAboutData::setApplicationData(aboutData);

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    if (!GhosttyCore::instance().initialize(argc, argv)) {
        qCritical("Failed to initialize libghostty core engine.");
        return 1;
    }

    auto* mainWindow = new MainWindow();
    mainWindow->show();

    int exitCode = app.exec();

    GhosttyCore::instance().shutdown();
    return exitCode;
}
