#include "astralix/modules/application/application.hpp"
#if !defined(ASTRA_SANDBOX_HOT_RELOAD)
#include "scenes/arena/arena.hpp"
#include "scenes/render_benchmark/render_benchmark.hpp"
#endif
#if defined(ASTRA_EDITOR_HOT_RELOAD)
#include "editor-loader.hpp"
#include "module-api.h"
#include "systems/workspace-shell-system.hpp"
#elif defined(ASTRA_EDITOR)
#include "astralix/modules/editor/builtin-plugins.hpp"
#endif
#if defined(ASTRA_SANDBOX_HOT_RELOAD)
#include "sandbox-loader.hpp"
#endif
#include "astralix/modules/project/assets/asset_registry.hpp"
#include "astralix/modules/project/managers/project-manager.hpp"
#include "astralix/modules/project/project.hpp"
#include "astralix/modules/renderer/managers/scene-manager.hpp"
#include "astralix/shared/foundation/exceptions/base-exception.hpp"
#include "astralix/shared/foundation/log.hpp"
#include "astralix/shared/foundation/trace.hpp"
#include "filesystem"

using astralix::Logger;
using astralix::LogLevel;

int handleException(const astralix::BaseException &exception) {
  std::cout << exception.what() << std::endl;

  return -1;
}

int main(int, char **) {
  try {
#if defined(ASTRA_EDITOR) && !defined(ASTRA_EDITOR_HOT_RELOAD)
    astralix::editor::register_builtin_plugins({
        .workspace_shell =
            {
                .toggle_visibility_key = astralix::input::KeyCode::GraveAccent,
            },
    });
#endif
    auto app = astralix::Application::init();

    auto directory = std::filesystem::absolute(std::filesystem::path(__FILE__))
                         .parent_path()
                         .parent_path();
    astralix::ProjectConfig project_config{};
    project_config.directory = directory;
    project_config.manifest = "src/project.ax";

    auto project = astralix::Project::create(project_config);

    {
      ASTRA_PROFILE_N("ProjectManager::add_project");
      astralix::ProjectManager::get()->add_project(project);
    }
#if !defined(ASTRA_SANDBOX_HOT_RELOAD)
    astralix::SceneManager::get()->register_scene_type<Arena>("sandbox.arena");
    astralix::SceneManager::get()->register_scene_type<RenderBenchmark>(
        "sandbox.render_benchmark"
    );
#endif

#if defined(ASTRA_SANDBOX_HOT_RELOAD)
    SandboxLoader sandbox_loader({
        .module =
            {
                .module_path = ASTRA_SANDBOX_MODULE_PATH,
                .source_dir = ASTRA_SANDBOX_SOURCE_DIR,
                .build_dir = ASTRA_SANDBOX_BUILD_DIR,
                .build_target = "sandbox_live",
            },
    });
#endif

#if defined(ASTRA_EDITOR_HOT_RELOAD)
    EditorLoader editor_loader({
        .module =
            {
                .module_path = ASTRA_EDITOR_MODULE_PATH,
                .source_dir = ASTRA_EDITOR_SOURCE_DIR,
                .build_dir = ASTRA_EDITOR_BUILD_DIR,
                .build_target = "editor_live",
            },
        .shell_config =
            {
                .toggle_visibility_key = astralix::input::KeyCode::GraveAccent,
            },
    });
#endif

    app->set_pre_frame_callback([&]() {
      if (auto *registry = project->asset_registry()) {
        try {
          registry->poll_reloads();
        } catch (const astralix::BaseException &exception) {
          LOG_ERROR("AssetRegistry: reload failed: ", exception.what());
        } catch (const std::exception &exception) {
          LOG_ERROR("AssetRegistry: reload failed: ", exception.what());
        }
      }
#if defined(ASTRA_SANDBOX_HOT_RELOAD)
      sandbox_loader.poll();
#endif
#if defined(ASTRA_EDITOR_HOT_RELOAD)
      editor_loader.poll();
#endif
    });

#if defined(ASTRA_SANDBOX_HOT_RELOAD)
    {
      ASTRA_PROFILE_N("SandboxLoader::initial_load");
      sandbox_loader.initial_load();
    }
#endif
    app->start();

#if defined(ASTRA_EDITOR_HOT_RELOAD)
    {
      ASTRA_PROFILE_N("EditorLoader::initial_load");
      editor_loader.initial_load();
    }
#endif
    app->run();
  } catch (const astralix::BaseException &exception) {
    return handleException(exception);
  }

  return 0;
}
