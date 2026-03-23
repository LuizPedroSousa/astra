cmake_minimum_required(VERSION 3.21)
project(axslc CXX)

set(CMAKE_CXX_STANDARD 20)

set(SHADER_LANG_DIR "${CMAKE_SOURCE_DIR}/../src/modules/renderer/shader-lang")
set(MODULES_DIR "${CMAKE_SOURCE_DIR}/../src/modules")
set(SHARED_DIR "${CMAKE_SOURCE_DIR}/../src/shared")
set(VCPKG_INSTALLED_ROOT "${CMAKE_SOURCE_DIR}/../vcpkg_installed/x64-linux")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include("${MODULES_DIR}/streams/serialization-formats.cmake")

file(GLOB_RECURSE SHADER_LANG_SRC
  "${SHADER_LANG_DIR}/*.cpp"
  "${SHADER_LANG_DIR}/emitters/*.cpp")
list(FILTER SHADER_LANG_SRC EXCLUDE REGEX "\\.test\\.cpp$")

set(SERIALIZATION_SRC
  "${MODULES_DIR}/streams/context-proxy.cpp"
  "${MODULES_DIR}/streams/serialization-context.cpp"
  "${SHARED_DIR}/foundation/exceptions/base-exception.cpp")

add_executable(axslc
  main.cpp
  args.cpp
  output.cpp
  ${SHADER_LANG_SRC}
  ${SERIALIZATION_SRC})

target_include_directories(axslc PRIVATE
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer"
  "${MODULES_DIR}"
  "${MODULES_DIR}/streams"
  "${SHARED_DIR}/allocators"
  "${SHARED_DIR}/foundation"
  "${SHARED_DIR}/ecs")

astralix_streams_enable_serialization_formats(axslc FORMATS Json)
