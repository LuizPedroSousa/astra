#pragma once

#include "base.hpp"
#include "graph/terrain-subgraph.hpp"
#include "heightmap-frame.hpp"
#include "heightmap-pass.hpp"
#include "recipe/terrain-recipe-data.hpp"
#include <vector>

namespace astralix::terrain {

class HeightmapSubgraph : public TerrainSubgraph {
public:
  void add_pass(Scope<HeightmapPass> pass);
  void set_recipe(const TerrainRecipeData &recipe);

  void compile() override;
  void process(const std::unordered_map<std::string, const SubgraphOutputData *> &inputs) override;

  std::string_view name() const override { return "Heightmap"; }
  ExecutionPolicy execution_policy() const override {
    return {ExecutionTrigger::OneShot, QueueHint::Main};
  }

  std::span<const SubgraphPort> input_ports() const override { return {}; }
  std::span<const SubgraphPort> output_ports() const override { return s_output_ports; }
  const SubgraphOutputData *get_output(const std::string &port_name) const override;

  const HeightmapFrame &frame() const { return m_frame; }
  std::span<const Scope<HeightmapPass>> passes() const;

private:
  static constexpr SubgraphPort s_output_ports[] = {
      {"heightmap", SubgraphOutputKind::Texture},
      {"normalmap", SubgraphOutputKind::Texture},
      {"splatmap", SubgraphOutputKind::Texture},
      {"mesh", SubgraphOutputKind::Mesh},
  };

  std::vector<Scope<HeightmapPass>> m_passes;
  HeightmapFrame m_frame;
  TerrainRecipeData m_recipe;
  bool m_compiled = false;

  std::unordered_map<std::string, SubgraphOutputData> m_outputs;
};

} // namespace astralix::terrain
