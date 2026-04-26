file(GLOB_RECURSE ENGINE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE ENGINE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

add_library(engine STATIC ${ENGINE_SRC} ${ENGINE_HEADERS})

include_directories(${CMAKE_SOURCE_DIR}/external)

target_include_directories(engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/astralix/modules/engine>
)

target_link_libraries(
  engine
  PUBLIC
  glad::glad
  window
  shared::ecs
  physics
  renderer
  ui
  streams)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ENGINE_SOURCES} ${ENGINE_HEADERS})
