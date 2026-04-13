#include "render-graph.hpp"
#include "arena.hpp"
#include "assert.hpp"
#include "exporters/render-graph-exporter.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "managers/window-manager.hpp"
#include "platform/OpenGL/opengl-executor.hpp"
#include "render-graph-compiled.hpp"
#include "render-graph-usage.hpp"
#include "renderer-api.hpp"
#include "storage-buffer.hpp"
#include "systems/render-system/render-residency.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace astralix {

namespace {

std::pair<uint32_t, uint32_t>
resolve_image_extent(const ImageDesc &desc, uint32_t width, uint32_t height) {
  switch (desc.extent.mode) {
    case ImageExtentMode::WindowRelative: {
      auto resolved_width =
          std::max(1u, static_cast<uint32_t>(width * desc.extent.scale_x));
      auto resolved_height =
          std::max(1u, static_cast<uint32_t>(height * desc.extent.scale_y));
      return {resolved_width, resolved_height};
    }

    case ImageExtentMode::Absolute:
    default: {
      auto resolved_width =
          desc.extent.width != 0 ? desc.extent.width : desc.width;
      auto resolved_height =
          desc.extent.height != 0 ? desc.extent.height : desc.height;
      return {resolved_width, resolved_height};
    }
  }
}

std::vector<const RenderGraphResource *>
collect_pass_resources(const std::vector<RenderGraphResource> &resources, const RenderGraphPass &pass) {
  std::vector<const RenderGraphResource *> result;
  const auto accesses = pass.get_resource_accesses();
  result.reserve(accesses.size());

  for (const auto &access : accesses) {
    result.emplace_back(&resources[access.resource_index]);
  }

  return result;
}

void append_pass_dependency_requests(
    const RenderPassDependencyDeclaration &dependency,
    rendering::SceneResidencyRequests &requests
) {
  switch (dependency.type) {
    case RenderPassDependencyType::Shader:
      rendering::request_shader(requests, dependency.descriptor_id);
      return;
    case RenderPassDependencyType::Texture2D:
      rendering::request_texture(requests, dependency.descriptor_id, false);
      return;
    case RenderPassDependencyType::Texture3D:
      rendering::request_texture(requests, dependency.descriptor_id, true);
      return;
    case RenderPassDependencyType::Material:
      rendering::request_material(requests, dependency.descriptor_id);
      return;
    case RenderPassDependencyType::Model:
      rendering::request_model(requests, dependency.descriptor_id);
      return;
    case RenderPassDependencyType::Font:
      requests.fonts.insert(dependency.descriptor_id);
      return;
    case RenderPassDependencyType::Svg:
      rendering::request_svg(requests, dependency.descriptor_id);
      return;
    case RenderPassDependencyType::Opaque:
      return;
  }
}

ResolvedRenderPassDependency resolve_pass_dependency(
    const RenderPassDependencyDeclaration &dependency
) {
  ResolvedRenderPassDependency resolved{.declaration = dependency};

  switch (dependency.type) {
    case RenderPassDependencyType::Shader:
      resolved.resource =
          resource_manager()->get_by_descriptor_id<Shader>(
              dependency.descriptor_id);
      return resolved;
    case RenderPassDependencyType::Texture2D:
      resolved.resource =
          resource_manager()->get_by_descriptor_id<Texture2D>(
              dependency.descriptor_id);
      return resolved;
    case RenderPassDependencyType::Texture3D:
      resolved.resource =
          resource_manager()->get_by_descriptor_id<Texture3D>(
              dependency.descriptor_id);
      return resolved;
    case RenderPassDependencyType::Material:
      resolved.resource =
          resource_manager()->get_by_descriptor_id<Material>(
              dependency.descriptor_id);
      return resolved;
    case RenderPassDependencyType::Model:
      resolved.resource =
          resource_manager()->get_by_descriptor_id<Model>(
              dependency.descriptor_id);
      return resolved;
    case RenderPassDependencyType::Font:
      resolved.resource =
          resource_manager()->get_by_descriptor_id<Font>(
              dependency.descriptor_id);
      return resolved;
    case RenderPassDependencyType::Svg:
      resolved.resource =
          resource_manager()->get_by_descriptor_id<Svg>(
              dependency.descriptor_id);
      return resolved;
    case RenderPassDependencyType::Opaque:
      return resolved;
  }

  return resolved;
}

} // namespace

