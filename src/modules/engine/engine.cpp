#include "engine.hpp"
#include "managers/entity-manager.hpp"
#include "managers/project-manager.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "managers/window-manager.hpp"
#include "systems/physics-system.hpp"
#include "systems/render-system/render-system.hpp"
#include "systems/scene-system.hpp"

namespace astralix {
Engine *Engine::m_instance = nullptr;

void Engine::init() {
  if (m_instance == nullptr) {
    m_instance = new Engine;
  }
}

void Engine::end() { delete m_instance; }

Engine::Engine() {
  EntityManager::init();
  ComponentManager::init();
  SceneManager::init();
}

void Engine::start() {
  auto system_manager = SystemManager::get();

  auto project_config = active_project()->get_config();

  system_manager->add_system<SceneSystem>();

  for (auto system : project_config.systems) {
    switch (system.type) {
    case SystemType::Render: {
      system_manager->add_system<RenderSystem>(
          std::get<RenderSystemConfig>(system.content));
      continue;
    }

    case SystemType::Physics: {
      system_manager->add_system<PhysicsSystem>(
          std::get<PhysicsSystemConfig>(system.content));
      continue;
    }

    default:
      continue;
    }
  }
}

void Engine::update() {}

} // namespace astralix
