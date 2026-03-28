#pragma once
#include "base.hpp"
#include "glm/glm.hpp"
#include "guid.hpp"
#include "path.hpp"
#include "resources/descriptors/font-descriptor.hpp"
#include "resources/resource.hpp"
#include <map>
#include <optional>
#include <unordered_map>

namespace astralix {
struct CharacterGlyph {
  ResourceDescriptorID texture_id;
  glm::ivec2 size;
  glm::ivec2 bearing;
  unsigned int advance;
};

class Font : public Resource {

  struct GlyphSet {
    std::map<char, CharacterGlyph> characters;
    float line_height = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
  };

public:
  Font(const ResourceHandle &resource_id, Ref<FontDescriptor> descriptor);
  static Ref<FontDescriptor> create(const ResourceDescriptorID &id,
                                    const Ref<Path> &font_path);

  static Ref<FontDescriptor> define(const ResourceDescriptorID &id,
                                    const Ref<Path> &font_path);

  static Ref<Font> from_descriptor(const ResourceHandle &id,
                                   Ref<FontDescriptor> descriptor);

  void load();

  const std::map<char, CharacterGlyph> &characters(uint32_t pixel_size = 48) const;
  std::optional<CharacterGlyph> glyph(char character,
                                      uint32_t pixel_size = 48) const;
  glm::vec2 measure_text(const std::string &text, float pixel_size) const;
  float line_height(float pixel_size) const;
  float ascent(float pixel_size) const;

private:
  void ensure_size_loaded(uint32_t pixel_size) const;

  mutable std::unordered_map<uint32_t, GlyphSet> m_glyph_sets;
  ResourceDescriptorID m_descriptor_id;
  Ref<Path> m_path;
  RendererBackend m_backend = RendererBackend::None;
};
} // namespace astralix
