file(GLOB_RECURSE UI_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE UI_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

list(FILTER UI_SRC EXCLUDE REGEX ".*\\.test\\.cpp$")
list(FILTER UI_HEADERS EXCLUDE REGEX ".*\\.test\\.hpp$")

add_library(ui STATIC ${UI_SRC} ${UI_HEADERS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../shared)
include_directories(${PROJECT_SOURCE_DIR}/external)

target_include_directories(ui
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/modules/renderer>
    $<INSTALL_INTERFACE:include/astralix/modules/renderer>
    $<INSTALL_INTERFACE:include/astralix/modules/ui>
  PRIVATE
    $<BUILD_INTERFACE:${ASTRALIX_ENGINE_GENERATED_DIR}>
)

target_link_libraries(
  ui
  PUBLIC
  shared::allocators
  shared::containers
  window
  shared::foundation
  shared::ecs)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${UI_SRC} ${UI_HEADERS})
