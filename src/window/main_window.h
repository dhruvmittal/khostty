#pragma once

#include <KXmlGuiWindow>

class WorklaneView;
class TerminalSurface;

class MainWindow : public KXmlGuiWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void newWindow();
    void newColumn();
    void splitVertical();
    void closeActive();
    void copyText();
    void pasteText();
    void showCommandBar();
    void updateTitle(const QString& title);

private:
    void setupActions();

    WorklaneView* m_worklane = nullptr;
};
