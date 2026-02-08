#include "graphviz-exporter.hpp"
#include "../render-graph.hpp"
#include "log.hpp"
#include <fstream>

namespace astralix {

void GraphvizExporter::export_graph(const RenderGraph& graph, const std::string& filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("[RenderGraph] Failed to open file for Graphviz export");
    return;
  }

  file << "digraph RenderGraph {\n";
  file << "  rankdir=TB;\n";
  file << "  node [shape=box];\n\n";

  for (uint32_t i = 0; i < graph.m_passes.size(); ++i) {
    const auto &pass = graph.m_passes[i];
    std::string color = pass->is_culled() ? "gray" : "lightblue";
    file << "  pass_" << i << " [label=\"" << pass->get_name() << "\"";
    file << ", style=filled, fillcolor=" << color << "];\n";
  }

  file << "\n";

  for (uint32_t i = 0; i < graph.m_resources.size(); ++i) {
    const auto &resource = graph.m_resources[i];
    std::string shape = resource.is_persistent() ? "ellipse" : "diamond";
    std::string color = resource.is_read ? "lightgreen" : "lightyellow";

    file << "  resource_" << i << " [label=\"" << resource.desc.name;
    if (resource.alias_group >= 0) {
      file << "\\nalias_group=" << resource.alias_group;
    }
    file << "\", shape=" << shape << ", style=filled, fillcolor=" << color
         << "];\n";
  }

  file << "\n";

  for (uint32_t pass_idx = 0; pass_idx < graph.m_passes.size(); ++pass_idx) {
    const auto &pass = graph.m_passes[pass_idx];

    for (const auto &access : pass->get_resource_accesses()) {
      if (access.mode == RenderGraphResourceAccessMode::Write) {
        file << "  pass_" << pass_idx << " -> resource_"
             << access.resource_index;
        file << " [label=\"write\", color=red];\n";
      } else if (access.mode == RenderGraphResourceAccessMode::Read) {
        file << "  resource_" << access.resource_index << " -> pass_"
             << pass_idx;
        file << " [label=\"read\", color=blue];\n";
      } else {
        file << "  pass_" << pass_idx << " -> resource_"
             << access.resource_index;
        file << " [label=\"read_write\", color=purple, dir=both];\n";
      }
    }
  }

  file << "\n";

  for (uint32_t i = 0; i < graph.m_passes.size(); ++i) {
    const auto &pass = graph.m_passes[i];
    for (uint32_t dep_idx : pass->get_computed_dependency_indices()) {
      file << "  pass_" << dep_idx << " -> pass_" << i;
      file << " [style=dashed, color=gray];\n";
    }
  }

  file << "}\n";
  file.close();

  LOG_INFO("[RenderGraph] Exported graph to DOT file");
}

}
