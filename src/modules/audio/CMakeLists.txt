file(GLOB_RECURSE AUDIO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE AUDIO_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

list(FILTER AUDIO_SRC EXCLUDE REGEX ".*\\.test\\.cpp$")
list(FILTER AUDIO_HEADERS EXCLUDE REGEX ".*\\.test\\.hpp$")

add_library(audio STATIC ${AUDIO_SRC} ${AUDIO_HEADERS})

target_include_directories(audio
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/external/miniaudio>
    $<INSTALL_INTERFACE:include/astralix/modules/audio>
)

target_link_libraries(
  audio
  PUBLIC
  renderer
  shared::foundation
  pthread
  m
  dl
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${AUDIO_SRC} ${AUDIO_HEADERS})
