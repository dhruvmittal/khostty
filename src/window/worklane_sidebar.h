#pragma once

#include <QColor>
#include <QFont>
#include <QList>
#include <QPointer>
#include <QTimer>
#include <QWidget>

class QTreeWidgetItem;
class QToolButton;
class MainWindow;
class WorklaneView;
class TerminalSurface;
class SidebarTreeWidget;

class WorklaneSidebar : public QWidget {
  Q_OBJECT

public:
  explicit WorklaneSidebar(MainWindow *mainWindow, QWidget *parent = nullptr);
  ~WorklaneSidebar() override;

  MainWindow *mainWindow() const { return m_mainWindow; }

  void refreshTree();
  void scheduleRefresh();
  void markSurfaceAlert(TerminalSurface *surface, bool alert);
  void clearAlert(TerminalSurface *surface);
  const QColor &accentColor() const { return m_accentColor; }
  const QColor &vibrantAccentColor() const { return m_vibrantAccentColor; }
  const QFont &monoFont() const { return m_monoFont; }
  void updateAccentColor();

signals:
  void newWorklaneRequested();

protected:
  void paintEvent(QPaintEvent *event) override;
  void changeEvent(QEvent *event) override;

private slots:
  void onItemClicked(QTreeWidgetItem *item, int column);

private:
  void refreshThemeCache();
  void populateTab(int tabIndex, WorklaneView *worklane);
  QString formatPwd(const QString &fullPath) const;

  MainWindow *m_mainWindow = nullptr;
  SidebarTreeWidget *m_tree = nullptr;
  QToolButton *m_newWorklaneBtn = nullptr;
  QList<QPointer<TerminalSurface>> m_alertSurfaces;
  QTimer m_debounceTimer;
  bool m_updating = false;
  QColor m_accentColor;
  QColor m_vibrantAccentColor;
  QFont m_monoFont;
};
