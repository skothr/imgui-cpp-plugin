cmake_minimum_required(VERSION 3.21)
project(imtool_smoke LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

# Enable CTest at the top level so `ctest --test-dir <build>` discovers the
# toolkit unit tests (registered by toolkit/tests when -DIMTOOL_BUILD_TESTS=ON).
enable_testing()

# Opt-in: also build the Application layer (imtool_app) + the examples, exercising
# the exact GLFW + OpenGL3 + ImGui-backends path a consumer (and agent-nexus) ships
# via the bootstrap scaffold. OFF keeps the smoke build backend-light (no GLFW/GL),
# matching the default CI job; the dedicated app-path CI job sets it ON.
option(IMTOOL_SMOKE_WITH_APP "Build the imtool Application layer + examples in the smoke tree (needs GLFW + OpenGL)" OFF)

include(FetchContent)

# Dear ImGui — pinned, manually wrapped into an imgui::imgui target since upstream
# ships no CMakeLists. With IMTOOL_SMOKE_WITH_APP the GLFW + OpenGL3 backend sources
# are compiled in too (imtool_app + the examples link against them).
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.7-docking
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMTOOL_IMGUI_SOURCES
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
if(IMTOOL_SMOKE_WITH_APP)
  list(APPEND IMTOOL_IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  )
endif()

add_library(imgui STATIC ${IMTOOL_IMGUI_SOURCES})
# SYSTEM: shield consumer warning flags from ImGui's headers (matches toolkit/
# CMakeLists.txt and the scaffold CMake template, which both mark imgui SYSTEM).
target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR})
add_library(imgui::imgui ALIAS imgui)

if(IMTOOL_SMOKE_WITH_APP)
  target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR}/backends)
  # GLFW — installed package preferred (offline / CI with libglfw3-dev), else fetched.
  find_package(glfw3 CONFIG QUIET)
  if(NOT TARGET glfw)
    FetchContent_Declare(glfw
      GIT_REPOSITORY https://github.com/glfw/glfw.git
      GIT_TAG 3.4
      GIT_SHALLOW TRUE)
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
  endif()
  find_package(OpenGL REQUIRED)
  target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
  # Turn on the Application layer + examples now that the backends/glfw/GL exist.
  set(IMTOOL_BUILD_APP      ON CACHE BOOL "" FORCE)
  set(IMTOOL_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
endif()

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
