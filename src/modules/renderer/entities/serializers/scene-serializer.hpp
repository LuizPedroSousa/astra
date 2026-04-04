#pragma once
#include "base.hpp"
#include "scene-snapshot-types.hpp"
#include "serializer.hpp"

#include <vector>

namespace astralix {
class Scene;
enum class SceneArtifactKind : int;

class SceneSerializer : public Serializer {

public:
  SceneSerializer(Ref<Scene> scene);
  SceneSerializer();

  void serialize() override;
  void deserialize() override;
  void serialize_snapshots(
      const std::vector<serialization::EntitySnapshot> &entities
  );
  void set_artifact_kind(SceneArtifactKind kind) { m_artifact_kind = kind; }
  SceneArtifactKind get_artifact_kind() const { return m_artifact_kind; }

private:
  Ref<Scene> m_scene = nullptr;
  SceneArtifactKind m_artifact_kind;
};

} // namespace astralix
