#pragma once

#include <QWidget>
#include <QPointer>
#include <QList>
#include <QTimer>

class QTreeWidget;
class QTreeWidgetItem;
class QToolButton;
class MainWindow;
class WorklaneView;
class TerminalSurface;

class WorklaneSidebar : public QWidget {
    Q_OBJECT

public:
    explicit WorklaneSidebar(MainWindow* mainWindow, QWidget* parent = nullptr);
    ~WorklaneSidebar() override;

    void refreshTree();
    void scheduleRefresh();
    void markSurfaceAlert(TerminalSurface* surface, bool alert);
    void clearAlert(TerminalSurface* surface);
    QColor accentColor() const;
    QColor vibrantAccentColor() const;
    void updateAccentColor();

signals:
    void newWorklaneRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);

private:
    void populateTab(int tabIndex, WorklaneView* worklane);
    QString formatPwd(const QString& fullPath) const;

    MainWindow* m_mainWindow = nullptr;
    QTreeWidget* m_tree = nullptr;
    QToolButton* m_newWorklaneBtn = nullptr;
    QList<QPointer<TerminalSurface>> m_alertSurfaces;
    QTimer m_debounceTimer;
    bool m_updating = false;
};
