#include <QApplication>
#include <QHideEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStandardPaths>
#include <QtTest>

#include "terminal/khostty_splitter.h"
#include "terminal/terminal_pane.h"
#include "terminal/terminal_surface.h"
#include "terminal/worklane_view.h"

namespace {
void resizeView(WorklaneView &wl, int w, int h) {
  const QSize oldWl = wl.size();
  const QSize oldVp = wl.viewport()->size();
  wl.resize(w, h);
  QResizeEvent outerEv(wl.size(), oldWl);
  QCoreApplication::sendEvent(&wl, &outerEv);
  QResizeEvent vpEv(wl.viewport()->size(), oldVp);
  QCoreApplication::sendEvent(wl.viewport(), &vpEv);
}
} // namespace

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

  void testSingleColumnVerticalSplitsResize() {
    WorklaneView wl;
    resizeView(wl, 900, 600);

    TerminalSurface *s1 = wl.activeSurface();
    QVERIFY(s1 != nullptr);
    QVERIFY(wl.splitVertical(s1) != nullptr);
    QVERIFY(wl.splitVertical(s1) != nullptr);

    QCOMPARE(wl.columnCount(), 1);
    QCOMPARE(wl.rowCount(0), 3);
    QVERIFY(wl.hasAnySplit());
    QVERIFY(wl.findChildren<WorklaneSeparator *>().isEmpty());

    auto *col =
        qobject_cast<KhosttySplitter *>(s1->parentWidget()->parentWidget());
    QVERIFY(col != nullptr);
    QCOMPARE(col->x(), 0);
    QCOMPARE(col->width(), 900);
    QCOMPARE(wl.widget()->width(), 900);

    resizeView(wl, 400, 600);
    QCOMPARE(col->x(), 0);
    QCOMPARE(col->width(), 400);

    resizeView(wl, 1200, 600);
    QCOMPARE(col->x(), 0);
    QCOMPARE(col->width(), 1200);
    QCOMPARE(wl.widget()->width(), 1200);
  }

  void testMultiColumnResizeAndFlushLayout() {
    WorklaneView wl;
    resizeView(wl, 1000, 600);

    TerminalSurface *s1 = wl.activeSurface();
    QVERIFY(s1 != nullptr);
    TerminalSurface *s2 = wl.addColumn();
    QVERIFY(s2 != nullptr);

    QCOMPARE(wl.columnCount(), 2);
    auto *col0 =
        qobject_cast<KhosttySplitter *>(s1->parentWidget()->parentWidget());
    auto *col1 =
        qobject_cast<KhosttySplitter *>(s2->parentWidget()->parentWidget());
    QVERIFY(col0 != nullptr && col1 != nullptr);

    auto seps = wl.findChildren<WorklaneSeparator *>();
    QCOMPARE(seps.size(), 2);
    WorklaneSeparator *sep0 = seps[0];
    WorklaneSeparator *sep1 = seps[1];

    QCOMPARE(col0->x(), 0);
    QCOMPARE(col0->width(), 492);
    QCOMPARE(sep0->x(), 492);
    QCOMPARE(col1->x(), 500);
    QCOMPARE(col1->width(), 492);
    QCOMPARE(sep1->x(), 992);
    QCOMPARE(wl.widget()->width(), 1000);

    resizeView(wl, 1001, 600);
    QCOMPARE(col0->width() + 8 + col1->width() + 8, 1001);
    QCOMPARE(wl.widget()->width(), 1001);

    resizeView(wl, 300, 600);
    QCOMPARE(col0->width(), 270);
    QCOMPARE(col1->width(), 270);
    QVERIFY(col0->width() <= 300);

    resizeView(wl, 1400, 600);
    QCOMPARE(col0->width(), 692);
    QCOMPARE(col1->width(), 692);
    QCOMPARE(wl.widget()->width(), 1400);

    wl.setColumnWidth(col0, 320);
    QCOMPARE(col0->x(), 0);
    QCOMPARE(col0->width(), 320);
    QCOMPARE(sep0->x(), 320);
    QCOMPARE(col1->x(), 328);
    QCOMPARE(sep1->x(), col1->x() + col1->width());

    TerminalPane *detached = wl.detachSurface(s2);
    QVERIFY(detached != nullptr);
    wl.attachPaneToColumn(detached, 0, 1);

    QCOMPARE(wl.columnCount(), 1);
    QCOMPARE(wl.rowCount(0), 2);
    QVERIFY(wl.findChildren<WorklaneSeparator *>().isEmpty());

    resizeView(wl, 1200, 600);
    QCOMPARE(col0->x(), 0);
    QCOMPARE(col0->width(), 1200);
  }

  void testLongPaneTitleDoesNotInflateMinWidth() {
    WorklaneView wl;
    resizeView(wl, 800, 600);

    TerminalSurface *s1 = wl.activeSurface();
    QVERIFY(s1 != nullptr);
    QVERIFY(wl.splitVertical(s1) != nullptr);

    auto *pane = qobject_cast<TerminalPane *>(s1->parentWidget());
    QVERIFY(pane != nullptr);

    pane->setTitle(QStringLiteral("x"));
    const int shortMinW = pane->minimumSizeHint().width();

    pane->setTitle(QString(120, QLatin1Char('a')));
    QCOMPARE(pane->minimumSizeHint().width(), shortMinW);
  }

  void testSurfaceOcclusionAndShowHideWithoutCore() {
    WorklaneView wl;
    resizeView(wl, 800, 600);

    TerminalSurface *surface = wl.activeSurface();
    QVERIFY(surface != nullptr);
    QCOMPARE(surface->surface(), nullptr);

    // Send show/hide events directly because QT_QPA_PLATFORM=offscreen does not
    // support creating a top-level QOpenGLWidget backing store via wl.show().
    QShowEvent showEv;
    QCoreApplication::sendEvent(surface, &showEv);
    surface->setWindowOccluded(true);

    surface->hide();
    QHideEvent hideEv;
    QCoreApplication::sendEvent(surface, &hideEv);
    QCoreApplication::processEvents();
    QVERIFY(!surface->isVisible());

    surface->show();
    QCoreApplication::sendEvent(surface, &showEv);
    surface->setWindowOccluded(false);
    QCoreApplication::processEvents();
    QVERIFY(!surface->isHidden());
  }
};

QTEST_MAIN(TestWorklane)
#include "test_worklane.moc"
