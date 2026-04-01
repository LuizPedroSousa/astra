#include "document/document.hpp"

#include <utility>

namespace astralix::ui {

UINodeId UIDocument::create_view() { return allocate_node(NodeType::View); }

} // namespace astralix::ui
