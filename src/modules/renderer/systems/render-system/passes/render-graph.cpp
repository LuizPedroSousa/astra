#include "render-graph.hpp"
#include "arena.hpp"
#include "exporters/render-graph-exporter.hpp"
#include "log.hpp"
#include "storage-buffer.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "targets/render-target.hpp"
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <vector>

namespace astralix {

RenderGraph::~RenderGraph() { cleanup(); }

void RenderGraph::compile(Ref<RenderTarget> target) {
  m_render_target = target;

  LOG_INFO("[RenderGraph] Compiling render graph");

  compute_resource_lifetimes();

  infer_dependencies();

  topological_sort();

  cull_passes();

  alias_resources();

  create_transient_resources();

  setup_passes();

  LOG_INFO("[RenderGraph] Compilation complete");
}

void RenderGraph::compute_resource_lifetimes() {
  for (auto &resource : m_resources) {
    resource.first_write_pass = -1;
    resource.last_read_pass = -1;
    resource.is_written = false;
    resource.is_read = false;
  }

  for (uint32_t pass_idx = 0; pass_idx < m_passes.size(); pass_idx++) {
    const auto &pass = m_passes[pass_idx];

    for (const auto &access : pass->get_resource_accesses()) {
      auto &resource = m_resources[access.resource_index];

      if (access.mode == RenderGraphResourceAccessMode::Write ||
          access.mode == RenderGraphResourceAccessMode::ReadWrite) {
        if (!resource.is_written) {
          resource.first_write_pass = static_cast<int32_t>(pass_idx);
          resource.is_written = true;
        }
      }

      if (access.mode == RenderGraphResourceAccessMode::Read ||
          access.mode == RenderGraphResourceAccessMode::ReadWrite) {
        resource.last_read_pass = static_cast<int32_t>(pass_idx);
        resource.is_read = true;
      }
    }
  }

  LOG_DEBUG("[RenderGraph] Resource lifetimes computed");
}

void RenderGraph::infer_dependencies() {
  for (auto &pass : m_passes) {
    pass->clear_computed_dependencies();
  }

  for (uint32_t pass_b_idx = 0; pass_b_idx < m_passes.size(); ++pass_b_idx) {
    const auto &pass_b = m_passes[pass_b_idx];

    for (const auto &b_access : pass_b->get_resource_accesses()) {
      if (b_access.mode == RenderGraphResourceAccessMode::Read ||
          b_access.mode == RenderGraphResourceAccessMode::ReadWrite) {
        for (uint32_t pass_a_idx = 0; pass_a_idx < pass_b_idx; ++pass_a_idx) {
          const auto &pass_a = m_passes[pass_a_idx];

          for (const auto &a_access : pass_a->get_resource_accesses()) {
            if (a_access.resource_index == b_access.resource_index &&
                (a_access.mode == RenderGraphResourceAccessMode::Write ||
                 a_access.mode == RenderGraphResourceAccessMode::ReadWrite)) {
              pass_b->add_computed_dependency(pass_a_idx);
              break;
            }
          }
        }
      }
    }

    for (const auto *manual_dep : pass_b->get_dependencies()) {
      for (uint32_t dep_idx = 0; dep_idx < m_passes.size(); ++dep_idx) {
        if (m_passes[dep_idx]->get_render_pass() == manual_dep) {
          pass_b->add_computed_dependency(dep_idx);
          break;
        }
      }
    }
  }

  LOG_DEBUG("[RenderGraph] Dependencies inferred");
}

void RenderGraph::topological_sort() {
  const uint32_t num_passes = static_cast<uint32_t>(m_passes.size());
  std::vector<uint32_t> in_degree(num_passes, 0);
  std::vector<std::vector<uint32_t>> adjacency_list(num_passes);

  for (uint32_t i = 0; i < num_passes; ++i) {
    for (uint32_t dep_idx : m_passes[i]->get_computed_dependency_indices()) {
      adjacency_list[dep_idx].push_back(i);
      in_degree[i]++;
    }
  }

  auto cmp = [this](uint32_t a, uint32_t b) {
    return m_passes[a]->get_priority() > m_passes[b]->get_priority();
  };
  std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(cmp)> queue(
      cmp);

  for (uint32_t i = 0; i < num_passes; ++i) {
    if (in_degree[i] == 0) {
      queue.push(i);
    }
  }

  m_execution_order.clear();
  m_execution_order.reserve(num_passes);

  while (!queue.empty()) {
    uint32_t pass_idx = queue.top();
    queue.pop();
    m_execution_order.push_back(pass_idx);
    m_passes[pass_idx]->set_execution_index(
        static_cast<uint32_t>(m_execution_order.size() - 1));

    for (uint32_t dependent : adjacency_list[pass_idx]) {
      in_degree[dependent]--;
      if (in_degree[dependent] == 0) {
        queue.push(dependent);
      }
    }
  }

  if (m_execution_order.size() != num_passes) {
    LOG_ERROR("[RenderGraph] Circular dependency detected!");
  } else {
    LOG_DEBUG("[RenderGraph] Topological sort complete");
  }
}

void RenderGraph::cull_passes() {
  std::vector<bool> resource_used(m_resources.size(), true);

  for (uint32_t i = 0; i < m_resources.size(); ++i) {
    const auto &resource = m_resources[i];
    if (resource.is_transient() && resource.is_written && !resource.is_read) {
      resource_used[i] = false;
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;

    for (uint32_t pass_idx : m_execution_order) {
      auto &pass = m_passes[pass_idx];
      if (pass->is_culled())
        continue;

      bool has_useful_output = false;

      for (const auto &access : pass->get_resource_accesses()) {
        if (access.mode == RenderGraphResourceAccessMode::Write ||
            access.mode == RenderGraphResourceAccessMode::ReadWrite) {
          if (resource_used[access.resource_index]) {
            has_useful_output = true;
            break;
          }
        }
      }

      for (const auto &other_pass : m_passes) {
        if (!other_pass->is_culled()) {
          for (uint32_t dep_idx :
               other_pass->get_computed_dependency_indices()) {
            if (dep_idx == pass_idx) {
              has_useful_output = true;
              break;
            }
          }
        }
        if (has_useful_output)
          break;
      }

      if (!has_useful_output) {
        pass->set_culled(true);
        changed = true;
      }
    }
  }
}

void RenderGraph::alias_resources() {
  std::vector<uint32_t> transient_indices;
  for (uint32_t i = 0; i < m_resources.size(); ++i) {
    if (m_resources[i].is_transient()) {
      transient_indices.push_back(i);
    }
  }

  std::sort(transient_indices.begin(), transient_indices.end(),
            [this](uint32_t a, uint32_t b) {
              return m_resources[a].first_write_pass <
                     m_resources[b].first_write_pass;
            });

  int32_t next_alias_group = 0;
  std::unordered_map<int32_t, std::vector<uint32_t>> alias_groups;

  for (uint32_t res_idx : transient_indices) {
    auto &resource = m_resources[res_idx];
    bool found_group = false;

    for (auto &[group_id, group_resources] : alias_groups) {
      bool can_alias = true;

      for (uint32_t other_idx : group_resources) {
        const auto &other = m_resources[other_idx];

        if (!are_resources_compatible(resource, other)) {
          can_alias = false;
          break;
        }

        if (has_lifetime_overlap(resource, other)) {
          can_alias = false;
          break;
        }
      }

      if (can_alias) {
        resource.alias_group = group_id;
        group_resources.push_back(res_idx);
        found_group = true;
        break;
      }
    }

    if (!found_group) {
      resource.alias_group = next_alias_group;
      alias_groups[next_alias_group] = {res_idx};
      next_alias_group++;
    }
  }

  LOG_INFO("[RenderGraph] Resource aliasing complete");
}

void RenderGraph::create_transient_resources() {
  std::unordered_map<int32_t, Framebuffer *> group_framebuffers;
  std::unordered_map<int32_t, StorageBuffer *> group_storage_buffers;

  for (auto &resource : m_resources) {
    if (!resource.is_transient()) {
      continue;
    }

    if (group_framebuffers.find(resource.alias_group) !=
        group_framebuffers.end()) {
      resource.set_content(group_framebuffers[resource.alias_group]);
      continue;
    }

    if (group_storage_buffers.find(resource.alias_group) !=
        group_storage_buffers.end()) {
      resource.set_content(group_storage_buffers[resource.alias_group]);
      continue;
    }

    switch (resource.desc.type) {
    case RenderGraphResourceType::Framebuffer: {
      const auto &spec = std::get<TextureSpec>(resource.desc.spec);

      FramebufferSpecification fb_spec;
      fb_spec.width = spec.width;
      fb_spec.height = spec.height;
      fb_spec.samples = spec.sample_count;
      fb_spec.attachments = {spec.format};

      auto backend = m_render_target->renderer_api()->get_backend();
      auto framebuffer = Framebuffer::create(backend, fb_spec);
      Framebuffer *fb_ptr = framebuffer.get();

      resource.set_content(fb_ptr);
      group_framebuffers[resource.alias_group] = fb_ptr;
      m_transient_framebuffers.push_back(std::move(framebuffer));
      break;
    }

    case RenderGraphResourceType::StorageBuffer: {
      const auto &spec = std::get<StorageBufferSpec>(resource.desc.spec);

      auto backend = m_render_target->renderer_api()->get_backend();

      auto storage_buffer = StorageBuffer::create(backend, spec.size);

      StorageBuffer *sb_ptr = storage_buffer.get();

      resource.set_content(sb_ptr);
      group_storage_buffers[resource.alias_group] = sb_ptr;
      m_transient_storage_buffers.push_back(std::move(storage_buffer));
      break;
    }

    case RenderGraphResourceType::LogicalBuffer: {
      const auto &spec = std::get<LogicalBufferSpec>(resource.desc.spec);

      auto backend = m_render_target->renderer_api()->get_backend();

      auto block = m_resource_allocator.allocate(spec.size_hint);

      void *constructed = block;

      if (spec.constructor) {
        constructed = spec.constructor(block);
      } else {
        std::memset(block, 0, spec.size_hint);
      }

      resource.set_content(block);
      break;
    }

    default:
      break;
    }
  }

  LOG_INFO("[RenderGraph] Transient resources created");
}

void RenderGraph::setup_passes() {
  for (auto &pass : m_passes) {
    LOG_DEBUG("[RenderGraph] Setting up pass: ", pass->get_name());
    if (!pass->is_culled()) {
      std::vector<const RenderGraphResource *> resources;

      auto accesses = pass->get_resource_accesses();
      resources.reserve(accesses.size());

      for (const auto &access : accesses) {
        resources.emplace_back(&m_resources[access.resource_index]);
      }

      pass->setup(m_render_target, resources);
    }
  }
}

void RenderGraph::execute(double dt) {
  for (uint32_t pass_idx : m_execution_order) {
    auto &pass = m_passes[pass_idx];
    if (!pass->is_culled() && pass->is_enabled()) {
      pass->begin(dt);
      pass->execute(dt);
      pass->end(dt);
    }
  }
}

void RenderGraph::cleanup() {
  for (auto &pass : m_passes) {
    pass->cleanup();
  }

  m_transient_framebuffers.clear();
}

bool RenderGraph::has_lifetime_overlap(const RenderGraphResource &a,
                                       const RenderGraphResource &b) const {
  if (a.last_read_pass < b.first_write_pass)
    return false;
  if (b.last_read_pass < a.first_write_pass)
    return false;
  return true;
}

bool RenderGraph::are_resources_compatible(
    const RenderGraphResource &resource_a,
    const RenderGraphResource &resource_b) const {
  if (resource_a.desc.type != resource_b.desc.type)
    return false;

  switch (resource_a.desc.type) {
  case RenderGraphResourceType::Framebuffer:
  case RenderGraphResourceType::Texture2D: {

    const auto &spec_a = std::get<TextureSpec>(resource_a.desc.spec);
    const auto &spec_b = std::get<TextureSpec>(resource_b.desc.spec);

    return spec_a.width == spec_b.width && spec_a.height == spec_b.height &&
           spec_a.format == spec_b.format &&
           spec_a.sample_count == spec_b.sample_count;
  }

  case RenderGraphResourceType::StorageBuffer: {
    const auto &spec_a = std::get<StorageBufferSpec>(resource_a.desc.spec);
    const auto &spec_b = std::get<StorageBufferSpec>(resource_b.desc.spec);

    return spec_a.size == spec_b.size;
  }

  case RenderGraphResourceType::LogicalBuffer: {
    const auto &spec_a = std::get<LogicalBufferSpec>(resource_a.desc.spec);
    const auto &spec_b = std::get<LogicalBufferSpec>(resource_b.desc.spec);

    return spec_a.size_hint == spec_b.size_hint;
  }

  default:
    return false;
  }
}

void RenderGraph::export_graph(const RenderGraphExporter &exporter,
                               const std::string &filename) const {
  exporter.export_graph(*this, filename);
}

Scope<RenderGraph> RenderGraphBuilder::build() {
  auto graph = create_scope<RenderGraph>();

  graph->m_resources.reserve(m_resource_descs.size());
  for (auto &desc : m_resource_descs) {
    graph->m_resources.emplace_back(desc);
  }

  graph->m_passes = std::move(m_passes);

  return graph;
}

} // namespace astralix
