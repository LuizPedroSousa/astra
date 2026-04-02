#include "axmesh-serializer.hpp"

#include "adapters/file/file-stream-reader.hpp"
#include "adapters/file/file-stream-writer.hpp"
#include "arena.hpp"
#include "assert.hpp"
#include "context-proxy.hpp"
#include "serialization-context.hpp"
#include "stream-buffer.hpp"

#include <cstring>
#include <filesystem>

namespace astralix {
namespace {

Scope<StreamBuffer> copy_to_stream_buffer(ElasticArena::Block *block) {
  auto buffer = create_scope<StreamBuffer>(block->size);
  std::memcpy(buffer->data(), block->data, block->size);
  return buffer;
}

void write_vec2(ContextProxy ctx, const glm::vec2 &value) {
  ctx["x"] = value.x;
  ctx["y"] = value.y;
}

void write_vec3(ContextProxy ctx, const glm::vec3 &value) {
  ctx["x"] = value.x;
  ctx["y"] = value.y;
  ctx["z"] = value.z;
}

float read_number(ContextProxy ctx) {
  switch (ctx.kind()) {
    case SerializationTypeKind::Float:
      return ctx.as<float>();
    case SerializationTypeKind::Int:
      return static_cast<float>(ctx.as<int>());
    default:
      ASTRA_EXCEPTION("AxMesh numeric field is missing or invalid");
  }
}

glm::vec2 read_vec2(ContextProxy ctx) {
  return glm::vec2(read_number(ctx["x"]), read_number(ctx["y"]));
}

glm::vec3 read_vec3(ContextProxy ctx) {
  return glm::vec3(
      read_number(ctx["x"]), read_number(ctx["y"]), read_number(ctx["z"])
  );
}

} // namespace

void AxMeshSerializer::write(const std::filesystem::path &path, const std::vector<Mesh> &meshes) {
  auto ctx = SerializationContext::create(SerializationFormat::Json);
  (*ctx)["axmesh"]["version"] = k_version;

  for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
    const auto &mesh = meshes[mesh_index];
    auto mesh_ctx = (*ctx)["axmesh"]["meshes"][static_cast<int>(mesh_index)];
    mesh_ctx["draw_type"] = static_cast<int>(mesh.draw_type);

    for (size_t vertex_index = 0; vertex_index < mesh.vertices.size();
         ++vertex_index) {
      const auto &vertex = mesh.vertices[vertex_index];
      auto vertex_ctx =
          mesh_ctx["vertices"][static_cast<int>(vertex_index)];
      write_vec3(vertex_ctx["position"], vertex.position);
      write_vec3(vertex_ctx["normal"], vertex.normal);
      write_vec2(vertex_ctx["texture_coordinates"], vertex.texture_coordinates);
      write_vec3(vertex_ctx["tangent"], vertex.tangent);
    }

    for (size_t index = 0; index < mesh.indices.size(); ++index) {
      mesh_ctx["indices"][static_cast<int>(index)] =
          static_cast<int>(mesh.indices[index]);
    }
  }

  std::filesystem::create_directories(path.parent_path());

  ElasticArena arena(KB(64));
  auto *block = ctx->to_buffer(arena);
  auto writer = FileStreamWriter(path, copy_to_stream_buffer(block));
  writer.write();
}

std::vector<Mesh> AxMeshSerializer::read(const std::filesystem::path &path) {
  ASTRA_ENSURE(!std::filesystem::exists(path), "AxMesh file not found: ", path);

  auto reader = FileStreamReader(path);
  reader.read();

  auto ctx = SerializationContext::create(
      SerializationFormat::Json, reader.get_buffer()
  );

  ASTRA_ENSURE((*ctx)["axmesh"]["version"].kind() != SerializationTypeKind::Int, "AxMesh version is missing or invalid");
  ASTRA_ENSURE((*ctx)["axmesh"]["version"].as<int>() != k_version, "Unsupported AxMesh version: ", (*ctx)["axmesh"]["version"].as<int>());

  std::vector<Mesh> meshes;
  auto meshes_ctx = (*ctx)["axmesh"]["meshes"];
  ASTRA_ENSURE(meshes_ctx.kind() != SerializationTypeKind::Array, "AxMesh meshes must be an array");

  meshes.reserve(meshes_ctx.size());

  for (size_t mesh_index = 0; mesh_index < meshes_ctx.size(); ++mesh_index) {
    auto mesh_ctx = meshes_ctx[static_cast<int>(mesh_index)];

    ASTRA_ENSURE(mesh_ctx["draw_type"].kind() != SerializationTypeKind::Int, "AxMesh draw_type is missing");

    std::vector<Vertex> vertices;
    auto vertices_ctx = mesh_ctx["vertices"];
    ASTRA_ENSURE(vertices_ctx.kind() != SerializationTypeKind::Array, "AxMesh vertices must be an array");
    vertices.reserve(vertices_ctx.size());

    for (size_t vertex_index = 0; vertex_index < vertices_ctx.size();
         ++vertex_index) {
      auto vertex_ctx = vertices_ctx[static_cast<int>(vertex_index)];
      vertices.push_back(Vertex{
          .position = read_vec3(vertex_ctx["position"]),
          .normal = read_vec3(vertex_ctx["normal"]),
          .texture_coordinates = read_vec2(vertex_ctx["texture_coordinates"]),
          .tangent = read_vec3(vertex_ctx["tangent"]),
      });
    }

    std::vector<unsigned int> indices;
    auto indices_ctx = mesh_ctx["indices"];
    ASTRA_ENSURE(indices_ctx.kind() != SerializationTypeKind::Array, "AxMesh indices must be an array");
    indices.reserve(indices_ctx.size());

    for (size_t index = 0; index < indices_ctx.size(); ++index) {
      auto index_ctx = indices_ctx[static_cast<int>(index)];
      ASTRA_ENSURE(index_ctx.kind() != SerializationTypeKind::Int, "AxMesh index is missing or invalid");
      indices.push_back(static_cast<unsigned int>(index_ctx.as<int>()));
    }

    Mesh mesh(std::move(vertices), std::move(indices));
    mesh.draw_type = static_cast<RendererAPI::DrawPrimitive>(
        mesh_ctx["draw_type"].as<int>()
    );
    meshes.push_back(std::move(mesh));
  }

  return meshes;
}

} // namespace astralix
