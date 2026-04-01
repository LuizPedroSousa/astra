#include "resources/svg.hpp"

#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"

#include <utility>

namespace astralix {

Svg::Svg(const ResourceHandle &resource_id, Ref<SvgDescriptor> descriptor)
    : Resource(resource_id), m_descriptor_id(descriptor->id) {
  m_document = compile_svg_file(path_manager()->resolve(descriptor->path));
}

Ref<SvgDescriptor> Svg::create(const ResourceDescriptorID &id, Ref<Path> path) {
  return resource_manager()->register_svg(SvgDescriptor::create(id, std::move(path)));
}

Ref<SvgDescriptor> Svg::define(const ResourceDescriptorID &id, Ref<Path> path) {
  return SvgDescriptor::create(id, std::move(path));
}

Ref<Svg> Svg::from_descriptor(
    const ResourceHandle &id,
    Ref<SvgDescriptor> descriptor
) {
  return create_ref<Svg>(id, std::move(descriptor));
}

} // namespace astralix
