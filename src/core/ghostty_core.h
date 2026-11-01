#pragma once

#include <QObject>
#include <QHash>
#include <QTimer>
#include <atomic>
#include <ghostty.h>
#include <khostty_bridge.h>

class TerminalSurface;

class GhosttyCore : public QObject {
    Q_OBJECT

public:
    static GhosttyCore& instance();

    bool initialize(int argc, char** argv);
    void shutdown();

    ghostty_app_t app() const { return m_app; }
    ghostty_config_t config() const { return m_config; }

    void registerSurface(ghostty_surface_t surface, TerminalSurface* widget);
    void unregisterSurface(ghostty_surface_t surface);
    TerminalSurface* findSurface(ghostty_surface_t surface) const;
    QList<TerminalSurface*> allSurfaces() const { return m_surfaces.values(); }
    void reloadConfig();

public slots:
    void tick();

signals:
    void newWindowRequested();
    void newTabRequested();
    void quitRequested();

private:
    GhosttyCore();
    ~GhosttyCore() override;
    Q_DISABLE_COPY(GhosttyCore)

    static void wakeupCallback(void* userdata);
    static bool actionCallback(ghostty_app_t app, ghostty_target_s target, ghostty_action_s action);
    static ghostty_clipboard_read_result_e readClipboardCallback(
        void* userdata,
        ghostty_clipboard_e clipboard,
        void* request_userdata,
        const char* const* available,
        size_t available_len,
        bool confirmed);
    static void confirmReadClipboardCallback(
        void* userdata,
        const ghostty_clipboard_confirm_s* confirm,
        void* request_userdata,
        ghostty_clipboard_request_e request);
    static void writeClipboardCallback(
        void* userdata,
        ghostty_clipboard_e clipboard,
        const ghostty_clipboard_content_s* contents,
        size_t len,
        bool confirmed);
    static void closeSurfaceCallback(void* userdata, bool process_alive);

    ghostty_app_t m_app = nullptr;
    ghostty_config_t m_config = nullptr;
    QHash<ghostty_surface_t, TerminalSurface*> m_surfaces;
    bool m_initialized = false;
    std::atomic<bool> m_tickPending{false};
    QTimer m_tickTimer;
};
