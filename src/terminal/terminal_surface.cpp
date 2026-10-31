#include "terminal/terminal_surface.h"
#include "core/ghostty_core.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFocusEvent>
#include <QOpenGLContext>
#include <QDebug>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_LINUX_DMA_BUF_EXT
#define EGL_LINUX_DMA_BUF_EXT 0x3270
#define EGL_LINUX_DRM_FOURCC_EXT 0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT 0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT 0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT 0x3274
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT 0x3443
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT 0x3444
#endif

#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif

typedef EGLImageKHR (*PFNEGLCREATEIMAGEKHRPROC)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint*);
typedef EGLBoolean (*PFNEGLDESTROYIMAGEKHRPROC)(EGLDisplay, EGLImageKHR);
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum, GLeglImageOES);

static PFNEGLCREATEIMAGEKHRPROC s_eglCreateImageKHR = nullptr;
static PFNEGLDESTROYIMAGEKHRPROC s_eglDestroyImageKHR = nullptr;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC s_glEGLImageTargetTexture2DOES = nullptr;

TerminalSurface::TerminalSurface(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    ghostty_app_t app = GhosttyCore::instance().app();
    if (!app) {
        qWarning() << "Ghostty core application not initialized!";
        return;
    }

    ghostty_surface_config_s config = ghostty_surface_config_new();
    config.platform_tag = GHOSTTY_PLATFORM_LINUX;
    config.platform.linux_platform.egl_display = nullptr;
    config.scale_factor = devicePixelRatioF();
    config.userdata = this;

    m_surface = ghostty_surface_new(app, &config);
    if (!m_surface) {
        qWarning() << "Failed to allocate ghostty surface!";
        return;
    }

    GhosttyCore::instance().registerSurface(m_surface, this);

    double dpr = devicePixelRatioF();
    uint32_t px_w = static_cast<uint32_t>(std::max(1, width()) * dpr);
    uint32_t px_h = static_cast<uint32_t>(std::max(1, height()) * dpr);
    ghostty_surface_set_content_scale(m_surface, dpr, dpr);
    ghostty_surface_set_size(m_surface, px_w, px_h);
}

TerminalSurface::~TerminalSurface() {
    if (m_glInitialized && isValid() && context()) {
        makeCurrent();
        freeGLResources();
        doneCurrent();
    }

    if (m_surface) {
        GhosttyCore::instance().unregisterSurface(m_surface);
        ghostty_surface_free(m_surface);
        m_surface = nullptr;
    }
}

void TerminalSurface::initializeGL() {
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!isValid() || !context() || !ctx || !ctx->isValid()) {
        qWarning() << "OpenGL context is not valid for TerminalSurface";
        return;
    }

    initializeOpenGLFunctions();

    m_eglDisplay = eglGetCurrentDisplay();
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        m_supportsDmaBuf = false;
    } else {
        s_eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(
            eglGetProcAddress("eglCreateImageKHR"));
        s_eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(
            eglGetProcAddress("eglDestroyImageKHR"));
        s_glEGLImageTargetTexture2DOES = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
            eglGetProcAddress("glEGLImageTargetTexture2DOES"));

        if (!s_eglCreateImageKHR || !s_eglDestroyImageKHR || !s_glEGLImageTargetTexture2DOES) {
            m_supportsDmaBuf = false;
        }
    }

    setupGLResources();
    m_glInitialized = true;
}

void TerminalSurface::setupGLResources() {
    const char* vsrc =
        "#version 330 core\n"
        "layout(location = 0) in vec2 aPos;\n"
        "layout(location = 1) in vec2 aTexCoord;\n"
        "out vec2 TexCoord;\n"
        "void main() {\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "    TexCoord = aTexCoord;\n"
        "}\n";

    const char* fsrc =
        "#version 330 core\n"
        "in vec2 TexCoord;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D uTexture;\n"
        "void main() {\n"
        "    FragColor = texture(uTexture, TexCoord);\n"
        "}\n";

    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);
    m_program.link();

    // Quad covering entire normalized device coordinates (-1 to 1)
    // Vertices: pos.x, pos.y, tex.u, tex.v
    static const float vertices[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,

        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices));

    m_program.enableAttributeArray(0);
    m_program.setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(float));

    m_program.enableAttributeArray(1);
    m_program.setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));

    m_vao.release();
    m_vbo.release();

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void TerminalSurface::freeGLResources() {
    if (!m_glInitialized) return;
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    m_vbo.destroy();
    m_vao.destroy();
    m_program.removeAllShaders();
    m_glInitialized = false;
}

