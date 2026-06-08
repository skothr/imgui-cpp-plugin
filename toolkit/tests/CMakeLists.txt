# imtool unit tests — built when -DIMTOOL_BUILD_TESTS=ON (see ../CMakeLists.txt,
# which calls enable_testing() then add_subdirectory(tests)).
#
# Each test is a standalone executable using the imtest.hpp assertion harness
# (no GoogleTest/Catch dependency, so the suite builds offline). Run with `ctest`.

set(IMTOOL_TEST_NAMES
  test_vector
  test_matrix
  test_rect
  test_geometry
  test_primitives
  test_logging
  test_imgui_ops
  test_version
  test_nodegraph
  test_keybindings
  test_nodeview
  test_integration
  test_app_status
  test_settings
  test_command
)

foreach(name ${IMTOOL_TEST_NAMES})
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE imtool::imtool)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ${name} COMMAND ${name})
endforeach()

# test_integration shares the compute-node fixture (toolkit/fixtures/calc_nodes.hpp)
# with the calc_graph example, so it validates the exact node types the example
# ships (DRY). The fixture lives under fixtures/ — owned by neither client.
target_include_directories(test_integration PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures)

# test_logging spins up worker threads to exercise the logger mutex under
# contention; a deadlock regression would hang, so cap it with a CTest timeout.
find_package(Threads REQUIRED)
target_link_libraries(test_logging PRIVATE Threads::Threads)
set_tests_properties(test_logging PROPERTIES TIMEOUT 30)
