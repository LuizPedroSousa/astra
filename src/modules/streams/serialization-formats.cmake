include_guard(GLOBAL)

function(astralix_streams_link_serialization_dependencies target)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR
            "astralix_streams_link_serialization_dependencies: target '${target}' does not exist")
  endif()

  get_target_property(ASTRALIX_STREAMS_TARGET_TYPE "${target}" TYPE)
  set(ASTRALIX_STREAMS_LINK_SCOPE PUBLIC)
  if(ASTRALIX_STREAMS_TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
    set(ASTRALIX_STREAMS_LINK_SCOPE INTERFACE)
  endif()

  set(options)
  set(oneValueArgs)
  set(multiValueArgs FORMATS)
  cmake_parse_arguments(ASTRALIX_STREAMS
                        "${options}"
                        "${oneValueArgs}"
                        "${multiValueArgs}"
                        ${ARGN})

  if(NOT ASTRALIX_STREAMS_FORMATS)
    message(FATAL_ERROR
            "astralix_streams_link_serialization_dependencies: FORMATS is required")
  endif()

  list(REMOVE_DUPLICATES ASTRALIX_STREAMS_FORMATS)

  foreach(format IN LISTS ASTRALIX_STREAMS_FORMATS)
    if(format STREQUAL "Json")
      if(NOT TARGET JsonCpp::JsonCpp)
        if(DEFINED VCPKG_INSTALLED_ROOT AND
           EXISTS "${VCPKG_INSTALLED_ROOT}/share/jsoncpp")
          set(jsoncpp_DIR "${VCPKG_INSTALLED_ROOT}/share/jsoncpp")
        endif()

        find_package(jsoncpp CONFIG REQUIRED)
      endif()

      target_link_libraries(${target} ${ASTRALIX_STREAMS_LINK_SCOPE} JsonCpp::JsonCpp)
    elseif(format STREQUAL "Yaml")
      if(NOT TARGET yaml-cpp::yaml-cpp AND NOT TARGET yaml-cpp)
        if(DEFINED VCPKG_INSTALLED_ROOT AND
           EXISTS "${VCPKG_INSTALLED_ROOT}/share/yaml-cpp")
          set(yaml-cpp_DIR "${VCPKG_INSTALLED_ROOT}/share/yaml-cpp")
        endif()

        find_package(yaml-cpp CONFIG REQUIRED)
      endif()

      if(TARGET yaml-cpp::yaml-cpp)
        target_link_libraries(${target} ${ASTRALIX_STREAMS_LINK_SCOPE} yaml-cpp::yaml-cpp)
      else()
        target_link_libraries(${target} ${ASTRALIX_STREAMS_LINK_SCOPE} yaml-cpp)
      endif()
    elseif(format STREQUAL "Toml")
      if(NOT TARGET tomlplusplus::tomlplusplus)
        if(DEFINED VCPKG_INSTALLED_ROOT AND
           EXISTS "${VCPKG_INSTALLED_ROOT}/share/tomlplusplus")
          set(tomlplusplus_DIR "${VCPKG_INSTALLED_ROOT}/share/tomlplusplus")
        endif()

        find_package(tomlplusplus CONFIG REQUIRED)
      endif()

      target_link_libraries(${target} ${ASTRALIX_STREAMS_LINK_SCOPE} tomlplusplus::tomlplusplus)
    elseif(format STREQUAL "Xml")
      if(NOT TARGET pugixml::pugixml AND NOT TARGET pugixml)
        if(DEFINED VCPKG_INSTALLED_ROOT AND
           EXISTS "${VCPKG_INSTALLED_ROOT}/share/pugixml")
          set(pugixml_DIR "${VCPKG_INSTALLED_ROOT}/share/pugixml")
        endif()

        find_package(pugixml CONFIG REQUIRED)
      endif()

      if(TARGET pugixml::pugixml)
        target_link_libraries(${target} ${ASTRALIX_STREAMS_LINK_SCOPE} pugixml::pugixml)
      else()
        target_link_libraries(${target} ${ASTRALIX_STREAMS_LINK_SCOPE} pugixml)
      endif()
    else()
      message(FATAL_ERROR
              "astralix_streams_link_serialization_dependencies: unknown format '${format}'")
    endif()
  endforeach()
endfunction()

function(astralix_streams_enable_serialization_formats target)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR
            "astralix_streams_enable_serialization_formats: target '${target}' does not exist")
  endif()

  set(options SKIP_LINK_DEPENDENCIES)
  set(oneValueArgs)
  set(multiValueArgs FORMATS)
  cmake_parse_arguments(ASTRALIX_STREAMS
                        "${options}"
                        "${oneValueArgs}"
                        "${multiValueArgs}"
                        ${ARGN})

  if(NOT ASTRALIX_STREAMS_FORMATS)
    message(FATAL_ERROR
            "astralix_streams_enable_serialization_formats: FORMATS is required")
  endif()

  list(REMOVE_DUPLICATES ASTRALIX_STREAMS_FORMATS)

  foreach(format IN LISTS ASTRALIX_STREAMS_FORMATS)
    if(format STREQUAL "Json")
      target_sources(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/adapters/json/json-serialization-context.cpp")
      target_compile_definitions(${target} PRIVATE
        ASTRALIX_SERIALIZATION_ENABLE_JSON)
    elseif(format STREQUAL "Yaml")
      target_sources(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/adapters/yaml/yaml-serialization-context.cpp")
      target_compile_definitions(${target} PRIVATE
        ASTRALIX_SERIALIZATION_ENABLE_YAML)
    elseif(format STREQUAL "Toml")
      target_sources(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/adapters/toml/toml-serialization-context.cpp")
      target_compile_definitions(${target} PRIVATE
        ASTRALIX_SERIALIZATION_ENABLE_TOML)
    elseif(format STREQUAL "Xml")
      target_sources(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/adapters/xml/xml-serialization-context.cpp")
      target_compile_definitions(${target} PRIVATE
        ASTRALIX_SERIALIZATION_ENABLE_XML)
    else()
      message(FATAL_ERROR
              "astralix_streams_enable_serialization_formats: unknown format '${format}'")
    endif()
  endforeach()

  if(NOT ASTRALIX_STREAMS_SKIP_LINK_DEPENDENCIES)
    astralix_streams_link_serialization_dependencies(${target}
      FORMATS ${ASTRALIX_STREAMS_FORMATS})
  endif()
endfunction()
