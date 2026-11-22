#include <QApplication>
#include <QStandardPaths>
#include <QtTest>

#include "terminal/terminal_pane.h"
#include "terminal/terminal_surface.h"
#include "terminal/worklane_view.h"

class TestWorklane : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

  void testAllowedExternalUrl() {
    QVERIFY(TerminalSurface::isAllowedExternalUrl(
        QUrl(QStringLiteral("https://ghostty.org/docs"))));
    QVERIFY(TerminalSurface::isAllowedExternalUrl(
        QUrl(QStringLiteral("http://example.com"))));
    QVERIFY(TerminalSurface::isAllowedExternalUrl(
        QUrl(QStringLiteral("mailto:user@example.com"))));
    QVERIFY(TerminalSurface::isAllowedExternalUrl(
        QUrl(QStringLiteral("HTTPS://example.com"))));

    QVERIFY(!TerminalSurface::isAllowedExternalUrl(
        QUrl(QStringLiteral("file:///etc/passwd"))));
    QVERIFY(!TerminalSurface::isAllowedExternalUrl(
        QUrl(QStringLiteral("javascript:alert(1)"))));
    QVERIFY(!TerminalSurface::isAllowedExternalUrl(
        QUrl(QStringLiteral("relative/path"))));
    QVERIFY(!TerminalSurface::isAllowedExternalUrl(QUrl()));
  }

  void testFormatPositionTag() {
    QCOMPARE(WorklaneView::formatPositionTag(0, 1, 0, 1), QString());
    QCOMPARE(WorklaneView::formatPositionTag(0, 2, 0, 1),
             QStringLiteral("Col 1/2"));
    QCOMPARE(WorklaneView::formatPositionTag(0, 1, 1, 3),
             QStringLiteral("Split 2/3"));
    QCOMPARE(WorklaneView::formatPositionTag(1, 3, 0, 2),
             QStringLiteral("Col 2/3 · 1/2"));
  }

  void testSplitZoomAndClose() {
    WorklaneView wl;
    wl.resize(800, 600);

    QCOMPARE(wl.columnCount(), 1);
    QCOMPARE(wl.paneCount(), 1);
    QVERIFY(!wl.hasAnySplit());

    TerminalSurface *first = wl.activeSurface();
    QVERIFY(first != nullptr);
    TerminalSurface *second = wl.splitVertical(first);
    QVERIFY(second != nullptr);

    QCOMPARE(wl.columnCount(), 1);
    QCOMPARE(wl.rowCount(0), 2);
    QCOMPARE(wl.paneCount(), 2);
    QVERIFY(wl.hasAnySplit());

    wl.setActiveSurface(first);
    wl.toggleSplitZoom();

    auto *firstPane = qobject_cast<TerminalPane *>(first->parentWidget());
    auto *secondPane = qobject_cast<TerminalPane *>(second->parentWidget());
    QVERIFY(firstPane && secondPane);
    QVERIFY(firstPane->isZoomed());
    QVERIFY(!firstPane->isHidden());
    QVERIFY(secondPane->isHidden());

    // Closing the zoomed pane must unzoom and unhide the remaining sibling.
    wl.closeSurface(first);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    QCOMPARE(wl.columnCount(), 1);
    QCOMPARE(wl.paneCount(), 1);
    QVERIFY(!wl.hasAnySplit());
    QVERIFY(!secondPane->isHidden());
    QVERIFY(!secondPane->isZoomed());
    QVERIFY(wl.findChildren<WorklaneSeparator *>().isEmpty());
  }

  void testCrossColumnZoom() {
    WorklaneView wl;
    wl.resize(1000, 600);

    TerminalSurface *col1Top = wl.activeSurface();
    TerminalSurface *col1Bot = wl.splitVertical(col1Top);
    TerminalSurface *col2 = wl.addColumn();
    QVERIFY(col1Top && col1Bot && col2);

    wl.setActiveSurface(col1Top);
    wl.toggleSplitZoom();

    auto *col1BotPane = qobject_cast<TerminalPane *>(col1Bot->parentWidget());
    QVERIFY(col1BotPane->isHidden());

    // Switching focus to a single-pane column and toggling zoom must unzoom Col 1.
    wl.setActiveSurface(col2);
    wl.toggleSplitZoom();
    QVERIFY(!col1BotPane->isHidden());
  }

  void testDetachAndReattach() {
    WorklaneView wl;
    wl.resize(800, 600);

    TerminalSurface *s1 = wl.activeSurface();
    TerminalSurface *s2 = wl.splitVertical(s1);
    QCOMPARE(wl.columnCount(), 1);
    QCOMPARE(wl.paneCount(), 2);

    TerminalPane *detached = wl.detachSurface(s2);
    QVERIFY(detached != nullptr);
    QCOMPARE(wl.columnCount(), 1);
    QCOMPARE(wl.paneCount(), 1);
    QVERIFY(!wl.hasAnySplit());

    wl.attachPaneAsColumn(detached, 1);
    QCOMPARE(wl.columnCount(), 2);
    QCOMPARE(wl.paneCount(), 2);
    QVERIFY(wl.hasAnySplit());
    QCOMPARE(wl.columnIndexOf(s2), 1);
  }
};

QTEST_MAIN(TestWorklane)
#include "test_worklane.moc"
