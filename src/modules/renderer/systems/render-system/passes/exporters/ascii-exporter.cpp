#include "ascii-exporter.hpp"
#include "../render-graph.hpp"
#include "log.hpp"
#include <algorithm>
#include <fstream>
#include <functional>

namespace astralix {

void AsciiExporter::export_graph(const RenderGraph& graph, const std::string& filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("[RenderGraph] Failed to open file for ASCII export");
    return;
  }

  file << "RENDER GRAPH\n";
  file << "============\n\n";

  write_visual_graph(file, graph);
  write_passes_list(file, graph);
  write_resources_list(file, graph);
  write_execution_order(file, graph);
  write_resource_flow(file, graph);
  write_statistics(file, graph);

  file.close();
  LOG_INFO("[RenderGraph] Exported graph to ASCII file");
}

void AsciiExporter::write_visual_graph(std::ofstream& file, const RenderGraph& graph) const {
  file << "VISUAL FLOW GRAPH:\n";
  file << "------------------\n\n";

  const auto active_passes = get_active_passes(graph);
  if (active_passes.empty()) {
    return;
  }

  const auto used_resources = get_used_resources(graph, active_passes);
  const LayoutConfig config;
  const auto layers = compute_pass_layers(graph, active_passes);

  const auto pass_positions = compute_pass_positions(layers, config);

  const size_t passes_section_height = [&]() {
    size_t max_layer_size = 0;
    for (const auto& layer : layers) {
      max_layer_size = std::max(max_layer_size, layer.size());
    }
    return max_layer_size * (config.box_height + config.vertical_spacing) + 2;
  }();

  const size_t resources_section_y = passes_section_height + 4;
  const size_t canvas_height = resources_section_y + config.box_height + used_resources.size() * 2;
  const size_t canvas_width = layers.size() * (config.box_width + config.horizontal_spacing) + 4;

  const auto resource_positions = compute_resource_positions(
    graph, active_passes, config, resources_section_y, canvas_width);

  Canvas canvas(canvas_height, std::string(canvas_width, ' '));
  BoxMask box_mask(canvas_height, std::vector<bool>(canvas_width, false));

  for (const auto& [pass_idx, pos] : pass_positions) {
    for (size_t dy = 0; dy < pos.height; ++dy) {
      for (size_t dx = 0; dx < pos.width; ++dx) {
        if (pos.box_y + dy < canvas_height && pos.box_x + dx < canvas_width) {
          box_mask[pos.box_y + dy][pos.box_x + dx] = true;
        }
      }
    }
  }

  for (const auto& [res_idx, pos] : resource_positions) {
    for (size_t dy = 0; dy < pos.height; ++dy) {
      for (size_t dx = 0; dx < pos.width; ++dx) {
        if (pos.box_y + dy < canvas_height && pos.box_x + dx < canvas_width) {
          box_mask[pos.box_y + dy][pos.box_x + dx] = true;
        }
      }
    }
  }

  std::vector<EdgeInfo> edges;
  for (uint32_t pass_idx : active_passes) {
    if (pass_positions.find(pass_idx) == pass_positions.end()) continue;
    const auto& pass_pos = pass_positions.at(pass_idx);

    for (const auto& access : graph.m_passes[pass_idx]->get_resource_accesses()) {
      if (resource_positions.find(access.resource_index) == resource_positions.end()) continue;
      const auto& res_pos = resource_positions.at(access.resource_index);

      std::string mode;
      if (access.mode == RenderGraphResourceAccessMode::Write) {
        mode = "W";
      } else if (access.mode == RenderGraphResourceAccessMode::Read) {
        mode = "R";
      } else {
        mode = "RW";
      }

      edges.push_back({pass_pos.center_x, pass_pos.box_y + pass_pos.height,
                      res_pos.center_x, res_pos.box_y, mode});
    }
  }

  draw_edges(canvas, box_mask, edges);

  for (const auto& [pass_idx, pos] : pass_positions) {
    const auto& pass = graph.m_passes[pass_idx];
    draw_pass_box(canvas, pos, pass->get_name());
  }

  for (const auto& [res_idx, pos] : resource_positions) {
    const auto& resource = graph.m_resources[res_idx];
    draw_resource_box(canvas, pos, resource.desc.name);
  }

  for (const auto& line : canvas) {
    size_t end = line.find_last_not_of(' ');
    if (end != std::string::npos) {
      file << "  " << line.substr(0, end + 1) << "\n";
    }
  }

  file << "\n";
  file << "  Legend: [Pass boxes] use +--+ borders, <Resource boxes> use <===> borders\n";
  file << "          Vertical lines show data flow from passes (top) to resources (bottom)\n";
  file << "          Labels: [W]=Write, [R]=Read, [RW]=ReadWrite\n";
}

