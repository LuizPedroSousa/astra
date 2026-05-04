#include "module-api.h"

#include "astralix/modules/renderer/managers/scene-manager.hpp"
#include "scenes/arena/arena.hpp"
#include "scenes/render_benchmark/render_benchmark.hpp"

#include <string>
#include <string_view>

namespace {

using namespace astralix;

constexpr std::string_view k_arena_scene_type = "sandbox.arena";
constexpr std::string_view k_render_benchmark_scene_type =
    "sandbox.render_benchmark";

void sandbox_load(void *config, uint32_t config_size) {
  (void)config;
  (void)config_size;

  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr) {
    return;
  }

  scene_manager->register_scene_type<Arena>(std::string(k_arena_scene_type));
  scene_manager->register_scene_type<RenderBenchmark>(
      std::string(k_render_benchmark_scene_type)
  );
  scene_manager->set_scene_activation_enabled(true);
}

void sandbox_unload() {
  auto scene_manager = SceneManager::get();
  if (scene_manager == nullptr) {
    return;
  }

  scene_manager->set_scene_activation_enabled(false);
  scene_manager->reset_scene_instances();
  scene_manager->unregister_scene_type(k_arena_scene_type);
  scene_manager->unregister_scene_type(k_render_benchmark_scene_type);
}

} // namespace

extern "C" __attribute__((visibility("default")))
const AstraModuleAPI *astra_get_module_api() {
  static AstraModuleAPI api{
      ASTRA_MODULE_API_VERSION,
      sandbox_load,
      sandbox_unload,
  };
  return &api;
}
