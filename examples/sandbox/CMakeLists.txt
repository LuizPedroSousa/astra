cmake_minimum_required(VERSION 3.11)

project(sandbox LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

option(ASTRA_EDITOR "Enable editor builtins in the sandbox example" ON)
option(ASTRA_EDITOR_HOT_RELOAD "Enable hot-reload for the editor module" ON)
option(ASTRA_SANDBOX_HOT_RELOAD "Enable hot-reload for sandbox scenes" ON)

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

# list(APPEND CMAKE_PREFIX_PATH "${ASTRA_INSTALL}" "${VCPKG_ROOT}")
set(ASTRALIX_STREAMS_FORMATS Yaml Xml Json)
if(CMAKE_VERSION VERSION_LESS 3.12)
  find_package(PythonInterp 3 REQUIRED)
  set(SANDBOX_PYTHON_EXECUTABLE "${PYTHON_EXECUTABLE}")
else()
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(SANDBOX_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
endif()
set(SANDBOX_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(SANDBOX_ASSETS_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/sandbox-assets")
set(SANDBOX_ASSETS_ZIP "${SANDBOX_ASSETS_WORK_DIR}/assets.zip")
set(SANDBOX_ASSETS_STAMP "${SANDBOX_ASSETS_DIR}/.assets-ready")
set(
  SANDBOX_ASSETS_URL
  "https://astra.assets.chronosops.com.br/examples/shared/assets.zip"
)
get_filename_component(SANDBOX_BUILDTOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../buildtools" ABSOLUTE)
set(
  SANDBOX_ASSETS_DOWNLOADER
  "${SANDBOX_BUILDTOOLS_DIR}/debug.py"
)
set(SANDBOX_ASSETS_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/fetch-sandbox-assets.cmake")

set(SANDBOX_ASSETS_SCRIPT_CONTENT [=[
file(MAKE_DIRECTORY "@SANDBOX_ASSETS_WORK_DIR@")

message(STATUS "Downloading sandbox assets from @SANDBOX_ASSETS_URL@")
execute_process(
  COMMAND
    "@SANDBOX_PYTHON_EXECUTABLE@"
    "@SANDBOX_ASSETS_DOWNLOADER@"
    "@SANDBOX_ASSETS_URL@"
    "@SANDBOX_ASSETS_ZIP@"
  RESULT_VARIABLE download_result
)
if(NOT download_result EQUAL 0)
  message(FATAL_ERROR "Failed to download sandbox assets from @SANDBOX_ASSETS_URL@")
endif()
message(STATUS "Sandbox assets download complete: @SANDBOX_ASSETS_ZIP@")

message(STATUS "Extracting sandbox assets into @SANDBOX_ASSETS_DIR@")
execute_process(
  COMMAND "@CMAKE_COMMAND@" -E tar xf "@SANDBOX_ASSETS_ZIP@"
  WORKING_DIRECTORY "@SANDBOX_ASSETS_DIR@"
  RESULT_VARIABLE extract_result
)
if(NOT extract_result EQUAL 0)
  message(FATAL_ERROR "Failed to extract sandbox assets from @SANDBOX_ASSETS_ZIP@")
endif()
message(STATUS "Sandbox assets are ready in @SANDBOX_ASSETS_DIR@")
]=])
string(CONFIGURE "${SANDBOX_ASSETS_SCRIPT_CONTENT}" SANDBOX_ASSETS_SCRIPT_CONTENT @ONLY)
file(GENERATE OUTPUT "${SANDBOX_ASSETS_SCRIPT}" CONTENT "${SANDBOX_ASSETS_SCRIPT_CONTENT}")

add_custom_command(
  OUTPUT "${SANDBOX_ASSETS_STAMP}"
  COMMAND "${CMAKE_COMMAND}" -P "${SANDBOX_ASSETS_SCRIPT}"
  COMMAND "${CMAKE_COMMAND}" -E touch "${SANDBOX_ASSETS_STAMP}"
  COMMENT "Fetching sandbox assets"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(sandbox-assets DEPENDS "${SANDBOX_ASSETS_STAMP}")

if (NOT TARGET astralix::astra)
  find_package(astralix CONFIG REQUIRED)
endif()

get_filename_component(ASTRA_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(ASTRA_INSTALL_INCLUDE "${ASTRA_PROJECT_ROOT}/install/include")
set(ASTRA_ENGINE_GENERATED_DIR "${ASTRA_PROJECT_ROOT}/src/assets/.astralix/generated")
if(NOT ASTRA_MAIN_BUILD_DIR)
  set(ASTRA_MAIN_BUILD_DIR "${ASTRA_PROJECT_ROOT}/builds/debug")
endif()

file(GLOB_RECURSE SANDBOX_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE SANDBOX_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp)
file(GLOB_RECURSE SANDBOX_SCENE_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/*.cpp
)
file(GLOB_RECURSE SANDBOX_SCENE_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/*.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/*.hpp
)

set(SANDBOX_COMMON_SRC ${SANDBOX_SRC})
list(FILTER SANDBOX_COMMON_SRC EXCLUDE REGEX "/main\\.cpp$")
list(FILTER SANDBOX_COMMON_SRC EXCLUDE REGEX "/scene_serialization_smoke\\.cpp$")
list(FILTER SANDBOX_COMMON_SRC EXCLUDE REGEX "/editor-loader\\.cpp$")
list(FILTER SANDBOX_COMMON_SRC EXCLUDE REGEX "/render-passes-loader\\.cpp$")
list(FILTER SANDBOX_COMMON_SRC EXCLUDE REGEX "/sandbox-loader\\.cpp$")
list(FILTER SANDBOX_COMMON_SRC EXCLUDE REGEX "/sandbox-module-entry\\.cpp$")

if(ASTRA_SANDBOX_HOT_RELOAD)
  list(FILTER SANDBOX_COMMON_SRC EXCLUDE REGEX "/scenes/")
  list(FILTER SANDBOX_COMMON_SRC EXCLUDE REGEX "/shared/")
endif()

add_library(sandbox-app STATIC ${SANDBOX_COMMON_SRC} ${SANDBOX_HEADERS})
target_include_directories(sandbox-app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sandbox-app PUBLIC astralix::astra)
target_compile_features(sandbox-app PUBLIC cxx_std_20)
target_compile_options(sandbox-app PRIVATE
    -Wall -Wextra -Wpedantic
)

if(ASTRA_SANDBOX_HOT_RELOAD)
  set(SANDBOX_LIVE_SRC
    ${SANDBOX_SCENE_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sandbox-module-entry.cpp
  )

  add_library(sandbox_live MODULE ${SANDBOX_LIVE_SRC} ${SANDBOX_SCENE_HEADERS})

  target_include_directories(sandbox_live PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ASTRA_INSTALL_INCLUDE}
    ${ASTRA_PROJECT_ROOT}/external
    ${ASTRA_PROJECT_ROOT}/external/PhysX/physx/include
    ${ASTRA_PROJECT_ROOT}/src/shared/foundation
    ${ASTRA_PROJECT_ROOT}/src/shared/allocators
    ${ASTRA_PROJECT_ROOT}/src/shared/events
    ${ASTRA_PROJECT_ROOT}/src/shared/ecs
    ${ASTRA_PROJECT_ROOT}/src/modules/streams
    ${ASTRA_PROJECT_ROOT}/src/modules/project
    ${ASTRA_PROJECT_ROOT}/src/modules/window
    ${ASTRA_PROJECT_ROOT}/src/modules/renderer
    ${ASTRA_PROJECT_ROOT}/src/modules/ui
    ${ASTRA_PROJECT_ROOT}/src/modules/physics
    ${ASTRA_PROJECT_ROOT}/src/modules/audio
    ${ASTRA_PROJECT_ROOT}/src/modules/terrain
    ${ASTRA_PROJECT_ROOT}/src/modules/engine
    ${ASTRA_PROJECT_ROOT}/src/modules/application
    ${ASTRA_ENGINE_GENERATED_DIR}
  )

  target_compile_definitions(sandbox_live PRIVATE
    GLM_ENABLE_EXPERIMENTAL
    PX_PHYSX_STATIC_LIB
    ENABLE_LOGS
    LOG_TO_CONSOLE
  )

  target_compile_options(sandbox_live PRIVATE -fPIC)
  target_link_libraries(sandbox_live PRIVATE glm::glm)
  target_compile_features(sandbox_live PRIVATE cxx_std_20)

  set_target_properties(sandbox_live PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "sandbox_live"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/hotreload"
  )
endif()

set(SANDBOX_MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
if(ASTRA_SANDBOX_HOT_RELOAD)
  list(APPEND SANDBOX_MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/sandbox-loader.cpp)
endif()
if(ASTRA_EDITOR_HOT_RELOAD)
  list(APPEND SANDBOX_MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-loader.cpp)
endif()
add_executable(sandbox ${SANDBOX_MAIN_SRC})
add_dependencies(sandbox sandbox-assets)

set(ASTRA_INSTALL_LIB "${ASTRA_PROJECT_ROOT}/install/lib")

function(collect_astralix_installed_archives out_var)
  set(_archives "")
  foreach(_name IN LISTS ARGN)
    set(
      _archive
      "${ASTRA_INSTALL_LIB}/${CMAKE_STATIC_LIBRARY_PREFIX}${_name}${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
    if(EXISTS "${_archive}")
      list(APPEND _archives "${_archive}")
    endif()
  endforeach()
  set(${out_var} "${_archives}" PARENT_SCOPE)
endfunction()

set(ASTRA_WHOLE_ARCHIVE_COMPONENTS
  allocators
  application
  audio
  ecs
  engine
  events
  foundation
  jobs
  physics
  project
  renderer
  shader-lang
  stb_image
  streams
  terrain
  tinyexr
  ui
  window
)

if(ASTRA_EDITOR_HOT_RELOAD)
  collect_astralix_installed_archives(
    ASTRA_INSTALLED_ARCHIVES
    ${ASTRA_WHOLE_ARCHIVE_COMPONENTS}
  )
  target_link_libraries(sandbox PRIVATE
    -Wl,--whole-archive
    sandbox-app
    ${ASTRA_INSTALLED_ARCHIVES}
    -Wl,--no-whole-archive
  )
  target_compile_definitions(sandbox PRIVATE ASTRA_EDITOR ASTRA_EDITOR_HOT_RELOAD)
  target_link_options(sandbox PRIVATE -rdynamic)
  target_link_libraries(sandbox PRIVATE ${CMAKE_DL_LIBS})
  target_include_directories(sandbox PRIVATE
    ${ASTRA_PROJECT_ROOT}/src/modules/editor
  )
elseif(ASTRA_SANDBOX_HOT_RELOAD)
  set(ASTRA_SANDBOX_WHOLE_ARCHIVE_COMPONENTS ${ASTRA_WHOLE_ARCHIVE_COMPONENTS})
  if(ASTRA_EDITOR)
    list(APPEND ASTRA_SANDBOX_WHOLE_ARCHIVE_COMPONENTS editor)
  endif()
  collect_astralix_installed_archives(
    ASTRA_INSTALLED_ARCHIVES
    ${ASTRA_SANDBOX_WHOLE_ARCHIVE_COMPONENTS}
  )
  target_link_libraries(sandbox PRIVATE
    -Wl,--whole-archive
    sandbox-app
    ${ASTRA_INSTALLED_ARCHIVES}
    -Wl,--no-whole-archive
  )
  target_link_options(sandbox PRIVATE -rdynamic)
  target_link_libraries(sandbox PRIVATE ${CMAKE_DL_LIBS})
else()
  target_link_libraries(sandbox PRIVATE sandbox-app)
endif()

if(ASTRA_EDITOR)
  target_compile_definitions(sandbox PRIVATE ASTRA_EDITOR)
endif()

if(ASTRA_EDITOR AND TARGET astralix::editor)
  if(NOT ASTRA_EDITOR_HOT_RELOAD AND NOT ASTRA_SANDBOX_HOT_RELOAD)
    target_link_libraries(sandbox PRIVATE astralix::editor)
  endif()
elseif(ASTRA_EDITOR AND NOT ASTRA_EDITOR_HOT_RELOAD AND NOT ASTRA_SANDBOX_HOT_RELOAD)
  message(WARNING
    "Sandbox editor support was requested, but the installed astralix package "
    "does not export astralix::editor. Rebuild astralix with -DASTRA_EDITOR=ON "
    "to enable editor builtins in the sandbox example."
  )
endif()

if(ASTRA_EDITOR_HOT_RELOAD)
  target_compile_definitions(sandbox PRIVATE
    ASTRA_EDITOR_HOT_RELOAD
    ASTRA_EDITOR_MODULE_PATH="${ASTRA_MAIN_BUILD_DIR}/hotreload/libeditor_live.so"
    ASTRA_EDITOR_SOURCE_DIR="${ASTRA_PROJECT_ROOT}/src/modules/editor"
    ASTRA_EDITOR_BUILD_DIR="${ASTRA_MAIN_BUILD_DIR}"
  )
endif()

if(ASTRA_SANDBOX_HOT_RELOAD)
  target_compile_definitions(sandbox PRIVATE
    ASTRA_SANDBOX_HOT_RELOAD
    ASTRA_SANDBOX_MODULE_PATH="${CMAKE_CURRENT_BINARY_DIR}/hotreload/libsandbox_live.so"
    ASTRA_SANDBOX_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src"
    ASTRA_SANDBOX_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}"
  )
endif()

target_compile_features(sandbox PRIVATE cxx_std_20)

target_compile_options(sandbox PRIVATE
    -Wall -Wextra -Wpedantic
)
