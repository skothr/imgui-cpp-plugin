#include <imtool/app/application.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>   // also declares glViewport/glClear/glClearColor/glEnable on this small surface

namespace imtool {

// toString(AppStatus) is now an inline function in application.hpp (header-only,
// so it is usable/testable without linking this GLFW/GL translation unit).

Application::Application(AppConfig config) : m_config(std::move(config)) {
    // Live-tunable app settings (Epic B): observer Settings bound into AppConfig's
    // runtime fields. AppConfig stays the immutable bootstrap bank; this is the
    // editable view. A subclass adds its own settings in its constructor.
    m_settings = std::make_unique<SettingGroup>("app", "Application");
    SettingMeta<Vec4f> clearMeta;
    clearMeta.hint = WidgetHint::Color;
    m_settings->add<Vec4f>("clear_color", "Clear Color", &m_config.clearColor, clearMeta);
    m_settings->add<bool>("vsync", "VSync", &m_config.vsync)
        .onChange([this](const bool &v) { if(m_window) { glfwSwapInterval(v ? 1 : 0); } });
}

std::expected<void, ToolkitError> Application::saveSession(const std::filesystem::path &path) const {
    return saveSettingsFile(*m_settings, path);
}
std::expected<void, ToolkitError> Application::loadSession(const std::filesystem::path &path) {
    return loadSettingsFile(*m_settings, path);
}
Application::~Application() { destroy(); }

void Application::glfwErrorCallback(int code, const char *desc) {
    log() << LogLevel::Error << "GLFW error " << code << ": " << (desc ? desc : "(null)");
    log().flush();
}

void Application::keyCallbackTrampoline(GLFWwindow *win, int key, int scancode, int action, int mods) {
    if(auto *app = static_cast<Application*>(glfwGetWindowUserPointer(win))) {
        app->onKey(key, scancode, action, mods);
    }
}

Vec2i Application::framebufferSize() const noexcept {
    if(!m_window) { return m_config.size; }
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_window, &w, &h);
    return Vec2i(w, h);
}

AppStatus Application::initWindow() {
    glfwSetErrorCallback(&glfwErrorCallback);
    if(!glfwInit()) {
        log() << LogLevel::Error << "Application: glfwInit failed"; log().flush();
        return AppStatus::GlfwInitFailed;
    }

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, m_config.glMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, m_config.glMinor);
#endif
    if(m_config.msaaSamples > 1) { glfwWindowHint(GLFW_SAMPLES, m_config.msaaSamples); }
    if(m_config.startMaximized)  { glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE); }

    m_mainScale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    if(m_mainScale <= 0.0f) { m_mainScale = 1.0f; }

    m_window = glfwCreateWindow(static_cast<int>(static_cast<float>(m_config.size.x) * m_mainScale),
                                static_cast<int>(static_cast<float>(m_config.size.y) * m_mainScale),
                                m_config.title.c_str(), nullptr, nullptr);
    if(!m_window) {
        log() << LogLevel::Error << "Application: glfwCreateWindow failed"; log().flush();
        return AppStatus::WindowFailed;   // create() calls cleanWindow() -> glfwTerminate() on the error path
    }
    glfwMakeContextCurrent(m_window);
    if(!glfwGetCurrentContext()) {
        log() << LogLevel::Error << "Application: no current GL context after MakeContextCurrent"; log().flush();
        return AppStatus::ContextFailed;
    }
    glfwSwapInterval(m_config.vsync ? 1 : 0);
    if(m_config.msaaSamples > 1) { glEnable(GL_MULTISAMPLE); }
    if(m_config.minSize.x > 0 && m_config.minSize.y > 0) {
        glfwSetWindowSizeLimits(m_window, m_config.minSize.x, m_config.minSize.y, GLFW_DONT_CARE, GLFW_DONT_CARE);
    }
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, &keyCallbackTrampoline);
    return AppStatus::Ok;
}

