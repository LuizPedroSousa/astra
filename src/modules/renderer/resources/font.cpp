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

#include <iostream>

namespace astralix {

Font::Font(const ResourceHandle &resource_id, Ref<FontDescriptor> descriptor)
    : Resource(resource_id), m_descriptor_id(descriptor->id),
      m_path(descriptor->path) {
  load();
};

void Font::load() {
  FT_Library ft;

  ASTRA_ENSURE(FT_Init_FreeType(&ft),
               "ERROR::FREETYPE: Could not init FreeType Library");

  FT_Face face;

  auto base_path = path_manager()->resolve(m_path);

  ASTRA_ENSURE(FT_New_Face(ft, base_path.c_str(), 0, &face),
               "ERROR::FREETYPE: Failed to load font");

  FT_Set_Pixel_Sizes(face, 0, 48);

  ASTRA_ENSURE(FT_Load_Char(face, 'X', FT_LOAD_RENDER),
               "ERROR::FREETYTPE: Failed to load Glyph");

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  std::map<char, CharacterGlyph> characters;

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
        .parameters = {{TextureParameter::WrapS, TextureValue::ClampToBorder},
                       {TextureParameter::WrapT, TextureValue::ClampToBorder},
                       {TextureParameter::MagFilter, TextureValue::Linear},
                       {TextureParameter::MinFilter, TextureValue::Linear}},
        .buffer = face->glyph->bitmap.buffer,

    };

    auto texture_id = m_descriptor_id + std::string("glyph[") +
                      std::to_string(c) + std::string("]");

    auto texture = resource_manager->register_texture(
        Texture2D::create(texture_id, config));

    CharacterGlyph character = {
        .texture_id = texture->id,
        .size = glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
        .bearing =
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
        .advance = static_cast<unsigned int>(face->glyph->advance.x)};

    characters.insert(std::pair<char, CharacterGlyph>(c, character));
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft);
}

Ref<FontDescriptor> Font::create(const ResourceDescriptorID &id,
                                 const Ref<Path> &font_path) {

  return resource_manager()->register_font(
      FontDescriptor::create(id, font_path));
}

Ref<FontDescriptor> Font::define(const ResourceDescriptorID &id,
                                 const Ref<Path> &font_path) {
  return FontDescriptor::create(id, font_path);
}

Ref<Font> Font::from_descriptor(const ResourceHandle &id,
                                Ref<FontDescriptor> descriptor) {
  return create_ref<Font>(id, descriptor);
}

} // namespace astralix
