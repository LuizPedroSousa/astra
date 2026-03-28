#include "commands.hpp"

#include "prologue.hpp"

#include "astralix/modules/physics/components/rigidbody.hpp"
#include "astralix/modules/physics/systems/physics-system.hpp"
#include "astralix/modules/renderer/components/tags.hpp"
#include "astralix/shared/foundation/console.hpp"

#include <string>

using namespace astralix;

void register_console_commands(Prologue &prologue) {
  auto &console = ConsoleManager::get();

  console.register_command("spawn_cube", "Spawn a dynamic cube into the arena.",
                           [&prologue](const ConsoleCommandInvocation &) {
                             prologue.request_spawn_cube();
                             ConsoleCommandResult result;
                             result.success = true;
                             result.lines.push_back("spawned one cube");
                             return result;
                           });

  console.register_command("reset_scene",
                           "Recreate the sandbox scene from scratch.",
                           [&prologue](const ConsoleCommandInvocation &) {
                             prologue.request_reset_scene();
                             ConsoleCommandResult result;
                             result.success = true;
                             result.lines.push_back("scene reset scheduled");
                             return result;
                           });

  console.register_command(
      "toggle_physics", "Toggle the physics simulation pause state.",
      [](const ConsoleCommandInvocation &) {
        toggle_physics_simulation();
        ConsoleCommandResult result;
        result.success = true;
        result.lines.push_back(
            std::string("physics simulation ") +
            (physics_simulation_enabled() ? "enabled" : "paused"));
        return result;
      });

  console.register_command(
      "stats", "Print scene, renderable, and rigidbody counters.",
      [&prologue](const ConsoleCommandInvocation &) {
        auto &scene_world = prologue.world();
        ConsoleCommandResult result;
        result.success = true;
        result.lines.push_back(
            "scene entities: " +
            std::to_string(scene_world.count<scene::SceneEntity>()));
        result.lines.push_back(
            "renderables: " +
            std::to_string(scene_world.count<rendering::Renderable>()));
        result.lines.push_back(
            "rigidbodies: " +
            std::to_string(scene_world.count<physics::RigidBody>()));
        return result;
      });
}
