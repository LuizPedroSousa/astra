#pragma once

#include "astralix/modules/renderer/entities/object.hpp"
#include "astralix/modules/renderer/entities/scene.hpp"

using namespace astralix;

struct Tile {
  Object *object;
  glm::vec3 position;
};

struct RotatingObject {
  Object *object;
  glm::vec3 axis;
  float speed;
};

class Prologue : public Scene {
public:
  Prologue();

  void start() override;
  void update() override;

private:
  void create_tile_grid(int columns, int rows, float tile_size,
                        float y = 0.0f,
                        glm::vec3 scale = glm::vec3(1.0f));

  void load_scene_components();

  std::vector<RotatingObject> rotating_objects;
};
