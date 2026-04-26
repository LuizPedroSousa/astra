# src/headers
file(GLOB_RECURSE APPLICATION_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE APPLICATION_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

add_library(application STATIC ${APPLICATION_SRC} ${APPLICATION_HEADER})

include_directories(${CMAKE_SOURCE_DIR}/external)

target_include_directories(application
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/astralix/modules/application>
)

target_link_libraries(
  application
  PUBLIC
  glad::glad
  project
  engine window shared::foundation
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${APPLICATION_SOURCES} ${APPLICATION_HEADERS})
