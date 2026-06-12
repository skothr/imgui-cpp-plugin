#include "imtest.hpp"

#include <cstdio>
#include <string_view>

#include <imtool/common/version.hpp>

using namespace imtool;

int main() {
    IMTEST_SUITE("version");

    // version.cpp derives the numeric fields by parsing kVersion (single source
    // of truth that release-please bumps). Assert the fields reconstruct the
    // string — release-bump-safe, unlike asserting a literal version.
    char buf[32];
    std::snprintf(buf, sizeof buf, "%u.%u.%u",
                  version_major(), version_minor(), version_patch());
    CHECK(std::string_view(buf) == version_string());

    // sanity: a real, non-empty version
    CHECK(!version_string().empty());

    return imtest::report();
}
