#pragma once

#include <string_view>

namespace imtool {

[[nodiscard]] std::string_view version_string() noexcept;
[[nodiscard]] unsigned version_major() noexcept;
[[nodiscard]] unsigned version_minor() noexcept;
[[nodiscard]] unsigned version_patch() noexcept;

}
