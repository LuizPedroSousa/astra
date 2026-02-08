#pragma once

#include "render-graph-exporter.hpp"

namespace astralix {

class MermaidExporter : public RenderGraphExporter {
public:
  void export_graph(const RenderGraph& graph, const std::string& filename) const override;
  std::string get_format_name() const override { return "Mermaid (Markdown)"; }
};

}
