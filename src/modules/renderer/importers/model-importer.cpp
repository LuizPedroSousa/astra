#include "importers/model-importer.hpp"

#include "assert.hpp"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

namespace astralix {
namespace {

unsigned int build_postprocess_flags(const ModelImportSettings &settings) {
  unsigned int flags = 0;
  if (settings.triangulate) {
    flags |= aiProcess_Triangulate;
  }
  if (settings.flip_uvs) {
    flags |= aiProcess_FlipUVs;
  }
  if (settings.generate_normals) {
    flags |= aiProcess_GenSmoothNormals;
  }
  if (settings.pre_transform_vertices) {
    flags |= aiProcess_PreTransformVertices;
  }

  return flags;
}

Mesh process_mesh(aiMesh *node_mesh) {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  for (u_int i = 0; i < node_mesh->mNumVertices; i++) {
    Vertex vertex{};

    vertex.position = glm::vec3(
        node_mesh->mVertices[i].x,
        node_mesh->mVertices[i].y,
        node_mesh->mVertices[i].z
    );

    if (node_mesh->HasNormals()) {
      vertex.normal = glm::vec3(
          node_mesh->mNormals[i].x,
          node_mesh->mNormals[i].y,
          node_mesh->mNormals[i].z
      );
    }

    if (node_mesh->mTextureCoords[0]) {
      vertex.texture_coordinates = glm::vec2(
          node_mesh->mTextureCoords[0][i].x,
          node_mesh->mTextureCoords[0][i].y
      );
    } else {
      vertex.texture_coordinates = glm::vec2(0.0f);
    }

    vertices.push_back(vertex);
  }

  for (unsigned int i = 0; i < node_mesh->mNumFaces; i++) {
    aiFace face = node_mesh->mFaces[i];
    for (unsigned int j = 0; j < face.mNumIndices; j++) {
      indices.push_back(face.mIndices[j]);
    }
  }

  return Mesh(vertices, indices);
}

void process_nodes(
    const aiNode *current_node,
    const aiScene *scene,
    std::vector<Mesh> &meshes,
    std::vector<uint32_t> &mesh_material_slots
) {
  for (u_int i = 0; i < current_node->mNumMeshes; i++) {
    aiMesh *mesh = scene->mMeshes[current_node->mMeshes[i]];
    meshes.push_back(process_mesh(mesh));
    mesh_material_slots.push_back(mesh->mMaterialIndex);
  }

  for (u_int i = 0; i < current_node->mNumChildren; i++) {
    process_nodes(
        current_node->mChildren[i],
        scene,
        meshes,
        mesh_material_slots
    );
  }
}

} // namespace

ImportedModelData import_model_file(
    const std::filesystem::path &path,
    const ModelImportSettings &settings
) {
  Assimp::Importer importer;
  const aiScene *scene =
      importer.ReadFile(path, build_postprocess_flags(settings));

  ASTRA_ENSURE(
      !scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
          scene->mRootNode == nullptr,
      importer.GetErrorString()
  );

  ImportedModelData imported;
  imported.material_slot_count = static_cast<size_t>(scene->mNumMaterials);
  process_nodes(
      scene->mRootNode,
      scene,
      imported.meshes,
      imported.mesh_material_slots
  );
  return imported;
}

} // namespace astralix
