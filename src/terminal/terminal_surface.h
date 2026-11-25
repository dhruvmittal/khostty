#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QUrl>
#include <string_view>
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
    bool performBindingAction(std::string_view action);

    static bool isAllowedExternalUrl(const QUrl& url);
    static bool openExternalUrl(const QString& rawUrl);

    void startSearch(const QString& needle);
    void navigateSearch(bool next);
    void endSearch();
    void setWindowOccluded(bool occluded);

signals:
    void titleChanged(const QString& title);
    void pwdChanged(const QString& pwd);
    void bell();
    void closeRequested();
    void newTabRequested();
    void contextMenuRequested(const QPoint& globalPos, const QString& hoveredUrl);
    void desktopNotification(const QString& title, const QString& body);
    void gotoSplitRequested(ghostty_action_goto_split_e direction);
    void gotoTabRequested(int tabIndex);
    void nextTabRequested();
    void previousTabRequested();
    void toggleCommandPaletteRequested();
    void splitRequested(Qt::Orientation orientation);
    void searchTotalChanged(int total);
    void searchSelectedChanged(int selected);

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
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private:
    void setupGLResources();
    void freeGLResources();
    void syncSurfaceSize();
    void syncSurfaceSize(uint32_t px_w, uint32_t px_h);
    bool importDmaBufFrame(const khostty_frame_dmabuf_s& dmabuf);
    void uploadMemoryFrame(const khostty_frame_memory_s& memory);
    void reportPresentationUnhealthy();
    void scheduleOcclusionSync();
    void syncOcclusion();
    void scheduleFocusSync();
    void syncFocus();
    ghostty_input_mods_e translateModifiers(Qt::KeyboardModifiers qmods) const;
    ghostty_input_mouse_button_e translateMouseButton(Qt::MouseButton btn) const;
    ghostty_input_key_s buildGhosttyKeyEvent(QKeyEvent* event, ghostty_input_action_e action, QByteArray& textStorage) const;
    static Qt::CursorShape mapGhosttyMouseShape(ghostty_action_mouse_shape_e shape);

    ghostty_surface_t m_surface = nullptr;

    // OpenGL texture & rendering state
    QOpenGLShaderProgram m_program;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_vao;
    GLuint m_texture = 0;
    bool m_glInitialized = false;
    bool m_hadGLContext = false;
    bool m_hasFrame = false;
    QMetaObject::Connection m_contextDestroyedConn;
    double m_lastDpr = 0.0;
    uint32_t m_lastPxW = 0;
    uint32_t m_lastPxH = 0;

    // DMA-BUF EGL state
    void* m_eglDisplay = nullptr;
    bool m_supportsDmaBuf = true;
    bool m_reportedUnhealthy = false;
    QString m_title;
    QString m_currentPwd;
    QString m_hoveredUrl;
    bool m_isComposing = false;
    bool m_shown = false;
    bool m_windowOccluded = false;
    bool m_occlusionSyncPending = false;
    bool m_lastSentFocus = true;
    bool m_focusSyncPending = false;
};
