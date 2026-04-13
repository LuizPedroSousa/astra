#include "mermaid-exporter.hpp"
#include "../render-graph.hpp"
#include "../render-graph-usage.hpp"
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
    file << "  pass_" << i << "[" << pass->get_name();

    const auto &transitions = pass->get_compiled_transitions();
    if (!transitions.empty()) {
      file << "<br/>---";
      for (const auto &transition : transitions) {
        std::string resource_name = "?";
        if (transition.view.resource_index() < graph.m_resources.size()) {
          resource_name =
              graph.m_resources[transition.view.resource_index()].desc.name;
        }
        file << "<br/>" << resource_name << ": "
             << resource_state_label(transition.before) << " -> "
             << resource_state_label(transition.after);
      }
    }

    file << "]\n";
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

    const auto &usages = pass->get_image_usages();
    if (!usages.empty()) {
      for (const auto &usage : usages) {
        const char *label = render_usage_label(usage.usage);
        const char *color = is_write_usage(usage.usage) ? "red" : "blue";
        if (is_write_usage(usage.usage)) {
          file << "  pass_" << pass_idx << " -->|" << label << "| resource_"
               << usage.view.resource_index() << "\n";
        } else {
          file << "  resource_" << usage.view.resource_index() << " -->|" << label
               << "| pass_" << pass_idx << "\n";
        }
        file << "  linkStyle " << link_index << " stroke:" << color << "\n";
        link_index++;
      }
    } else {
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

    if (pass->has_present()) {
      file << "  pass_" << pass_idx << " -->|present| present_target\n";
      file << "  linkStyle " << link_index << " stroke:green\n";
      link_index++;
    }

    for (const auto &export_request : pass->get_exports()) {
      file << "  pass_" << pass_idx << " -->|export| export_"
           << static_cast<int>(export_request.key.resource) << "\n";
      file << "  linkStyle " << link_index << " stroke:orange\n";
      link_index++;
    }
  }

  if (!graph.compiled_present_edges().empty()) {
    file << "  present_target{{Present}}\n";
    file << "  style present_target fill:#ff6666,color:#fff\n";
  }

  for (const auto &compiled_export : graph.compiled_exports()) {
    file << "  export_" << static_cast<int>(compiled_export.key.resource)
         << "{{Export "
         << (compiled_export.direct_bind ? "direct" : "materialize") << "}}\n";
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
