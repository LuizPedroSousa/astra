cmake_minimum_required(VERSION 3.21)

set(SHADER_LANG_DIR "${CMAKE_SOURCE_DIR}/src/modules/renderer/shader-lang")
set(MODULES_DIR "${CMAKE_SOURCE_DIR}/src/modules")
set(SHARED_DIR "${CMAKE_SOURCE_DIR}/src/shared")

include("${MODULES_DIR}/streams/serialization-formats.cmake")

file(GLOB_RECURSE AXGEN_SHADER_LANG_SRC
  "${SHADER_LANG_DIR}/*.cpp"
  "${SHADER_LANG_DIR}/emitters/*.cpp")
list(FILTER AXGEN_SHADER_LANG_SRC EXCLUDE REGEX "\\.test\\.cpp$")

set(AXGEN_STREAMS_SRC
  "${MODULES_DIR}/streams/context-proxy.cpp"
  "${MODULES_DIR}/streams/serialization-context.cpp"
  "${MODULES_DIR}/streams/adapters/file/file-stream-reader.cpp"
  "${MODULES_DIR}/streams/adapters/file/file-stream-writer.cpp")

set(AXGEN_PROJECT_ASSET_SRC
  "${MODULES_DIR}/project/assets/asset_graph.cpp"
  "${MODULES_DIR}/project/assets/asset_cooker.cpp"
  "${MODULES_DIR}/project/assets/pack_manifest.cpp")

set(AXGEN_RENDERER_SUPPORT_SRC
  "${MODULES_DIR}/renderer/importers/model-importer.cpp"
  "${MODULES_DIR}/renderer/entities/serializers/axmesh-serializer.cpp"
  "${MODULES_DIR}/renderer/resources/mesh.cpp")

add_executable(axgen
  src/args.cpp
  src/cook.cpp
  src/main.cpp
  src/project-bootstrap.cpp
  src/project-serializer.cpp
  src/project-locator.cpp
  src/sync.cpp
  src/watch.cpp
  ${AXGEN_SHADER_LANG_SRC}
  ${AXGEN_STREAMS_SRC}
  ${AXGEN_PROJECT_ASSET_SRC}
  ${AXGEN_RENDERER_SUPPORT_SRC})

target_include_directories(axgen PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/src/modules/renderer"
  "${MODULES_DIR}"
  "${MODULES_DIR}/project"
  "${MODULES_DIR}/streams"
  "${SHARED_DIR}/allocators"
  "${SHARED_DIR}/foundation"
  "${SHARED_DIR}/ecs")

target_compile_definitions(axgen PRIVATE
  ASTRALIX_ENGINE_ASSETS_DIR="${ASTRALIX_ENGINE_ASSETS_DIR}"
  ASTRALIX_ENGINE_GENERATED_ROOT="${ASTRALIX_ENGINE_GENERATED_ROOT}")

target_link_libraries(axgen PRIVATE
  shared::allocators
  shared::foundation
  assimp
  mikktspace
  glm::glm)

astralix_streams_enable_serialization_formats(axgen FORMATS
  ${ASTRALIX_STREAMS_FORMATS})

install(TARGETS axgen
        RUNTIME DESTINATION bin)
