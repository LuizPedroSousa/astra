#pragma once

#include <string>

namespace astralix {

class RenderGraph;

class RenderGraphExporter {
public:
  virtual ~RenderGraphExporter() = default;

  virtual void export_graph(const RenderGraph& graph, const std::string& filename) const = 0;
  virtual std::string get_format_name() const = 0;
};

}
