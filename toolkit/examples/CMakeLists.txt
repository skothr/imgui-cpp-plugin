# imtool example apps (Epic H). Built when -DIMTOOL_BUILD_EXAMPLES=ON, which also
# requires the Application layer: configure with -DIMTOOL_BUILD_APP=ON and provide
# an imgui::imgui built WITH the GLFW + OpenGL3 backends plus glfw + OpenGL — the
# same deps imtool_app links. The in-repo smoke harness wires these when configured
# with -DIMTOOL_SMOKE_WITH_APP=ON; a consumer gets them from the bootstrap scaffold.

if(NOT TARGET imtool::app)
  message(FATAL_ERROR
    "IMTOOL_BUILD_EXAMPLES=ON requires the Application layer (imtool::app). "
    "Configure with -DIMTOOL_BUILD_APP=ON and provide imgui-with-backends + glfw + OpenGL.")
endif()

# calc_graph — a runnable node-graph calculator (Application + NodeGraphDisplay +
# KeyBindingManager). See calc_graph/main.cpp; mirrored headlessly by
# toolkit/tests/test_integration.cpp.
add_executable(calc_graph calc_graph/main.cpp)
target_link_libraries(calc_graph PRIVATE imtool::app imgui::imgui)
target_compile_features(calc_graph PRIVATE cxx_std_23)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(calc_graph PRIVATE -Wall -Wextra -Wpedantic)
endif()
