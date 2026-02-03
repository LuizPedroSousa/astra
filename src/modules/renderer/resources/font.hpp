#pragma once
#include "base.hpp"
#include "glm/glm.hpp"
#include "guid.hpp"
#include "path.hpp"
#include "resources/descriptors/font-descriptor.hpp"
#include "resources/resource.hpp"
#include <map>

namespace astralix {
struct CharacterGlyph {
  ResourceDescriptorID texture_id;
  glm::ivec2 size;
  glm::ivec2 bearing;
  unsigned int advance;
};

class Font : public Resource {

public:
  Font(const ResourceHandle &resource_id, Ref<FontDescriptor> descriptor);
  static Ref<FontDescriptor> create(const ResourceDescriptorID &id,
                                    const Ref<Path> &font_path);

  static Ref<FontDescriptor> define(const ResourceDescriptorID &id,
                                    const Ref<Path> &font_path);

  static Ref<Font> from_descriptor(const ResourceHandle &id,
                                   Ref<FontDescriptor> descriptor);

  void load();

  std::map<char, CharacterGlyph> characters() const { return m_characters; }

private:
  std::map<char, CharacterGlyph> m_characters;
  ResourceDescriptorID m_descriptor_id;
  Ref<Path> m_path;
};
} // namespace astralix
