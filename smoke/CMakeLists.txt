cmake_minimum_required(VERSION 3.21)
project(imtool_smoke LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

# Enable CTest at the top level so `ctest --test-dir <build>` discovers the
# toolkit unit tests (registered by toolkit/tests when -DIMTOOL_BUILD_TESTS=ON).
enable_testing()

include(FetchContent)

# Dear ImGui — pinned, manually wrapped into an imgui::imgui target since
# upstream ships no CMakeLists.
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.7-docking
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
add_library(imgui::imgui ALIAS imgui)

# nlohmann/json — prefer an installed package (offline builds, and CI with the
# distro dev package); otherwise fetch the pinned release tarball over the network.
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../toolkit ${CMAKE_BINARY_DIR}/toolkit)

add_executable(imtool_smoke main.cpp)
target_link_libraries(imtool_smoke PRIVATE imtool::imtool)
