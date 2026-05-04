#pragma once

#include "terrain-subgraph.hpp"
#include "base.hpp"
#include "systems/job-system/job-system.hpp"
#include <unordered_map>
#include <vector>

namespace astralix::terrain {

class TerrainGraph {
public:
  void add_subgraph(Scope<TerrainSubgraph> subgraph);
  void compile();
  void process();

  std::span<const Scope<TerrainSubgraph>> subgraphs() const;
  const TerrainSubgraph *find_subgraph(std::string_view name) const;

private:
  std::vector<Scope<TerrainSubgraph>> m_subgraphs;
  std::vector<size_t> m_execution_order;
  std::vector<std::vector<size_t>> m_dependencies;
  std::unordered_map<size_t, JobHandle> m_running_jobs;
  std::vector<bool> m_one_shot_executed;
  bool m_compiled = false;
};

} // namespace astralix::terrain