AppStatus Application::initImGui() {
    IMGUI_CHECKVERSION();
    m_imguiContext = ImGui::CreateContext();
    if(!m_imguiContext) {
        log() << LogLevel::Error << "Application: ImGui::CreateContext failed"; log().flush();
        return AppStatus::ImGuiInitFailed;
    }
    ImGui::SetCurrentContext(m_imguiContext);

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = m_config.iniPath;
    if(m_config.navKeyboard) { io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; }
    if(m_config.docking)     { io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; }
    if(m_config.viewports)   { io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; }
    io.ConfigDpiScaleFonts          = m_config.dpiScaleFonts;
    io.ConfigWindowsResizeFromEdges = true;

    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(m_mainScale);
    style.FontScaleDpi = m_mainScale;
    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding              = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    if(!ImGui_ImplGlfw_InitForOpenGL(m_window, /*install_callbacks=*/true)) {
        log() << LogLevel::Error << "Application: ImGui_ImplGlfw_InitForOpenGL failed"; log().flush();
        return AppStatus::BackendInitFailed;
    }
    m_glfwBackend = true;
    if(!ImGui_ImplOpenGL3_Init(m_config.glslVersion.c_str())) {
        log() << LogLevel::Error << "Application: ImGui_ImplOpenGL3_Init failed"; log().flush();
        return AppStatus::BackendInitFailed;   // cleanImGui shuts down only the GLFW backend, not GL3
    }
    m_gl3Backend = true;
    return AppStatus::Ok;
}

AppStatus Application::create() {
    if(m_created) { destroy(); }
    if(AppStatus s = initWindow(); s != AppStatus::Ok) { cleanWindow(); return s; }
    if(AppStatus s = initImGui();  s != AppStatus::Ok) { cleanImGui(); cleanWindow(); return s; }
    if(!onInit()) {
        log() << LogLevel::Error << "Application: onInit() returned false"; log().flush();
        cleanImGui(); cleanWindow();
        return AppStatus::UserInitFailed;
    }
    m_created = true;
    return AppStatus::Ok;
}

void Application::beginFrame() {
    const double now = glfwGetTime();
    m_deltaTime = (m_lastTime > 0.0) ? static_cast<float>(now - m_lastTime) : 0.0f;
    m_lastTime  = now;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Application::endFrame() {
    ImGui::Render();
    const Vec2i fb = framebufferSize();
    glViewport(0, 0, fb.x, fb.y);
    const Vec4f &c = m_config.clearColor;
    glClearColor(c.x * c.w, c.y * c.w, c.z * c.w, c.w);   // premultiplied (matches the skill template)
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow *backup = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup);
    }
    glfwSwapBuffers(m_window);
}

int Application::run() {
    if(const AppStatus s = create(); s != AppStatus::Ok) { return static_cast<int>(s); }
    m_running  = true;
    m_lastTime = glfwGetTime();
    while(m_running && !glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        if(glfwGetWindowAttrib(m_window, GLFW_ICONIFIED)) { ImGui_ImplGlfw_Sleep(10); continue; }
        beginFrame();
        onGui();
        endFrame();
    }
    destroy();
    return 0;
}

void Application::requestQuit() noexcept {
    m_running = false;
    if(m_window) { glfwPostEmptyEvent(); }   // avoid GLFW_NOT_INITIALIZED if called before create()/after destroy()
}

void Application::cleanImGui() noexcept {
    // Shut down each backend only if its init actually succeeded. On the
    // BackendInitFailed path (GLFW backend up, GL3 backend not), shutting down the
    // GL3 backend that was never initialized is a crash/UB.
    if(m_gl3Backend)  { ImGui_ImplOpenGL3_Shutdown(); m_gl3Backend  = false; }
    if(m_glfwBackend) { ImGui_ImplGlfw_Shutdown();    m_glfwBackend = false; }
    if(m_imguiContext) { ImGui::DestroyContext(m_imguiContext); m_imguiContext = nullptr; }
}

void Application::cleanWindow() noexcept {
    if(m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

void Application::destroy() noexcept {
    if(!m_created) { return; }
    onShutdown();
    cleanImGui();
    cleanWindow();
    m_created = false;
    m_running = false;
}

}  // namespace imtool
