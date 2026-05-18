#include <imtool/common/logging.hpp>

#include <cstdio>
#include <iostream>

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

void Logger::clear() {
    std::lock_guard<std::mutex> lk(m_logLock);
    m_lines.clear();
    m_lineStream.str(std::string());
    m_lineStream.clear();
    m_updated = true;
}

void Logger::flush() {
    std::lock_guard<std::mutex> lk(m_logLock);
    if(!m_lineStream.str().empty()) {
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
}

void Logger::newline() {
    if(!m_lineStream.str().empty()) {
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
        if(!m_stickyLevel) { m_currentLevel = m_defaultLevel; }
    }
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
    std::lock_guard<std::mutex> lk(m_logLock);

    if(ImGui::Button("Clear##imtool-log")) {
        m_lines.clear();
        m_lineStream.str(std::string());
        m_lineStream.clear();
        m_updated = true;
    }
    ImGui::SameLine();
    const char* levelNames[] = { "None", "Error", "Warning", "Info", "Debug" };
    int currentLevel = static_cast<int>(m_printLevel);
    ImGui::SetNextItemWidth(120.0f);
    if(ImGui::Combo("level##imtool-log", &currentLevel, levelNames, IM_ARRAYSIZE(levelNames))) {
        m_printLevel = static_cast<LogLevel>(currentLevel);
    }

    ImGui::BeginChild("imtool-log-scroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    for(const auto &line : m_lines) {
        if(line.level > m_printLevel) { continue; }
        ImGui::PushStyleColor(ImGuiCol_Text, levelColor(line.level));
        ImGui::TextUnformatted(line.text.c_str());
        ImGui::PopStyleColor();
    }
    if(m_updated && m_scrollOnUpdate) {
        ImGui::SetScrollHereY(1.0f);
        m_scrollUpdate = true;
        m_updated = false;
    }
    ImGui::EndChild();
}

}
