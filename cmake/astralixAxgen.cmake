include(CMakeParseArguments)

function(axgen_sync_shaders)
  set(options "")
  set(oneValueArgs TARGET MANIFEST)
  set(multiValueArgs "")
  cmake_parse_arguments(AXGEN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT AXGEN_TARGET)
    message(FATAL_ERROR "axgen_sync_shaders requires TARGET <name>")
  endif()

  if(NOT AXGEN_MANIFEST)
    message(FATAL_ERROR "axgen_sync_shaders requires MANIFEST <path>")
  endif()

  get_filename_component(_axgen_manifest "${AXGEN_MANIFEST}" ABSOLUTE
                         BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  get_filename_component(_axgen_project_root "${_axgen_manifest}" DIRECTORY)
  set(_axgen_generated_dir "${_axgen_project_root}/.astralix/generated")
  set(_axgen_executable "${PACKAGE_PREFIX_DIR}/bin/axgen")
  set(_axgen_target "${AXGEN_TARGET}_axgen_sync")

  add_custom_target("${_axgen_target}"
    COMMAND "${_axgen_executable}" sync-shaders --manifest "${_axgen_manifest}"
    WORKING_DIRECTORY "${_axgen_project_root}"
    COMMENT "Synchronizing Astralix shader bindings for ${AXGEN_TARGET}"
    VERBATIM)

  add_dependencies("${AXGEN_TARGET}" "${_axgen_target}")
  target_include_directories("${AXGEN_TARGET}" PRIVATE "${_axgen_generated_dir}")
endfunction()
