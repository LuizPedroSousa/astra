#pragma once

#include "arena.hpp"
#include "render-graph-builder.hpp"
#include "render-graph-compiled.hpp"
#include "render-graph-pass.hpp"
#include "render-graph-resource.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/frame-stats.hpp"
#include <optional>
#include <string>
#include <vector>

namespace astralix {

class RenderTarget;
class RenderSystem;
class RenderGraphExporter;
class OpenGLExecutor;

class RenderGraph {
public:
  RenderGraph() = default;
  ~RenderGraph();

  void compile(Ref<RenderTarget> target);
  void resize(uint32_t width, uint32_t height);

  void execute(double dt, const rendering::SceneFrame *scene_frame = nullptr);

  void cleanup();

  void export_graph(const RenderGraphExporter &exporter,
                    const std::string &filename) const;

  const RenderGraphResource *resource_at(uint32_t index) const {
    return index < m_resources.size() ? &m_resources[index] : nullptr;
  }

  const CompiledFrame &latest_compiled_frame() const {
    return m_latest_compiled_frame;
  }

  const FrameStats &latest_frame_stats() const { return m_latest_frame_stats; }


  const std::vector<CompiledExportImage> &compiled_exports() const {
    return m_compiled_exports;
  }

  const std::vector<CompiledPresentEdge> &compiled_present_edges() const {
    return m_compiled_present_edges;
  }
private:
  void compute_resource_lifetimes();
  void infer_dependencies();
  void topological_sort();
  void cull_passes();
  void alias_resources();
  void create_transient_resources();
  void setup_passes();
  void compile_transitions();
  void compile_exports();
  void compile_present_edges();
  void validate_graph();

  bool has_lifetime_overlap(const RenderGraphResource &a,
                            const RenderGraphResource &b) const;
  bool are_resources_compatible(const RenderGraphResource &a,
                                const RenderGraphResource &b) const;

  std::vector<RenderGraphResource> m_resources;
  std::vector<Scope<RenderGraphPass>> m_passes;
  std::vector<uint32_t> m_execution_order;
  std::vector<Ref<Framebuffer>> m_transient_framebuffers;
  std::vector<Ref<StorageBuffer>> m_transient_storage_buffers;

  Ref<RenderTarget> m_render_target = nullptr;
  FrameStats m_latest_frame_stats;
  CompiledFrame m_latest_compiled_frame;
  Scope<OpenGLExecutor> m_opengl_executor;
  std::vector<CompiledExportImage> m_compiled_exports;
  std::vector<CompiledPresentEdge> m_compiled_present_edges;

  ElasticArena m_resource_allocator;

  friend class RenderGraphBuilder;
  friend class GraphvizExporter;
  friend class MermaidExporter;
  friend class AsciiExporter;
};

} // namespace astralix
