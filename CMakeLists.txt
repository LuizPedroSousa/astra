cmake_minimum_required(VERSION 3.11)
project(astralix VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_EXAMPLES "Build example projects" OFF)
option(ASTRA_EDITOR "Astra internal Layer Editor" false)
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

if(ASTRA_TRACE)
  add_definitions(-DASTRA_TRACE)
  set(TRACE_LIBS "${VCPKG_INSTALLED_ROOT}/lib/libTracyClient.a")
endif()

find_package(OpenGL REQUIRED)
find_package(assimp REQUIRED)

set(VCPKG_INSTALLED_ROOT "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-linux")

set(jsoncpp_DIR "${VCPKG_INSTALLED_ROOT}/share/jsoncpp")
set(freetype_DIR "${VCPKG_INSTALLED_ROOT}/share/freetype")
set(imgui_DIR "${VCPKG_INSTALLED_ROOT}/share/imgui")
set(imguizmo_DIR "${VCPKG_INSTALLED_ROOT}/share/imguizmo")
set(glfw3_DIR "${VCPKG_INSTALLED_ROOT}/share/glfw3")
set(uwebsockets_DIR "${VCPKG_INSTALLED_ROOT}/share/unofficial-uwebsockets")
set(glad_DIR "${VCPKG_INSTALLED_ROOT}/share/glad")
set(glm_DIR "${VCPKG_INSTALLED_ROOT}/share/glm")

find_path(USOCKET_LIB_A "${VCPKG_INSTALLED_ROOT}/lib/libuSockets.a")
find_path(GLAD_LIB_A "${VCPKG_INSTALLED_ROOT}/lib/libglad.a")
find_package(glad CONFIG REQUIRED)

find_package(OpenMP REQUIRED)

find_package(ZLIB REQUIRED)


find_package(glm CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

find_package(PkgConfig)

find_package(PkgConfig REQUIRED)

find_package(glfw3 CONFIG REQUIRED)

find_package(X11 REQUIRED)

if(ASTRA_ENABLE_TESTS)
  set(GTest_DIR "${VCPKG_INSTALLED_ROOT}/share/gtest")

  enable_testing()

  find_package(GTest CONFIG REQUIRED)

  file(GLOB_RECURSE TEST_SRC ${CMAKE_SOURCE_DIR}/src/**/*.test.cpp)

  add_executable(astralix tests/main.cpp ${TEST_SRC})
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

if(NOT ASTRA_ENABLE_TESTS)
  add_subdirectory(src/shared/events)
  add_subdirectory(src/shared/ecs)
  add_subdirectory(src/modules/streams)
  add_subdirectory(src/modules/renderer/shader-lang)
  add_subdirectory(src/modules/project)
  add_subdirectory(src/modules/window)
  add_subdirectory(src/modules/renderer)
  add_subdirectory(src/modules/physics)
  add_subdirectory(src/modules/engine)
  add_subdirectory(src/modules/application)
endif()

if(ASTRA_EDITOR)
  add_subdirectory(src/modules/editor)
  set(EDITOR editor)
endif()

# if(OpenMP_CXX_FOUND)
#   set_target_properties(astralix PROPERTIES COMPILE_FLAGS "${OpenMP_CXX_FLAGS}"
#                                        LINK_FLAGS "${OpenMP_CXX_FLAGS}")
# endif()

if(ASTRA_ENABLE_TESTS)
  set(BUILD_TESTING OFF)
  add_subdirectory(external/faker-cxx)

  target_link_libraries(
    astralix PRIVATE faker-cxx shared::allocators GTest::gtest GTest::gtest_main
                GTest::gmock GTest::gmock_main)

  include(GoogleTest)
  gtest_discover_tests(astralix)
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
add_library(astra INTERFACE)
add_library(astralix::astra ALIAS astra)

target_link_libraries(astra INTERFACE
    foundation
    allocators
    events
    ecs
    streams
    project
    window
    renderer
    physics
    engine
    application
    ${EDITOR}
    ZLIB::ZLIB
    OpenMP::OpenMP_CXX
    glfw
    glad::glad
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
    physics
    engine
    application
)
    target_compile_definitions(${tgt}
        PUBLIC ASTRALIX_ASSETS_DIR="${ASTRALIX_ASSETS_DIR_VALUE}"
    )
endforeach()

if(NOT ASTRA_ENABLE_TESTS)
  add_subdirectory(axgen)
endif()

if(BUILD_EXAMPLES)
  add_subdirectory(examples/sandbox)
endif()


install(TARGETS astra
        foundation
        allocators
        events
        ecs
        streams
        project
        window
        shader-lang
        renderer
        physics
        engine
        application
        ${EDITOR}
        stb_image
        EXPORT astralixTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
)

install(DIRECTORY src/ DESTINATION include/astralix FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

install(EXPORT astralixTargets
        FILE astralixTargets.cmake
        NAMESPACE astralix::
        DESTINATION lib/cmake/astralix
)

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
