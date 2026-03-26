#pragma once
#include "world.hpp"

#include "guid.hpp"
#include "string"
#include <utility>

namespace astralix {

class SceneSerializer;

class Scene {
public:
  Scene(std::string name);

  virtual void start() = 0;
  virtual void update() = 0;

  void serialize();

  SceneSerializer *get_serializer() { return m_serializer.get(); };
  ecs::World &world() { return m_world; }
  const ecs::World &world() const { return m_world; }

  ecs::EntityRef spawn_entity(std::string name, bool active = true) {
    return m_world.spawn(std::move(name), active);
  }

  void save();
  bool load();

  void set_serializer(Ref<SceneSerializer> scene_serializer) {
    m_serializer = scene_serializer;
  }

  SceneID get_id() const { return m_id; }
  std::string get_name() const { return m_name; }

  ~Scene() {}

  friend class SceneManager;

protected:
  Ref<SceneSerializer> m_serializer;
  SceneID m_id;
  std::string m_name;
  ecs::World m_world;
};

} // namespace astralix
