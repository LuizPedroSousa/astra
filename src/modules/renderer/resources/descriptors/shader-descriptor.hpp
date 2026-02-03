#pragma once

#include "base.hpp"
#include "guid.hpp"
#include "path.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/resource-descriptor.hpp"

namespace astralix {
struct ShaderDescriptor {
public:
  static Ref<ShaderDescriptor> create(const ResourceDescriptorID &id,
                                      Ref<Path> fragment_path,
                                      Ref<Path> vertex_path,
                                      Ref<Path> geometry_path = nullptr);

  ShaderDescriptor(const ResourceDescriptorID &id, Ref<Path> fragment_path,
                   Ref<Path> vertex_path, Ref<Path> geometry_path)
      : RESOURCE_DESCRIPTOR_INIT(), fragment_path(fragment_path),
        vertex_path(vertex_path), geometry_path(geometry_path) {}

  RESOURCE_DESCRIPTOR_PARAMS;

  Ref<Path> fragment_path = nullptr;
  Ref<Path> vertex_path = nullptr;
  Ref<Path> geometry_path = nullptr;
  RendererBackend backend = RendererBackend::None;
};

} // namespace astralix
