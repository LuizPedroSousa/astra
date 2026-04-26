#pragma once

#include "assert.hpp"
#include "base.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/passes/render-pass-dependency.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace astralix {

namespace rendering {
struct SceneFrame;
}

class RenderTarget;
struct RenderGraphResource;
class PassRecorder;
struct CompiledFrame;

inline const RenderGraphResource *find_graph_image_resource(
    const std::vector<const RenderGraphResource *> &resources,
    const std::string &name
) {
  for (const auto *resource : resources) {
    if (resource != nullptr && resource->desc.type == RenderGraphResourceType::Image &&
        resource->desc.name == name && resource->get_graph_image() != nullptr) {
      return resource;
    }
  }

  return nullptr;
}

inline const RenderPassDependencyDeclaration *find_declared_dependency(
    const std::vector<RenderPassDependencyDeclaration> &dependencies,
    std::string_view slot
) {
  for (const auto &dependency : dependencies) {
    if (dependency.slot == slot) {
      return &dependency;
    }
  }

  return nullptr;
}

inline const ResolvedRenderPassDependency *find_resolved_dependency(
    const std::vector<ResolvedRenderPassDependency> &dependencies,
    std::string_view slot
) {
  for (const auto &dependency : dependencies) {
    if (dependency.declaration.slot == slot) {
      return &dependency;
    }
  }

  return nullptr;
}

template <typename T>
inline Ref<T> find_dependency_resource(
    const std::vector<ResolvedRenderPassDependency> *dependencies,
    std::string_view slot
) {
  if (dependencies == nullptr) {
    return nullptr;
  }

  const auto *dependency = find_resolved_dependency(*dependencies, slot);
  if (dependency == nullptr) {
    return nullptr;
  }

  const auto *resource = std::get_if<Ref<T>>(&dependency->resource);
  return resource != nullptr ? *resource : nullptr;
}

struct PassSetupContext {
  Ref<RenderTarget> render_target = nullptr;
  const std::vector<const RenderGraphResource *> *resources = nullptr;
  const std::vector<RenderPassDependencyDeclaration> *declared_dependencies =
      nullptr;
  const std::vector<ResolvedRenderPassDependency> *resolved_dependencies =
      nullptr;

  Ref<RenderTarget> target() const { return render_target; }

  const std::vector<const RenderGraphResource *> &resource_views() const {
    static const std::vector<const RenderGraphResource *> k_empty;
    return resources != nullptr ? *resources : k_empty;
  }

  const RenderGraphResource *find_graph_image(
      const std::string &name
  ) const {
    return find_graph_image_resource(resource_views(), name);
  }

  const RenderPassDependencyDeclaration *find_dependency(
      std::string_view slot
  ) const {
    static const std::vector<RenderPassDependencyDeclaration> k_empty;
    return find_declared_dependency(
        declared_dependencies != nullptr ? *declared_dependencies : k_empty, slot
    );
  }

  Ref<Shader> find_shader(std::string_view slot) const {
    return find_dependency_resource<Shader>(resolved_dependencies, slot);
  }

  Ref<Texture2D> find_texture_2d(std::string_view slot) const {
    return find_dependency_resource<Texture2D>(resolved_dependencies, slot);
  }

  Ref<Shader> require_shader(std::string_view slot) const {
    return find_shader(slot);
  }

  Ref<Texture2D> require_texture_2d(std::string_view slot) const {
    return find_texture_2d(slot);
  }
};

struct PassRecordContext {
  double dt = 0.0;
  const std::vector<const RenderGraphResource *> *resources = nullptr;
  CompiledFrame *compiled_frame = nullptr;
  const rendering::SceneFrame *scene_frame = nullptr;
  const std::vector<ResolvedRenderPassDependency> *resolved_dependencies =
      nullptr;

  const std::vector<const RenderGraphResource *> &resource_views() const {
    static const std::vector<const RenderGraphResource *> k_empty;
    return resources != nullptr ? *resources : k_empty;
  }

  const RenderGraphResource *find_graph_image(
      const std::string &name
  ) const {
    return find_graph_image_resource(resource_views(), name);
  }

  CompiledFrame &frame() const {
    ASTRA_ENSURE(compiled_frame == nullptr, "PassRecordContext is missing a compiled frame");
    return *compiled_frame;
  }

  ImageHandle register_graph_image(
      const std::string &debug_name, const RenderGraphResource &resource,
      ImageAspect aspect = ImageAspect::Color0
  ) const {
    const auto image = resource.get_graph_image();
    ASTRA_ENSURE(image == nullptr, "PassRecordContext::register_graph_image requires an image "
                                   "resource");
    return frame().register_graph_image(debug_name, image, aspect);
  }

  ImageExtent graph_image_extent(const RenderGraphResource &resource) const {
    const auto image = resource.get_graph_image();
    ASTRA_ENSURE(image == nullptr, "PassRecordContext::graph_image_extent requires an image "
                                   "resource");
    return image->extent();
  }

  const rendering::SceneFrame *scene() const noexcept { return scene_frame; }

  Ref<Shader> find_shader(std::string_view slot) const {
    return find_dependency_resource<Shader>(resolved_dependencies, slot);
  }

  Ref<Texture2D> find_texture_2d(std::string_view slot) const {
    return find_dependency_resource<Texture2D>(resolved_dependencies, slot);
  }

  Ref<Shader> require_shader(std::string_view slot) const {
    return find_shader(slot);
  }

  Ref<Texture2D> require_texture_2d(std::string_view slot) const {
    return find_texture_2d(slot);
  }
};

class PassBase {
public:
  virtual ~PassBase() = default;

  virtual std::string name() const = 0;
  virtual bool is_enabled() const { return m_enabled; }
  virtual void set_enabled(bool enabled) { m_enabled = enabled; }
  virtual bool has_side_effects() const { return false; }

  virtual int priority() const { return m_priority; }
  virtual void set_priority(int priority) { m_priority = priority; }

protected:
  PassBase() = default;

  int m_priority = -1;
  bool m_enabled = true;
};

class FramePass : public PassBase {
public:
  virtual ~FramePass() = default;

  virtual void setup(PassSetupContext &ctx) = 0;
  virtual void record(PassRecordContext &ctx, PassRecorder &recorder) = 0;
  virtual void cleanup() {}
};

} // namespace astralix
