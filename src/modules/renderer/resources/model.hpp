#pragma once
#include "assimp/scene.h"
#include "filesystem"
#include "guid.hpp"
#include "mesh.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include "resources/material.hpp"
#include "resources/resource.hpp"
#include "vector"

namespace astralix {

class Model : public Resource {
public:
  Model(RESOURCE_INIT_PARAMS, std::vector<Mesh> meshes,
        std::vector<ResourceDescriptorID> materials)
      : RESOURCE_INIT(), meshes(meshes), materials(materials) {};

  static Ref<ModelDescriptor> create(const ResourceDescriptorID &id,
                                     Ref<Path> path);
  static Ref<ModelDescriptor> define(const ResourceDescriptorID &id,
                                     Ref<Path> path);
  static Ref<Model> from_descriptor(const ResourceHandle &id,
                                    Ref<ModelDescriptor> descriptor);

  std::vector<Mesh> meshes;
  std::vector<ResourceDescriptorID> materials;

private:
  static void process_nodes(const aiNode *first_node, const aiScene *scene,
                            std::vector<Mesh> &meshes,
                            std::vector<ResourceDescriptorID> &materials,
                            std::filesystem::path path);
  static Mesh process_mesh(aiMesh *mesh, const aiScene *scene,
                           std::vector<ResourceDescriptorID> &materials,
                           std::filesystem::path path);

  static void load_material(ResourceDescriptorID material_id,
                            aiMaterial *ai_material,
                            std::filesystem::path path);
};

} // namespace astralix
