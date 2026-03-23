file(GLOB_RECURSE STREAMS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE STREAMS_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

list(FILTER STREAMS_SRC EXCLUDE REGEX "\\.test\\.cpp$")
list(FILTER STREAMS_SRC EXCLUDE REGEX "/adapters/(json|yaml|toml|xml)/.*\\.cpp$")

set(ASTRALIX_STREAMS_FORMATS "${ASTRALIX_STREAMS_FORMATS}" CACHE STRING
    "Serialization formats compiled into the streams library")

if("${ASTRALIX_STREAMS_FORMATS}" STREQUAL "")
  message(FATAL_ERROR
    "ASTRALIX_STREAMS_FORMATS must be explicitly defined. "
    "Example: -DASTRALIX_STREAMS_FORMATS=Json")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/serialization-formats.cmake)

add_library(streams STATIC ${STREAMS_SRC} ${STREAMS_HEADERS})
add_library(astralix_streams_serialization_deps INTERFACE)

target_include_directories(streams 
  PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/astralix/modules/streams>
)
target_link_libraries(
  streams PUBLIC
    engine
    project
    shared::allocators
    $<BUILD_INTERFACE:astralix_streams_serialization_deps>
    $<INSTALL_INTERFACE:astralix::streams_serialization_deps>)

astralix_streams_link_serialization_dependencies(astralix_streams_serialization_deps
  FORMATS ${ASTRALIX_STREAMS_FORMATS})

astralix_streams_enable_serialization_formats(streams
  SKIP_LINK_DEPENDENCIES
  FORMATS ${ASTRALIX_STREAMS_FORMATS})
