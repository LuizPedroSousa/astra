#include "heightmap-subgraph.hpp"
#include "assert.hpp"
#include "trace.hpp"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace astralix::terrain {

void HeightmapSubgraph::add_pass(Scope<HeightmapPass> pass) {
  m_passes.push_back(std::move(pass));
  m_compiled = false;
}

void HeightmapSubgraph::set_recipe(const TerrainRecipeData &recipe) {
  m_recipe = recipe;
}

void HeightmapSubgraph::compile() {
  ASTRA_PROFILE_N("HeightmapSubgraph::compile");

  size_t pass_count = m_passes.size();

  std::unordered_map<HeightmapField, std::vector<size_t>> writers;
  for (size_t index = 0; index < pass_count; ++index) {
    for (HeightmapField field : m_passes[index]->writes()) {
      writers[field].push_back(index);
    }
  }

  std::vector<std::unordered_set<size_t>> dependencies(pass_count);
  for (size_t index = 0; index < pass_count; ++index) {
    for (HeightmapField field : m_passes[index]->reads()) {
      auto iterator = writers.find(field);
      if (iterator == writers.end()) {
        continue;
      }
      for (size_t writer_index : iterator->second) {
        if (writer_index != index) {
          dependencies[index].insert(writer_index);
        }
      }
    }
  }

  std::vector<size_t> sorted_order;
  sorted_order.reserve(pass_count);
  std::vector<bool> visited(pass_count, false);
  std::vector<bool> in_stack(pass_count, false);

  auto topological_visit = [&](auto &self, size_t node) -> void {
    if (visited[node]) {
      return;
    }

    ASTRA_ENSURE(in_stack[node], "HeightmapSubgraph: cyclic dependency detected involving pass '%.*s'", static_cast<int>(m_passes[node]->name().size()), m_passes[node]->name().data());

    in_stack[node] = true;

    for (size_t dependency : dependencies[node]) {
      self(self, dependency);
    }

    in_stack[node] = false;
    visited[node] = true;
    sorted_order.push_back(node);
  };

  for (size_t index = 0; index < pass_count; ++index) {
    topological_visit(topological_visit, index);
  }

  std::vector<Scope<HeightmapPass>> sorted_passes;
  sorted_passes.reserve(pass_count);
  for (size_t index : sorted_order) {
    sorted_passes.push_back(std::move(m_passes[index]));
  }
  m_passes = std::move(sorted_passes);
  m_compiled = true;
}

void HeightmapSubgraph::process(
    const std::unordered_map<std::string, const SubgraphOutputData *> &inputs
) {
  ASTRA_PROFILE_N("HeightmapSubgraph::process");

  ASTRA_ENSURE(!m_compiled, "HeightmapSubgraph::process called before compile()");

  m_frame.clear();

  for (auto &pass : m_passes) {
    if (pass->enabled) {
      pass->process(m_frame, m_recipe);
    }
  }

  uint32_t res = m_frame.resolution;
  m_outputs["heightmap"] = TextureOutput{
      .id = "heightmap",
      .width = res,
      .height = res,
      .data = m_frame.heightmap.data(),
      .bytes_per_texel = sizeof(float),
  };
  m_outputs["normalmap"] = TextureOutput{
      .id = "normalmap",
      .width = res,
      .height = res,
      .data = m_frame.normal_map.data(),
      .bytes_per_texel = sizeof(glm::vec4),
  };
  m_outputs["splatmap"] = TextureOutput{
      .id = "splatmap",
      .width = res,
      .height = res,
      .data = m_frame.splat_map.data(),
      .bytes_per_texel = sizeof(glm::u8vec4),
  };
}

const SubgraphOutputData *HeightmapSubgraph::get_output(const std::string &port_name) const {
  auto iterator = m_outputs.find(port_name);
  return iterator != m_outputs.end() ? &iterator->second : nullptr;
}

std::span<const Scope<HeightmapPass>> HeightmapSubgraph::passes() const {
  return m_passes;
}

} // namespace astralix::terrain
