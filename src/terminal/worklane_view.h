#pragma once

#include "terminal/khostty_splitter.h"
#include "terminal/worklane_separator.h"
#include <QHBoxLayout>
#include <QHash>
#include <QList>
#include <QPointer>
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
  void unzoom();
  void setActiveSurface(TerminalSurface *surface);

  static QString formatPositionTag(int colIdx, int colCount, int rowIdx,
                                   int rowCount);

  void toggleSearchBar();
  void showSearchBar();
  void hideSearchBar();

  void rebuildCanvasLayout();
  void updateCanvasLayout();
  void updatePaneDecorations();
  void setColumnWidth(KhosttySplitter *col, int width);
  static int minColumnWidth(int viewportW);
  static int maxColumnWidth(int viewportW);

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
  void updateSearchCountLabel();
  TerminalPane *findPaneForSurface(TerminalSurface *surface) const;
  KhosttySplitter *findSplitterForSurface(TerminalSurface *surface) const;

  QWidget *m_canvas = nullptr;
  QHBoxLayout *m_canvasLayout = nullptr;
  QList<KhosttySplitter *> m_columns;
  QHash<KhosttySplitter *, double> m_columnRatios;
  QList<WorklaneSeparator *> m_separators;
  TerminalSurface *m_activeSurface = nullptr;
  QString m_activeTitle;
  QPointer<TerminalPane> m_zoomedPane;

  // Search bar overlay
  QWidget *m_searchBar = nullptr;
  QLineEdit *m_searchEdit = nullptr;
  QLabel *m_searchCountLabel = nullptr;
  int m_searchTotal = -1;
  int m_searchSelected = -1;
};
