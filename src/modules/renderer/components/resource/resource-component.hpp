#pragma once
#include "components/component.hpp"
#include "guid.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/shader.hpp"

namespace astralix {

struct TextureRenderData {
  ResourceDescriptorID id;
  std::string name;
};

class ResourceComponent : public Component<ResourceComponent> {
public:
  ResourceComponent(COMPONENT_INIT_PARAMS);
  ResourceComponent() {}

  void start();
  void update();

  ResourceComponent *attach_texture(TextureRenderData data);
  ResourceComponent *attach_cubemap(TextureRenderData data);
  ResourceComponent *attach_shader(ResourceDescriptorID id);
  ResourceComponent *set_shader(ResourceDescriptorID id);

  bool has_shader() { return m_shader != nullptr; };

  Ref<Shader> &shader() { return m_shader; }

private:
  Ref<Shader> m_shader;
  ResourceDescriptorID m_shader_descriptor_id;
  std::vector<TextureRenderData> m_textures;
  std::vector<TextureRenderData> m_cubemaps;
};

} // namespace astralix