RenderGraph::~RenderGraph() { cleanup(); }

void RenderGraph::compile(Ref<RenderTarget> target) {
  ASTRA_PROFILE_N("RenderGraph::compile");
  m_render_target = target;

  LOG_INFO("[RenderGraph] Compiling render graph");

  compute_resource_lifetimes();

  infer_dependencies();

  topological_sort();

  cull_passes();

  alias_resources();

  create_transient_resources();

  compile_transitions();
  compile_exports();
  compile_present_edges();

  validate_graph();

  setup_passes();

  ASTRA_ENSURE(m_render_target == nullptr, "[RenderGraph] RenderTarget must be created");
  auto backend = m_render_target->backend();

  switch (backend) {
    case RendererBackend::OpenGL: {
      m_opengl_executor.reset();
      m_opengl_executor = create_scope<OpenGLExecutor>(*m_render_target);
      break;
    }
    default:
      ASTRA_ENSURE(m_render_target == nullptr, "[RenderGraph] RenderTarget must create a RenderBackend first");
      break;
  }

  LOG_INFO("[RenderGraph] Compilation complete");
}

void RenderGraph::resize(uint32_t width, uint32_t height) {
  for (auto &resource : m_resources) {
    if (resource.desc.type == RenderGraphResourceType::Image) {
      auto spec = std::get_if<ImageDesc>(&resource.desc.spec);
      const auto image = resource.get_graph_image();
      if (spec == nullptr || image == nullptr) {
      continue;
    }

    auto [resolved_width, resolved_height] =
          resolve_image_extent(*spec, width, height);

    if (resolved_width == 0 || resolved_height == 0) {
      continue;
    }

    spec->width = resolved_width;
    spec->height = resolved_height;
      image->update_desc(*spec);
    }
  }

  }
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

    for (const auto &export_request : pass->get_exports()) {
      const uint32_t resource_index = export_request.resource_index;
      if (resource_index < m_resources.size()) {
        auto &resource = m_resources[resource_index];
        resource.last_read_pass = std::max(
            resource.last_read_pass, static_cast<int32_t>(pass_idx)
        );
        resource.is_read = true;
      }
    }

    if (pass->has_present()) {
      const uint32_t resource_index =
          pass->get_present()->source.resource_index();
      if (resource_index < m_resources.size()) {
        auto &resource = m_resources[resource_index];
        resource.last_read_pass = std::max(
            resource.last_read_pass, static_cast<int32_t>(pass_idx)
        );
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

      if (b_access.mode == RenderGraphResourceAccessMode::Write ||
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
        if (m_passes[dep_idx].get() == manual_dep) {
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
      cmp
  );

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
        static_cast<uint32_t>(m_execution_order.size() - 1)
    );

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
  const uint32_t num_passes = static_cast<uint32_t>(m_passes.size());
  std::vector<bool> needed(num_passes, false);

  for (uint32_t pass_idx = 0; pass_idx < num_passes; ++pass_idx) {
    const auto &pass = m_passes[pass_idx];
    if (pass->has_side_effects() || pass->has_present() ||
        pass->has_exports()) {
      needed[pass_idx] = true;
      continue;
    }

    for (const auto &usage : pass->get_image_usages()) {
      if (is_write_usage(usage.usage) &&
          usage.view.resource_index() < m_resources.size() &&
          m_resources[usage.view.resource_index()].is_persistent()) {
        needed[pass_idx] = true;
        break;
      }
    }

    if (!needed[pass_idx]) {
      for (const auto &access : pass->get_resource_accesses()) {
        if ((access.mode == RenderGraphResourceAccessMode::Write ||
             access.mode == RenderGraphResourceAccessMode::ReadWrite) &&
            access.resource_index < m_resources.size() &&
            m_resources[access.resource_index].is_persistent()) {
          needed[pass_idx] = true;
            break;
          }
        }
      }
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (uint32_t pass_idx = 0; pass_idx < num_passes; ++pass_idx) {
      if (!needed[pass_idx]) {
        continue;
      }
          for (uint32_t dep_idx :
           m_passes[pass_idx]->get_computed_dependency_indices()) {
        if (!needed[dep_idx]) {
          needed[dep_idx] = true;
          changed = true;
        }
      }
    }
  }

  for (uint32_t pass_idx = 0; pass_idx < num_passes; ++pass_idx) {
    m_passes[pass_idx]->set_culled(!needed[pass_idx]);
            }
          }

void RenderGraph::alias_resources() {
  std::unordered_set<uint32_t> export_or_present_resources;
  for (const auto &pass : m_passes) {
    for (const auto &export_request : pass->get_exports()) {
      export_or_present_resources.insert(export_request.resource_index);
    }
    if (pass->has_present()) {
      export_or_present_resources.insert(
          pass->get_present()->source.resource_index()
      );
      }
    }

  int32_t next_alias_group = 0;

  std::vector<uint32_t> transient_indices;
  for (uint32_t i = 0; i < m_resources.size(); ++i) {
    if (!m_resources[i].is_transient()) {
      continue;
    }
    if (export_or_present_resources.find(i) !=
        export_or_present_resources.end()) {
      m_resources[i].alias_group = next_alias_group++;
      continue;
    }
      transient_indices.push_back(i);
  }

  std::sort(transient_indices.begin(), transient_indices.end(), [this](uint32_t a, uint32_t b) {
              return m_resources[a].first_write_pass <
                     m_resources[b].first_write_pass;
            });
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
  std::unordered_map<int32_t, std::shared_ptr<RenderGraphImageResource>>
      group_images;
  std::unordered_map<int32_t, StorageBuffer *> group_storage_buffers;

  for (auto &resource : m_resources) {
    if (!resource.is_transient()) {
      continue;
    }

    if (group_images.find(resource.alias_group) != group_images.end()) {
      resource.set_content(group_images[resource.alias_group]);
      continue;
    }

    if (group_storage_buffers.find(resource.alias_group) !=
        group_storage_buffers.end()) {
      resource.set_content(group_storage_buffers[resource.alias_group]);
      continue;
    }

    switch (resource.desc.type) {
      case RenderGraphResourceType::Image: {
        const auto &spec = std::get<ImageDesc>(resource.desc.spec);

        auto image = std::make_shared<RenderGraphImageResource>();
        image->update_desc(spec);

        resource.set_content(image);
        group_images[resource.alias_group] = std::move(image);
        break;
      }

      case RenderGraphResourceType::StorageBuffer: {
        const auto &spec = std::get<StorageBufferSpec>(resource.desc.spec);

        auto backend = m_render_target->backend();

        auto storage_buffer = StorageBuffer::create(backend, spec.size);

        StorageBuffer *sb_ptr = storage_buffer.get();

        resource.set_content(sb_ptr);
        group_storage_buffers[resource.alias_group] = sb_ptr;
        m_transient_storage_buffers.push_back(std::move(storage_buffer));
        break;
      }

      case RenderGraphResourceType::LogicalBuffer: {
        const auto &spec = std::get<LogicalBufferSpec>(resource.desc.spec);

        auto block = m_resource_allocator.allocate(spec.size_hint);

        void *constructed = block;

        if (spec.constructor) {
          constructed = spec.constructor(block->data);
        } else {
          std::memset(block->data, 0, spec.size_hint);
        }

        resource.set_content(block->data);
        break;
      }

      default:
        break;
    }
  }

  for (auto &resource : m_resources) {
    if (resource.is_transient()) {
      continue;
    }

    if (resource.get_graph_image() != nullptr) {
      continue;
    }

    if (resource.desc.type == RenderGraphResourceType::Image) {
      const auto *spec = std::get_if<ImageDesc>(&resource.desc.spec);
      if (spec == nullptr) {
        continue;
      }

      auto image = std::make_shared<RenderGraphImageResource>();
      image->update_desc(*spec);
      resource.set_content(std::move(image));
    }
  }

  LOG_INFO("[RenderGraph] Transient resources created");
}

void RenderGraph::setup_passes() {
  rendering::SceneResidencyRequests requests;

  for (const auto &pass : m_passes) {
    if (pass->is_culled()) {
      continue;
    }

    for (const auto &dependency : pass->get_asset_dependencies()) {
      append_pass_dependency_requests(dependency, requests);
    }
  }

  rendering::resolve_scene_residency(requests, m_render_target);

  for (auto &pass : m_passes) {
    LOG_DEBUG("[RenderGraph] Setting up pass: ", pass->get_name());
    if (!pass->is_culled()) {
      std::vector<ResolvedRenderPassDependency> resolved_dependencies;
      resolved_dependencies.reserve(pass->get_asset_dependencies().size());

      for (const auto &dependency : pass->get_asset_dependencies()) {
        resolved_dependencies.push_back(resolve_pass_dependency(dependency));
      }

      pass->set_resolved_dependencies(std::move(resolved_dependencies));

      auto resources = collect_pass_resources(m_resources, *pass);
      pass->setup(m_render_target, std::move(resources));
    }
  }
}

void RenderGraph::execute(double dt, const rendering::SceneFrame *scene_frame) {
  ASTRA_PROFILE_N("RenderGraph::execute");
  m_latest_compiled_frame.clear();

  if (m_render_target != nullptr &&
      m_render_target->backend() == RendererBackend::OpenGL) {
    auto *api = m_render_target->renderer_api();
    api->reset_frame_stats();
    api->begin_gpu_timer();
  }

  for (const auto &compiled_export : m_compiled_exports) {
    const uint32_t resource_index =
        compiled_export.needs_materialize &&
                compiled_export.materialized_resource_index.has_value()
            ? *compiled_export.materialized_resource_index
            : compiled_export.source.resource_index();

    if (resource_index >= m_resources.size()) {
      continue;
    }

    ImageHandle image_handle{};
    ImageExtent extent{};

    if (const auto graph_image = m_resources[resource_index].get_graph_image();
        graph_image != nullptr) {
      image_handle = m_latest_compiled_frame.register_graph_image(
          "export:" + m_resources[resource_index].desc.name,
          graph_image,
          compiled_export.source.aspect
      );
      extent = graph_image->extent();
    } else {
      continue;
    }

    m_latest_compiled_frame.export_entries.push_back(
        CompiledExportEntry{
            .key = compiled_export.key,
            .image = image_handle,
            .extent = extent,
        }
    );
  }

  for (uint32_t pass_idx : m_execution_order) {
    auto &pass = m_passes[pass_idx];
    if (!pass->is_culled() && pass->is_enabled()) {
      auto resources = collect_pass_resources(m_resources, *pass);
      pass->record(dt, m_latest_compiled_frame, std::move(resources), scene_frame);
    }
  }

  for (const auto &present_edge : m_compiled_present_edges) {
    if (present_edge.resource_index < m_resources.size()) {
      const auto &resource = m_resources[present_edge.resource_index];
      if (const auto graph_image = resource.get_graph_image();
          graph_image != nullptr) {
        const auto image = m_latest_compiled_frame.register_graph_image(
            "present:" + resource.desc.name, graph_image, present_edge.aspect
        );
        m_latest_compiled_frame.present_edges.push_back(
            CompiledFramePresentEdge{
                .source =
                    ImageViewRef{
                        .image = image,
                        .aspect = present_edge.aspect,
                    },
                .extent = graph_image->extent(),
            }
        );
      }
    }
  }

  if (m_opengl_executor != nullptr && !m_latest_compiled_frame.empty()) {
    m_opengl_executor->execute(m_latest_compiled_frame);
  }

  if (m_render_target != nullptr &&
      m_render_target->backend() == RendererBackend::OpenGL) {
    auto *api = m_render_target->renderer_api();
    api->end_gpu_timer();
    float used_mb = 0.0f;
    float total_mb = 0.0f;
    api->query_gpu_memory(used_mb, total_mb);
    m_latest_frame_stats = api->frame_stats();
    m_latest_frame_stats.gpu_memory_used_mb = used_mb;
    m_latest_frame_stats.gpu_memory_total_mb = total_mb;
  }
}

void RenderGraph::cleanup() {
  for (auto &pass : m_passes) {
    pass->cleanup();
  }

  m_transient_storage_buffers.clear();
  m_opengl_executor.reset();
}

bool RenderGraph::has_lifetime_overlap(const RenderGraphResource &a, const RenderGraphResource &b) const {
  if (a.last_read_pass < b.first_write_pass)
    return false;
  if (b.last_read_pass < a.first_write_pass)
    return false;
  return true;
}

bool RenderGraph::are_resources_compatible(
    const RenderGraphResource &resource_a,
    const RenderGraphResource &resource_b
) const {
  if (resource_a.desc.type != resource_b.desc.type)
    return false;

  switch (resource_a.desc.type) {
    case RenderGraphResourceType::Image: {
      const auto &spec_a = std::get<ImageDesc>(resource_a.desc.spec);
      const auto &spec_b = std::get<ImageDesc>(resource_b.desc.spec);

      return spec_a.width == spec_b.width && spec_a.height == spec_b.height &&
             spec_a.depth == spec_b.depth &&
             spec_a.mip_levels == spec_b.mip_levels &&
             spec_a.samples == spec_b.samples &&
             spec_a.format == spec_b.format;
    }

    case RenderGraphResourceType::Buffer: {
      const auto &spec_a = std::get<BufferDesc>(resource_a.desc.spec);
      const auto &spec_b = std::get<BufferDesc>(resource_b.desc.spec);

      return spec_a.size == spec_b.size && spec_a.usage == spec_b.usage;
    }

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

void RenderGraph::validate_graph() {
  struct SubresourceWriterKey {
    uint32_t resource_index;
    uint8_t aspect;
    uint32_t mip;
    uint32_t layer;

    bool operator==(const SubresourceWriterKey &) const = default;
  };

  struct SubresourceWriterKeyHash {
    size_t operator()(const SubresourceWriterKey &key) const {
      size_t seed = std::hash<uint32_t>{}(key.resource_index);
      seed ^= std::hash<uint8_t>{}(key.aspect) + 0x9e3779b9 +
              (seed << 6u) + (seed >> 2u);
      seed ^= std::hash<uint32_t>{}(key.mip) + 0x9e3779b9 +
              (seed << 6u) + (seed >> 2u);
      seed ^= std::hash<uint32_t>{}(key.layer) + 0x9e3779b9 +
              (seed << 6u) + (seed >> 2u);
      return seed;
    }
  };

  struct WriterRecord {
    uint32_t pass_index = 0;
    std::string pass_name;
  };

  std::unordered_map<SubresourceWriterKey, WriterRecord, SubresourceWriterKeyHash>
      last_writer;

  for (uint32_t pass_idx : m_execution_order) {
    const auto &pass = m_passes[pass_idx];
    if (pass->is_culled() || !pass->is_enabled()) {
      continue;
    }

    for (const auto &usage : pass->get_image_usages()) {
      const uint32_t resource_index = usage.view.resource_index();
      if (resource_index >= m_resources.size()) {
        LOG_WARN("[RenderGraph] Pass '", pass->get_name(), "' references out-of-bounds resource index ", resource_index);
        continue;
      }

      const auto &resource = m_resources[resource_index];

      if (is_read_usage(usage.usage) && resource.is_transient() &&
          resource.first_write_pass < 0) {
        LOG_WARN("[RenderGraph] Pass '", pass->get_name(), "' reads transient resource '", resource.desc.name, "' which has no producer");
      }

      if (usage.usage == RenderUsage::Present && resource.is_transient()) {
        LOG_WARN("[RenderGraph] Pass '", pass->get_name(), "' declares Present on transient resource '", resource.desc.name, "' (should be persistent)");
      }

      if (is_write_usage(usage.usage)) {
        const SubresourceWriterKey key{
            .resource_index = resource_index,
            .aspect = static_cast<uint8_t>(usage.view.aspect),
            .mip = usage.view.mip,
            .layer = usage.view.layer,
        };

        auto [iterator, inserted] = last_writer.try_emplace(
            key,
            WriterRecord{.pass_index = pass_idx, .pass_name = pass->get_name()}
        );

        if (!inserted) {
          const auto &previous = iterator->second;
          bool has_ordering = false;
          for (uint32_t dep_idx :
               pass->get_computed_dependency_indices()) {
            if (dep_idx == previous.pass_index) {
              has_ordering = true;
              break;
            }
          }

          if (!has_ordering) {
            LOG_WARN("[RenderGraph] Passes '", previous.pass_name, "' and '", pass->get_name(), "' both write to resource '", resource.desc.name, "' without explicit ordering");
          }

          iterator->second = WriterRecord{
              .pass_index = pass_idx, .pass_name = pass->get_name()
          };
        }
      }

      if (usage.usage == RenderUsage::ResolveSrc &&
          resource.desc.type == RenderGraphResourceType::Image) {
        const auto *spec = std::get_if<ImageDesc>(&resource.desc.spec);
        if (spec != nullptr && spec->samples <= 1) {
          LOG_WARN("[RenderGraph] Pass '", pass->get_name(), "' declares ResolveSrc on single-sampled resource '", resource.desc.name, "'");
        }
      }

      if (usage.usage == RenderUsage::ResolveDst &&
          resource.desc.type == RenderGraphResourceType::Image) {
        const auto *spec = std::get_if<ImageDesc>(&resource.desc.spec);
        if (spec != nullptr && spec->samples > 1) {
          LOG_WARN("[RenderGraph] Pass '", pass->get_name(), "' declares ResolveDst on multi-sampled resource '", resource.desc.name, "' (resolve destination must be single-sampled)");
        }
      }
    }

    for (const auto &export_request : pass->get_exports()) {
      const uint32_t resource_index = export_request.resource_index;
      bool declared_in_usages = false;
      for (const auto &usage : pass->get_image_usages()) {
        if (usage.view.resource_index() == resource_index) {
          declared_in_usages = true;
          break;
        }
      }
      if (!declared_in_usages) {
        LOG_WARN("[RenderGraph] Pass '", pass->get_name(), "' exports resource index ", resource_index, " but does not declare it in usages");
      }
    }
  }

  LOG_DEBUG("[RenderGraph] Validation complete");
}

void RenderGraph::compile_transitions() {
  struct SubresourceKey {
    uint32_t resource_index;
    uint8_t aspect;
    uint32_t mip;
    uint32_t layer;

    bool operator==(const SubresourceKey &) const = default;
  };

  struct SubresourceKeyHash {
    size_t operator()(const SubresourceKey &key) const {
      size_t seed = std::hash<uint32_t>{}(key.resource_index);
      seed ^= std::hash<uint8_t>{}(key.aspect) + 0x9e3779b9 + (seed << 6u) + (seed >> 2u);
      seed ^= std::hash<uint32_t>{}(key.mip) + 0x9e3779b9 + (seed << 6u) + (seed >> 2u);
      seed ^= std::hash<uint32_t>{}(key.layer) + 0x9e3779b9 + (seed << 6u) + (seed >> 2u);
      return seed;
    }
  };

  std::unordered_map<SubresourceKey, ResourceState, SubresourceKeyHash>
      current_states;

  for (uint32_t pass_idx : m_execution_order) {
    auto &pass = m_passes[pass_idx];
    if (pass->is_culled() || !pass->is_enabled()) {
      continue;
    }

    const auto &usages = pass->get_image_usages();
    std::vector<CompiledTransition> transitions;

    for (const auto &usage : usages) {
      const ResourceState required = usage_to_state(usage.usage);
      const SubresourceKey key{
          .resource_index = usage.view.resource_index(),
          .aspect = static_cast<uint8_t>(usage.view.aspect),
          .mip = usage.view.mip,
          .layer = usage.view.layer,
      };

      auto [state_iterator, inserted] =
          current_states.try_emplace(key, ResourceState::Undefined);
      const ResourceState previous = state_iterator->second;

      if (previous != required) {
        transitions.push_back(CompiledTransition{
            .view = usage.view,
            .before = previous,
            .after = required,
        });
        state_iterator->second = required;
      }
    }

    pass->set_compiled_transitions(std::move(transitions));
  }

  LOG_DEBUG("[RenderGraph] Transitions compiled");
}

void RenderGraph::compile_exports() {
  m_compiled_exports.clear();

  for (uint32_t pass_idx : m_execution_order) {
    const auto &pass = m_passes[pass_idx];
    if (pass->is_culled() || !pass->is_enabled()) {
      continue;
    }

    for (const auto &export_request : pass->get_exports()) {
      const uint32_t resource_index = export_request.resource_index;

      bool is_single_sampled = true;
      if (resource_index < m_resources.size()) {
        const auto &resource = m_resources[resource_index];
        const auto *spec = std::get_if<ImageDesc>(&resource.desc.spec);
        if (spec != nullptr && spec->samples > 1) {
          is_single_sampled = false;
        }
      }

      const bool depth_aspect = export_request.aspect == ImageAspect::Depth;
      const bool direct = is_single_sampled && !depth_aspect;

      std::optional<uint32_t> materialized_index;

      if (!direct) {
        if (!is_single_sampled) {
          for (const auto &usage : pass->get_image_usages()) {
            if (usage.usage == RenderUsage::ResolveDst) {
              materialized_index = usage.view.resource_index();
              break;
            }
          }

          if (!materialized_index.has_value()) {
            LOG_WARN("[RenderGraph] Export from '", m_resources[resource_index].desc.name, "' requires MSAA resolve but no ResolveDst found in pass '", pass->get_name(), "'");
          }
        }

        if (depth_aspect) {
          materialized_index = resource_index;
        }
      }

      m_compiled_exports.push_back(CompiledExportImage{
          .key = export_request.key,
          .source = export_request.to_subresource(),
          .direct_bind = direct,
          .needs_materialize = !direct,
          .materialized_resource_index = materialized_index,
      });
    }
  }

  LOG_DEBUG("[RenderGraph] Exports compiled: ", m_compiled_exports.size());
}

void RenderGraph::compile_present_edges() {
  m_compiled_present_edges.clear();

  for (uint32_t pass_idx : m_execution_order) {
    const auto &pass = m_passes[pass_idx];

    if (pass->is_culled() || !pass->is_enabled() || !pass->has_present()) {
      continue;
    }

    const auto &present_request = pass->get_present();
    m_compiled_present_edges.push_back(CompiledPresentEdge{
        .resource_index = present_request->source.resource_index(),
        .aspect = present_request->source.aspect,
        .producer_pass_index = pass_idx,
    });
  }

  LOG_DEBUG("[RenderGraph] Present edges compiled: ", m_compiled_present_edges.size());
}

void RenderGraph::export_graph(const RenderGraphExporter &exporter, const std::string &filename) const {
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
