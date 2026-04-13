cmake_minimum_required(VERSION 3.21)
project(astralix_tests CXX)

set(CMAKE_CXX_STANDARD 20)

set(VCPKG_INSTALLED_ROOT "${CMAKE_SOURCE_DIR}/../vcpkg_installed/x64-linux")
set(glad_DIR "${VCPKG_INSTALLED_ROOT}/share/glad")
set(glslang_DIR "${VCPKG_INSTALLED_ROOT}/share/glslang")
set(unofficial-shaderc_DIR "${VCPKG_INSTALLED_ROOT}/share/unofficial-shaderc")
set(SPIRV-Tools_DIR "${VCPKG_INSTALLED_ROOT}/share/spirv-tools")
set(SPIRV-Tools-opt_DIR "${VCPKG_INSTALLED_ROOT}/share/spirv-tools-opt")

set(SHADER_LANG_DIR "${CMAKE_SOURCE_DIR}/../src/modules/renderer/shader-lang")
set(MODULES_DIR "${CMAKE_SOURCE_DIR}/../src/modules")
set(SHARED_DIR "${CMAKE_SOURCE_DIR}/../src/shared")
set(AXSLC_DIR "${CMAKE_SOURCE_DIR}/../axslc")
set(AXGEN_DIR "${CMAKE_SOURCE_DIR}/../axgen")
set(AXGEN_SRC_DIR "${AXGEN_DIR}/src")
set(FAKER_CXX_DIR "${CMAKE_SOURCE_DIR}/../external/faker-cxx")
set(GOOGLETEST_DIR "${FAKER_CXX_DIR}/externals/googletest")

include("${MODULES_DIR}/streams/serialization-formats.cmake")

set(ASTRALIX_TEST_SERIALIZATION_FORMATS Json Yaml Toml Xml)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("${FAKER_CXX_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/faker-cxx"
                 EXCLUDE_FROM_ALL)
add_subdirectory("${GOOGLETEST_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/googletest"
                 EXCLUDE_FROM_ALL)
find_package(glad CONFIG REQUIRED)
find_package(unofficial-shaderc CONFIG REQUIRED)

file(GLOB_RECURSE SHADER_LANG_SRC
  "${SHADER_LANG_DIR}/*.cpp"
  "${SHADER_LANG_DIR}/emitters/*.cpp")
list(FILTER SHADER_LANG_SRC EXCLUDE REGEX "\\.test\\.cpp$")

file(GLOB_RECURSE SHARED_ALLOCATORS_SRC
  "${SHARED_DIR}/allocators/*.cpp")
list(FILTER SHARED_ALLOCATORS_SRC EXCLUDE REGEX "\\.test\\.cpp$")

set(SERIALIZATION_SRC
  "${SHARED_DIR}/ecs/guid.cpp"
  "${MODULES_DIR}/streams/context-proxy.cpp"
  "${MODULES_DIR}/streams/serialization-context.cpp"
  "${MODULES_DIR}/streams/adapters/file/file-stream-reader.cpp"
  "${SHARED_DIR}/foundation/exceptions/base-exception.cpp")

file(GLOB_RECURSE TEST_SRC CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer/shader-lang/*.test.cpp"
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer/resources/*.test.cpp"
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer/systems/render-system/core/*.test.cpp"
  "${CMAKE_SOURCE_DIR}/../src/shared/allocators/*.test.cpp"
  "${CMAKE_SOURCE_DIR}/../src/shared/containers/*.test.cpp"
  "${CMAKE_SOURCE_DIR}/../src/modules/streams/**/*.test.cpp"
  "${AXSLC_DIR}/*.test.cpp"
  "${AXGEN_SRC_DIR}/*.test.cpp")

