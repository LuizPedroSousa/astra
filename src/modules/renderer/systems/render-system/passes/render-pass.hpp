#pragma once

#include "base.hpp"
#include <string>
#include <vector>

namespace astralix {

class Framebuffer;
class RenderTarget;
struct RenderGraphResource;

class RenderPass {
public:
  virtual ~RenderPass() = default;

  virtual void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource*>& resources) = 0;
  virtual void set_framebuffer(Framebuffer *framebuffer) {}
  virtual void begin(double dt) = 0;
  virtual void execute(double dt) = 0;
  virtual void end(double dt) = 0;
  virtual void cleanup() = 0;
  virtual std::string name() const = 0;

  virtual bool is_enabled() const { return m_enabled; }
  virtual void set_enabled(bool enabled) { m_enabled = enabled; }

  virtual int priority() const { return m_priority; }
  virtual void set_priority(int priority) { m_priority = priority; }

  virtual std::vector<RenderPass *> dependencies() const {
    return m_dependencies;
  }

  virtual void add_dependency(RenderPass *pass) {
    if (pass && pass != this) {
      m_dependencies.push_back(pass);
    }
  }

  virtual void clear_dependencies() { m_dependencies.clear(); }

protected:
  RenderPass() = default;

  Ref<RenderTarget> m_render_target;
  std::vector<RenderPass *> m_dependencies;
  int m_priority = -1;
  bool m_enabled = true;
};

} // namespace astralix
