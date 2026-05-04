#pragma once
#include "base.hpp"
#include "scene-snapshot-types.hpp"
#include "serializer.hpp"

#include <vector>

namespace astralix::ecs {
class World;
}

namespace astralix {
class Scene;
enum class SceneArtifactKind : int;

class SceneSerializer : public Serializer {

public:
  SceneSerializer(Ref<Scene> scene);
  SceneSerializer();

  void serialize() override;
  void deserialize() override;
  std::vector<serialization::EntitySnapshot>
  collect_artifact_snapshots(const ecs::World &world) const;
  void serialize_world(const ecs::World &world);
  void serialize_snapshots(
      const std::vector<serialization::EntitySnapshot> &entities
  );
  void set_artifact_kind(SceneArtifactKind kind) { m_artifact_kind = kind; }
  SceneArtifactKind get_artifact_kind() const { return m_artifact_kind; }

private:
  std::weak_ptr<Scene> m_scene;
  SceneArtifactKind m_artifact_kind;
};

} // namespace astralix
