#pragma once

#include "entities/serializers/axmesh-serializer.hpp"
#include "components/mesh.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"
#include "managers/project-manager.hpp"
#include "renderer-api.hpp"
#include "serialized-fields.hpp"
#include "world.hpp"
#include <string>
#include <vector>

namespace astralix::serialization {

inline ComponentSnapshot snapshot_component(const rendering::MeshSet &mesh_set) {
  ComponentSnapshot snapshot{.name = "MeshSet"};
  snapshot.fields.push_back(
      {"mesh_count", static_cast<int>(mesh_set.meshes.size())});

  for (size_t mesh_index = 0; mesh_index < mesh_set.meshes.size();
       ++mesh_index) {
    const auto &mesh = mesh_set.meshes[mesh_index];
    const auto prefix = "mesh_" + std::to_string(mesh_index);

    snapshot.fields.push_back(
        {prefix + ".draw_type", static_cast<int>(mesh.draw_type)});
    snapshot.fields.push_back(
        {prefix + ".vertex_count", static_cast<int>(mesh.vertices.size())});
    snapshot.fields.push_back(
        {prefix + ".index_count", static_cast<int>(mesh.indices.size())});

    for (size_t vertex_index = 0; vertex_index < mesh.vertices.size();
         ++vertex_index) {
      const auto &vertex = mesh.vertices[vertex_index];
      const auto vertex_prefix =
          prefix + ".vertex_" + std::to_string(vertex_index);
      serialization::fields::append_vec3(snapshot.fields,
                                         vertex_prefix + ".position",
                                         vertex.position);
      serialization::fields::append_vec3(snapshot.fields,
                                         vertex_prefix + ".normal",
                                         vertex.normal);
      serialization::fields::append_vec2(
          snapshot.fields, vertex_prefix + ".texture_coordinates",
          vertex.texture_coordinates);
      serialization::fields::append_vec3(snapshot.fields,
                                         vertex_prefix + ".tangent",
                                         vertex.tangent);
      snapshot.fields.push_back(
          {vertex_prefix + ".bitangent_sign", vertex.bitangent_sign});
    }

    for (size_t index = 0; index < mesh.indices.size(); ++index) {
      snapshot.fields.push_back({prefix + ".index_" + std::to_string(index),
                                 static_cast<int>(mesh.indices[index])});
    }
  }

  return snapshot;
}

inline std::vector<Mesh>
read_mesh_set(const serialization::fields::FieldList &fields) {
  if (const auto asset_path =
          serialization::fields::read_string(fields, "asset.path");
      asset_path.has_value() && !asset_path->empty()) {
    auto project = active_project();
    ASTRA_ENSURE(project == nullptr,
                 "Cannot resolve MeshSet asset without an active project");
    return AxMeshSerializer::read(project->resolve_path(*asset_path));
  }

  std::vector<Mesh> meshes;
  const int mesh_count =
      serialization::fields::read_int(fields, "mesh_count").value_or(0);
  meshes.reserve(static_cast<size_t>(mesh_count));

  for (int mesh_index = 0; mesh_index < mesh_count; ++mesh_index) {
    const auto prefix = "mesh_" + std::to_string(mesh_index);
    const int vertex_count =
        serialization::fields::read_int(fields, prefix + ".vertex_count")
            .value_or(0);
    const int index_count =
        serialization::fields::read_int(fields, prefix + ".index_count")
            .value_or(0);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(static_cast<size_t>(vertex_count));
    indices.reserve(static_cast<size_t>(index_count));

    for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
      const auto vertex_prefix =
          prefix + ".vertex_" + std::to_string(vertex_index);
      vertices.push_back(Vertex{
          .position = serialization::fields::read_vec3(
              fields, vertex_prefix + ".position"),
          .normal = serialization::fields::read_vec3(
              fields, vertex_prefix + ".normal"),
          .texture_coordinates =
              serialization::fields::read_vec2(
                  fields, vertex_prefix + ".texture_coordinates"),
          .tangent = serialization::fields::read_vec3(
              fields, vertex_prefix + ".tangent"),
          .bitangent_sign = serialization::fields::read_float(
                                fields, vertex_prefix + ".bitangent_sign")
                                .value_or(1.0f),
      });
    }

    for (int index = 0; index < index_count; ++index) {
      indices.push_back(static_cast<unsigned int>(
          serialization::fields::read_int(
              fields, prefix + ".index_" + std::to_string(index))
              .value_or(0)));
    }

    Mesh mesh(vertices, indices, true);
    mesh.draw_type = static_cast<RendererAPI::DrawPrimitive>(
        serialization::fields::read_int(fields, prefix + ".draw_type")
            .value_or(static_cast<int>(RendererAPI::DrawPrimitive::TRIANGLES)));
    meshes.push_back(std::move(mesh));
  }

  return meshes;
}

inline void apply_mesh_set_snapshot(ecs::EntityRef entity,
                                    const serialization::fields::FieldList
                                        &fields) {
  entity.emplace<rendering::MeshSet>(rendering::MeshSet{.meshes = read_mesh_set(fields)});
}

} // namespace astralix::serialization
