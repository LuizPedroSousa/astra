#include "commands.hpp"

#include "render_benchmark.hpp"

#include "astralix/modules/physics/components/rigidbody.hpp"
#include "astralix/modules/physics/systems/physics-system.hpp"
#include "astralix/modules/renderer/components/model.hpp"
#include "astralix/modules/renderer/components/tags.hpp"
#include "astralix/shared/foundation/console.hpp"

#include <string>

using namespace astralix;

void register_console_commands(RenderBenchmark &render_benchmark) {
  auto &console = ConsoleManager::get();

  console.register_command("reset_scene", "Recreate the sandbox scene from scratch.", [&render_benchmark](const ConsoleCommandInvocation &) {
    render_benchmark.request_reset_scene();
    ConsoleCommandResult result;
    result.success = true;
    result.lines.push_back("scene reset scheduled");
    return result;
  });

  console.register_command(
      "toggle_physics", "Toggle the physics simulation pause state.", [](const ConsoleCommandInvocation &) {
        toggle_physics_simulation();
        ConsoleCommandResult result;
        result.success = true;
        result.lines.push_back(
            std::string("physics simulation ") +
            (physics_simulation_enabled() ? "enabled" : "paused")
        );
        return result;
      }
  );

  console.register_command(
      "switch_model", "Toggle between Sponza and Bistro Exterior models.", [&render_benchmark](const ConsoleCommandInvocation &) {
        const std::string current = render_benchmark.active_model_id();
        const std::string next =
            current == "models::bistro_exterior"
                ? "models::sponza_atrium"
                : "models::bistro_exterior";

        render_benchmark.set_active_model_id(next);
        render_benchmark.request_reset_scene();

        ConsoleCommandResult result;
        result.success = true;
        result.lines.push_back("switching to: " + next);
        return result;
      }
  );

  console.register_command(
      "stats", "Print scene, model, and renderable counters.", [&render_benchmark](const ConsoleCommandInvocation &) {
        auto &scene_world = render_benchmark.world();
        ConsoleCommandResult result;
        result.success = true;
        result.lines.push_back(
            "scene entities: " +
            std::to_string(scene_world.count<scene::SceneEntity>())
        );
        result.lines.push_back(
            "renderables: " +
            std::to_string(scene_world.count<rendering::Renderable>())
        );
        result.lines.push_back(
            "model refs: " +
            std::to_string(scene_world.count<rendering::ModelRef>())
        );
        result.lines.push_back(
            "shadow casters: " +
            std::to_string(scene_world.count<rendering::ShadowCaster>())
        );
        return result;
      }
  );
}
