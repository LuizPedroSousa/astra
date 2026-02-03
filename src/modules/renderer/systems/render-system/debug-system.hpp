#include "entities/entity.hpp"
#include "managers/entity-manager.hpp"
#include "systems/system.hpp"
#include "targets/render-target.hpp"

namespace astralix {

class DebugDepth : public Entity<DebugDepth> {
public:
  DebugDepth(ENTITY_INIT_PARAMS);
  ~DebugDepth() = default;

  void start(Ref<RenderTarget> render_target);
  void update(Ref<RenderTarget> render_target);

  void on_enable() override {};
  void on_disable() override {};
};

class DebugNormal : public Entity<DebugNormal> {
public:
  DebugNormal(ENTITY_INIT_PARAMS);
  ~DebugNormal() = default;

  void start();

  void update();

  void on_enable() override {};
  void on_disable() override {};
};

class DebugSystem : public System<DebugSystem> {
public:
  DebugSystem(Ref<RenderTarget> render_target);
  ~DebugSystem();

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

  EntityManager *m_entity_manager = nullptr;
  Ref<RenderTarget> m_render_target = nullptr;
};

} // namespace astralix
