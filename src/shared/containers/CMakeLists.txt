add_library(containers INTERFACE)

add_library(shared::containers ALIAS containers)

target_include_directories(containers
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include/astralix/shared>
)
