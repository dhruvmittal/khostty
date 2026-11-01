#pragma once

#include <KXmlGuiWindow>
#include <QPointer>

class QTabBar;
class QStackedWidget;
class WorklaneView;
class TerminalSurface;
class KCommandBar;

class MainWindow : public KXmlGuiWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    WorklaneView* currentWorklane() const;
    TerminalSurface* activeSurface() const;

private slots:
    void newWindow();
    void newTab();
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
    void showPreferencesDialog();
    void showCommandBar();
    void updateTitle(const QString& title);

protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupActions();
    void updateTabBarVisibility();
    WorklaneView* createTab(const QString& title = QString());

    QTabBar* m_tabBar = nullptr;
    QStackedWidget* m_stack = nullptr;
    QPointer<KCommandBar> m_commandBar;
};
