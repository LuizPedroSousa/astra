file(GLOB_RECURSE ENGINE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE ENGINE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

list(FILTER ENGINE_SRC EXCLUDE REGEX ".*\\.test\\.cpp$")
list(FILTER ENGINE_HEADERS EXCLUDE REGEX ".*\\.test\\.hpp$")

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
  jobs
  window
  shared::ecs
  physics
  terrain
  renderer
  ui
  streams
  PRIVATE
  audio)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ENGINE_SOURCES} ${ENGINE_HEADERS})
