file(GLOB_RECURSE SHADER_LANG_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

list(FILTER SHADER_LANG_SRC EXCLUDE REGEX ".*\\.test\\.cpp$")

add_library(shader-lang STATIC ${SHADER_LANG_SRC})

target_include_directories(shader-lang
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  $<INSTALL_INTERFACE:include/astralix/modules/renderer>
)

target_link_libraries(shader-lang
  PUBLIC
    streams
    shared::foundation
    allocators)

target_compile_features(shader-lang PUBLIC cxx_std_20)
