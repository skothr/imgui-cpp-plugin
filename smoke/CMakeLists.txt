cmake_minimum_required(VERSION 3.21)
project(imtool_smoke LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

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

# nlohmann/json — single-header release tarball; native CMake support.
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../toolkit ${CMAKE_BINARY_DIR}/toolkit)

add_executable(imtool_smoke main.cpp)
target_link_libraries(imtool_smoke PRIVATE imtool::imtool)
