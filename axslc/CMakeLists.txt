cmake_minimum_required(VERSION 3.21)
project(axslc CXX)

set(CMAKE_CXX_STANDARD 20)

set(SHADER_LANG_DIR "${CMAKE_SOURCE_DIR}/../src/modules/renderer/shader-lang")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

file(GLOB_RECURSE SHADER_LANG_SRC
  "${SHADER_LANG_DIR}/*.cpp"
  "${SHADER_LANG_DIR}/emitters/*.cpp")
list(FILTER SHADER_LANG_SRC EXCLUDE REGEX "\\.test\\.cpp$")

add_executable(axslc main.cpp args.cpp ${SHADER_LANG_SRC})

target_include_directories(axslc PRIVATE
  "${CMAKE_SOURCE_DIR}/../src/modules/renderer")
