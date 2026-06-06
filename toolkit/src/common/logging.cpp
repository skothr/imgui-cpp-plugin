#include <imtool/common/logging.hpp>

#include <cstdio>
#include <vector>

#include <imgui.h>

namespace imtool {

namespace {

[[nodiscard]] const char* levelLabel(LogLevel level) {
    switch(level) {
        case LogLevel::Error:   return "ERROR  ";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Info:    return "INFO   ";
        case LogLevel::Debug:   return "DEBUG  ";
        case LogLevel::None:    return "       ";
    }
    return "       ";
}

[[nodiscard]] ImVec4 levelColor(LogLevel level) {
    switch(level) {
        case LogLevel::Error:   return ImVec4(1.00f, 0.35f, 0.35f, 1.00f);
        case LogLevel::Warning: return ImVec4(1.00f, 0.85f, 0.20f, 1.00f);
        case LogLevel::Info:    return ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
        case LogLevel::Debug:   return ImVec4(0.55f, 0.85f, 1.00f, 1.00f);
        case LogLevel::None:    return ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    }
    return ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
}

}

Logger::Logger(bool terminal_echo, std::size_t max_lines)
    : m_terminalEcho(terminal_echo), m_maxLines(static_cast<int>(max_lines)) {}

Logger::~Logger() { flush(); }

void Logger::setPrintLevel(LogLevel level) {
    std::lock_guard<std::mutex> lk(m_logLock);
    m_printLevel = level;
}

void Logger::clear_locked() {
    m_lines.clear();
    m_lineStream.str(std::string());
    m_lineStream.clear();
    m_updated = true;
}

void Logger::clear() {
    std::lock_guard<std::mutex> lk(m_logLock);
    clear_locked();
}

std::size_t Logger::lineCount() const {
    std::lock_guard<std::mutex> lk(m_logLock);
    return m_lines.size();
}

// Commit the current line buffer to m_lines (with optional terminal echo and
// max-line trimming). Caller must hold m_logLock.
void Logger::emit_locked() {
    if(m_lineStream.str().empty()) { return; }
    const LogLevel level = (m_currentLevel == LogLevel::None) ? m_defaultLevel : m_currentLevel;
    const std::string text = m_lineStream.str();
    if(m_terminalEcho && level <= m_printLevel) {
        std::FILE* out = (level == LogLevel::Error || level == LogLevel::Warning) ? stderr : stdout;
        std::fprintf(out, "[%s] %s\n", levelLabel(level), text.c_str());
    }
    m_lines.push_back({level, text});
    m_lineStream.str(std::string());
    m_lineStream.clear();
    while(static_cast<int>(m_lines.size()) > m_maxLines) { m_lines.pop_front(); }
    m_updated = true;
}

void Logger::flush() {
    std::lock_guard<std::mutex> lk(m_logLock);
    emit_locked();
}

void Logger::newline_locked() {
    const bool had = !m_lineStream.str().empty();
    emit_locked();
    if(had && !m_stickyLevel) { m_currentLevel = m_defaultLevel; }
}

void Logger::newline() {
    std::lock_guard<std::mutex> lk(m_logLock);
    newline_locked();
}

void Logger::pushLevel(LogLevel level) {
    std::lock_guard<std::mutex> lk(m_logLock);
    m_levelStack.push(m_defaultLevel);
    m_defaultLevel = level;
}

void Logger::popLevel(std::size_t count) {
    std::lock_guard<std::mutex> lk(m_logLock);
    for(std::size_t i = 0; i < count && !m_levelStack.empty(); i++) {
        m_defaultLevel = m_levelStack.top();
        m_levelStack.pop();
    }
}

void Logger::draw() {
    // Snapshot under the lock, then render without it. Holding m_logLock across
    // the ImGui calls would deadlock if any of them (a nested widget, a draw
    // callback) logs — m_logLock is not recursive.
    std::vector<LogLine> snapshot;
    LogLevel printLevel = LogLevel::Warning;
    bool autoScroll = false;
    {
        std::lock_guard<std::mutex> lk(m_logLock);
        snapshot.assign(m_lines.begin(), m_lines.end());
        printLevel = m_printLevel;
        autoScroll = m_updated && m_scrollOnUpdate;
        m_updated = false;
    }

    if(ImGui::Button("Clear##imtool-log")) { clear(); }   // clear() re-acquires the lock
    ImGui::SameLine();
    const char* levelNames[] = { "None", "Error", "Warning", "Info", "Debug" };
    int currentLevel = static_cast<int>(printLevel);
    ImGui::SetNextItemWidth(120.0f);
    if(ImGui::Combo("level##imtool-log", &currentLevel, levelNames, IM_ARRAYSIZE(levelNames))) {
        setPrintLevel(static_cast<LogLevel>(currentLevel));   // locks internally
    }

    ImGui::BeginChild("imtool-log-scroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    for(const auto &line : snapshot) {
        if(line.level > printLevel) { continue; }
        ImGui::PushStyleColor(ImGuiCol_Text, levelColor(line.level));
        ImGui::TextUnformatted(line.text.c_str());
        ImGui::PopStyleColor();
    }
    if(autoScroll) { ImGui::SetScrollHereY(1.0f); }
    ImGui::EndChild();
}

}
