#include "imtest.hpp"

#include <cstring>

#include <imtool/app/application.hpp>

// MAIN-380: pin the AppStatus -> exit-code mapping. Application::run() returns
// static_cast<int>(status), so Ok must be 0 and each error a distinct nonzero
// code; a future edit that collides two enumerators or reorders them would change
// process exit codes silently. toString() is the inline header function (no GLFW
// link needed), so this builds in the backend-light test path.

using namespace imtool;

int main() {
    IMTEST_SUITE("app_status");

    // Documented contract: Ok == 0, errors are distinct, contiguous 1..7.
    const AppStatus all[] = {
        AppStatus::Ok, AppStatus::AlreadyCreated, AppStatus::GlfwInitFailed,
        AppStatus::WindowFailed, AppStatus::ContextFailed, AppStatus::ImGuiInitFailed,
        AppStatus::BackendInitFailed, AppStatus::UserInitFailed,
    };
    CHECK(static_cast<int>(AppStatus::Ok) == 0);
    for(int i = 0; i < 8; i++) { CHECK(static_cast<int>(all[i]) == i); }   // distinct + contiguous

    // toString covers every enumerator (non-empty, never the "Unknown" fallthrough).
    for(const AppStatus s : all) {
        const char *str = toString(s);
        CHECK(str != nullptr && str[0] != '\0');
        CHECK(std::strcmp(str, "Unknown") != 0);
    }
    CHECK(std::strcmp(toString(AppStatus::Ok), "Ok") == 0);
    CHECK(std::strcmp(toString(static_cast<AppStatus>(99)), "Unknown") == 0);   // out-of-range -> fallthrough

    return imtest::report();
}
