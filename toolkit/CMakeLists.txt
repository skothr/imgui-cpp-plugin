cmake_minimum_required(VERSION 3.21)
project(imtool VERSION 0.1.0 LANGUAGES CXX) # x-release-please-version

if(NOT TARGET imgui::imgui)
  message(FATAL_ERROR
    "imtool requires an `imgui::imgui` target before `add_subdirectory(toolkit)`. "
    "Use `find_package(imgui CONFIG REQUIRED)` or add the Dear ImGui sources first.")
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
  message(FATAL_ERROR
    "imtool requires an `nlohmann_json::nlohmann_json` target before `add_subdirectory(toolkit)`. "
    "Use `find_package(nlohmann_json CONFIG REQUIRED)` or add the sources first.")
endif()

add_library(imtool STATIC
  src/version.cpp
  src/common/logging.cpp
  src/node/node_graph.cpp
  src/node/node_graph_display.cpp
  src/input/key_binding.cpp
)
add_library(imtool::imtool ALIAS imtool)

# SYSTEM so a consumer's strict flags (-Wall -Wextra -Wpedantic) don't fire on
# the toolkit headers — e.g. the named-member union anonymous structs in
# vector.hpp are a deliberate GNU extension. Same treatment the scaffold gives
# the vendored ImGui headers. The toolkit's own build is warning-checked separately.
target_include_directories(imtool SYSTEM PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(imtool PUBLIC cxx_std_23)
target_link_libraries(imtool PUBLIC
  imgui::imgui
  nlohmann_json::nlohmann_json
)

# ---------------------------------------------------------------------------
# imtool_app — the Application layer. Pulls in GLFW + OpenGL + the ImGui
# GLFW/OpenGL3 backends, so it is opt-in (OFF by default) to keep the core
# `imtool` library backend-light (the smoke build and a consumer that only wants
# the common types / nodegraph data model need not link GLFW/GL). A consumer
# that wants Application (e.g. the bootstrap scaffold) sets -DIMTOOL_BUILD_APP=ON
# and must provide an `imgui::imgui` target built WITH the GLFW + OpenGL3 backends
# plus a `glfw` target and OpenGL.
# ---------------------------------------------------------------------------
option(IMTOOL_BUILD_APP "Build the imtool Application layer (needs GLFW + OpenGL + imgui backends)" OFF)
if(IMTOOL_BUILD_APP)
  add_library(imtool_app STATIC src/app/application.cpp)
  add_library(imtool::app ALIAS imtool_app)
  target_link_libraries(imtool_app PUBLIC imtool imgui::imgui)
  if(NOT TARGET glfw)
    find_package(glfw3 QUIET)
  endif()
  if(TARGET glfw)
    target_link_libraries(imtool_app PUBLIC glfw)
  else()
    # Fail fast at configure time (symmetric with the OpenGL check below) rather
    # than letting the build reach a cryptic undefined-reference to glfwInit etc.
    message(FATAL_ERROR "IMTOOL_BUILD_APP=ON requires GLFW (no `glfw` target found). Install libglfw3-dev or provide a `glfw` target.")
  endif()
  find_package(OpenGL QUIET)
  if(TARGET OpenGL::GL)
    target_link_libraries(imtool_app PUBLIC OpenGL::GL)
  else()
    # application.cpp calls glViewport/glClear/glClearColor/glEnable — without GL,
    # main would fail at link with a cryptic undefined-reference. Fail here instead.
    message(FATAL_ERROR "IMTOOL_BUILD_APP=ON requires OpenGL (OpenGL::GL not found). Install an OpenGL dev package or provide the target.")
  endif()
endif()

option(IMTOOL_BUILD_EXAMPLES "Build imtool example apps (Epic H)" OFF)
if(IMTOOL_BUILD_EXAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt)
  add_subdirectory(examples)
endif()

option(IMTOOL_BUILD_TESTS "Build imtool unit tests" OFF)
if(IMTOOL_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
  enable_testing()
  add_subdirectory(tests)
endif()
