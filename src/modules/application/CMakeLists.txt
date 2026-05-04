# src/headers
file(GLOB_RECURSE APPLICATION_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE APPLICATION_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

list(FILTER APPLICATION_SRC EXCLUDE REGEX ".*\\.test\\.cpp$")
list(FILTER APPLICATION_HEADER EXCLUDE REGEX ".*\\.test\\.hpp$")

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
  jobs
  engine window shared::foundation
)

if(ASTRA_RENDERER_HOT_RELOAD)
  target_compile_definitions(application PRIVATE
    ASTRA_RENDERER_HOT_RELOAD
    ASTRA_RENDER_PASSES_MODULE_PATH="${CMAKE_BINARY_DIR}/hotreload/librender_passes_live.so"
    ASTRA_RENDER_PASSES_SOURCE_DIR="${PROJECT_SOURCE_DIR}/src/modules/renderer"
    ASTRA_RENDER_PASSES_BUILD_DIR="${CMAKE_BINARY_DIR}"
  )
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${APPLICATION_SOURCES} ${APPLICATION_HEADERS})
