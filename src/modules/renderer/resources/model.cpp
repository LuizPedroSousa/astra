#include "model.hpp"

#include "assert.hpp"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "guid.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include <filesystem>
#include <optional>

namespace astralix {

Ref<ModelDescriptor> Model::create(const ResourceDescriptorID &id,
                                   Ref<Path> path) {

  auto full_path = path_manager()->resolve(path);

  return resource_manager()->register_model(ModelDescriptor::create(id, path));
};

Ref<Model> Model::from_descriptor(const ResourceHandle &id,
                                  Ref<ModelDescriptor> descriptor) {
  Assimp::Importer importer;

  auto full_path = path_manager()->resolve(descriptor->path);

  const aiScene *scene =
      importer.ReadFile(full_path, aiProcess_Triangulate | aiProcess_FlipUVs |
                                       aiProcess_GenNormals);

  ASTRA_ENSURE(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE,
               importer.GetErrorString());

  std::vector<Mesh> meshes;
  std::vector<ResourceDescriptorID> materials;

  process_nodes(scene->mRootNode, scene, meshes, materials, full_path);

  return create_ref<Model>(id, meshes, materials);
};

void Model::process_nodes(const aiNode *current_node, const aiScene *scene,
                          std::vector<Mesh> &meshes,
                          std::vector<ResourceDescriptorID> &materials,
                          std::filesystem::path path) {
  for (u_int i = 0; i < current_node->mNumMeshes; i++) {
    aiMesh *mesh = scene->mMeshes[current_node->mMeshes[i]];

    auto processed_mesh = process_mesh(mesh, scene, materials, path);

    meshes.push_back(processed_mesh);
  }

  for (u_int i = 0; i < current_node->mNumChildren; i++) {
    process_nodes(current_node->mChildren[i], scene, meshes, materials, path);
  }
}

Mesh Model::process_mesh(aiMesh *node_mesh, const aiScene *scene,
                         std::vector<ResourceDescriptorID> &materials,
                         std::filesystem::path path) {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  for (u_int i = 0; i < node_mesh->mNumVertices; i++) {
    Vertex vertex;

    vertex.position =
        glm::vec3(node_mesh->mVertices[i].x, node_mesh->mVertices[i].y,
                  node_mesh->mVertices[i].z);

    if (node_mesh->HasNormals()) {
      vertex.normal =
          glm::vec3(node_mesh->mNormals[i].x, node_mesh->mNormals[i].y,
                    node_mesh->mNormals[i].z);
    }

    if (node_mesh->mTextureCoords[0]) {
      vertex.texture_coordinates = glm::vec2(node_mesh->mTextureCoords[0][i].x,
                                             node_mesh->mTextureCoords[0][i].y);
    } else {
      vertex.texture_coordinates = glm::vec2(0);
    }

    vertices.push_back(vertex);
  };

  for (unsigned int i = 0; i < node_mesh->mNumFaces; i++) {
    aiFace face = node_mesh->mFaces[i];
    for (unsigned int j = 0; j < face.mNumIndices; j++)
      indices.push_back(face.mIndices[j]);
  }

  if (node_mesh->mMaterialIndex > 0) {
    aiMaterial *ai_material = scene->mMaterials[node_mesh->mMaterialIndex];
    if (ai_material->GetTextureCount(aiTextureType_DIFFUSE) > 0 ||
        ai_material->GetTextureCount(aiTextureType_SPECULAR) > 0) {

      auto resource_manager = ResourceManager::get();

      std::string name = ai_material->GetName().C_Str();

      ResourceDescriptorID id = "materials::" + name;

      auto material_exists =
          resource_manager->get_by_descriptor_id<Material>(id);

      if (material_exists == nullptr) {
        load_material(id, ai_material, path);
      }

      materials.push_back(id);
    }
  }

  return Mesh(vertices, indices);
}
void Model::load_material(ResourceDescriptorID material_id,
                          aiMaterial *ai_material, std::filesystem::path path) {

  auto manager = ResourceManager::get();

  auto get_texture = [&](aiTextureType type) {
    std::vector<ResourceDescriptorID> textures;
    for (unsigned int i = 0; i < ai_material->GetTextureCount(type); i++) {
      aiString filename_str;
      ai_material->GetTexture(type, i, &filename_str);

      const char *filename = filename_str.C_Str();

      ResourceDescriptorID texture_id =
          "textures::" + material_id + "::" + filename;

      auto get_name = [type]() -> std::string {
        std::string prefix = "materials[0].";

        std::string type_str =
            type == aiTextureType_DIFFUSE ? "diffuse" : "specular";

        std::string name = prefix + type_str;

        return name;
      };

      std::string texture_path = path / filename;

      Texture2D::create(texture_id, Path::create(texture_path));

      textures.push_back(texture_id);
    };

    return textures;
  };

  auto diffuses = get_texture(aiTextureType_DIFFUSE);
  auto speculars = get_texture(aiTextureType_SPECULAR);

  Material::create(material_id, diffuses, speculars, std::nullopt);
}
} // namespace astralix