void AsciiExporter::write_passes_list(std::ofstream& file, const RenderGraph& graph) const {
  file << "\n";
  file << "PASSES (" << graph.m_passes.size() << "):\n";
  file << "-----------\n";
  for (uint32_t i = 0; i < graph.m_passes.size(); ++i) {
    const auto &pass = graph.m_passes[i];
    file << "  [" << i << "] " << pass->get_name();

    std::string status = pass->is_culled() ? "(culled)" : "(active)";
    file << " " << status;

    if (!pass->is_culled()) {
      file << " [priority=" << pass->get_priority() << "]";
    }
    file << "\n";
  }
}

void AsciiExporter::write_resources_list(std::ofstream& file, const RenderGraph& graph) const {
  file << "\n";
  file << "RESOURCES (" << graph.m_resources.size() << "):\n";
  file << "--------------\n";
  for (uint32_t i = 0; i < graph.m_resources.size(); ++i) {
    const auto &resource = graph.m_resources[i];
    file << "  [" << i << "] " << resource.desc.name;
    file << " (" << (resource.is_transient() ? "transient" : "persistent");
    if (resource.alias_group >= 0) {
      file << ", alias_group=" << resource.alias_group;
    }
    file << ")\n";

    std::vector<uint32_t> writers;
    std::vector<uint32_t> readers;

    for (uint32_t pass_idx = 0; pass_idx < graph.m_passes.size(); ++pass_idx) {
      const auto &pass = graph.m_passes[pass_idx];
      for (const auto &access : pass->get_resource_accesses()) {
        if (access.resource_index == i) {
          if (access.mode == RenderGraphResourceAccessMode::Write) {
            writers.push_back(pass_idx);
          } else if (access.mode == RenderGraphResourceAccessMode::Read) {
            readers.push_back(pass_idx);
          } else {
            writers.push_back(pass_idx);
            readers.push_back(pass_idx);
          }
        }
      }
    }

    if (!writers.empty()) {
      file << "      - Written by: ";
      for (size_t j = 0; j < writers.size(); ++j) {
        file << graph.m_passes[writers[j]]->get_name();
        if (j < writers.size() - 1) file << ", ";
      }
      file << "\n";
    }

    if (!readers.empty()) {
      file << "      - Read by: ";
      for (size_t j = 0; j < readers.size(); ++j) {
        file << graph.m_passes[readers[j]]->get_name();
        if (j < readers.size() - 1) file << ", ";
      }
      file << "\n";
    }

    if (resource.is_transient()) {
      file << "      - Lifetime: pass " << resource.first_write_pass
           << " to pass " << resource.last_read_pass << "\n";
    }
  }
}

void AsciiExporter::write_execution_order(std::ofstream& file, const RenderGraph& graph) const {
  file << "\n";
  file << "EXECUTION ORDER:\n";
  file << "----------------\n";
  for (size_t i = 0; i < graph.m_execution_order.size(); ++i) {
    uint32_t pass_idx = graph.m_execution_order[i];
    const auto &pass = graph.m_passes[pass_idx];

    if (pass->is_culled()) continue;

    file << "  " << (i + 1) << ". " << pass->get_name();

    const auto &deps = pass->get_computed_dependency_indices();
    if (!deps.empty()) {
      file << " (depends on: ";
      for (size_t j = 0; j < deps.size(); ++j) {
        file << graph.m_passes[deps[j]]->get_name();
        if (j < deps.size() - 1) file << ", ";
      }
      file << ")";
    }
    file << "\n";
  }
}

