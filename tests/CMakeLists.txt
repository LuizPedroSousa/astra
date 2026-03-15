cmake_minimum_required(VERSION 3.21)
project(astralix_tests CXX)

set(CMAKE_CXX_STANDARD 20)

set(VCPKG_INSTALLED_ROOT "${CMAKE_SOURCE_DIR}/../vcpkg_installed/x64-linux")

set(GTest_DIR "${VCPKG_INSTALLED_ROOT}/share/gtest")
find_package(GTest CONFIG REQUIRED)

set(SHADER_LANG_DIR "${CMAKE_SOURCE_DIR}/../src/modules/renderer/shader-lang")
set(AXSLC_DIR "${CMAKE_SOURCE_DIR}/../axslc")

file(GLOB_RECURSE SHADER_LANG_SRC
  "${SHADER_LANG_DIR}/*.cpp"
  "${SHADER_LANG_DIR}/emitters/*.cpp")
list(FILTER SHADER_LANG_SRC EXCLUDE REGEX "\\.test\\.cpp$")

file(GLOB_RECURSE TEST_SRC
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer/shader-lang/*.test.cpp"
  "${AXSLC_DIR}/*.test.cpp")

add_executable(astralix_tests
  main.cpp
  ${TEST_SRC}
  ${SHADER_LANG_SRC}
  "${AXSLC_DIR}/args.cpp")

target_include_directories(astralix_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer"
  "${AXSLC_DIR}")

target_link_libraries(astralix_tests PRIVATE
  GTest::gtest
  GTest::gtest_main)

include(GoogleTest)
enable_testing()
gtest_discover_tests(astralix_tests)
