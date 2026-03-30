#include "astralix/modules/application/application.hpp"
#include "scenes/arena/arena.hpp"
// #include "scenes/render_benchmark/render_benchmark.hpp"
#ifdef ASTRA_EDITOR
#include "astralix/modules/editor/builtin-plugins.hpp"
#endif
#include "astralix/modules/project/managers/project-manager.hpp"
#include "astralix/modules/project/project.hpp"
#include "astralix/modules/renderer/managers/scene-manager.hpp"
#include "astralix/shared/foundation/exceptions/base-exception.hpp"
#include "filesystem"

int handleException(astralix::BaseException exception) {
  std::cout << exception.what() << std::endl;

  return -1;
}

int main(int argc, char **argv) {
  try {
#ifdef ASTRA_EDITOR
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
    auto project = astralix::Project::create({
        .directory = directory,
        .manifest = "src/project.ax",
    });

    astralix::ProjectManager::get()->add_project(project);
    astralix::SceneManager::get()->add_scene<Arena>();
    // astralix::SceneManager::get()->add_scene<RenderBenchmark>();

    app->start();
    app->run();
  } catch (astralix::BaseException exception) {
    return handleException(exception);
  }

  return 0;
}
