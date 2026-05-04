set(EDITOR_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/builtin-plugins.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/commands/builtin-editor-commands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/context-tool-registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contexts/gizmo/gizmo-context-tool-provider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-camera-navigation-store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-context-store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-selection-store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-viewport-hud-store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-viewport-navigation-store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts/layout-registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/panel-registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/workspace-shell-system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/workspace-shell-system-camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/workspace-shell-system-gizmo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/workspace-shell-system-helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/workspace-shell-system-layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/workspace-shell-system-panels.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/workspace-shell-system-root.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/workspace-shell-system-state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/workspace-shell-system-workspace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/context-toolbox/context-toolbox-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/context-toolbox/styles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/interaction-hud/interaction-hud-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/modes/modes-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/navigation-gizmo/navigation-gizmo-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/console/styles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/console/console-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/console/entry-presentation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/console/rows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/inspector/actions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/inspector/fields.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/inspector/styles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/inspector/cards.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/inspector/component-descriptors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/inspector/inspector-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/runtime/runtime-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/runtime/snapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/runtime/timing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/shading/shading-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene/styles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene/helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene/scene-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene-hierachy/actions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene-hierachy/styles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene-hierachy/helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene-hierachy/menus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene-hierachy/refresh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene-hierachy/rows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene-hierachy/scene-hierarchy-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene-hierachy/snapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/file-browser/file-browser-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/file-browser/render.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/build-overlay/build-overlay-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/toolbar/toolbar-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/viewport/viewport-panel-controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/workspaces/workspace-registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/workspaces/workspace-ui-store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/workspaces/workspace-store.cpp)

set(EDITOR_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/builtin-plugins.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/commands/builtin-editor-commands.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/context-tool-registry.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contexts/gizmo/gizmo-context-tool-provider.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-camera-navigation-store.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-context-store.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-gizmo-store.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-selection-store.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-theme.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-viewport-hud-store.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-viewport-navigation-store.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts/layout-node.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts/layout-registry.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/panel-registry.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/workspace-shell-system.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/workspace-shell-system-internal.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/context-toolbox/context-toolbox-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/context-toolbox/styles.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/interaction-hud/interaction-hud-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/modes/modes-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/navigation-gizmo/navigation-gizmo-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/navigation-gizmo/navigation-gizmo-shared.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/console/console-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/console/entry-presentation.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/inspector/fields.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/inspector/styles.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/inspector/inspector-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/runtime/runtime-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/shading/shading-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene/styles.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene/helpers.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene/scene-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene-hierachy/helpers.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/scene-hierachy/scene-hierarchy-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/file-browser/file-browser-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/build-overlay/build-overlay-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/toolbar/toolbar-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/viewport/gizmo-math.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/viewport/viewport-panel-controller.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/workspaces/workspace-definition.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/workspaces/workspace-registry.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/workspaces/workspace-ui-store.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/workspaces/workspace-store.hpp)

add_library(editor STATIC ${EDITOR_SRC} ${EDITOR_HEADERS})

target_include_directories(
  editor
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/astralix/modules/editor>)

target_include_directories(
  editor
  PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/modules/audio>)

target_link_libraries(
  editor
  PRIVATE
    application
    project
    streams
    renderer
    ui
    window
    shared::foundation
    shared::allocators
    shared::events)

if(ASTRA_EDITOR_HOT_RELOAD)
  set(EDITOR_LIVE_SRC
    ${EDITOR_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/editor-module-entry.cpp
  )

  add_library(editor_live MODULE ${EDITOR_LIVE_SRC})

  target_include_directories(editor_live PRIVATE
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
    ${PROJECT_SOURCE_DIR}/src/modules/ui
    ${PROJECT_SOURCE_DIR}/src/modules/physics
    ${PROJECT_SOURCE_DIR}/src/modules/audio
    ${PROJECT_SOURCE_DIR}/src/modules/terrain
    ${PROJECT_SOURCE_DIR}/src/modules/engine
    ${PROJECT_SOURCE_DIR}/src/modules/application
    ${ASTRALIX_ENGINE_GENERATED_DIR}
    $<$<BOOL:${ASTRA_TRACE}>:${VCPKG_INSTALLED_ROOT}/include/tracy>
  )

  target_compile_definitions(editor_live PRIVATE
    ASTRA_EDITOR
    ASTRA_EDITOR_MODULE_BUILDING
    GLM_ENABLE_EXPERIMENTAL
    PX_PHYSX_STATIC_LIB
    ENABLE_LOGS
    LOG_TO_CONSOLE
  )

  target_compile_options(editor_live PRIVATE
    -fPIC
  )

  target_link_libraries(editor_live PRIVATE glm::glm $<$<BOOL:${ASTRA_TRACE}>:${TRACE_LIBS}>)

  target_compile_features(editor_live PRIVATE cxx_std_20)

  set_target_properties(editor_live PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "editor_live"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/hotreload"
  )
endif()
