file(GLOB_RECURSE RENDERER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE RENDERER_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

list(FILTER RENDERER_SRC EXCLUDE REGEX ".*/shader-lang/.*")
list(FILTER RENDERER_HEADER EXCLUDE REGEX ".*/shader-lang/.*")
list(FILTER RENDERER_SRC EXCLUDE REGEX ".*\\.test\\.cpp$")
list(FILTER RENDERER_HEADER EXCLUDE REGEX ".*\\.test\\.hpp$")
list(FILTER RENDERER_SRC EXCLUDE REGEX ".*/render-passes-module-entry\\.cpp$")

if(NOT ASTRA_EDITOR)
  list(FILTER RENDERER_SRC EXCLUDE REGEX ".*/components/.*")
  list(FILTER RENDERER_HEADER EXCLUDE REGEX ".*/components/.*")
  list(FILTER RENDERER_SRC EXCLUDE REGEX ".*/entities/(object|camera|skybox|text|post-processing)\\.cpp$")
  list(FILTER RENDERER_HEADER EXCLUDE REGEX ".*/entities/(object|camera|skybox|text|post-processing)\\.hpp$")
endif()

add_library(renderer STATIC ${RENDERER_SRC} ${RENDERER_HEADER})

if(ASTRA_RENDERER_HOT_RELOAD)
  file(GLOB RENDER_PASSES_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/render-system/passes/debug-pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/render-system/passes/debug-draw-pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/render-system/passes/editor-gizmo-pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/render-system/passes/navigation-gizmo-pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/render-system/passes/terrain-pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/render-system/passes/ui-pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/render-system/render-passes-module-entry.cpp
  )

  add_library(render_passes_live MODULE ${RENDER_PASSES_SRC})

  target_include_directories(render_passes_live PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src/shared
    ${PROJECT_SOURCE_DIR}/src/shared/containers
    ${PROJECT_SOURCE_DIR}/src/shared/foundation
    ${PROJECT_SOURCE_DIR}/src/shared/allocators
    ${PROJECT_SOURCE_DIR}/src/shared/events
    ${PROJECT_SOURCE_DIR}/src/shared/ecs
    ${PROJECT_SOURCE_DIR}/src/modules/streams
    ${PROJECT_SOURCE_DIR}/src/modules/project
    ${PROJECT_SOURCE_DIR}/src/modules/window
    ${PROJECT_SOURCE_DIR}/src/modules/renderer
    ${PROJECT_SOURCE_DIR}/src/modules/editor
    ${PROJECT_SOURCE_DIR}/src/modules/ui
    ${PROJECT_SOURCE_DIR}/src/modules/physics
    ${PROJECT_SOURCE_DIR}/src/modules/audio
    ${PROJECT_SOURCE_DIR}/src/modules/terrain
    ${PROJECT_SOURCE_DIR}/src/modules/engine
    ${PROJECT_SOURCE_DIR}/src/modules/application
    ${ASTRALIX_ENGINE_GENERATED_DIR}
    $<$<BOOL:${ASTRA_TRACE}>:${VCPKG_INSTALLED_ROOT}/include/tracy>
  )

  target_compile_definitions(render_passes_live PRIVATE
    ASTRA_EDITOR
    ASTRA_RENDERER_HOT_RELOAD
    ASTRA_RENDERER_PASSES_MODULE_BUILDING
    GLM_ENABLE_EXPERIMENTAL
    PX_PHYSX_STATIC_LIB
    ENABLE_LOGS
    LOG_TO_CONSOLE
  )

  target_compile_options(render_passes_live PRIVATE -fPIC)

  target_link_libraries(render_passes_live PRIVATE glm::glm $<$<BOOL:${ASTRA_TRACE}>:${TRACE_LIBS}>)

  target_compile_features(render_passes_live PRIVATE cxx_std_20)

  set_target_properties(render_passes_live PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "render_passes_live"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/hotreload"
  )
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../shared)
include_directories(${PROJECT_SOURCE_DIR}/external)

target_include_directories(renderer
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/modules/editor>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/modules/ui>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/modules/physics>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/modules/audio>
    ${ASSIMP_INCLUDE_DIRS}
    $<BUILD_INTERFACE:${ASTRALIX_ENGINE_GENERATED_DIR}>
    $<INSTALL_INTERFACE:include/astralix/modules/physics>
    $<INSTALL_INTERFACE:include/astralix/modules/renderer>
    $<INSTALL_INTERFACE:include/astralix/modules/ui>
)                                   

target_link_libraries(
  renderer
  PUBLIC
  glad::glad
  ${OPENGL_LIBRARIES}
  Vulkan::Vulkan
  unofficial::shaderc::shaderc
  ${GLFW_LIBRARIES}
  assimp
  engine
  jobs
  window
  project
  glm::glm
  stb_image
  tinyexr
  mikktspace
  shared::ecs
  shared::foundation
  Freetype::Freetype
  streams
  shader-lang)
