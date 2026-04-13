#pragma once

#include "render-graph-compiled.hpp"
#include "render-graph-usage.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "render-graph-resource.hpp"
#include "render-pass-dependency.hpp"
#include "render-pass.hpp"
#include <optional>
#include <string>
#include <vector>

namespace astralix {

enum class RenderGraphResourceAccessMode { Read, Write, ReadWrite };

struct RenderGraphPassResourceAccess {
  uint32_t resource_index;
  RenderGraphResourceAccessMode mode;
};

class RenderGraphPass {
public:
  explicit RenderGraphPass(
      Scope<FramePass> pass,
      RenderGraphPassType type = RenderGraphPassType::Graphics)
      : m_frame_pass(std::move(pass)), m_type(type) {}

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

  void use_image(RenderImageSubresourceRef view, RenderUsage usage) {
    m_image_usages.push_back(RenderPassImageUsage{.view = view, .usage = usage});

    const uint32_t resource_index = view.resource_index();
    if (is_write_usage(usage) && is_read_usage(usage)) {
      m_resource_accesses.push_back(
          {resource_index, RenderGraphResourceAccessMode::ReadWrite});
    } else if (is_write_usage(usage) || usage == RenderUsage::Present) {
      m_resource_accesses.push_back(
          {resource_index, RenderGraphResourceAccessMode::Write});
    } else {
      m_resource_accesses.push_back(
          {resource_index, RenderGraphResourceAccessMode::Read});
    }
  }

  void add_asset_dependency(RenderPassDependencyDeclaration dependency) {
    m_asset_dependencies.push_back(std::move(dependency));
  }

  void set_resolved_dependencies(
      std::vector<ResolvedRenderPassDependency> dependencies
  ) {
    m_resolved_dependencies = std::move(dependencies);
  }

  void export_image(const RenderImageExport &export_request) {
    m_exports.push_back(export_request);
  }

  void present(RenderImageSubresourceRef source) {
    m_present = RenderPassPresentRequest{.source = source};
  }

  const std::vector<RenderPassImageUsage> &get_image_usages() const {
    return m_image_usages;
  }

  const std::vector<RenderPassDependencyDeclaration> &
  get_asset_dependencies() const {
    return m_asset_dependencies;
  }

  const std::vector<RenderImageExport> &get_exports() const {
    return m_exports;
  }

  const std::optional<RenderPassPresentRequest> &get_present() const {
    return m_present;
  }

  bool has_present() const { return m_present.has_value(); }

  bool has_exports() const { return !m_exports.empty(); }

  void set_compiled_transitions(std::vector<CompiledTransition> transitions) {
    m_compiled_transitions = std::move(transitions);
  }

  const std::vector<CompiledTransition> &get_compiled_transitions() const {
    return m_compiled_transitions;
  }

  const std::vector<RenderGraphPassResourceAccess> &
  get_resource_accesses() const {
    return m_resource_accesses;
  }

  FramePass *get_frame_pass() const { return m_frame_pass.get(); }

  RenderGraphPassType get_type() const { return m_type; }

  std::string get_name() const { return m_frame_pass->name(); }

  int get_priority() const { return m_frame_pass->priority(); }

  bool is_enabled() const { return m_frame_pass->is_enabled(); }

  bool has_side_effects() const { return m_frame_pass->has_side_effects(); }

  void add_dependency(const RenderGraphPass *dependency) {
    if (dependency != nullptr && dependency != this) {
      m_manual_dependencies.push_back(dependency);
    }
  }

  const std::vector<const RenderGraphPass *> &get_dependencies() const {
    return m_manual_dependencies;
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
    PassSetupContext ctx{
        .render_target = target,
        .resources = &resources,
        .declared_dependencies = &m_asset_dependencies,
        .resolved_dependencies = &m_resolved_dependencies,
    };
    m_frame_pass->setup(ctx);
  }

  void record(double dt, CompiledFrame &frame,
              std::vector<const RenderGraphResource *> resources,
              const rendering::SceneFrame *scene_frame = nullptr) {
    if (m_is_culled || !m_frame_pass->is_enabled()) {
      return;
    }

    PassRecordContext ctx{
        .dt = dt,
        .resources = &resources,
        .compiled_frame = &frame,
        .scene_frame = scene_frame,
        .resolved_dependencies = &m_resolved_dependencies,
    };

    PassRecorder recorder;
    RenderApiAccessScope::Guard guard;
    m_frame_pass->record(ctx, recorder);

    CompiledPass compiled_pass{
        .debug_name = m_frame_pass->name(),
        .type = m_type,
        .dependency_pass_indices = m_computed_dependency_indices,
        .commands = recorder.take_commands(),
    };

    if (compiled_pass.commands.empty()) {
      return;
    }

    frame.passes.push_back(std::move(compiled_pass));
  }

  void cleanup() {
    if (m_frame_pass != nullptr) {
      m_frame_pass->cleanup();
    }
  }

private:
  Scope<FramePass> m_frame_pass;
  RenderGraphPassType m_type;
  std::vector<RenderGraphPassResourceAccess> m_resource_accesses;
  std::vector<RenderPassImageUsage> m_image_usages;
  std::vector<RenderPassDependencyDeclaration> m_asset_dependencies;
  std::vector<ResolvedRenderPassDependency> m_resolved_dependencies;
  std::vector<RenderImageExport> m_exports;
  std::optional<RenderPassPresentRequest> m_present;
  std::vector<CompiledTransition> m_compiled_transitions;
  std::vector<const RenderGraphPass *> m_manual_dependencies;
  std::vector<uint32_t> m_computed_dependency_indices;
  uint32_t m_execution_index = 0;
  bool m_is_culled = false;
};

} // namespace astralix
