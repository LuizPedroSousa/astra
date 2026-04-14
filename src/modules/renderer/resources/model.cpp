#include "model.hpp"

#include "assert.hpp"
#include "assimp/Importer.hpp"
#include "assimp/material.h"
#include "assimp/pbrmaterial.h"
#include "assimp/postprocess.h"
#include "guid.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include <filesystem>
#include <optional>

namespace astralix {

namespace {

std::optional<ResourceDescriptorID> load_material_texture(
    const ResourceDescriptorID &material_id, aiMaterial *ai_material,
    aiTextureType type, const std::filesystem::path &path,
    const std::string &suffix) {
  if (ai_material->GetTextureCount(type) == 0) {
    return std::nullopt;
  }

  aiString filename_str;
  if (ai_material->GetTexture(type, 0, &filename_str) != aiReturn_SUCCESS) {
    return std::nullopt;
  }

  const std::string filename = filename_str.C_Str();
  if (filename.empty()) {
    return std::nullopt;
  }

  const std::string material_name =
      material_id.starts_with("materials::")
          ? material_id.substr(std::string("materials::").size())
          : std::string(material_id);
  const ResourceDescriptorID texture_id =
      "textures::" + material_name + "::" + suffix;
  Texture2D::create(
      texture_id, Path::create((path.parent_path() / filename).string()));
  return texture_id;
}

glm::vec4 read_base_color_factor(aiMaterial *ai_material) {
  aiColor4D base_color(1.0f, 1.0f, 1.0f, 1.0f);
  if (ai_material->Get(AI_MATKEY_BASE_COLOR, base_color) == aiReturn_SUCCESS) {
    return glm::vec4(base_color.r, base_color.g, base_color.b, base_color.a);
  }

  aiColor3D diffuse(1.0f, 1.0f, 1.0f);
  if (ai_material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == aiReturn_SUCCESS) {
    return glm::vec4(diffuse.r, diffuse.g, diffuse.b, 1.0f);
  }

  return glm::vec4(1.0f);
}

glm::vec3 read_emissive_factor(aiMaterial *ai_material) {
  aiColor3D emissive(0.0f, 0.0f, 0.0f);
  if (ai_material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == aiReturn_SUCCESS) {
    return glm::vec3(emissive.r, emissive.g, emissive.b);
  }

  return glm::vec3(0.0f);
}

float read_assimp_factor(aiMaterial *ai_material, const char *key,
                         unsigned int type, unsigned int index,
                         float fallback) {
  float value = fallback;
  if (ai_material->Get(key, type, index, value) == aiReturn_SUCCESS) {
    return value;
  }
  return fallback;
}

} // namespace

Ref<ModelDescriptor> Model::create(const ResourceDescriptorID &id,
                                   Ref<Path> path) {
  return resource_manager()->register_model(ModelDescriptor::create(id, path));
};

Ref<Model> Model::from_descriptor(const ResourceHandle &id,
                                  Ref<ModelDescriptor> descriptor) {
  Assimp::Importer importer;

  auto full_path = path_manager()->resolve(descriptor->path);

  const aiScene *scene =
      importer.ReadFile(full_path, aiProcess_Triangulate | aiProcess_FlipUVs |
                                       aiProcess_GenNormals |
                                       aiProcess_CalcTangentSpace);

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
    Vertex vertex{};

    vertex.position =
        glm::vec3(node_mesh->mVertices[i].x, node_mesh->mVertices[i].y,
                  node_mesh->mVertices[i].z);

    if (node_mesh->HasNormals()) {
      vertex.normal =
          glm::vec3(node_mesh->mNormals[i].x, node_mesh->mNormals[i].y,
                    node_mesh->mNormals[i].z);
    }

    if (node_mesh->HasTangentsAndBitangents()) {
      vertex.tangent =
          glm::vec3(node_mesh->mTangents[i].x, node_mesh->mTangents[i].y,
                    node_mesh->mTangents[i].z);
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

  if (node_mesh->mMaterialIndex < scene->mNumMaterials) {
    aiMaterial *ai_material = scene->mMaterials[node_mesh->mMaterialIndex];

    auto manager = ResourceManager::get();
    std::string name = ai_material->GetName().C_Str();
    ResourceDescriptorID id = "materials::" + name;

    auto material_exists = manager->get_by_descriptor_id<Material>(id);
    if (material_exists == nullptr) {
      load_material(id, ai_material, path);
    }

    materials.push_back(id);
  }

  return Mesh(vertices, indices);
}

void Model::load_material(ResourceDescriptorID material_id,
                          aiMaterial *ai_material,
                          std::filesystem::path path) {
  const auto base_color = load_material_texture(
      material_id, ai_material, aiTextureType_BASE_COLOR, path, "base_color");
  const auto normal = load_material_texture(
      material_id, ai_material, aiTextureType_NORMALS, path, "normal");
  const auto metallic = load_material_texture(
      material_id, ai_material, aiTextureType_METALNESS, path, "metallic");
  const auto roughness = load_material_texture(
      material_id, ai_material, aiTextureType_DIFFUSE_ROUGHNESS, path,
      "roughness");
  const auto metallic_roughness = load_material_texture(
      material_id, ai_material, aiTextureType_GLTF_METALLIC_ROUGHNESS, path,
      "metallic_roughness");
  const auto occlusion = load_material_texture(
      material_id, ai_material, aiTextureType_AMBIENT_OCCLUSION, path,
      "occlusion");
  const auto emissive = load_material_texture(
      material_id, ai_material, aiTextureType_EMISSIVE, path, "emissive");
  const auto displacement = load_material_texture(
      material_id, ai_material, aiTextureType_HEIGHT, path, "displacement");

  const bool has_metallic_texture =
      metallic.has_value() || metallic_roughness.has_value();
  const bool has_roughness_texture =
      roughness.has_value() || metallic_roughness.has_value();
  const float metallic_factor =
      has_metallic_texture
          ? 1.0f
          : read_assimp_factor(
                ai_material, AI_MATKEY_METALLIC_FACTOR, 0.0f
            );
  const float roughness_factor =
      has_roughness_texture
          ? 1.0f
          : read_assimp_factor(
                ai_material, AI_MATKEY_ROUGHNESS_FACTOR, 1.0f
            );

  Material::create(material_id, base_color, normal, metallic, roughness,
                   metallic_roughness, occlusion, emissive, displacement,
                   read_base_color_factor(ai_material),
                   read_emissive_factor(ai_material), metallic_factor,
                   roughness_factor, 1.0f, 1.0f, 0.0f);
}

} // namespace astralix
