#pragma once

#include "render-graph-resource.hpp"
#include "render-pass.hpp"
#include <string>
#include <vector>

namespace astralix {

enum class RenderGraphPassType { Graphics, Compute, Transfer };

enum class RenderGraphResourceAccessMode { Read, Write, ReadWrite };

struct RenderGraphPassResourceAccess {
  uint32_t resource_index;
  RenderGraphResourceAccessMode mode;
};

class RenderGraphPass {
public:
  explicit RenderGraphPass(
      Scope<RenderPass> pass,
      RenderGraphPassType type = RenderGraphPassType::Graphics)
      : m_pass(std::move(pass)), m_type(type) {}

  void read(uint32_t resource_index) {
    m_resource_accesses.push_back(
        {resource_index, RenderGraphResourceAccessMode::Read});
  }

  void write(uint32_t resource_index) {
    m_resource_accesses.push_back(
        {resource_index, RenderGraphResourceAccessMode::Write});
  }

  void read_write(uint32_t resource_index) {
    m_resource_accesses.push_back(
        {resource_index, RenderGraphResourceAccessMode::ReadWrite});
  }

  const std::vector<RenderGraphPassResourceAccess> &
  get_resource_accesses() const {
    return m_resource_accesses;
  }

  RenderPass *get_render_pass() const { return m_pass.get(); }

  RenderGraphPassType get_type() const { return m_type; }

  std::string get_name() const { return m_pass->name(); }

  int get_priority() const { return m_pass->priority(); }

  bool is_enabled() const { return m_pass->is_enabled(); }

  void add_dependency(RenderPass *dependency) {
    m_pass->add_dependency(dependency);
  }

  std::vector<RenderPass *> get_dependencies() const {
    return m_pass->dependencies();
  }

  void set_execution_index(uint32_t index) { m_execution_index = index; }

  uint32_t get_execution_index() const { return m_execution_index; }

  void set_culled(bool culled) { m_is_culled = culled; }

  bool is_culled() const { return m_is_culled; }

  void add_computed_dependency(uint32_t pass_index) {
    m_computed_dependency_indices.push_back(pass_index);
  }

  const std::vector<uint32_t> &get_computed_dependency_indices() const {
    return m_computed_dependency_indices;
  }

  void clear_computed_dependencies() { m_computed_dependency_indices.clear(); }

  void setup(Ref<RenderTarget> target,
             std::vector<const RenderGraphResource *> resources) {
    m_pass->setup(target, resources);
  }

  void begin(double dt) {
    if (!m_is_culled && m_pass->is_enabled()) {
      m_pass->begin(dt);
    }
  }

  void execute(double dt) {
    if (!m_is_culled && m_pass->is_enabled()) {
      m_pass->execute(dt);
    }
  }

  void end(double dt) {
    if (!m_is_culled && m_pass->is_enabled()) {
      m_pass->end(dt);
    }
  }

  void cleanup() { m_pass->cleanup(); }

private:
  Scope<RenderPass> m_pass;
  RenderGraphPassType m_type;
  std::vector<RenderGraphPassResourceAccess> m_resource_accesses;
  std::vector<uint32_t> m_computed_dependency_indices;
  uint32_t m_execution_index = 0;
  bool m_is_culled = false;
};

} // namespace astralix
