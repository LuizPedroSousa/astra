#include "graphviz-exporter.hpp"
#include "../render-graph-usage.hpp"
#include "../render-graph.hpp"
#include "log.hpp"
#include <fstream>

namespace astralix {

void GraphvizExporter::export_graph(const RenderGraph &graph, const std::string &filename) const {
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
    file << "  pass_" << i << " [label=\"" << pass->get_name();

    const auto &transitions = pass->get_compiled_transitions();
    if (!transitions.empty()) {
      file << "\\n---";
      for (const auto &transition : transitions) {
        std::string resource_name = "?";
        if (transition.view.resource_index() < graph.m_resources.size()) {
          resource_name =
              graph.m_resources[transition.view.resource_index()].desc.name;
        }
        file << "\\n"
             << resource_name << ": "
             << resource_state_label(transition.before) << " -> "
             << resource_state_label(transition.after);
      }
    }

    file << "\"";
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

    const auto &usages = pass->get_image_usages();
    if (!usages.empty()) {
      for (const auto &usage : usages) {
        const char *label = render_usage_label(usage.usage);
        const char *color = is_write_usage(usage.usage) ? "red" : "blue";
        if (is_write_usage(usage.usage)) {
          file << "  pass_" << pass_idx << " -> resource_"
               << usage.view.resource_index();
        } else {
          file << "  resource_" << usage.view.resource_index() << " -> pass_"
               << pass_idx;
        }
        file << " [label=\"" << label << "\", color=" << color << "];\n";
      }
    } else {
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

    if (pass->has_present()) {
      const auto &present_request = pass->get_present();
      file << "  pass_" << pass_idx << " -> present_edge";
      file << " [label=\"present\", color=green, style=bold];\n";
    }

    for (const auto &export_request : pass->get_exports()) {
      file << "  pass_" << pass_idx << " -> export_"
           << static_cast<int>(export_request.key.resource);
      file << " [label=\"export\", color=orange, style=bold];\n";
    }
  }

  if (!graph.compiled_present_edges().empty()) {
    file << "  present_edge [label=\"Present\", shape=octagon, "
            "style=filled, fillcolor=lightcoral];\n";
  }

  for (const auto &compiled_export : graph.compiled_exports()) {
    file << "  export_" << static_cast<int>(compiled_export.key.resource)
         << " [label=\"Export\\n"
         << (compiled_export.direct_bind ? "direct" : "materialize")
         << "\", shape=hexagon, style=filled, fillcolor=lightsalmon];\n";
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

} // namespace astralix
