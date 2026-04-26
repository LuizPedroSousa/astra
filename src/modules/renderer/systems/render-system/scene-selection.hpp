#pragma once

#include "components/camera.hpp"
#include "components/skybox.hpp"
#include "components/tags.hpp"
#include "components/text.hpp"
#include "components/transform.hpp"
#include "components/ui.hpp"
#include "render-frame.hpp"
#include "world.hpp"
#include <algorithm>
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
      [&](EntityID entity_id, MainCamera &, scene::Transform &transform, Camera &camera) {
        if (!selection.has_value() && world.active(entity_id)) {
          selection = CameraSelection{
              .entity_id = entity_id,
              .transform = &transform,
              .camera = &camera,
          };
        }
      }
  );

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
      }
  );

  return selection;
}

inline std::optional<CameraFrame> extract_main_camera_frame(ecs::World &world) {
  const auto selection = select_main_camera(world);
  if (!selection.has_value() || selection->transform == nullptr ||
      selection->camera == nullptr) {
    return std::nullopt;
  }

  return CameraFrame{
      .entity_id = selection->entity_id,
      .position = selection->transform->position,
      .forward = selection->camera->front,
      .up = selection->camera->up,
      .view = selection->camera->view_matrix,
      .projection = selection->camera->projection_matrix,
      .orthographic = selection->camera->orthographic,
      .fov_degrees = selection->camera->fov_degrees,
      .orthographic_scale = selection->camera->orthographic_scale,
  };
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

inline std::optional<SkyboxFrame> extract_skybox_frame(ecs::World &world) {
  const auto selection = select_skybox(world);
  if (!selection.has_value() || selection->skybox == nullptr) {
    return std::nullopt;
  }

  return SkyboxFrame{
      .entity_id = selection->entity_id,
      .shader_id = selection->skybox->shader,
      .cubemap_id = selection->skybox->cubemap,
  };
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

inline std::vector<TextDrawItem> extract_text_items(ecs::World &world) {
  std::vector<TextDrawItem> items;
  const auto selections = collect_text_sprites(world);
  items.reserve(selections.size());

  for (const auto &selection : selections) {
    if (selection.sprite == nullptr) {
      continue;
    }

    items.push_back(TextDrawItem{
        .entity_id = selection.entity_id,
        .sprite = *selection.sprite,
    });
  }

  return items;
}

inline std::vector<UIRootDrawList> extract_ui_roots(ecs::World &world) {
  std::vector<UIRootDrawList> roots;

  world.each<UIRoot>([&](EntityID entity_id, UIRoot &root) {
    if (!world.active(entity_id) || !root.visible || root.document == nullptr) {
      return;
    }

    roots.push_back(UIRootDrawList{
        .entity_id = entity_id,
        .sort_order = root.sort_order,
        .commands = root.document->draw_list().commands,
    });
  });

  std::stable_sort(
      roots.begin(), roots.end(), [](const UIRootDrawList &lhs, const UIRootDrawList &rhs) {
        return lhs.sort_order < rhs.sort_order;
      }
  );

  return roots;
}

} // namespace astralix::rendering
