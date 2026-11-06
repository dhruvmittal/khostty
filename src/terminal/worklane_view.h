#pragma once

#include "terminal/khostty_splitter.h"
#include "terminal/worklane_separator.h"
#include <QHBoxLayout>
#include <QList>
#include <QScrollArea>
#include <QWidget>

class TerminalSurface;
class TerminalPane;
class QLineEdit;
class QLabel;

class WorklaneView : public QScrollArea {
  Q_OBJECT

public:
  explicit WorklaneView(QWidget *parent = nullptr,
                        const QString &initialDir = QString());
  ~WorklaneView() override;

  TerminalSurface *activeSurface() const;
  int columnCount() const;
  int rowCount(int colIdx) const;
  TerminalSurface *surfaceAt(int colIdx, int rowIdx) const;
  int paneCount() const;
  QList<TerminalSurface *> allSurfaces() const;
  QString activeTitle() const { return m_activeTitle; }
  bool hasAnySplit() const;
  bool canAcceptPaneFrom(const WorklaneView *source) const;

  int columnIndexOf(TerminalSurface *surface) const;
  int rowIndexOf(TerminalSurface *surface) const;
  TerminalPane *detachSurface(TerminalSurface *surface);
  void attachPaneAsColumn(TerminalPane *pane, int colIndex);
  void attachPaneToColumn(TerminalPane *pane, int colIndex, int rowIndex);

  void updateScrollBarPolicy();

  TerminalSurface *addColumn(const QString &initialDir = QString());
  TerminalSurface *addColumnAfter(KhosttySplitter *targetCol = nullptr,
                                  const QString &initialDir = QString());
  TerminalSurface *splitVertical(TerminalSurface *target = nullptr,
                                 const QString &initialDir = QString());
  TerminalSurface *splitCurrentVertical();
  void closeActiveSurface();
  void closeSurface(TerminalSurface *surface);

  void focusLeftColumn();
  void focusRightColumn();
  void focusUpSplit();
  void focusDownSplit();
  void equalizeSplits();
  void toggleSplitZoom();

  void toggleSearchBar();
  void showSearchBar();
  void hideSearchBar();

  void rebuildCanvasLayout();
  void updateCanvasLayout();
  void updatePaneDecorations();

  void ensureSurfaceVisible(TerminalSurface *surface);

signals:
  void activeSurfaceChanged(TerminalSurface *surface);
  void titleChanged(const QString &title);
  void layoutChanged();
  void empty();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void setupSurface(TerminalSurface *surface);
  void setupPane(TerminalPane *pane);
  void setActiveSurface(TerminalSurface *surface);
  TerminalPane *findPaneForSurface(TerminalSurface *surface) const;
  KhosttySplitter *findSplitterForSurface(TerminalSurface *surface) const;

  QWidget *m_canvas = nullptr;
  QHBoxLayout *m_canvasLayout = nullptr;
  QList<KhosttySplitter *> m_columns;
  QList<WorklaneSeparator *> m_separators;
  TerminalSurface *m_activeSurface = nullptr;
  QString m_activeTitle;
  bool m_isZoomed = false;

  // Search bar overlay
  QWidget *m_searchBar = nullptr;
  QLineEdit *m_searchEdit = nullptr;
  QLabel *m_searchCountLabel = nullptr;
  int m_searchTotal = 0;
  int m_searchSelected = 0;
};
