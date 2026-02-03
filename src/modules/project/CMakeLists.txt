file(GLOB_RECURSE PROJECT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE PROJECT_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

add_library(project STATIC ${PROJECT_SRC} ${PROJECT_HEADER})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../shared)
include_directories(${PROJECT_SOURCE_DIR}/external)

target_include_directories(project 
  PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/astralix/modules/project>
)

target_link_libraries(project shared::foundation streams renderer)
