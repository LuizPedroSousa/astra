#pragma once

#include "components/camera.hpp"
#include "components/skybox.hpp"
#include "components/tags.hpp"
#include "components/text.hpp"
#include "components/transform.hpp"
#include "world.hpp"
#include <optional>
#include <vector>

namespace astralix::rendering {

struct CameraSelection {
  EntityID entity_id;
  scene::Transform *transform = nullptr;
  Camera *camera = nullptr;
};

struct SkyboxSelection {
  EntityID entity_id;
  SkyboxBinding *skybox = nullptr;
};

struct TextSpriteSelection {
  EntityID entity_id;
  TextSprite *sprite = nullptr;
};

inline std::optional<CameraSelection> select_main_camera(ecs::World &world) {
  std::optional<CameraSelection> selection;

  world.each<MainCamera, scene::Transform, Camera>(
      [&](EntityID entity_id, MainCamera &, scene::Transform &transform,
          Camera &camera) {
        if (!selection.has_value() && world.active(entity_id)) {
          selection = CameraSelection{
              .entity_id = entity_id,
              .transform = &transform,
              .camera = &camera,
          };
        }
      });

  if (selection.has_value()) {
    return selection;
  }

  world.each<scene::Transform, Camera>(
      [&](EntityID entity_id, scene::Transform &transform, Camera &camera) {
        if (!selection.has_value() && world.active(entity_id)) {
          selection = CameraSelection{
              .entity_id = entity_id,
              .transform = &transform,
              .camera = &camera,
          };
        }
      });

  return selection;
}

inline std::optional<SkyboxSelection> select_skybox(ecs::World &world) {
  std::optional<SkyboxSelection> selection;

  world.each<SkyboxBinding>([&](EntityID entity_id, SkyboxBinding &skybox) {
    if (!selection.has_value() && world.active(entity_id)) {
      selection = SkyboxSelection{
          .entity_id = entity_id,
          .skybox = &skybox,
      };
    }
  });

  return selection;
}

inline std::vector<TextSpriteSelection> collect_text_sprites(ecs::World &world) {
  std::vector<TextSpriteSelection> selections;

  world.each<TextSprite>([&](EntityID entity_id, TextSprite &sprite) {
    if (!world.active(entity_id)) {
      return;
    }

    selections.push_back(TextSpriteSelection{
        .entity_id = entity_id,
        .sprite = &sprite,
    });
  });

  return selections;
}

} // namespace astralix::rendering
