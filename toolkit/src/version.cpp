#include <imtool/common/version.hpp>

namespace imtool {

std::string_view version_string() noexcept { return "0.1.0"; }
unsigned version_major() noexcept { return 0; }
unsigned version_minor() noexcept { return 1; }
unsigned version_patch() noexcept { return 0; }

}
