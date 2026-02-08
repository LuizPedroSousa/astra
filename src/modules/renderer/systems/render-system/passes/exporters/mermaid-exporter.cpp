#include "mermaid-exporter.hpp"
#include "../render-graph.hpp"
#include "log.hpp"
#include <fstream>

namespace astralix {

void MermaidExporter::export_graph(const RenderGraph& graph, const std::string& filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("[RenderGraph] Failed to open file for Mermaid export");
    return;
  }

  file << "graph TD\n\n";

  for (uint32_t i = 0; i < graph.m_passes.size(); ++i) {
    const auto &pass = graph.m_passes[i];
    file << "  pass_" << i << "[" << pass->get_name() << "]\n";
  }

  file << "\n";

  for (uint32_t i = 0; i < graph.m_resources.size(); ++i) {
    const auto &resource = graph.m_resources[i];
    if (resource.is_persistent()) {
      file << "  resource_" << i << "((" << resource.desc.name;
    } else {
      file << "  resource_" << i << "{" << resource.desc.name;
    }
    if (resource.alias_group >= 0) {
      file << "<br/>alias_group=" << resource.alias_group;
    }
    if (resource.is_persistent()) {
      file << "))\n";
    } else {
      file << "}\n";
    }
  }

  file << "\n";

  uint32_t link_index = 0;
  for (uint32_t pass_idx = 0; pass_idx < graph.m_passes.size(); ++pass_idx) {
    const auto &pass = graph.m_passes[pass_idx];

    for (const auto &access : pass->get_resource_accesses()) {
      if (access.mode == RenderGraphResourceAccessMode::Write) {
        file << "  pass_" << pass_idx << " -->|write| resource_"
             << access.resource_index << "\n";
        file << "  linkStyle " << link_index << " stroke:red\n";
        link_index++;
      } else if (access.mode == RenderGraphResourceAccessMode::Read) {
        file << "  resource_" << access.resource_index << " -->|read| pass_"
             << pass_idx << "\n";
        file << "  linkStyle " << link_index << " stroke:blue\n";
        link_index++;
      } else {
        file << "  pass_" << pass_idx << " <-->|read_write| resource_"
             << access.resource_index << "\n";
        file << "  linkStyle " << link_index << " stroke:purple\n";
        link_index++;
      }
    }
  }

  file << "\n";

  for (uint32_t i = 0; i < graph.m_passes.size(); ++i) {
    const auto &pass = graph.m_passes[i];
    for (uint32_t dep_idx : pass->get_computed_dependency_indices()) {
      file << "  pass_" << dep_idx << " -.-> pass_" << i << "\n";
      file << "  linkStyle " << link_index << " stroke:gray\n";
      link_index++;
    }
  }

  file << "\n";

  for (uint32_t i = 0; i < graph.m_passes.size(); ++i) {
    const auto &pass = graph.m_passes[i];
    std::string fill_color = pass->is_culled() ? "#999" : "#add8e6";
    std::string text_color = pass->is_culled() ? "#666" : "#000";
    file << "  style pass_" << i << " fill:" << fill_color
         << ",color:" << text_color << "\n";
  }

  file << "\n";

  for (uint32_t i = 0; i < graph.m_resources.size(); ++i) {
    const auto &resource = graph.m_resources[i];
    std::string fill_color = resource.is_read ? "#90ee90" : "#ffffe0";
    std::string text_color = resource.is_read ? "#004400" : "#666600";
    file << "  style resource_" << i << " fill:" << fill_color
         << ",color:" << text_color << "\n";
  }

  file.close();

  LOG_INFO("[RenderGraph] Exported graph to Mermaid file");
}

}
