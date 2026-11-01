#include "core/ghostty_core.h"
#include "terminal/terminal_surface.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QMetaObject>
#include <QDebug>

GhosttyCore::GhosttyCore() = default;

GhosttyCore::~GhosttyCore() {
    shutdown();
}

GhosttyCore& GhosttyCore::instance() {
    static GhosttyCore core;
    return core;
}

bool GhosttyCore::initialize(int argc, char** argv) {
    if (m_initialized) {
        return true;
    }

#ifdef GHOSTTY_PREFIX
    if (qEnvironmentVariableIsEmpty("TERMINFO_DIRS")) {
        qputenv("TERMINFO_DIRS", GHOSTTY_PREFIX "/share/terminfo");
    }
    if (qEnvironmentVariableIsEmpty("GHOSTTY_RESOURCES_DIR")) {
        qputenv("GHOSTTY_RESOURCES_DIR", GHOSTTY_PREFIX "/share/ghostty");
    }
#endif

    int init_res = ghostty_init(static_cast<uintptr_t>(argc), argv);
    if (init_res != 0) {
        qWarning() << "ghostty_init failed with code" << init_res;
        return false;
    }

    m_config = ghostty_config_new();
    if (!m_config) {
        qWarning() << "Failed to allocate ghostty configuration";
        return false;
    }

    ghostty_config_load_default_files(m_config);
    ghostty_config_finalize(m_config);

    ghostty_runtime_config_s runtime_config = {};
    runtime_config.userdata = this;
    runtime_config.supports_selection_clipboard = true;
    runtime_config.wakeup_cb = &GhosttyCore::wakeupCallback;
    runtime_config.action_cb = &GhosttyCore::actionCallback;
    runtime_config.read_clipboard_cb = &GhosttyCore::readClipboardCallback;
    runtime_config.confirm_read_clipboard_cb = &GhosttyCore::confirmReadClipboardCallback;
    runtime_config.write_clipboard_cb = &GhosttyCore::writeClipboardCallback;
    runtime_config.close_surface_cb = &GhosttyCore::closeSurfaceCallback;

    m_app = ghostty_app_new(&runtime_config, m_config);
    if (!m_app) {
        qWarning() << "Failed to create ghostty application";
        ghostty_config_free(m_config);
        m_config = nullptr;
        return false;
    }

    m_initialized = true;

    // Ghostty uses wakeupCallback for event-driven ticks.
    // Keep a relaxed 500ms heartbeat timer as a safeguard rather than 16ms busy-polling.
    connect(&m_tickTimer, &QTimer::timeout, this, &GhosttyCore::tick);
    m_tickTimer.start(500);

    return true;
}

void GhosttyCore::shutdown() {
    if (!m_initialized) {
        return;
    }
    m_initialized = false;
    m_tickTimer.stop();

    m_surfaces.clear();

    if (m_app) {
        ghostty_app_free(m_app);
        m_app = nullptr;
    }

    if (m_config) {
        ghostty_config_free(m_config);
        m_config = nullptr;
    }
}

void GhosttyCore::registerSurface(ghostty_surface_t surface, TerminalSurface* widget) {
    if (surface && widget) {
        m_surfaces.insert(surface, widget);
    }
}

void GhosttyCore::unregisterSurface(ghostty_surface_t surface) {
    m_surfaces.remove(surface);
}

TerminalSurface* GhosttyCore::findSurface(ghostty_surface_t surface) const {
    return m_surfaces.value(surface, nullptr);
}

void GhosttyCore::tick() {
    m_tickPending.store(false);
    if (m_app && m_initialized) {
        ghostty_app_tick(m_app);
    }
}

void GhosttyCore::wakeupCallback(void* userdata) {
    auto* self = static_cast<GhosttyCore*>(userdata);
    if (!self) return;
    if (!self->m_tickPending.exchange(true)) {
        QMetaObject::invokeMethod(self, &GhosttyCore::tick, Qt::QueuedConnection);
    }
}

bool GhosttyCore::actionCallback(ghostty_app_t app, ghostty_target_s target, ghostty_action_s action) {
    Q_UNUSED(app);
    GhosttyCore& self = GhosttyCore::instance();

    if (target.tag == GHOSTTY_TARGET_SURFACE && target.target.surface) {
        TerminalSurface* surfaceWidget = self.findSurface(target.target.surface);
        if (surfaceWidget) {
            return surfaceWidget->handleAction(action);
        }
    } else if (target.tag == GHOSTTY_TARGET_APP) {
        switch (action.tag) {
            case GHOSTTY_ACTION_QUIT:
                emit self.quitRequested();
                return true;
            case GHOSTTY_ACTION_NEW_WINDOW:
                emit self.newWindowRequested();
                return true;
            case GHOSTTY_ACTION_NEW_TAB:
                emit self.newTabRequested();
                return true;
            default:
                break;
        }
    }

    return false;
}

