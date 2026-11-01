#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <ghostty.h>
#include <khostty_bridge.h>

class TerminalSurface : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit TerminalSurface(QWidget* parent = nullptr, const QString& workingDirectory = QString());
    ~TerminalSurface() override;

    ghostty_surface_t surface() const { return m_surface; }
    QString title() const { return m_title; }
    QString processName() const;
    QString currentWorkingDirectory() const;

    bool handleAction(const ghostty_action_s& action);
    void handleProcessExited(bool exited);

    void pasteText(const QString& text);
    void sendText(const QString& text);
    void applyFontConfig();
    void setFontSize(float points);

    void startSearch(const QString& needle);
    void navigateSearch(bool next);
    void endSearch();

signals:
    void titleChanged(const QString& title);
    void pwdChanged(const QString& pwd);
    void bell();
    void closeRequested();
    void desktopNotification(const QString& title, const QString& body);
    void newSplitRequested(ghostty_action_split_direction_e direction);
    void gotoSplitRequested(ghostty_action_goto_split_e direction);
    void gotoTabRequested(int tabIndex);
    void nextTabRequested();
    void previousTabRequested();
    void toggleCommandPaletteRequested();
    void splitRequested(Qt::Orientation orientation);
    void searchStatsChanged(int total, int selected);

protected:
    bool event(QEvent* event) override;
    bool focusNextPrevChild(bool next) override;

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private:
    void setupGLResources();
    void freeGLResources();
    bool importDmaBufFrame(const khostty_frame_dmabuf_s& dmabuf);
    void uploadMemoryFrame(const khostty_frame_memory_s& memory);
    ghostty_input_mods_e translateModifiers(Qt::KeyboardModifiers qmods) const;
    ghostty_input_mouse_button_e translateMouseButton(Qt::MouseButton btn) const;
    ghostty_input_key_s buildGhosttyKeyEvent(QKeyEvent* event, ghostty_input_action_e action, QByteArray& textStorage) const;
    void showContextMenu(const QPoint& globalPos);
    static Qt::CursorShape mapGhosttyMouseShape(ghostty_action_mouse_shape_e shape);

    ghostty_surface_t m_surface = nullptr;

    // OpenGL texture & rendering state
    QOpenGLShaderProgram m_program;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_vao;
    GLuint m_texture = 0;
    bool m_glInitialized = false;

    // DMA-BUF EGL state
    void* m_eglDisplay = nullptr;
    bool m_supportsDmaBuf = true;
    QString m_title;
    QString m_currentPwd;
    QString m_hoveredUrl;
    bool m_isComposing = false;
};
