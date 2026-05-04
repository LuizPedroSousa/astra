file(GLOB_RECURSE WINDOW_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE WINDOW_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

list(FILTER WINDOW_SRC EXCLUDE REGEX ".*\\.test\\.cpp$")
list(FILTER WINDOW_HEADERS EXCLUDE REGEX ".*\\.test\\.hpp$")

add_library(window STATIC ${WINDOW_SRC} ${WINDOW_HEADERS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../shared)
include_directories(${PROJECT_SOURCE_DIR}/external)

target_include_directories(window 
  PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/astralix/modules/window>
)

target_link_libraries(window PRIVATE glad::glad glfw shared::foundation engine)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${WINDOW_SOURCES} ${WINDOW_HEADERS})
