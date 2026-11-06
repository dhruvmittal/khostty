#pragma once

#include <KXmlGuiWindow>
#include <QPointer>

class QTabBar;
class QStackedWidget;
class QSplitter;
class WorklaneView;
class TerminalSurface;
class KCommandBar;
class WorklaneSidebar;

class MainWindow : public KXmlGuiWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  WorklaneView *currentWorklane() const;
  TerminalSurface *activeSurface() const;
  int tabCount() const;
  WorklaneView *worklaneAt(int index) const;
  QString tabText(int index) const;
  void setCurrentTabIndex(int index);
  void moveTab(int fromIndex, int toIndex);
  void movePane(TerminalSurface *surface, int targetTabIndex,
                TerminalSurface *targetSurface, bool insertAfter,
                bool stackUnder);

public slots:
  void newWindow();
  void newTab();
  void toggleSidebar();
  void closeCurrentTab();
  void closeTab(int index);
  void gotoTab(int tabIdx);
  void nextTab();
  void prevTab();
  void onTabChanged(int index);

  void newColumn();
  void splitVertical();
  void closeActive();
  void toggleSplitZoom();
  void equalizeSplits();
  void copyText();
  void pasteText();
  void findText();
  void zoomIn();
  void zoomOut();
  void zoomReset();
  void showPreferencesDialog();
  void showCommandBar();
  void updateTitle(const QString &title);
  void syncColorScheme();
  void handleBell();
  void handleDesktopNotification(const QString &title, const QString &body);

protected:
  void changeEvent(QEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void setupActions();
  void updateTabBarVisibility();
  void updateTabTitleWithSplitCount(int index);
  WorklaneView *createTab(const QString &title = QString(),
                          const QString &initialDir = QString());

  QTabBar *m_tabBar = nullptr;
  QStackedWidget *m_stack = nullptr;
  QSplitter *m_mainSplitter = nullptr;
  WorklaneSidebar *m_sidebar = nullptr;
  bool m_sidebarVisible = false;
  int m_sidebarSavedWidth = 240;
  QPointer<KCommandBar> m_commandBar;
  QAction *m_toggleSidebarAction = nullptr;
  QList<QAction *> m_tabSwitchActions;
};
