#include "document/document.hpp"

#include <utility>

namespace astralix::ui {

UINodeId UIDocument::create_view(std::string name) {
  return allocate_node(NodeType::View, std::move(name));
}

} // namespace astralix::ui