void TerminalSurface::resizeGL(int w, int h) {
    if (!m_surface) return;

    double dpr = devicePixelRatioF();
    uint32_t px_w = static_cast<uint32_t>(std::max(1, w));
    uint32_t px_h = static_cast<uint32_t>(std::max(1, h));

    ghostty_surface_set_content_scale(m_surface, dpr, dpr);
    ghostty_surface_set_size(m_surface, px_w, px_h);
}

bool TerminalSurface::importDmaBufFrame(const khostty_frame_dmabuf_s& dmabuf) {
    if (!m_supportsDmaBuf || !m_eglDisplay || dmabuf.planes.count == 0) {
        return false;
    }

    EGLint attribs[32];
    int idx = 0;

    attribs[idx++] = EGL_WIDTH;
    attribs[idx++] = static_cast<EGLint>(dmabuf.width);
    attribs[idx++] = EGL_HEIGHT;
    attribs[idx++] = static_cast<EGLint>(dmabuf.height);
    attribs[idx++] = EGL_LINUX_DRM_FOURCC_EXT;
    attribs[idx++] = static_cast<EGLint>(dmabuf.fourcc);

    // Plane 0
    attribs[idx++] = EGL_DMA_BUF_PLANE0_FD_EXT;
    attribs[idx++] = dmabuf.planes.fds[0];
    attribs[idx++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
    attribs[idx++] = dmabuf.planes.offsets[0];
    attribs[idx++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
    attribs[idx++] = dmabuf.planes.strides[0];

    // Only set modifier if not DRM_FORMAT_MOD_INVALID (0x00ffffffffffffffULL)
    if (dmabuf.modifier != 0x00ffffffffffffffULL) {
        attribs[idx++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
        attribs[idx++] = static_cast<EGLint>(dmabuf.modifier & 0xFFFFFFFF);
        attribs[idx++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
        attribs[idx++] = static_cast<EGLint>((dmabuf.modifier >> 32) & 0xFFFFFFFF);
    }

    attribs[idx++] = EGL_NONE;

    EGLImageKHR image = s_eglCreateImageKHR(
        m_eglDisplay,
        EGL_NO_CONTEXT,
        EGL_LINUX_DMA_BUF_EXT,
        nullptr,
        attribs
    );

    if (image == EGL_NO_IMAGE_KHR) {
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, m_texture);
    while (glGetError() != GL_NO_ERROR) {}
    s_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    GLenum err = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);

    s_eglDestroyImageKHR(m_eglDisplay, image);
    return (err == GL_NO_ERROR);
}

void TerminalSurface::uploadMemoryFrame(const khostty_frame_memory_s& memory) {
    if (!memory.pixels || memory.width == 0 || memory.height == 0) {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        static_cast<GLsizei>(memory.width),
        static_cast<GLsizei>(memory.height),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        memory.pixels
    );
    glBindTexture(GL_TEXTURE_2D, 0);
}

void TerminalSurface::paintGL() {
    if (!m_surface || !m_glInitialized || !isValid() || !context()) return;

    khostty_exported_frame_s frame = {};
    if (khostty_surface_take_frame(m_surface, &frame)) {
        if (frame.frame_type == KHOSTTY_FRAME_DMABUF) {
            bool ok = importDmaBufFrame(frame.dmabuf);
            khostty_surface_release_frame(m_surface, &frame);
            khostty_surface_report_presentation_health(m_surface, ok);
        } else if (frame.frame_type == KHOSTTY_FRAME_MEMORY) {
            uploadMemoryFrame(frame.memory);
            khostty_surface_release_frame(m_surface, &frame);
        }
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_program.bind();
    m_vao.bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    m_program.setUniformValue("uTexture", 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindTexture(GL_TEXTURE_2D, 0);
    m_vao.release();
    m_program.release();
}

ghostty_input_mods_e TerminalSurface::translateModifiers(Qt::KeyboardModifiers qmods) const {
    int mods = GHOSTTY_MODS_NONE;
    if (qmods & Qt::ShiftModifier)   mods |= GHOSTTY_MODS_SHIFT;
    if (qmods & Qt::ControlModifier) mods |= GHOSTTY_MODS_CTRL;
    if (qmods & Qt::AltModifier)     mods |= GHOSTTY_MODS_ALT;
    if (qmods & Qt::MetaModifier)    mods |= GHOSTTY_MODS_SUPER;
    return static_cast<ghostty_input_mods_e>(mods);
}

ghostty_input_mouse_button_e TerminalSurface::translateMouseButton(Qt::MouseButton btn) const {
    switch (btn) {
        case Qt::LeftButton:   return GHOSTTY_MOUSE_LEFT;
        case Qt::RightButton:  return GHOSTTY_MOUSE_RIGHT;
        case Qt::MiddleButton: return GHOSTTY_MOUSE_MIDDLE;
        case Qt::BackButton:   return GHOSTTY_MOUSE_FOUR;
        case Qt::ForwardButton:return GHOSTTY_MOUSE_FIVE;
        default:               return GHOSTTY_MOUSE_UNKNOWN;
    }
}

ghostty_input_key_s TerminalSurface::buildGhosttyKeyEvent(
    QKeyEvent* event,
    ghostty_input_action_e action,
    QByteArray& textStorage) const
{
    ghostty_input_key_s key = {};
    key.action = action;
    key.mods = translateModifiers(event->modifiers());
    key.keycode = static_cast<uint32_t>(event->nativeScanCode());

    textStorage = event->text().toUtf8();
    uint32_t unshifted = 0;
    int qkey = event->key();

    switch (qkey) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            unshifted = '\r';
            textStorage = "\r";
            break;
        case Qt::Key_Tab:
            unshifted = '\t';
            textStorage = "\t";
            break;
        case Qt::Key_Backspace:
            unshifted = 0x7F;
            textStorage = "\x7f";
            break;
        case Qt::Key_Escape:
            unshifted = 0x1B;
            textStorage = "\x1b";
            break;
        default:
            if (qkey >= Qt::Key_A && qkey <= Qt::Key_Z) {
                unshifted = static_cast<uint32_t>((qkey - Qt::Key_A) + 'a');
            } else if (qkey <= 0xFFFF) {
                unshifted = static_cast<uint32_t>(qkey);
            }

            if (event->modifiers() & Qt::ControlModifier) {
                if (qkey >= Qt::Key_A && qkey <= Qt::Key_Z) {
                    textStorage = QByteArray(1, static_cast<char>((qkey - Qt::Key_A) + 'a'));
                } else if (qkey >= 0x20 && qkey <= 0x7E) {
                    textStorage = QByteArray(1, static_cast<char>(qkey));
                } else {
                    textStorage.clear();
                }
            } else if (!textStorage.isEmpty() &&
                       (static_cast<unsigned char>(textStorage.at(0)) < 0x20 ||
                        static_cast<unsigned char>(textStorage.at(0)) == 0x7F)) {
                textStorage.clear();
            }
            break;
    }

    key.text = textStorage.isEmpty() ? nullptr : textStorage.constData();
    key.unshifted_codepoint = unshifted;
    return key;
}

void TerminalSurface::keyPressEvent(QKeyEvent* event) {
    if (!m_surface) return;

    QByteArray textStorage;
    ghostty_input_action_e action = event->isAutoRepeat() ? GHOSTTY_ACTION_REPEAT : GHOSTTY_ACTION_PRESS;
    ghostty_input_key_s key = buildGhosttyKeyEvent(event, action, textStorage);

    bool handled = ghostty_surface_key(m_surface, key);
    if (handled) {
        event->accept();
    } else {
        QOpenGLWidget::keyPressEvent(event);
    }
}

void TerminalSurface::keyReleaseEvent(QKeyEvent* event) {
    if (!m_surface) return;

    QByteArray textStorage;
    ghostty_input_key_s key = buildGhosttyKeyEvent(event, GHOSTTY_ACTION_RELEASE, textStorage);

    ghostty_surface_key(m_surface, key);
    event->accept();
}

void TerminalSurface::mousePressEvent(QMouseEvent* event) {
    if (!m_surface) return;
    ghostty_surface_mouse_button(
        m_surface,
        GHOSTTY_MOUSE_PRESS,
        translateMouseButton(event->button()),
        translateModifiers(event->modifiers())
    );
    event->accept();
}

void TerminalSurface::mouseReleaseEvent(QMouseEvent* event) {
    if (!m_surface) return;
    ghostty_surface_mouse_button(
        m_surface,
        GHOSTTY_MOUSE_RELEASE,
        translateMouseButton(event->button()),
        translateModifiers(event->modifiers())
    );
    event->accept();
}

void TerminalSurface::mouseMoveEvent(QMouseEvent* event) {
    if (!m_surface) return;
    double dpr = devicePixelRatioF();
    ghostty_surface_mouse_pos(
        m_surface,
        event->position().x() * dpr,
        event->position().y() * dpr,
        translateModifiers(event->modifiers())
    );
    event->accept();
}

void TerminalSurface::wheelEvent(QWheelEvent* event) {
    if (!m_surface) return;

    // Route horizontal gestures or Shift/Alt + Wheel to parent WorklaneView for column scrolling
    if ((event->modifiers() & (Qt::ShiftModifier | Qt::AltModifier)) ||
        (std::abs(event->angleDelta().x()) > std::abs(event->angleDelta().y()) && event->angleDelta().x() != 0) ||
        (!event->pixelDelta().isNull() && std::abs(event->pixelDelta().x()) > std::abs(event->pixelDelta().y()) && event->pixelDelta().x() != 0)) {
        event->ignore();
        return;
    }

    ghostty_surface_mouse_scroll(
        m_surface,
        event->angleDelta().x(),
        event->angleDelta().y(),
        0
    );
    event->accept();
}

void TerminalSurface::focusInEvent(QFocusEvent* event) {
    QOpenGLWidget::focusInEvent(event);
    if (m_surface) {
        ghostty_surface_set_focus(m_surface, true);
    }
}

void TerminalSurface::focusOutEvent(QFocusEvent* event) {
    QOpenGLWidget::focusOutEvent(event);
    if (m_surface) {
        ghostty_surface_set_focus(m_surface, false);
    }
}

void TerminalSurface::pasteText(const QString& text) {
    if (!m_surface || text.isEmpty()) return;
    QByteArray utf8 = text.toUtf8();
    ghostty_surface_paste_text(m_surface, utf8.constData(), static_cast<uintptr_t>(utf8.size()));
}

void TerminalSurface::sendText(const QString& text) {
    if (!m_surface || text.isEmpty()) return;
    QByteArray utf8 = text.toUtf8();
    ghostty_surface_text(m_surface, utf8.constData(), static_cast<uintptr_t>(utf8.size()));
}

bool TerminalSurface::handleAction(const ghostty_action_s& action) {
    switch (action.tag) {
        case GHOSTTY_ACTION_RENDER:
            QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
            return true;

        case GHOSTTY_ACTION_SET_TITLE:
            if (action.action.set_title.title) {
                QString title = QString::fromUtf8(action.action.set_title.title);
                m_title = title;
                emit titleChanged(title);
            }
            return true;

        case GHOSTTY_ACTION_PWD:
            if (action.action.pwd.pwd) {
                QString pwd = QString::fromUtf8(action.action.pwd.pwd);
                emit pwdChanged(pwd);
            }
            return true;

        case GHOSTTY_ACTION_NEW_TAB:
            emit GhosttyCore::instance().newTabRequested();
            return true;

        case GHOSTTY_ACTION_NEW_SPLIT:
            emit newSplitRequested(action.action.new_split);
            return true;

        case GHOSTTY_ACTION_GOTO_SPLIT:
            emit gotoSplitRequested(action.action.goto_split);
            return true;

        case GHOSTTY_ACTION_GOTO_TAB:
            emit gotoTabRequested(static_cast<int>(action.action.goto_tab));
            return true;

        case GHOSTTY_ACTION_MOVE_TAB:
            if (action.action.move_tab.amount > 0) emit nextTabRequested();
            else emit previousTabRequested();
            return true;

        case GHOSTTY_ACTION_RING_BELL:
            emit bell();
            return true;

        case GHOSTTY_ACTION_CLOSE_WINDOW:
        case GHOSTTY_ACTION_CLOSE_TAB:
            emit closeRequested();
            return true;

        case GHOSTTY_ACTION_DESKTOP_NOTIFICATION:
            if (action.action.desktop_notification.title && action.action.desktop_notification.body) {
                emit desktopNotification(
                    QString::fromUtf8(action.action.desktop_notification.title),
                    QString::fromUtf8(action.action.desktop_notification.body)
                );
            }
            return true;

        default:
            break;
    }
    return false;
}

void TerminalSurface::handleProcessExited(bool exited) {
    if (exited) {
        emit closeRequested();
    }
}
