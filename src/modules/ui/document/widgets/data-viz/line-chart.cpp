#include "document/document.hpp"

namespace astralix::ui {

UINodeId UIDocument::create_line_chart() {
  return allocate_node(NodeType::LineChart);
}

} // namespace astralix::ui