void AsciiExporter::write_resource_flow(std::ofstream& file, const RenderGraph& graph) const {
  file << "\n";
  file << "RESOURCE FLOW:\n";
  file << "--------------\n";

  std::unordered_map<uint32_t, std::vector<std::string>> pass_flows;

  for (uint32_t pass_idx = 0; pass_idx < graph.m_passes.size(); ++pass_idx) {
    const auto &pass = graph.m_passes[pass_idx];
    if (pass->is_culled()) continue;

    for (const auto &access : pass->get_resource_accesses()) {
      const auto &resource = graph.m_resources[access.resource_index];
      std::string flow;

      if (access.mode == RenderGraphResourceAccessMode::Write) {
        flow = pass->get_name() + " --[write]--> " + resource.desc.name;
      } else if (access.mode == RenderGraphResourceAccessMode::Read) {
        flow = resource.desc.name + " --[read]--> " + pass->get_name();
      } else {
        flow = pass->get_name() + " <--[read/write]--> " + resource.desc.name;
      }

      pass_flows[pass_idx].push_back(flow);
    }
  }

  for (uint32_t pass_idx : graph.m_execution_order) {
    if (graph.m_passes[pass_idx]->is_culled()) continue;

    if (pass_flows.find(pass_idx) != pass_flows.end()) {
      for (const auto &flow : pass_flows[pass_idx]) {
        file << "  " << flow << "\n";
      }
    }
  }
}

void AsciiExporter::write_statistics(std::ofstream& file, const RenderGraph& graph) const {
  file << "\n";
  file << "STATISTICS:\n";
  file << "-----------\n";

  uint32_t active_passes = 0;
  uint32_t culled_passes = 0;
  for (const auto &pass : graph.m_passes) {
    if (pass->is_culled()) {
      culled_passes++;
    } else {
      active_passes++;
    }
  }

  uint32_t transient_resources = 0;
  uint32_t persistent_resources = 0;
  for (const auto &resource : graph.m_resources) {
    if (resource.is_transient()) {
      transient_resources++;
    } else {
      persistent_resources++;
    }
  }

  file << "  Total passes: " << graph.m_passes.size() << "\n";
  file << "    - Active: " << active_passes << "\n";
  file << "    - Culled: " << culled_passes << "\n";
  file << "  Total resources: " << graph.m_resources.size() << "\n";
  file << "    - Transient: " << transient_resources << "\n";
  file << "    - Persistent: " << persistent_resources << "\n";
}

std::vector<std::vector<uint32_t>> AsciiExporter::compute_pass_layers(
  const RenderGraph& graph,
  const std::vector<uint32_t>& active_passes) const {

  std::vector<std::vector<uint32_t>> layers;
  std::vector<int32_t> pass_layer(graph.m_passes.size(), -1);

  std::function<int32_t(uint32_t)> compute_layer = [&](uint32_t pass_idx) -> int32_t {
    if (pass_layer[pass_idx] != -1) {
      return pass_layer[pass_idx];
    }

    const auto& deps = graph.m_passes[pass_idx]->get_computed_dependency_indices();
    if (deps.empty()) {
      pass_layer[pass_idx] = 0;
      return 0;
    }

    int32_t max_dep_layer = -1;
    for (uint32_t dep_idx : deps) {
      if (!graph.m_passes[dep_idx]->is_culled()) {
        int32_t dep_layer = compute_layer(dep_idx);
        max_dep_layer = std::max(max_dep_layer, dep_layer);
      }
    }

    pass_layer[pass_idx] = max_dep_layer + 1;
    return pass_layer[pass_idx];
  };

  for (uint32_t pass_idx : active_passes) {
    int32_t layer = compute_layer(pass_idx);
    while (layers.size() <= static_cast<size_t>(layer)) {
      layers.emplace_back();
    }
    layers[layer].push_back(pass_idx);
  }

  return layers;
}

