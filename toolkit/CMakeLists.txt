cmake_minimum_required(VERSION 3.21)
project(imtool VERSION 0.1.0 LANGUAGES CXX)

if(NOT TARGET imgui::imgui)
  message(FATAL_ERROR
    "imtool requires an `imgui::imgui` target before `add_subdirectory(toolkit)`. "
    "Use `find_package(imgui CONFIG REQUIRED)` or add the Dear ImGui sources first.")
endif()

add_library(imtool STATIC
  src/version.cpp
)
add_library(imtool::imtool ALIAS imtool)

target_include_directories(imtool PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(imtool PUBLIC cxx_std_23)
target_link_libraries(imtool PUBLIC imgui::imgui)

if(TARGET nlohmann_json::nlohmann_json)
  target_link_libraries(imtool PUBLIC nlohmann_json::nlohmann_json)
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