add_executable(astralix_tests
  main.cpp
  ${TEST_SRC}
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer/platform/OpenGL/opengl-executor.cpp"
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer/virtual-index-buffer.cpp"
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer/virtual-vertex-buffer.cpp"
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer/virtual-vertex-array.cpp"
  "${CMAKE_SOURCE_DIR}/stubs/renderer-stubs.cpp"
  ${SHADER_LANG_SRC}
  ${SHARED_ALLOCATORS_SRC}
  "${AXSLC_DIR}/args.cpp"
  "${AXSLC_DIR}/output.cpp"
  "${AXGEN_SRC_DIR}/args.cpp"
  "${AXGEN_SRC_DIR}/project-locator.cpp"
  "${AXGEN_SRC_DIR}/project-bootstrap.cpp"
  "${AXGEN_SRC_DIR}/project-serializer.cpp"
  "${AXGEN_SRC_DIR}/sync.cpp"
  ${SERIALIZATION_SRC})

target_include_directories(astralix_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer"
  "${CMAKE_SOURCE_DIR}/../src/shared"
  "${CMAKE_SOURCE_DIR}/../src/assets/.astralix/generated"
  "${MODULES_DIR}"
  "${MODULES_DIR}/project"
  "${MODULES_DIR}/audio"
  "${MODULES_DIR}/terrain"
  "${MODULES_DIR}/ui"
  "${MODULES_DIR}/window"
  "${MODULES_DIR}/streams"
  "${SHARED_DIR}/allocators"
  "${SHARED_DIR}/containers"
  "${SHARED_DIR}/events"
  "${SHARED_DIR}/foundation"
  "${SHARED_DIR}/ecs"
  "${VCPKG_INSTALLED_ROOT}/include"
  "${AXSLC_DIR}"
  "${AXGEN_SRC_DIR}")

target_link_libraries(astralix_tests PRIVATE
  faker-cxx
  gtest
  gtest_main
  glad::glad)

target_compile_definitions(astralix_tests PRIVATE
  ASTRALIX_ENGINE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/../src/assets"
  ASTRALIX_ENGINE_GENERATED_ROOT="${CMAKE_CURRENT_BINARY_DIR}/test-engine-assets"
  ASTRALIX_ASSETS_DIR="${CMAKE_CURRENT_BINARY_DIR}/share/astralix/assets")

astralix_streams_enable_serialization_formats(astralix_tests
  FORMATS ${ASTRALIX_TEST_SERIALIZATION_FORMATS})

add_executable(shader_glsl_snapshot
  tools/shader-glsl-corpus-snapshot.cpp
  ${SHADER_LANG_SRC}
  ${SHARED_ALLOCATORS_SRC}
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer/platform/Vulkan/shaderc-compiler.cpp"
  ${SERIALIZATION_SRC})

target_include_directories(shader_glsl_snapshot PRIVATE
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer"
  "${CMAKE_SOURCE_DIR}/../src/shared"
  "${CMAKE_SOURCE_DIR}/../src/assets/.astralix/generated"
  "${MODULES_DIR}"
  "${MODULES_DIR}/project"
  "${MODULES_DIR}/audio"
  "${MODULES_DIR}/terrain"
  "${MODULES_DIR}/ui"
  "${MODULES_DIR}/window"
  "${MODULES_DIR}/streams"
  "${SHARED_DIR}/allocators"
  "${SHARED_DIR}/containers"
  "${SHARED_DIR}/events"
  "${SHARED_DIR}/foundation"
  "${SHARED_DIR}/ecs"
  "${VCPKG_INSTALLED_ROOT}/include")

target_link_libraries(shader_glsl_snapshot PRIVATE
  unofficial::shaderc::shaderc)

target_compile_definitions(shader_glsl_snapshot PRIVATE
  ASTRALIX_ENGINE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/../src/assets"
  ASTRALIX_ENGINE_GENERATED_ROOT="${CMAKE_CURRENT_BINARY_DIR}/snapshot-engine-assets"
  ASTRALIX_ASSETS_DIR="${CMAKE_CURRENT_BINARY_DIR}/share/astralix/assets")

astralix_streams_enable_serialization_formats(shader_glsl_snapshot
  FORMATS ${ASTRALIX_TEST_SERIALIZATION_FORMATS})

include(GoogleTest)
enable_testing()
gtest_discover_tests(astralix_tests)
