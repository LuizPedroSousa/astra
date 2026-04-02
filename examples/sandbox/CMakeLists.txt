cmake_minimum_required(VERSION 3.11)

project(sandbox LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

option(ASTRA_EDITOR "Enable editor builtins in the sandbox example" ON)

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

file(REMOVE_RECURSE "@SANDBOX_ASSETS_DIR@")
file(MAKE_DIRECTORY "@SANDBOX_ASSETS_DIR@")

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

file(GLOB_RECURSE SANDBOX_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE SANDBOX_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp)

set(SANDBOX_COMMON_SRC ${SANDBOX_SRC})
list(FILTER SANDBOX_COMMON_SRC EXCLUDE REGEX "/main\\.cpp$")
list(FILTER SANDBOX_COMMON_SRC EXCLUDE REGEX "/scene_serialization_smoke\\.cpp$")

add_library(sandbox-app STATIC ${SANDBOX_COMMON_SRC} ${SANDBOX_HEADERS})
target_include_directories(sandbox-app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sandbox-app PUBLIC astralix::astra)
target_compile_features(sandbox-app PUBLIC cxx_std_20)
target_compile_options(sandbox-app PRIVATE
    -Wall -Wextra -Wpedantic
)

add_executable(sandbox ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
add_dependencies(sandbox sandbox-assets)
target_link_libraries(sandbox PRIVATE sandbox-app)
if(ASTRA_EDITOR AND TARGET astralix::editor)
  target_compile_definitions(sandbox PRIVATE ASTRA_EDITOR)
  target_link_libraries(sandbox PRIVATE astralix::editor)
elseif(ASTRA_EDITOR)
  message(WARNING
    "Sandbox editor support was requested, but the installed astralix package "
    "does not export astralix::editor. Rebuild astralix with -DASTRA_EDITOR=ON "
    "to enable editor builtins in the sandbox example."
  )
endif()

target_compile_features(sandbox PRIVATE cxx_std_20)

target_compile_options(sandbox PRIVATE
    -Wall -Wextra -Wpedantic
)
