#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QList>
#include "terminal/khostty_splitter.h"
#include "terminal/worklane_separator.h"

class TerminalSurface;
class TerminalPane;

class WorklaneView : public QScrollArea {
    Q_OBJECT

public:
    explicit WorklaneView(QWidget* parent = nullptr);
    ~WorklaneView() override;

    TerminalSurface* activeSurface() const;
    int columnCount() const;
    QString activeTitle() const { return m_activeTitle; }
    bool hasAnySplit() const;

    TerminalSurface* addColumn();
    TerminalSurface* splitCurrentVertical();
    void closeActiveSurface();
    void closeSurface(TerminalSurface* surface);

    void focusLeftColumn();
    void focusRightColumn();
    void focusUpSplit();
    void focusDownSplit();
    void equalizeSplits();
    void toggleSplitZoom();

    void rebuildCanvasLayout();
    void updateCanvasLayout();

signals:
    void activeSurfaceChanged(TerminalSurface* surface);
    void titleChanged(const QString& title);
    void empty();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupSurface(TerminalSurface* surface);
    void setupPane(TerminalPane* pane);
    void setActiveSurface(TerminalSurface* surface);
    void ensureSurfaceVisible(TerminalSurface* surface);
    TerminalPane* findPaneForSurface(TerminalSurface* surface) const;
    KhosttySplitter* findSplitterForSurface(TerminalSurface* surface) const;

    QWidget* m_canvas = nullptr;
    QHBoxLayout* m_canvasLayout = nullptr;
    QList<KhosttySplitter*> m_columns;
    QList<WorklaneSeparator*> m_separators;
    TerminalSurface* m_activeSurface = nullptr;
    QString m_activeTitle;
    bool m_isZoomed = false;
};
