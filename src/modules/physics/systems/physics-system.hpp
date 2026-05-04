#pragma once

#include "PxRigidActor.h"
#include "project.hpp"
#include "systems/system.hpp"
#include "unordered_map"

#include <cstdint>

namespace astralix {

class Scene;

bool physics_simulation_enabled();
void set_physics_simulation_enabled(bool enabled);
void toggle_physics_simulation();

struct Pvd {
  std::string host;
  int port;
  int timeout;
};

class PhysicsSystem : public System<PhysicsSystem> {
public:
  PhysicsSystem(PhysicsSystemConfig &config);
  ~PhysicsSystem();

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

private:
  std::string
      m_backend; // Later we're going to add support for multiple backends

  glm::vec3 m_gravity;

  Pvd m_pvd;
  Scene *m_registered_scene = nullptr;
  uint64_t m_registered_scene_revision = 0u;
  uint64_t m_registered_scene_generation = 0u;
  std::unordered_map<EntityID, physx::PxRigidActor *> m_actors;
};

} // namespace astralix