std::unordered_map<uint32_t, AsciiExporter::NodePosition> AsciiExporter::compute_pass_positions(
  const std::vector<std::vector<uint32_t>>& layers,
  const LayoutConfig& config) const {

  std::unordered_map<uint32_t, NodePosition> positions;

  for (size_t layer_idx = 0; layer_idx < layers.size(); ++layer_idx) {
    const auto& layer = layers[layer_idx];
    size_t x = layer_idx * (config.box_width + config.horizontal_spacing) + 2;
    size_t start_y = 2;

    for (size_t pos_in_layer = 0; pos_in_layer < layer.size(); ++pos_in_layer) {
      uint32_t pass_idx = layer[pos_in_layer];
      size_t y = start_y + pos_in_layer * (config.box_height + config.vertical_spacing);

      NodePosition pos;
      pos.box_x = x;
      pos.box_y = y;
      pos.width = config.box_width;
      pos.height = config.box_height;
      pos.center_x = x + config.box_width / 2;
      pos.center_y = y + config.box_height / 2;
      positions[pass_idx] = pos;
    }
  }

  return positions;
}

std::unordered_map<uint32_t, AsciiExporter::NodePosition> AsciiExporter::compute_resource_positions(
  const RenderGraph& graph,
  const std::vector<uint32_t>& active_passes,
  const LayoutConfig& config,
  size_t resources_y,
  size_t canvas_width) const {

  const auto used_resources = get_used_resources(graph, active_passes);
  std::unordered_map<uint32_t, NodePosition> positions;

  size_t resource_spacing = canvas_width / (used_resources.size() + 1);
  for (size_t i = 0; i < used_resources.size(); ++i) {
    uint32_t res_idx = used_resources[i];
    size_t x = (i + 1) * resource_spacing - config.box_width / 2;
    size_t y = resources_y;

    if (x + config.box_width > canvas_width) {
      x = canvas_width - config.box_width - 2;
    }

    NodePosition pos;
    pos.box_x = x;
    pos.box_y = y;
    pos.width = config.box_width;
    pos.height = config.box_height;
    pos.center_x = x + config.box_width / 2;
    pos.center_y = y + config.box_height / 2;
    positions[res_idx] = pos;
  }

  return positions;
}

void AsciiExporter::draw_edges(Canvas& canvas, BoxMask& mask, const std::vector<EdgeInfo>& edges) const {
  const size_t canvas_height = canvas.size();
  const size_t canvas_width = canvas.empty() ? 0 : canvas[0].size();

  for (const auto& edge : edges) {
    size_t from_x = edge.from_x;
    size_t from_y = edge.from_y;
    size_t to_x = edge.to_x;
    size_t to_y = edge.to_y;

    for (size_t y = from_y; y < to_y && y < canvas_height; ++y) {
      if (from_x < canvas_width && !mask[y][from_x]) {
        canvas[y][from_x] = '|';
      }
    }

    if (from_x < canvas_width && to_y < canvas_height && !mask[to_y][from_x]) {
      canvas[to_y][from_x] = '+';
    }

    if (from_x != to_x) {
      size_t min_x = std::min(from_x, to_x);
      size_t max_x = std::max(from_x, to_x);
      for (size_t x = min_x; x <= max_x && x < canvas_width; ++x) {
        if (to_y < canvas_height && !mask[to_y][x]) {
          canvas[to_y][x] = '-';
        }
      }
    }

    if (!edge.label.empty()) {
      std::string label = "[" + edge.label + "]";
      size_t label_x = from_x > label.length() / 2 ? from_x - label.length() / 2 : from_x + 2;
      size_t label_y = (from_y + to_y) / 2;

      if (label_y < canvas_height && label_x + label.length() < canvas_width) {
        bool can_place = true;
        for (size_t i = 0; i < label.length(); ++i) {
          if (label_x + i < canvas_width && mask[label_y][label_x + i]) {
            can_place = false;
            break;
          }
        }
        if (can_place) {
          for (size_t i = 0; i < label.length() && label_x + i < canvas_width; ++i) {
            canvas[label_y][label_x + i] = label[i];
          }
        }
      }
    }
  }
}

