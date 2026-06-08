#pragma once

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <sstream>
#include <stack>
#include <string>
#include <type_traits>

namespace imtool {

enum class LogLevel : int {
    None    = 0,
    Error   = 1,
    Warning = 2,
    Info    = 3,
    Debug   = 4,
};

class Logger {
public:
    Logger(bool terminal_echo = true, std::size_t max_lines = 4096);
    ~Logger();

    void setPrintLevel(LogLevel level);

    void draw();
    void clear();
    void flush();
    [[nodiscard]] std::size_t lineCount() const;

    template<typename T>
    Logger& operator<<(const T &arg);

    template<typename... Args>
    Logger& log(const std::string &fmt, Args&& ...args);
    template<typename... Args>
    Logger& log(LogLevel level, const std::string &fmt, Args&& ...args);

    void pushLevel(LogLevel level);
    void popLevel(std::size_t count = 1);
    void newline();

private:
    // Lock-free internals — the caller must already hold m_logLock. The public
    // flush()/newline()/clear() acquire the lock once and delegate here, and
    // operator<< (which holds the lock for the whole statement) calls
    // newline_locked() directly. This removes the prior re-entrant-lock paths.
    void emit_locked();
    void newline_locked();
    void clear_locked();

    struct LogLine {
        LogLevel    level = LogLevel::None;
        std::string text;
    };

    std::deque<LogLine> m_lines;
    std::ostringstream  m_lineStream;
    LogLevel m_currentLevel = LogLevel::None;
    LogLevel m_defaultLevel = LogLevel::None;
    LogLevel m_printLevel   = LogLevel::Warning;

    bool m_updated      = false;
    bool m_scrollUpdate = false;

    bool m_terminalEcho   = true;
    bool m_scrollOnUpdate = true;
    bool m_stickyLevel    = false;
    int  m_maxLines       = 4096;

    float m_lastHeight         = 0.0f;
    float m_lastSettingsHeight = 0.0f;
    float m_lastScroll         = 0.0f;
    float m_maxScroll          = 0.0f;

    std::stack<LogLevel> m_levelStack;
    mutable std::mutex   m_logLock;
};

template<typename T>
Logger& Logger::operator<<(const T &arg) {
    std::lock_guard<std::mutex> lk(m_logLock);

    if constexpr(std::is_same_v<std::decay_t<T>, LogLevel>) {
        // A level manipulator is ALWAYS honored — even when the current level is
        // filtered out — so a later `<< LogLevel::X` can lower the level and
        // un-latch the stream (the content filter must not gate this). Kept as the
        // first arm of one if-constexpr chain so the content `else` below is never
        // instantiated for a LogLevel arg (it isn't string-streamable).
        if(arg != m_currentLevel) {
            if(!m_lineStream.str().empty()) { newline_locked(); }
            m_currentLevel = arg;
        }
    } else {
        // Content token: dropped while the current level exceeds the print level.
        if(m_currentLevel > m_printLevel) { return *this; }
        if constexpr(std::is_convertible_v<T, std::string>) {
            std::string arg_str = std::string(arg);
            if(arg_str.find('\n') == std::string::npos) {
                m_lineStream << arg_str;
            } else {
                std::istringstream ss(arg_str);
                std::string token;
                while(std::getline(ss, token, '\n')) {
                    m_lineStream << token;
                    newline_locked();
                }
            }
        } else {
            m_lineStream << arg;
        }
    }
    return *this;
}

template<typename... Args>
Logger& Logger::log(const std::string &fmt, Args&& ...args) {
    if constexpr(sizeof...(args) > 0) {
        char line[4096];
        std::snprintf(line, 4096, fmt.c_str(), std::forward<Args>(args)...);
        (*this) << std::string(line);
    } else {
        (*this) << fmt;
    }
    return *this;
}

template<typename... Args>
Logger& Logger::log(LogLevel level, const std::string &fmt, Args&& ...args) {
    if constexpr(sizeof...(args) > 0) {
        char line[4096];
        std::snprintf(line, 4096, fmt.c_str(), std::forward<Args>(args)...);
        (*this) << level << std::string(line);
    } else {
        (*this) << level << fmt;
    }
    return *this;
}

[[nodiscard]] inline Logger& log() {
    static Logger instance;
    return instance;
}

}
