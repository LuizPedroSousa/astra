file(GLOB_RECURSE TERRAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE TERRAIN_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

add_library(terrain STATIC ${TERRAIN_SRC} ${TERRAIN_HEADERS})

target_include_directories(terrain
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/astralix/modules/terrain>
)

target_link_libraries(
  terrain
  PUBLIC
  renderer
  shared::foundation
  m
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${TERRAIN_SRC} ${TERRAIN_HEADERS})
