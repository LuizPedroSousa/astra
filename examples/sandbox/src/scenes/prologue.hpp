#pragma once

#include "astralix/modules/physics/components/rigidbody/rigidbody-component.hpp"
#include "astralix/modules/renderer/entities/object.hpp"
#include "astralix/modules/renderer/entities/scene.hpp"

using namespace astralix;

struct Tile {
  Object *object;
  glm::vec3 position;
};

class Prologue : public Scene {
public:
  Prologue();

  void start() override;
  void update() override;

private:
  void create_tile_grid(int columns, int rows,

                        float tile_size,

                        RigidType type = RigidType::Static, float y = 0.0f,
                        glm::vec3 scale = glm::vec3(1.0));

  void load_scene_components();
};
