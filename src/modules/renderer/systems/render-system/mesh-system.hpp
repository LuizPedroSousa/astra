#pragma once

#include "entities/object.hpp"
#include "guid.hpp"
#include "resources/mesh.hpp"
#include "storage-buffer.hpp"
#include "systems/system.hpp"
#include "targets/render-target.hpp"
#include <unordered_map>
#include <vector>

namespace astralix {

struct Batch {
  std::vector<Mesh> meshes;
  std::unordered_map<EntityID, Object *> sources;
  std::unordered_map<EntityID, glm::mat4> transforms;
  bool is_dirty = true;
};

class MeshSystem : public System<MeshSystem> {
public:
  MeshSystem(Ref<RenderTarget> render_target);
  ~MeshSystem();

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

private:
  MeshGroupID compute_group_id(std::vector<Mesh> &mesh, uint32_t shader_id);

  std::unordered_map<MeshGroupID, Batch> m_batches;

  Ref<StorageBuffer> m_storage_buffer;

  Ref<RenderTarget> m_render_target = nullptr;
};

} // namespace astralix
