#pragma once

// Application — base class owning the GLFW window + OpenGL context + ImGui
// context, running the canonical frame loop. Subclass and override the hooks.
// Lifted + modernized from 19-logos application.hpp and the imgui-cpp skill's
// bootstrap template (so a scaffolded project's main.cpp is just a subclass).
// Backend: GLFW + OpenGL3 + Dear ImGui docking branch — matches the skill.
//
// This header forward-declares the backend handles so consumers do NOT
// transitively include <GLFW/glfw3.h> or <imgui.h>; those live in the .cpp.
//
// Deferred (post-beta): std::expected init boundary (kept as AppStatus enum —
// maps cleanly to process exit codes; std::expected is available but adds no
// value over the enum for the run()->exit-code path), GL debug callback,
// settings auto-inspector, session persistence, app-managed dockspace host,
// font management, multiple concurrent Application instances.

#include <string>

#include <imtool/common/vector.hpp>    // Vec2i, Vec4f
#include <imtool/common/logging.hpp>   // imtool::log()

struct GLFWwindow;
struct ImGuiContext;

namespace imtool {

// Fallible-init result. run() returns static_cast<int>(status): Ok -> 0, each
// error -> a distinct non-zero exit code. The human-readable cause is logged
// via imtool::log() << LogLevel::Error before the status returns.
enum class AppStatus : int {
    Ok                = 0,
    AlreadyCreated    = 1,
    GlfwInitFailed    = 2,
    WindowFailed      = 3,
    ContextFailed     = 4,
    ImGuiInitFailed   = 5,
    BackendInitFailed = 6,
    UserInitFailed    = 7,
};

// Inline (header-only) so the pure enum->string mapping is usable + testable
// without linking the Application .cpp (which pulls in GLFW/GL).
[[nodiscard]] inline const char* toString(AppStatus s) noexcept {
    switch(s) {
        case AppStatus::Ok:                return "Ok";
        case AppStatus::AlreadyCreated:    return "AlreadyCreated";
        case AppStatus::GlfwInitFailed:    return "GlfwInitFailed";
        case AppStatus::WindowFailed:      return "WindowFailed";
        case AppStatus::ContextFailed:     return "ContextFailed";
        case AppStatus::ImGuiInitFailed:   return "ImGuiInitFailed";
        case AppStatus::BackendInitFailed: return "BackendInitFailed";
        case AppStatus::UserInitFailed:    return "UserInitFailed";
    }
    return "Unknown";
}

// Plain config aggregate. Docking/viewports are supported but OFF by default.
struct AppConfig {
    std::string title          = "imtool app";
    Vec2i       size           = Vec2i(1280, 800);   // initial window size (logical px, pre-DPI)
    Vec2i       minSize        = Vec2i(640, 480);     // size limit; (0,0) => none
    Vec4f       clearColor     = Vec4f(0.10f, 0.12f, 0.14f, 1.00f);

    int         glMajor        = 3;
    int         glMinor        = 0;
    std::string glslVersion    = "#version 130";      // passed to ImGui_ImplOpenGL3_Init
    int         msaaSamples    = 1;                   // GLFW_SAMPLES; >1 enables GL_MULTISAMPLE

    bool        vsync          = true;
    bool        startMaximized = false;

    const char *iniPath        = nullptr;             // io.IniFilename (nullptr disables .ini persistence)
    bool        navKeyboard    = true;
    bool        dpiScaleFonts  = true;                // v1.92 io.ConfigDpiScaleFonts

    bool        docking        = false;
    bool        viewports      = false;
};

class Application {
public:
    explicit Application(AppConfig config = {});
    virtual ~Application();

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&)                 = delete;
    Application& operator=(Application&&)      = delete;

    // create() -> loop -> destroy(). Returns 0 on clean exit, else the AppStatus int.
    [[nodiscard]] int run();

    [[nodiscard]] AppStatus create();   // re-create() destroys first
    void                     destroy() noexcept;
    [[nodiscard]] bool       created() const noexcept { return m_created; }
    void requestQuit() noexcept;

    [[nodiscard]] const AppConfig& config()       const noexcept { return m_config; }
    [[nodiscard]] GLFWwindow*      window()        const noexcept { return m_window; }
    [[nodiscard]] ImGuiContext*    imguiContext()  const noexcept { return m_imguiContext; }
    [[nodiscard]] Vec2i            framebufferSize() const noexcept;
    [[nodiscard]] float            deltaTime()     const noexcept { return m_deltaTime; }

protected:
    [[nodiscard]] virtual bool onInit() { return true; }   // false => UserInitFailed
    virtual void onGui() {}                                 // submit ImGui UI here (between NewFrame/Render)
    virtual void onShutdown() {}                            // before backends/context torn down
    virtual void onKey(int /*key*/, int /*scancode*/, int /*action*/, int /*mods*/) {}

private:
    [[nodiscard]] AppStatus initWindow();
    [[nodiscard]] AppStatus initImGui();
    void                     beginFrame();
    void                     endFrame();
    void                     cleanImGui() noexcept;
    void                     cleanWindow() noexcept;

    static void glfwErrorCallback(int code, const char *desc);
    static void keyCallbackTrampoline(GLFWwindow *win, int key, int scancode, int action, int mods);

    AppConfig     m_config;
    bool          m_created      = false;
    bool          m_running      = false;
    GLFWwindow   *m_window       = nullptr;
    ImGuiContext *m_imguiContext = nullptr;
    bool          m_glfwBackend  = false;   // ImGui_ImplGlfw_InitForOpenGL succeeded (shutdown gate)
    bool          m_gl3Backend   = false;   // ImGui_ImplOpenGL3_Init succeeded (shutdown gate)
    float         m_mainScale    = 1.0f;
    double        m_lastTime     = 0.0;
    float         m_deltaTime    = 0.0f;
};

}  // namespace imtool
