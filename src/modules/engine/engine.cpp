#include "engine.hpp"
#include "managers/project-manager.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "managers/window-manager.hpp"
#include "systems/audio-system.hpp"
#include "systems/job-system/job-system.hpp"
#include "systems/terrain-system.hpp"
#include "systems/physics-system.hpp"
#include "systems/render-system/render-system.hpp"
#include "systems/scene-system.hpp"
#include "systems/ui-system/ui-system.hpp"
#include "trace.hpp"

namespace astralix {
Engine *Engine::m_instance = nullptr;

void Engine::init() {
  if (m_instance == nullptr) {
    m_instance = new Engine;
  }
}

void Engine::end() { delete m_instance; }

Engine::Engine() {
  ASTRA_PROFILE_N("SceneManager::init");
  SceneManager::init();
}

void Engine::start() {
  ASTRA_PROFILE_N("Engine::start");
  auto system_manager = SystemManager::get();

  auto project = active_project();
  auto project_config = project->get_config();

  {
    ASTRA_PROFILE_N("Engine::add_system<JobSystem>");
    system_manager->add_system<JobSystem>();
  }
  {
    ASTRA_PROFILE_N("Engine::add_system<SceneSystem>");
    system_manager->add_system<SceneSystem>();
  }
  {
    ASTRA_PROFILE_N("Engine::add_system<UISystem>");
    system_manager->add_system<UISystem>();
  }

  for (auto system : project_config.systems) {
    switch (system.type) {
      case SystemType::Render: {
        ASTRA_PROFILE_N("Engine::add_system<RenderSystem>");
        system_manager->add_system<RenderSystem>(
            std::get<RenderSystemConfig>(system.content));
        continue;
      }

      case SystemType::Physics: {
        ASTRA_PROFILE_N("Engine::add_system<PhysicsSystem>");
        system_manager->add_system<PhysicsSystem>(
            std::get<PhysicsSystemConfig>(system.content));
        continue;
      }

      case SystemType::Audio: {
        ASTRA_PROFILE_N("Engine::add_system<AudioSystem>");
        system_manager->add_system<AudioSystem>(
            std::get<AudioSystemConfig>(system.content));
        continue;
      }

      case SystemType::Terrain: {
        ASTRA_PROFILE_N("Engine::add_system<TerrainSystem>");
        system_manager->add_system<TerrainSystem>(
            std::get<TerrainSystemConfig>(system.content));
        continue;
      }

      default:
        continue;
    }
  }

  project->on_manifest_reload([system_manager](const ProjectConfig &config) {
    for (const auto &system : config.systems) {
      if (system.type == SystemType::Render) {
        const auto &render_config = std::get<RenderSystemConfig>(system.content);
        if (auto *render_system = system_manager->get_system<RenderSystem>()) {
          render_system->set_ssgi_config(render_config.ssgi);
          render_system->set_volumetric_config(render_config.volumetric);
          render_system->set_lens_flare_config(render_config.lens_flare);
          render_system->set_eye_adaptation_config(render_config.eye_adaptation);
          render_system->set_render_graph_config(render_config.render_graph);
        }
        break;
      }
    }
  });
}

void Engine::update() {}

} // namespace astralix
