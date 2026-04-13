cmake_minimum_required(VERSION 3.11)
project(astralix VERSION 0.1.0 LANGUAGES CXX)

if(POLICY CMP0079)
  cmake_policy(SET CMP0079 NEW)
endif()

option(ASTRA_EDITOR "Astra internal Layer Editor" OFF)
option(ASTRA_EDITOR_USOCKET "Astra forward serialization with USocket for external Editor" true)
option(ASTRA_ENABLE_TESTS "Enable unit tests" false)
option(ASTRA_TRACE "Enable tracing" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fopenmp")
add_compile_options(-fPIC)


add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
add_definitions(-DPX_PHYSX_STATIC_LIB)
add_definitions(-DENABLE_LOGS)
add_definitions(-DLOG_TO_CONSOLE)

if(ASTRA_EDITOR)
  add_definitions(-DASTRA_EDITOR)
  add_definitions(-DASTRA_EDITOR_USOCKET)
endif()

set(VCPKG_INSTALLED_ROOT "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-linux")

if(ASTRA_TRACE)
  add_definitions(-DASTRA_TRACE)
  set(TRACE_LIBS "${VCPKG_INSTALLED_ROOT}/lib/libTracyClient.a")
endif()

find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)
find_package(assimp REQUIRED)

set(freetype_DIR "${VCPKG_INSTALLED_ROOT}/share/freetype")
set(glfw3_DIR "${VCPKG_INSTALLED_ROOT}/share/glfw3")
set(uwebsockets_DIR "${VCPKG_INSTALLED_ROOT}/share/unofficial-uwebsockets")
set(glad_DIR "${VCPKG_INSTALLED_ROOT}/share/glad")
set(glm_DIR "${VCPKG_INSTALLED_ROOT}/share/glm")
set(glslang_DIR "${VCPKG_INSTALLED_ROOT}/share/glslang")
set(SPIRV-Tools_DIR "${VCPKG_INSTALLED_ROOT}/share/spirv-tools")
set(SPIRV-Tools-opt_DIR "${VCPKG_INSTALLED_ROOT}/share/spirv-tools-opt")
set(unofficial-shaderc_DIR "${VCPKG_INSTALLED_ROOT}/share/unofficial-shaderc")

find_package(unofficial-shaderc CONFIG REQUIRED)

find_path(USOCKET_LIB_A "${VCPKG_INSTALLED_ROOT}/lib/libuSockets.a")
find_path(GLAD_LIB_A "${VCPKG_INSTALLED_ROOT}/lib/libglad.a")
find_package(glad CONFIG REQUIRED)

find_package(OpenMP REQUIRED)

find_package(ZLIB REQUIRED)


find_package(glm CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)

find_package(PkgConfig)

find_package(PkgConfig REQUIRED)

find_package(glfw3 CONFIG REQUIRED)

find_package(X11 REQUIRED)

