#pragma once

#include "project.hpp"
#include "systems/system.hpp"

namespace astralix {

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
};

} // namespace astralix
