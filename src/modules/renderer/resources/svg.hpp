#pragma once

#include "guid.hpp"
#include "path.hpp"
#include "resources/descriptors/svg-descriptor.hpp"
#include "resources/resource.hpp"
#include "resources/svg-compiler.hpp"

namespace astralix {

class Svg : public Resource {
public:
  Svg(const ResourceHandle &resource_id, Ref<SvgDescriptor> descriptor);

  static Ref<SvgDescriptor> create(const ResourceDescriptorID &id, Ref<Path> path);
  static Ref<SvgDescriptor> define(const ResourceDescriptorID &id, Ref<Path> path);
  static Ref<Svg> from_descriptor(const ResourceHandle &id, Ref<SvgDescriptor> descriptor);

  float width() const { return m_document.width; }
  float height() const { return m_document.height; }
  const std::vector<SvgTriangleBatch> &batches() const { return m_document.batches; }
  ResourceDescriptorID descriptor_id() const { return m_descriptor_id; }

private:
  ResourceDescriptorID m_descriptor_id;
  SvgDocumentData m_document;
};

} // namespace astralix
