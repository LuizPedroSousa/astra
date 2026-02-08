#pragma once

#include "render-graph-exporter.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace astralix {

class AsciiExporter : public RenderGraphExporter {
public:
  void export_graph(const RenderGraph& graph, const std::string& filename) const override;
  std::string get_format_name() const override { return "ASCII Art"; }

private:
  struct LayoutConfig {
    size_t box_width = 18;
    size_t box_height = 3;
    size_t horizontal_spacing = 4;
    size_t vertical_spacing = 1;
  };

  struct NodePosition {
    size_t box_x, box_y, width, height;
    size_t center_x, center_y;
  };

  struct EdgeInfo {
    size_t from_x, from_y, to_x, to_y;
    std::string label;
  };

  using Canvas = std::vector<std::string>;
  using BoxMask = std::vector<std::vector<bool>>;

  void write_visual_graph(std::ofstream& file, const RenderGraph& graph) const;
  void write_passes_list(std::ofstream& file, const RenderGraph& graph) const;
  void write_resources_list(std::ofstream& file, const RenderGraph& graph) const;
  void write_execution_order(std::ofstream& file, const RenderGraph& graph) const;
  void write_resource_flow(std::ofstream& file, const RenderGraph& graph) const;
  void write_statistics(std::ofstream& file, const RenderGraph& graph) const;

  std::vector<std::vector<uint32_t>> compute_pass_layers(
    const RenderGraph& graph,
    const std::vector<uint32_t>& active_passes) const;

  std::unordered_map<uint32_t, NodePosition> compute_pass_positions(
    const std::vector<std::vector<uint32_t>>& layers,
    const LayoutConfig& config) const;

  std::unordered_map<uint32_t, NodePosition> compute_resource_positions(
    const RenderGraph& graph,
    const std::vector<uint32_t>& active_passes,
    const LayoutConfig& config,
    size_t resources_y,
    size_t canvas_width) const;

  void draw_edges(Canvas& canvas, BoxMask& mask, const std::vector<EdgeInfo>& edges) const;
  void draw_box(Canvas& canvas, const NodePosition& pos, const std::string& name,
                char h_border, char v_border, char corner) const;
  void draw_pass_box(Canvas& canvas, const NodePosition& pos, const std::string& name) const;
  void draw_resource_box(Canvas& canvas, const NodePosition& pos, const std::string& name) const;

  std::vector<uint32_t> get_active_passes(const RenderGraph& graph) const;
  std::vector<uint32_t> get_used_resources(const RenderGraph& graph,
                                            const std::vector<uint32_t>& active_passes) const;
  std::string truncate_name(const std::string& name, size_t max_width) const;
};

}
