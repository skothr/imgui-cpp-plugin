#pragma once

#include <filesystem>
#include <string>
#include <vector>

#if defined(__linux__) || defined(__APPLE__)
  #include <pwd.h>
  #include <sys/types.h>
  #include <unistd.h>
#endif

namespace imtool {

namespace fs = std::filesystem;

[[nodiscard]] inline fs::path getHomeDir() {
#if defined(_WIN32)
    const char *home = std::getenv("USERPROFILE");
    return fs::path(home ? home : "C:");
#else
    const char *home = std::getenv("HOME");
    if(!home) {
        if(auto *pw = ::getpwuid(::getuid())) { home = pw->pw_dir; }
    }
    return fs::path(home ? home : "");
#endif
}

[[nodiscard]] inline fs::path getLocalStorageDir() {
    static const std::vector<fs::path> POSSIBILITIES = {
        "~/.local/share",
        "/usr/local/share",
    };
    static const fs::path DEFAULT = "/usr/local/share";
    static fs::path cached = "/";

    if(cached == "/") {
        const fs::path home = getHomeDir();
        for(const auto &p : POSSIBILITIES) {
            std::string s = p.string();
            if(!s.empty() && s[0] == '~') { s = home.string() + s.substr(1); }
            const fs::path candidate = s;
            if(fs::exists(candidate) && fs::is_directory(candidate)) {
                cached = candidate;
                break;
            }
        }
        if(cached == "/") { cached = DEFAULT; }
    }
    return cached;
}

}
