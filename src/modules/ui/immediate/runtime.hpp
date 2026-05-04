#pragma once

#include "base.hpp"
#include "document/document.hpp"
#include "immediate/frame.hpp"
#include "trace.hpp"

#include "containers/flat-hash-map.hpp"

#include <unordered_map>
#include <vector>

namespace astralix::ui::im {

  class Runtime {
  public:
    Runtime(Ref<UIDocument> document, UINodeId host_node_id);

    template <typename DrawFn>
    void render(DrawFn&& draw) {
      ASTRA_PROFILE_N("im::Runtime::render");

      ASTRA_ENSURE(
        m_document == nullptr || m_host_node_id == k_invalid_node_id,
        "ui::im::Runtime requires a valid document and host node"
      );

      m_frame_allocator.reset();
      pre_reserve_frame_allocator();
      Frame frame(this, &m_frame_allocator, m_root_scope_id, m_last_frame_node_count);
      draw(frame);
      frame.flatten_children();
      {
        ASTRA_PROFILE_N("im::Runtime::reconcile");
        reconcile_host_children(frame);
      }
      apply_requests(frame);
      m_last_frame_node_count = frame.node_count();
    }

    Ref<UIDocument> document() const { return m_document; }
    UINodeId host_node_id() const { return m_host_node_id; }

    VirtualListState virtual_list_state(WidgetId widget_id) const;
    std::optional<UIViewTransform2D> view_transform(WidgetId widget_id) const;
    std::optional<UIGraphSelection> graph_selection(WidgetId widget_id) const;
    float measured_node_height(WidgetId widget_id) const;
    std::optional<UIRect> layout_bounds(WidgetId widget_id) const;
    bool combobox_open(WidgetId widget_id) const;
    size_t combobox_highlighted_index(WidgetId widget_id) const;

    UINodeId node_id_for(WidgetId widget_id) const;

  private:
    struct NodeRuntime {
      UINodeId node_id = k_invalid_node_id;
      dsl::NodeKind kind = dsl::NodeKind::View;
      UIDocument::UINode defaults;
      uint64_t last_parent_generation = 0u;
      uint64_t last_style_hash = 0u;
    };

    UINodeId reconcile_node(const Frame& frame, NodeId node_id);
    void reconcile_children(
      UINodeId parent_id,
      const Frame& frame,
      uint32_t first_child_offset,
      size_t child_count
    );
    UINodeId create_node(const NodeHeader& header);
    void apply_node(
      UINodeId node_id,
      const Frame& frame,
      const NodeHeader& header,
      const NodeState& state
    );
    void destroy_runtime_subtree(UINodeId node_id);
    void erase_mapping_for_subtree(UINodeId node_id);
    void reconcile_host_children(const Frame& frame);
    void apply_requests(const Frame& frame);
    void apply_popover_state(UINodeId node_id, const PopoverState& state);

    struct CallbackSlot {
      NodeCallbackPayload payload;
    };

    void install_callback_forwarders(UINodeId node_id, WidgetId widget_id);

    void pre_reserve_frame_allocator();

    BumpAllocator m_frame_allocator{ 0u };
    size_t m_last_frame_node_count = 0u;
    Ref<UIDocument> m_document = nullptr;
    UINodeId m_host_node_id = k_invalid_node_id;
    WidgetId m_root_scope_id = WidgetId{ 1u };
    uint64_t m_next_parent_generation = 1u;
    FlatHashMap<WidgetId, NodeRuntime> m_widget_nodes;
    std::vector<WidgetId> m_node_widgets;
    std::vector<bool> m_live_nodes;
    std::unordered_map<WidgetId, Scope<CallbackSlot>> m_callback_slots;
  };

} // namespace astralix::ui::im
