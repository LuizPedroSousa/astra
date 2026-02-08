#include "prologue.hpp"

#include "astralix/modules/physics/components/mesh-collision/mesh-collision-component.hpp"
#include "astralix/modules/physics/components/rigidbody/rigidbody-component.hpp"
#include "astralix/modules/renderer/components/light/light-component.hpp"
#include "astralix/modules/renderer/components/light/strategies/directional-strategy.hpp"
#include "astralix/modules/renderer/components/material/material-component.hpp"
#include "astralix/modules/renderer/components/mesh/mesh-component.hpp"
#include "astralix/modules/renderer/components/resource/resource-component.hpp"
#include "astralix/modules/renderer/components/transform/transform-component.hpp" #include "astralix/modules/renderer/entities/camera.hpp"
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

#define ATTACH_MESH(var)                                                       \
  var->add_component<MeshComponent>()->attach_mesh(astralix::Mesh::cube(1.0f));
#define ATTACH_LIGHTING_SHADER(var)                                            \
  var->get_component<ResourceComponent>()->set_shader("shaders::lighting");    \
  var->get_or_add_component<MaterialComponent>()->attach_material(             \
      "materials::wood")

static std::vector<Tile> tiles;

void Prologue::create_tile_grid(int columns, int rows, float tile_size,
                                RigidType type, float y, glm::vec3 scale) {
  float offset_x = (columns - 1) * tile_size * 0.5f;
  float offset_z = (rows - 1) * tile_size * 0.5f;

  tiles.resize(columns * rows);

  std::vector<glm::vec3> positions;

  positions.resize(columns * rows);

#pragma omp parallel for collapse(2)
  for (int col = 0; col < columns; ++col) {
    for (int row = 0; row < rows; ++row) {
      float x = col * tile_size - offset_x;
      float z = row * tile_size - offset_z;

      int index = col * rows + row;

      positions[index] = glm::vec3(x, y, z);
    }
  }

  auto size = positions.size();

  for (int i = 0; i < size; ++i) {
    auto &position = positions[i];

    auto tile = add_entity<Object>("tile", position);

    auto meshComp = tile->add_component<MeshComponent>();
    meshComp->attach_mesh(Mesh::cube(1.0f));

    ATTACH_LIGHTING_SHADER(tile);

    auto transform = tile->get_component<TransformComponent>();
    transform->set_scale(scale);

    tile->add_component<MeshCollisionComponent>();
    tile->add_component<RigidBodyComponent>(type);

    tiles[i] = Tile{tile, position};
  }
}

void Prologue::load_scene_components() {
  auto camera = add_entity<Camera>("camera", CameraMode::Free,
                                   glm::vec3(0.0f, 4.0f, 0.0f));

  auto directional_light =
      add_entity<astralix::Object>("Light", glm::vec3(-2.0f, 4.0f, -1.0f));

  directional_light->add_component<LightComponent>(camera->get_entity_id())
      ->strategy(create_scope<DirectionalStrategy>());

  std::vector<glm::vec3> lightColors;
  lightColors.push_back(glm::vec3(0.5f, 0.0f, 0.0f));
  lightColors.push_back(glm::vec3(0.0f, 0.0f, 0.5f));
  lightColors.push_back(glm::vec3(0.0f, 0.5f, 0.0f));

  for (int i = 0; i < lightColors.size(); i++) {
    auto point_light = add_entity<astralix::Object>(
        "Light", glm::vec3(i * 2.0f - 0.5f, 2.0f, i * 2.0f));

    point_light->get_component<ResourceComponent>()->set_shader(
        "shaders::lighting");

    point_light->add_component<MeshComponent>()->attach_mesh(Mesh::cube(1.0f));
  }

  add_entity<Skybox>("skybox", "cubemaps::skybox", "shaders::skybox");

  create_tile_grid(16, 16, 1.0f, RigidType::Static);
  create_tile_grid(12, 12, 0.025f, RigidType::Dynamic, 20.0f,
                   glm::vec3(0.025f));
}

void Prologue::start() { load_scene_components(); }

void Prologue::update() {
  using namespace astralix::input;

  if (IS_KEY_DOWN(KeyCode::F5)) {
    for (auto tile : tiles) {
      auto transform = tile.object->get_component<TransformComponent>();

      transform->translate(tile.position);
    }
  }
}