void AsciiExporter::draw_box(Canvas& canvas, const NodePosition& pos, const std::string& name,
                              char h_border, char v_border, char corner) const {
  const std::string truncated_name = truncate_name(name, pos.width);

  for (size_t i = 0; i < pos.width; ++i) {
    canvas[pos.box_y][pos.box_x + i] = h_border;
    canvas[pos.box_y + pos.height - 1][pos.box_x + i] = h_border;
  }

  for (size_t i = 0; i < pos.height; ++i) {
    canvas[pos.box_y + i][pos.box_x] = v_border;
    canvas[pos.box_y + i][pos.box_x + pos.width - 1] = v_border;
  }

  canvas[pos.box_y][pos.box_x] = corner;
  canvas[pos.box_y][pos.box_x + pos.width - 1] = corner;
  canvas[pos.box_y + pos.height - 1][pos.box_x] = corner;
  canvas[pos.box_y + pos.height - 1][pos.box_x + pos.width - 1] = corner;

  size_t text_start = pos.box_x + (pos.width - truncated_name.length()) / 2;
  for (size_t i = 0; i < truncated_name.length(); ++i) {
    canvas[pos.center_y][text_start + i] = truncated_name[i];
  }
}

void AsciiExporter::draw_pass_box(Canvas& canvas, const NodePosition& pos, const std::string& name) const {
  draw_box(canvas, pos, name, '-', '|', '+');
}

void AsciiExporter::draw_resource_box(Canvas& canvas, const NodePosition& pos, const std::string& name) const {
  const std::string truncated_name = truncate_name(name, pos.width);

  for (size_t i = 0; i < pos.width; ++i) {
    canvas[pos.box_y][pos.box_x + i] = '=';
    canvas[pos.box_y + pos.height - 1][pos.box_x + i] = '=';
  }

  for (size_t i = 0; i < pos.height; ++i) {
    canvas[pos.box_y + i][pos.box_x] = '<';
    canvas[pos.box_y + i][pos.box_x + pos.width - 1] = '>';
  }

  size_t text_start = pos.box_x + (pos.width - truncated_name.length()) / 2;
  for (size_t i = 0; i < truncated_name.length(); ++i) {
    canvas[pos.center_y][text_start + i] = truncated_name[i];
  }
}

std::vector<uint32_t> AsciiExporter::get_active_passes(const RenderGraph& graph) const {
  std::vector<uint32_t> active_passes;
  for (uint32_t pass_idx : graph.m_execution_order) {
    if (!graph.m_passes[pass_idx]->is_culled()) {
      active_passes.push_back(pass_idx);
    }
  }
  return active_passes;
}

std::vector<uint32_t> AsciiExporter::get_used_resources(
  const RenderGraph& graph,
  const std::vector<uint32_t>& active_passes) const {

  std::vector<uint32_t> used_resources;
  for (uint32_t pass_idx : active_passes) {
    for (const auto& access : graph.m_passes[pass_idx]->get_resource_accesses()) {
      if (std::find(used_resources.begin(), used_resources.end(),
                   access.resource_index) == used_resources.end()) {
        used_resources.push_back(access.resource_index);
      }
    }
  }
  return used_resources;
}

std::string AsciiExporter::truncate_name(const std::string& name, size_t max_width) const {
  if (name.length() > max_width - 4) {
    return name.substr(0, max_width - 7) + "...";
  }
  return name;
}

}
