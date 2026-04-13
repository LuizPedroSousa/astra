#include "font.hpp"
#include "assert.hpp"
#include "guid.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/font-descriptor.hpp"
#include "resources/texture.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include "glad/glad.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <utility>

namespace astralix {

Font::Font(const ResourceHandle &resource_id, Ref<FontDescriptor> descriptor)
    : Resource(resource_id), m_descriptor_id(descriptor->id),
      m_path(descriptor->path), m_backend(descriptor->backend) {
  load();
};

void Font::load() {
  ensure_size_loaded(48);
}

void Font::ensure_size_loaded(uint32_t pixel_size) const {
  if (m_glyph_sets.find(pixel_size) != m_glyph_sets.end()) {
    return;
  }

  FT_Library ft;

  ASTRA_ENSURE(FT_Init_FreeType(&ft), "ERROR::FREETYPE: Could not init FreeType Library");

  FT_Face face;

  auto base_path = path_manager()->resolve(m_path);

  ASTRA_ENSURE(FT_New_Face(ft, base_path.c_str(), 0, &face), "ERROR::FREETYPE: Failed to load font");

  FT_Set_Pixel_Sizes(face, 0, pixel_size);

  ASTRA_ENSURE(FT_Load_Char(face, 'X', FT_LOAD_RENDER), "ERROR::FREETYTPE: Failed to load Glyph");

  if (m_backend == RendererBackend::OpenGL) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  }

  GlyphSet glyph_set;
  glyph_set.glyphs.reserve(128u);

  auto resource_manager = ResourceManager::get();

  for (unsigned char c = 0; c < 128; c++) {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
      std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
      continue;
    }

    auto config = TextureConfig{
        .width = face->glyph->bitmap.width,
        .height = face->glyph->bitmap.rows,
        .bitmap = false,
        .format = TextureFormat::Red,
        .parameters = {{TextureParameter::WrapS, TextureValue::ClampToBorder}, {TextureParameter::WrapT, TextureValue::ClampToBorder}, {TextureParameter::MagFilter, TextureValue::Linear}, {TextureParameter::MinFilter, TextureValue::Linear}},
        .buffer = face->glyph->bitmap.buffer,

    };

    auto texture_id = m_descriptor_id + std::string("::glyph[") +
                      std::to_string(pixel_size) + std::string("][") +
                      std::to_string(c) + std::string("]");

    auto texture = resource_manager->register_texture(
        Texture2D::define(texture_id, config)
    );
    resource_manager->load_from_descriptors_by_ids<Texture2DDescriptor>(
        m_backend, {texture_id}
    );

    CharacterGlyph character = {
        .texture_id = texture->id,
        .size = glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
        .bearing =
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
        .advance = static_cast<unsigned int>(face->glyph->advance.x)
    };

    const GlyphHandle handle =
        static_cast<GlyphHandle>(glyph_set.glyphs.size());
    glyph_set.glyph_lut[c] = handle;
    glyph_set.glyphs.push_back(std::move(character));
  }

  glyph_set.line_height = static_cast<float>(face->size->metrics.height >> 6);
  glyph_set.ascent = static_cast<float>(face->size->metrics.ascender >> 6);
  glyph_set.descent =
      static_cast<float>(std::abs(face->size->metrics.descender >> 6));

  m_glyph_sets.emplace(pixel_size, std::move(glyph_set));

  FT_Done_Face(face);
  FT_Done_FreeType(ft);
}

const std::vector<CharacterGlyph> &Font::glyphs(uint32_t pixel_size) const {
  ensure_size_loaded(pixel_size);
  return m_glyph_sets.at(pixel_size).glyphs;
}

const std::array<GlyphHandle, 256u> &Font::glyph_lut(uint32_t pixel_size) const {
  ensure_size_loaded(pixel_size);
  return m_glyph_sets.at(pixel_size).glyph_lut;
}

GlyphHandle Font::glyph_handle(char character, uint32_t pixel_size) const {
  const auto &lut = glyph_lut(pixel_size);
  return lut[static_cast<unsigned char>(character)];
}

const CharacterGlyph &Font::glyph(GlyphHandle handle, uint32_t pixel_size) const {
  ensure_size_loaded(pixel_size);
  return m_glyph_sets.at(pixel_size).glyphs[handle];
}

glm::vec2 Font::measure_text(const std::string &text, float pixel_size) const {
  const uint32_t resolved_size =
      static_cast<uint32_t>(std::max(1.0f, std::round(pixel_size)));
  ensure_size_loaded(resolved_size);
  const auto &glyph_set = m_glyph_sets.at(resolved_size);

  float width = 0.0f;
  for (const char character : text) {
    const GlyphHandle handle =
        glyph_set.glyph_lut[static_cast<unsigned char>(character)];
    if (handle == k_invalid_glyph_handle) {
      continue;
    }

    width += static_cast<float>(glyph_set.glyphs[handle].advance >> 6);
  }

  return glm::vec2(width, line_height(resolved_size));
}

float Font::line_height(float pixel_size) const {
  const uint32_t resolved_size =
      static_cast<uint32_t>(std::max(1.0f, std::round(pixel_size)));
  ensure_size_loaded(resolved_size);
  return m_glyph_sets.at(resolved_size).line_height;
}

float Font::ascent(float pixel_size) const {
  const uint32_t resolved_size =
      static_cast<uint32_t>(std::max(1.0f, std::round(pixel_size)));
  ensure_size_loaded(resolved_size);
  return m_glyph_sets.at(resolved_size).ascent;
}

Ref<FontDescriptor> Font::create(const ResourceDescriptorID &id, const Ref<Path> &font_path) {

  return resource_manager()->register_font(
      FontDescriptor::create(id, font_path)
  );
}

Ref<FontDescriptor> Font::define(const ResourceDescriptorID &id, const Ref<Path> &font_path) {
  return FontDescriptor::create(id, font_path);
}

Ref<Font> Font::from_descriptor(const ResourceHandle &id, Ref<FontDescriptor> descriptor) {
  return create_ref<Font>(id, descriptor);
}

} // namespace astralix
