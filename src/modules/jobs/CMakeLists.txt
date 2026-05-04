file(GLOB_RECURSE JOB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE JOB_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

list(FILTER JOB_SRC EXCLUDE REGEX ".*\\.test\\.cpp$")
list(FILTER JOB_HEADERS EXCLUDE REGEX ".*\\.test\\.hpp$")

add_library(jobs STATIC ${JOB_SRC} ${JOB_HEADERS})

target_include_directories(jobs
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/astralix/modules/jobs>
)

target_link_libraries(
  jobs
  PUBLIC
  shared::ecs
  shared::foundation
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${JOB_SRC} ${JOB_HEADERS})