if(ASTRA_ENABLE_TESTS)
  include(${CMAKE_SOURCE_DIR}/src/modules/streams/serialization-formats.cmake)
  set(GTest_DIR "${VCPKG_INSTALLED_ROOT}/share/gtest")

  enable_testing()

  find_package(GTest CONFIG REQUIRED)
  file(GLOB_RECURSE UI_DOCUMENT_SUPPORT_SRC CONFIGURE_DEPENDS
       ${CMAKE_SOURCE_DIR}/src/modules/ui/document/*.cpp)
  file(GLOB_RECURSE UI_IMMEDIATE_SUPPORT_SRC CONFIGURE_DEPENDS
       ${CMAKE_SOURCE_DIR}/src/modules/ui/immediate/*.cpp)
  file(GLOB_RECURSE UI_LAYOUT_SUPPORT_SRC CONFIGURE_DEPENDS
       ${CMAKE_SOURCE_DIR}/src/modules/ui/layout/*.cpp)
  list(
    FILTER UI_LAYOUT_SUPPORT_SRC EXCLUDE
    REGEX ".*/widgets/content/render-image-view\\.cpp$")

  file(GLOB_RECURSE ARCHETYPE_ECS_TEST_SRC
       ${CMAKE_SOURCE_DIR}/src/shared/foundation/*.test.cpp
       ${CMAKE_SOURCE_DIR}/src/shared/containers/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/renderer/entities/serializers/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/physics/systems/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/ui/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/renderer/systems/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/renderer/systems/camera-system/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/renderer/systems/render-system/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/renderer/systems/transform-system/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/shared/ecs/*.test.cpp)
  file(GLOB_RECURSE HEADER_ONLY_TEST_SRC
       ${CMAKE_SOURCE_DIR}/src/shared/containers/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/renderer/entities/serializers/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/physics/systems/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/ui/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/renderer/systems/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/renderer/systems/camera-system/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/renderer/systems/render-system/*.test.hpp
       ${CMAKE_SOURCE_DIR}/src/modules/renderer/systems/transform-system/*.test.hpp)
  set(TEST_SUPPORT_SRC
      ${CMAKE_SOURCE_DIR}/src/shared/ecs/guid.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/streams/context-proxy.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/streams/serialization-context.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/streams/serializer.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/streams/adapters/file/file-stream-reader.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/streams/adapters/file/file-stream-writer.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/streams/adapters/json/json-serialization-context.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/streams/adapters/yaml/yaml-serialization-context.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/streams/adapters/toml/toml-serialization-context.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/streams/adapters/xml/xml-serialization-context.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/project/managers/project-manager.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/project/project.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/renderer/entities/derived-override.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/renderer/entities/scene.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/renderer/entities/serializers/axmesh-serializer.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/renderer/entities/serializers/derived-override-serializer.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/renderer/entities/serializers/scene-serializer.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/renderer/managers/scene-manager.cpp
      ${CMAKE_SOURCE_DIR}/src/modules/window/clipboard.cpp
      ${CMAKE_SOURCE_DIR}/tests/project-serializer-stub.cpp
      ${CMAKE_SOURCE_DIR}/tests/model-stub.cpp
      ${CMAKE_SOURCE_DIR}/tests/ui-layout-renderer-stub.cpp
      ${UI_DOCUMENT_SUPPORT_SRC}
      ${UI_IMMEDIATE_SUPPORT_SRC}
      ${UI_LAYOUT_SUPPORT_SRC})
  set_source_files_properties(
    ${HEADER_ONLY_TEST_SRC}
    PROPERTIES LANGUAGE CXX HEADER_FILE_ONLY OFF COMPILE_FLAGS "-x c++")

  add_executable(archetype_ecs_tests tests/main.cpp ${ARCHETYPE_ECS_TEST_SRC}
                                     ${TEST_SUPPORT_SRC})
  target_include_directories(
    archetype_ecs_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/shared/ecs
                                ${CMAKE_SOURCE_DIR}/src/shared/events
                                ${CMAKE_SOURCE_DIR}/src/shared/allocators
                                ${CMAKE_SOURCE_DIR}/src/shared/foundation
                                ${CMAKE_SOURCE_DIR}/src/modules/editor
                                ${CMAKE_SOURCE_DIR}/src/modules/streams
                                ${CMAKE_SOURCE_DIR}/src/modules/renderer
                                ${CMAKE_SOURCE_DIR}/src/modules/ui
                                ${CMAKE_SOURCE_DIR}/src/modules/physics
                                ${CMAKE_SOURCE_DIR}/src/modules/audio
                                ${CMAKE_SOURCE_DIR}/src/modules/project
                                ${CMAKE_SOURCE_DIR}/src/modules/window)
endif()

include_directories(
  ${OPENGL_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/external/PhysX/physx/include)

link_directories(external/PhysX/physx/bin/linux.x86_64/release)

add_library(stb_image STATIC "external/stb_image/stb_image.cpp")

target_include_directories(stb_image
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/external/stb_image>
)

# add_subdirectory(${PROJECT_SOURCE_DIR}/external/imgui/impl)

set(LIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external
                    ${VCPKG_INSTALLED_ROOT}/include)

include_directories(${LIB_INCLUDE_DIR})
link_directories(${VCPKG_INSTALLED_ROOT}/lib)

set(ASTRALIX_ASSETS_DIR_VALUE
    "${CMAKE_INSTALL_PREFIX}/share/astralix/assets"
)
set(ASTRALIX_ENGINE_ASSETS_DIR
    "${CMAKE_SOURCE_DIR}/src/assets"
)
set(ASTRALIX_ENGINE_GENERATED_ROOT
    "${ASTRALIX_ENGINE_ASSETS_DIR}"
)
set(ASTRALIX_ENGINE_GENERATED_DIR
    "${ASTRALIX_ENGINE_GENERATED_ROOT}/.astralix/generated"
)

add_subdirectory(src/shared/foundation)
add_subdirectory(src/shared/allocators)
add_subdirectory(src/shared/containers)

if(NOT ASTRA_ENABLE_TESTS)
  add_subdirectory(src/shared/events)
  add_subdirectory(src/shared/ecs)
  add_subdirectory(src/modules/streams)
  add_subdirectory(src/modules/renderer/shader-lang)
  add_subdirectory(src/modules/project)
  add_subdirectory(src/modules/window)
  add_subdirectory(src/modules/renderer)
  add_subdirectory(src/modules/ui)
  add_subdirectory(src/modules/physics)
  add_subdirectory(src/modules/audio)
  add_subdirectory(src/modules/engine)
  add_subdirectory(src/modules/application)
endif()

if(NOT ASTRA_ENABLE_TESTS)
  add_subdirectory(src/modules/editor)
endif()

# if(OpenMP_CXX_FOUND)
#   set_target_properties(astralix PROPERTIES COMPILE_FLAGS "${OpenMP_CXX_FLAGS}"
#                                        LINK_FLAGS "${OpenMP_CXX_FLAGS}")
# endif()

if(ASTRA_ENABLE_TESTS)
  target_link_libraries(
    archetype_ecs_tests PRIVATE shared::foundation glm::glm GTest::gtest
                                GTest::gtest_main GTest::gmock glfw
                                shared::containers
                                GTest::gmock_main)
  target_include_directories(
    archetype_ecs_tests PRIVATE ${ASTRALIX_ENGINE_GENERATED_DIR})
  astralix_streams_enable_serialization_formats(
    archetype_ecs_tests
    FORMATS Json Yaml Toml Xml)

  include(GoogleTest)
  gtest_discover_tests(archetype_ecs_tests)
endif()

if(NOT ASTRA_ENABLE_TESTS)
  foreach(_astralix_target IN ITEMS
      foundation
      allocators
      containers
      events
      ecs
      streams
      project
      window
      shader-lang
      renderer
      ui
      physics
      audio
      engine
      application
      stb_image
  )
    if(TARGET "${_astralix_target}" AND NOT TARGET "astralix::${_astralix_target}")
      add_library("astralix::${_astralix_target}" ALIAS "${_astralix_target}")
    endif()
  endforeach()

  if(TARGET editor AND NOT TARGET astralix::editor)
    add_library(astralix::editor ALIAS editor)
  endif()
endif()

# set(ASTRA_LIBS 
#     ${EDITOR}
#     application
#     ZLIB::ZLIB
#     "${VCPKG_INSTALLED_ROOT}/lib/libglad.a"
#     ${TRACE_LIBS}
#     "${VCPKG_INSTALLED_ROOT}/lib/libuSockets.a"
#     ${PROJECT_SOURCE_DIR}/external/PhysX/physx/bin/linux.x86_64/release/libPhysXFoundation_static_64.a
#     OpenMP::OpenMP_CXX
# )
#
if(NOT ASTRA_ENABLE_TESTS)
  add_library(astra INTERFACE)
  add_library(astralix::astra ALIAS astra)

  set(ASTRA_CORE_LIBS
      astralix::foundation
      astralix::allocators
      astralix::events
      astralix::ecs
      astralix::streams
      astralix::project
      astralix::window
      astralix::shader-lang
      astralix::renderer
      astralix::ui
      astralix::physics
      astralix::audio
      astralix::engine
      astralix::application
      astralix::stb_image)
  list(JOIN ASTRA_CORE_LIBS "," ASTRALIX_CORE_LIBS_RESCAN)

  target_include_directories(astra INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/shared/foundation>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/shared/allocators>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/shared/events>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/shared/ecs>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/modules/streams>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/modules/project>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/modules/window>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/modules/renderer>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/modules/ui>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/modules/physics>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/modules/audio>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/modules/engine>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/modules/application>
    $<BUILD_INTERFACE:${ASTRALIX_ENGINE_GENERATED_DIR}>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/astralix/shared/foundation>
    $<INSTALL_INTERFACE:include/astralix/shared/allocators>
    $<INSTALL_INTERFACE:include/astralix/shared/events>
    $<INSTALL_INTERFACE:include/astralix/shared/ecs>
    $<INSTALL_INTERFACE:include/astralix/modules/streams>
    $<INSTALL_INTERFACE:include/astralix/modules/project>
    $<INSTALL_INTERFACE:include/astralix/modules/window>
    $<INSTALL_INTERFACE:include/astralix/modules/renderer>
    $<INSTALL_INTERFACE:include/astralix/modules/ui>
    $<INSTALL_INTERFACE:include/astralix/modules/physics>
    $<INSTALL_INTERFACE:include/astralix/modules/audio>
    $<INSTALL_INTERFACE:include/astralix/modules/engine>
    $<INSTALL_INTERFACE:include/astralix/modules/application>
    $<INSTALL_INTERFACE:include/astralix/assets/.astralix/generated>
  )

  target_link_libraries(astra INTERFACE
      "$<LINK_GROUP:RESCAN,${ASTRALIX_CORE_LIBS_RESCAN}>"
      ZLIB::ZLIB
      OpenMP::OpenMP_CXX
      glfw
      glad::glad
      Vulkan::Vulkan
      glm::glm
      ${TRACE_LIBS}
      "${VCPKG_INSTALLED_ROOT}/lib/libuSockets.a"
      ${PROJECT_SOURCE_DIR}/external/PhysX/physx/bin/linux.x86_64/release/libPhysXFoundation_static_64.a
      # PhysX — better to link them in physics module, but if needed:
      # "${PHYSX_BIN}/libPhysX_static_64.a"
      # "${PHYSX_BIN}/libPhysXCommon_static_64.a"
      # "${PHYSX_BIN}/libPhysXFoundation_static_64.a"
  )

  target_compile_options(astra INTERFACE -fopenmp -fPIC)
  target_link_libraries(astra INTERFACE OpenMP::OpenMP_CXX)
  target_compile_features(astra INTERFACE cxx_std_20)

  target_compile_definitions(astra INTERFACE GLM_ENABLE_EXPERIMENTAL)
  target_compile_definitions(astra INTERFACE PX_PHYSX_STATIC_LIB)
  target_compile_definitions(astra INTERFACE ENABLE_LOGS)
  target_compile_definitions(astra INTERFACE LOG_TO_CONSOLE)

  target_compile_definitions(astra INTERFACE
    ASTRALIX_ASSETS_DIR=${ASTRALIX_ASSETS_DIR_VALUE}
  )

  foreach(tgt
      foundation
      allocators
      events
      ecs
      streams
      project
      window
      renderer
      ui
      physics
      audio
      engine
      application
  )
      target_compile_definitions(${tgt}
          PUBLIC ASTRALIX_ASSETS_DIR="${ASTRALIX_ASSETS_DIR_VALUE}"
      )
  endforeach()

  if(TARGET editor)
    target_link_libraries(editor PUBLIC astra)
    set_property(TARGET editor PROPERTY INTERFACE_LINK_LIBRARIES astra)
  endif()
endif()

if(NOT ASTRA_ENABLE_TESTS)
  add_subdirectory(axgen)
endif()

if(NOT ASTRA_ENABLE_TESTS)
  install(TARGETS astra
          foundation
          allocators
          containers
          events
          ecs
          streams
          project
          window
          shader-lang
          renderer
          ui
          physics
          audio
          engine
          application
          editor
          stb_image
          EXPORT astralixTargets
          ARCHIVE DESTINATION lib
          LIBRARY DESTINATION lib
          RUNTIME DESTINATION bin
          INCLUDES DESTINATION include
  )

  install(CODE [[
    file(REMOVE_RECURSE
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/include/astralix/modules/ui/ui"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/include/astralix/modules/renderer/ui"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/include/astralix/modules/renderer/systems/ui-system")
    file(REMOVE
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/include/astralix/modules/renderer/components/ui.hpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/include/astralix/modules/renderer/systems/ui-system.hpp")
  ]])

  install(DIRECTORY src/ DESTINATION include/astralix
          FILES_MATCHING
          PATTERN "*.h"
          PATTERN "*.hpp")

  install(EXPORT astralixTargets
          FILE astralixTargets.cmake
          NAMESPACE astralix::
          DESTINATION lib/cmake/astralix
  )
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/astralixConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/astralixConfig.cmake
  INSTALL_DESTINATION lib/cmake/astralix
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/astralixConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/astralixConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/astralixConfigVersion.cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/astralixAxgen.cmake
  DESTINATION lib/cmake/astralix
)

install(DIRECTORY 
    ${CMAKE_SOURCE_DIR}/external/PhysX/physx/include/
    DESTINATION include/astralix/external/PhysX/include
    FILES_MATCHING 
    PATTERN "*.h"
    PATTERN "*.hpp"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/assets
        DESTINATION share/astralix
        PATTERN "*.git*" EXCLUDE
        PATTERN "__pycache__" EXCLUDE
        PATTERN "*.pyc" EXCLUDE
)
