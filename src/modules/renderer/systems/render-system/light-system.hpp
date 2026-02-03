#include "systems/system.hpp"
#include "targets/render-target.hpp"

namespace astralix {

class LightSystem : public System<LightSystem> {
public:
  LightSystem(Ref<RenderTarget> render_target);
  ~LightSystem();

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

private:
  Ref<RenderTarget> m_render_target = nullptr;
};

} // namespace astralix
