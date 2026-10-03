#include <imtool/common/version.hpp>

namespace imtool {

// Single source of truth for the toolkit version. release-please bumps the
// literal on the marked line below (Generic updater, x-release-please-version);
// the numeric accessors derive their fields from it, so there is one place to
// change and nothing can drift out of sync.
namespace {
constexpr std::string_view kVersion = "0.1.0"; // x-release-please-version

constexpr unsigned versionField(int which) {
    unsigned value = 0;
    int field = 0;
    for(char c : kVersion) {
        if(c == '.') { ++field; continue; }
        if(field == which && c >= '0' && c <= '9') { value = value * 10u + unsigned(c - '0'); }
    }
    return value;
}
}

std::string_view version_string() noexcept { return kVersion; }
unsigned version_major() noexcept { return versionField(0); }
unsigned version_minor() noexcept { return versionField(1); }
unsigned version_patch() noexcept { return versionField(2); }

}
