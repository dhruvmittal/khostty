#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QSplitter>
#include <QList>

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

    TerminalSurface* addColumn();
    TerminalSurface* splitCurrentVertical();
    void closeActiveSurface();

    void focusLeftColumn();
    void focusRightColumn();
    void focusUpSplit();
    void focusDownSplit();
    void equalizeSplits();
    void toggleSplitZoom();

signals:
    void activeSurfaceChanged(TerminalSurface* surface);
    void titleChanged(const QString& title);
    void empty();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void setupSurface(TerminalSurface* surface);
    void setupPane(TerminalPane* pane);
    void setActiveSurface(TerminalSurface* surface);
    void ensureSurfaceVisible(TerminalSurface* surface);
    TerminalPane* findPaneForSurface(TerminalSurface* surface) const;
    QSplitter* findSplitterForSurface(TerminalSurface* surface) const;

    QWidget* m_container = nullptr;
    QHBoxLayout* m_columnsLayout = nullptr;
    QList<QSplitter*> m_columns;
    TerminalSurface* m_activeSurface = nullptr;
    QString m_activeTitle;
    bool m_isZoomed = false;
};
