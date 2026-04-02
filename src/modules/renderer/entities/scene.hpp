#pragma once
#include "world.hpp"

#include "guid.hpp"
#include <filesystem>
#include "string"
#include <utility>

namespace astralix {

class SceneSerializer;
struct ProjectSceneEntryConfig;

class Scene {
public:
  Scene(std::string type);

  virtual void update() = 0;

  const std::string &get_name() const {
    return m_scene_id.empty() ? m_scene_type : m_scene_id;
  }
  const std::string &get_scene_id() const { return m_scene_id; }
  const std::string &get_type() const { return m_scene_type; }
  const std::string &get_scene_path() const { return m_scene_path; }
  bool is_ready() const { return m_world_ready; }

  void serialize();
  void save();
  bool load();

  SceneSerializer *get_serializer() { return m_serializer.get(); };
  void set_serializer(Ref<SceneSerializer> scene_serializer) {
    m_serializer = scene_serializer;
  }

  ecs::World &world() { return m_world; }
  const ecs::World &world() const { return m_world; }
  SceneID get_id() const { return m_id; }

  ecs::EntityRef spawn_entity(std::string name, bool active = true) {
    return m_world.spawn(std::move(name), active);
  }

  ~Scene() {}

  friend class SceneManager;

protected:
  virtual void setup() {}
  virtual void build_default_world() {}
  virtual void after_world_ready() {}

  Ref<SceneSerializer> m_serializer;
  SceneID m_id;
  ecs::World m_world;

private:
  void bind_to_manifest_entry(const ProjectSceneEntryConfig &entry);
  void ensure_setup();
  void mark_world_ready(bool ready) { m_world_ready = ready; }

  std::string m_scene_type;
  std::string m_scene_id;
  std::string m_scene_path;
  bool m_setup_complete = false;
  bool m_world_ready = false;
};

} // namespace astralix
