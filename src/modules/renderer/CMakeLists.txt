file(GLOB_RECURSE RENDERER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE RENDERER_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

list(FILTER RENDERER_SRC EXCLUDE REGEX ".*/shader-lang/.*")
list(FILTER RENDERER_HEADER EXCLUDE REGEX ".*/shader-lang/.*")
list(FILTER RENDERER_SRC EXCLUDE REGEX ".*\\.test\\.cpp$")
list(FILTER RENDERER_HEADER EXCLUDE REGEX ".*\\.test\\.hpp$")

if(NOT ASTRA_EDITOR)
  list(FILTER RENDERER_SRC EXCLUDE REGEX ".*/components/.*")
  list(FILTER RENDERER_HEADER EXCLUDE REGEX ".*/components/.*")
  list(FILTER RENDERER_SRC EXCLUDE REGEX ".*/entities/(object|camera|skybox|text|post-processing)\\.cpp$")
  list(FILTER RENDERER_HEADER EXCLUDE REGEX ".*/entities/(object|camera|skybox|text|post-processing)\\.hpp$")
endif()

add_library(renderer STATIC ${RENDERER_SRC} ${RENDERER_HEADER})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../shared)
include_directories(${PROJECT_SOURCE_DIR}/external)

target_include_directories(renderer
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/modules/physics>
    ${ASSIMP_INCLUDE_DIRS}
    $<BUILD_INTERFACE:${ASTRALIX_ENGINE_GENERATED_DIR}>
    $<INSTALL_INTERFACE:include/astralix/modules/physics>
  $<INSTALL_INTERFACE:include/astralix/modules/renderer>
)                                   

target_link_libraries(
  renderer
  PUBLIC
  glad::glad
  ${OPENGL_LIBRARIES}
  ${GLFW_LIBRARIES}
  assimp
  engine
  window
  project
  glm::glm
  stb_image
  shared::ecs
  shared::foundation
  imgui::imgui
  # imgui_impl
  Freetype::Freetype
  streams
  shader-lang)
