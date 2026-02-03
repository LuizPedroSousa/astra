#pragma once
#include "managers/entity-manager.hpp"

#include "guid.hpp"
#include "serializers/scene-serializer.hpp"
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
  void save();
  void load();

  void set_serializer(Ref<SceneSerializer> scene_serializer) {
    m_serializer = scene_serializer;
  }

  template <typename T, typename... Args> T *add_entity(Args &&...params) {
    auto manager = EntityManager::get();

    return manager->add_entity<T>(std::forward<Args>(params)...);
  }

  SceneID get_id() const { return m_id; }
  std::string get_name() const { return m_name; }

  ~Scene() {}

  friend class SceneManager;

protected:
  Ref<SceneSerializer> m_serializer;
  SceneID m_id;
  std::string m_name;
};

} // namespace astralix
