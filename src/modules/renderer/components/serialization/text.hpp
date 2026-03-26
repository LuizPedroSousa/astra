#pragma once

#include "components/text.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const rendering::TextSprite &sprite) {
  ComponentSnapshot snapshot{.name = "TextSprite"};
  snapshot.fields.push_back({"text", sprite.text});
  snapshot.fields.push_back({"font_id", sprite.font_id});
  serialization::fields::append_vec2(snapshot.fields, "position",
                                     sprite.position);
  snapshot.fields.push_back({"scale", sprite.scale});
  serialization::fields::append_vec3(snapshot.fields, "color", sprite.color);
  return snapshot;
}

inline void apply_text_sprite_snapshot(ecs::EntityRef entity,
                                       const serialization::fields::FieldList
                                           &fields) {
  entity.emplace<rendering::TextSprite>(rendering::TextSprite{
      .text = serialization::fields::read_string(fields, "text").value_or(""),
      .font_id =
          serialization::fields::read_string(fields, "font_id").value_or(""),
      .position = serialization::fields::read_vec2(fields, "position"),
      .scale = serialization::fields::read_float(fields, "scale").value_or(1.0f),
      .color = serialization::fields::read_vec3(fields, "color",
                                                glm::vec3(1.0f)),
  });
}

} // namespace astralix::serialization
