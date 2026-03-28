file(GLOB_RECURSE UI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE UI_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

list(FILTER UI_SRC EXCLUDE REGEX ".*\\.test\\.cpp$")
list(FILTER UI_HEADERS EXCLUDE REGEX ".*\\.test\\.hpp$")

add_library(ui STATIC ${UI_SRC} ${UI_HEADERS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../shared)
include_directories(${PROJECT_SOURCE_DIR}/external)

target_include_directories(ui
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/astralix/modules/ui>
  PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/modules/renderer>
    $<BUILD_INTERFACE:${ASTRALIX_ENGINE_GENERATED_DIR}>
)

target_link_libraries(
  ui
  PUBLIC
  window
  shared::foundation
  shared::ecs)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${UI_SRC} ${UI_HEADERS})
