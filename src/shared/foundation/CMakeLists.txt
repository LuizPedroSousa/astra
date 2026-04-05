file(GLOB_RECURSE SHARED_FOUNDATION_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE SHARED_FOUNDATION_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

list(FILTER SHARED_FOUNDATION_SRC EXCLUDE REGEX ".*\\.test\\.cpp$")
list(FILTER SHARED_FOUNDATION_HEADERS EXCLUDE REGEX ".*\\.test\\.hpp$")

add_library(foundation STATIC ${SHARED_FOUNDATION_SRC}
                              ${SHARED_FOUNDATION_HEADERS})

add_library(shared::foundation ALIAS foundation)

target_include_directories(foundation
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/astralix/shared/foundation>
)

if(ASTRA_TRACE)
  target_include_directories(foundation
    PUBLIC
      $<BUILD_INTERFACE:${VCPKG_INSTALLED_ROOT}/include/tracy>
      $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(foundation PUBLIC ${TRACE_LIBS})
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${FOUNDATION_SOURCES} ${FOUNDATION_HEADERS})
