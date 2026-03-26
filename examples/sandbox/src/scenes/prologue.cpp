#include "prologue.hpp"

#include "astralix/modules/renderer/components/light/light-component.hpp"
#include "astralix/modules/renderer/components/light/strategies/directional-strategy.hpp"
#include "astralix/modules/renderer/components/material/material-component.hpp"
#include "astralix/modules/renderer/components/mesh/mesh-component.hpp"
#include "astralix/modules/renderer/components/resource/resource-component.hpp"
#include "astralix/modules/renderer/components/transform/transform-component.hpp"
#include "astralix/modules/renderer/entities/camera.hpp"
#include "astralix/modules/renderer/entities/object.hpp"
#include "astralix/modules/renderer/entities/serializers/scene-serializer.hpp"
#include "astralix/modules/renderer/entities/skybox.hpp"
#include "astralix/modules/renderer/entities/text.hpp"
#include "astralix/modules/window/managers/window-manager.hpp"

#include "base.hpp"
#include "glad/glad.h"
#include "glm/fwd.hpp"
#include "log.hpp"
#include "omp.h"
#include <entities/camera.hpp>
#include <glm/gtx/string_cast.hpp>

Prologue::Prologue() : Scene("prologue") {}

static std::vector<Tile> tiles;

void Prologue::create_tile_grid(int columns, int rows, float tile_size, float y,
                                glm::vec3 scale) {
  float offset_x = (columns - 1) * tile_size * 0.5f;
  float offset_z = (rows - 1) * tile_size * 0.5f;

  tiles.resize(columns * rows);

  std::vector<glm::vec3> positions(columns * rows);

#pragma omp parallel for collapse(2)
  for (int col = 0; col < columns; ++col) {
    for (int row = 0; row < rows; ++row) {
      float x = col * tile_size - offset_x;
      float z = row * tile_size - offset_z;

      int index = col * rows + row;

      positions[index] = glm::vec3(x, y, z);
    }
  }

  for (size_t i = 0; i < positions.size(); ++i) {
    auto &position = positions[i];

    auto tile = add_entity<Object>("tile", position);

    auto mesh_component = tile->add_component<MeshComponent>();
    mesh_component->attach_mesh(Mesh::cube(1.0f));

    tile->get_component<ResourceComponent>()->set_shader(
        "shaders::lighting_forward");
    tile->get_or_add_component<MaterialComponent>()->attach_material(
        "materials::wood");

    auto transform = tile->get_component<TransformComponent>();
    transform->set_scale(scale);

    tiles[i] = Tile{tile, position};
  }
}

void Prologue::load_scene_components() {
  rotating_objects.clear();
  tiles.clear();

  auto attach_wood_lighting = [](astralix::Object *object) {
    object->get_component<ResourceComponent>()->set_shader(
        "shaders::lighting_forward");
    object->get_or_add_component<MaterialComponent>()->attach_material(
        "materials::wood");
  };

  auto camera = add_entity<Camera>("camera", CameraMode::Free,
                                   glm::vec3(-6.0f, 8.0f, 10.0f));

  auto directional_light = add_entity<astralix::Object>(
      "directional_light", glm::vec3(-4.0f, 8.0f, -3.0f));

  directional_light->add_component<LightComponent>(camera->get_entity_id())
      ->strategy(create_scope<DirectionalStrategy>());

  auto add_rotating_cube = [&](const char *name, glm::vec3 position,
                               glm::vec3 scale, glm::vec3 axis, float speed,
                               glm::vec3 initial_axis = glm::vec3(0.0f),
                               float initial_degrees = 0.0f) {
    auto cube = add_entity<astralix::Object>(name, position);
    cube->add_component<MeshComponent>()->attach_mesh(Mesh::cube(1.0f));
    attach_wood_lighting(cube);

    auto transform = cube->get_component<TransformComponent>();
    transform->set_scale(scale);

    if (glm::length(initial_axis) > 0.0f && initial_degrees != 0.0f) {
      transform->rotate(initial_axis, initial_degrees);
    }

    rotating_objects.push_back({cube, axis, speed});
  };

  add_rotating_cube("front_left_cube", glm::vec3(-3.0f, 2.0f, -2.25f),
                    glm::vec3(1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 16.0f);

  add_rotating_cube("front_right_cube", glm::vec3(3.0f, 2.0f, -1.5f),
                    glm::vec3(1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 22.0f);

  add_rotating_cube("back_left_cube", glm::vec3(-2.0f, 2.0f, 2.5f),
                    glm::vec3(1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 18.0f);

  add_rotating_cube("back_right_cube", glm::vec3(2.4f, 2.0f, 2.2f),
                    glm::vec3(1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 14.0f);

  add_rotating_cube("floating_cube", glm::vec3(0.0f, 2.0f, 0.0f),
                    glm::vec3(0.85f), glm::vec3(0.0f, 1.0f, 0.0f), 28.0f);

  add_entity<Skybox>("skybox", "cubemaps::skybox", "shaders::skybox");

  create_tile_grid(16, 16, 1.0f, -0.1f, glm::vec3(1.0f, 1.0f, 1.0f));
}

void Prologue::start() { load_scene_components(); }

void Prologue::update() {
  using namespace astralix::input;

  if (IS_KEY_DOWN(KeyCode::F5)) {
    for (auto tile : tiles) {
      auto transform = tile.object->get_component<TransformComponent>();

      transform->set_position(tile.position);
    }
  }

  for (auto &rotating : rotating_objects) {
    auto transform = rotating.object->get_component<TransformComponent>();
    transform->rotate(rotating.axis, rotating.speed * 0.016f);
  }
}
