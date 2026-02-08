#pragma once

#include "arena.hpp"
#include "render-graph-builder.hpp"
#include "render-graph-pass.hpp"
#include "render-graph-resource.hpp"
#include <string>
#include <vector>

namespace astralix {

class RenderTarget;
class RenderSystem;
class RenderGraphExporter;

class RenderGraph {
public:
  RenderGraph() = default;
  ~RenderGraph();

  void compile(Ref<RenderTarget> target);

  void execute(double dt);

  void cleanup();

  void export_graph(const RenderGraphExporter &exporter,
                    const std::string &filename) const;

private:
  void compute_resource_lifetimes();
  void infer_dependencies();
  void topological_sort();
  void cull_passes();
  void alias_resources();
  void create_transient_resources();
  void setup_passes();

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

  ElasticArena m_resource_allocator;

  friend class RenderGraphBuilder;
};

} // namespace astralix
