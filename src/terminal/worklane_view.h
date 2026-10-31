#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QSplitter>
#include <QList>

class TerminalSurface;

class WorklaneView : public QScrollArea {
    Q_OBJECT

public:
    explicit WorklaneView(QWidget* parent = nullptr);
    ~WorklaneView() override;

    TerminalSurface* activeSurface() const;
    int columnCount() const;

    TerminalSurface* addColumn();
    TerminalSurface* splitCurrentVertical();
    void closeActiveSurface();

    void focusLeftColumn();
    void focusRightColumn();
    void focusUpSplit();
    void focusDownSplit();

signals:
    void activeSurfaceChanged(TerminalSurface* surface);
    void titleChanged(const QString& title);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setActiveSurface(TerminalSurface* surface);
    void ensureSurfaceVisible(TerminalSurface* surface);
    QSplitter* findSplitterForSurface(TerminalSurface* surface) const;

    QWidget* m_container = nullptr;
    QHBoxLayout* m_columnsLayout = nullptr;
    QList<QSplitter*> m_columns;
    TerminalSurface* m_activeSurface = nullptr;
};
