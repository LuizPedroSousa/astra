#include "systems/system.hpp"
#include "targets/render-target.hpp"

namespace astralix {

class HDRSystem : public System<HDRSystem> {
public:
  HDRSystem(Ref<RenderTarget> render_target);

  ~HDRSystem();

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

private:
  Ref<RenderTarget> m_render_target;
};

} // namespace astralix