ghostty_clipboard_read_result_e GhosttyCore::readClipboardCallback(
    void* userdata,
    ghostty_clipboard_e clipboard,
    void* request_userdata,
    const char* const* available,
    size_t available_len,
    bool confirmed)
{
    Q_UNUSED(userdata);
    Q_UNUSED(available);
    Q_UNUSED(available_len);
    Q_UNUSED(confirmed);

    QClipboard* qclip = QGuiApplication::clipboard();
    if (!qclip) {
        return GHOSTTY_CLIPBOARD_READ_UNAVAILABLE;
    }

    QClipboard::Mode mode = (clipboard == GHOSTTY_CLIPBOARD_PRIMARY || clipboard == GHOSTTY_CLIPBOARD_SELECTION)
        ? QClipboard::Selection
        : QClipboard::Clipboard;

    QString text = qclip->text(mode);
    QByteArray utf8 = text.toUtf8();

    ghostty_clipboard_content_s content = {
        .mime = "text/plain;charset=utf-8",
        .data = utf8.constData(),
        .len = static_cast<size_t>(utf8.size())
    };

    ghostty_clipboard_complete_s complete = {
        .contents = &content,
        .contents_len = 1,
        .available = nullptr,
        .available_len = 0,
        .confirmed = true,
        .remember = false
    };

    // If request_userdata is surface, complete it
    ghostty_surface_t surface = static_cast<ghostty_surface_t>(request_userdata);
    if (surface) {
        ghostty_surface_complete_clipboard_request(surface, &complete, request_userdata);
        return GHOSTTY_CLIPBOARD_READ_STARTED;
    }

    return GHOSTTY_CLIPBOARD_READ_UNAVAILABLE;
}

void GhosttyCore::confirmReadClipboardCallback(
    void* userdata,
    const ghostty_clipboard_confirm_s* confirm,
    void* request_userdata,
    ghostty_clipboard_request_e request)
{
    Q_UNUSED(userdata);
    Q_UNUSED(confirm);
    Q_UNUSED(request);

    // Auto-confirm clipboard reads
    ghostty_surface_t surface = static_cast<ghostty_surface_t>(request_userdata);
    if (surface && confirm) {
        ghostty_clipboard_complete_s complete = {
            .contents = confirm->contents,
            .contents_len = confirm->contents_len,
            .available = confirm->available,
            .available_len = confirm->available_len,
            .confirmed = true,
            .remember = false
        };
        ghostty_surface_complete_clipboard_request(surface, &complete, request_userdata);
    }
}

void GhosttyCore::writeClipboardCallback(
    void* userdata,
    ghostty_clipboard_e clipboard,
    const ghostty_clipboard_content_s* contents,
    size_t len,
    bool confirmed)
{
    Q_UNUSED(userdata);
    Q_UNUSED(confirmed);

    QClipboard* qclip = QGuiApplication::clipboard();
    if (!qclip || !contents || len == 0) {
        return;
    }

    QClipboard::Mode mode = (clipboard == GHOSTTY_CLIPBOARD_PRIMARY || clipboard == GHOSTTY_CLIPBOARD_SELECTION)
        ? QClipboard::Selection
        : QClipboard::Clipboard;

    // Use the first content matching text/plain or plain data
    for (size_t i = 0; i < len; ++i) {
        if (contents[i].data && contents[i].len > 0) {
            QString text = QString::fromUtf8(contents[i].data, static_cast<qsizetype>(contents[i].len));
            qclip->setText(text, mode);
            break;
        }
    }
}

void GhosttyCore::closeSurfaceCallback(void* userdata, bool process_alive) {
    TerminalSurface* widget = static_cast<TerminalSurface*>(userdata);
    if (widget) {
        QMetaObject::invokeMethod(widget, [widget, process_alive]() {
            // When close_surface_cb is invoked, Ghostty requests that the surface be closed.
            // process_alive indicates whether the child process is still running.
            // In either case, the surface should be closed.
            widget->handleProcessExited(!process_alive);
        }, Qt::QueuedConnection);
    }
}
