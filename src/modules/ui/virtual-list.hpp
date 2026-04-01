#pragma once

#include "document/document.hpp"
#include "foundations.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <vector>

namespace astralix::ui {

struct UIVirtualListRange {
  size_t start = 0u;
  size_t end = 0u;
  bool valid = false;

  bool empty() const { return !valid; }
  size_t count() const { return valid ? (end - start + 1u) : 0u; }
};

class VirtualListController {
public:
  using CreateSlotCallback = std::function<UINodeId(size_t slot_index)>;
  using BindSlotCallback =
      std::function<void(size_t slot_index, UINodeId slot_root, size_t item_index)>;

  VirtualListController(
      Ref<UIDocument> document,
      UINodeId scroll_view,
      CreateSlotCallback create_slot,
      BindSlotCallback bind_slot
  )
      : m_document(std::move(document)),
        m_scroll_view(scroll_view),
        m_create_slot(std::move(create_slot)),
        m_bind_slot(std::move(bind_slot)) {
    ensure_structure();
  }

  void reset() {
    m_item_heights.clear();
    m_prefix_heights.clear();
    m_prefix_dirty = true;
    m_current_range = UIVirtualListRange{};
    m_content_width = 0.0f;

    update_spacer_node(m_top_spacer, 0.0f, false, 0.0f);
    update_spacer_node(m_bottom_spacer, 0.0f, false, 0.0f);

    if (m_document == nullptr) {
      return;
    }

    for (Slot &slot : m_slots) {
      m_document->set_visible(slot.root, false);
      slot.item_index.reset();
    }
  }

  void set_item_count(size_t item_count) {
    if (m_item_heights.size() == item_count) {
      return;
    }

    m_item_heights.resize(item_count, 0.0f);
    m_prefix_dirty = true;
  }

  void set_item_height(size_t index, float height) {
    if (index >= m_item_heights.size()) {
      return;
    }

    const float clamped_height = std::max(0.0f, height);
    if (m_item_heights[index] == clamped_height) {
      return;
    }

    m_item_heights[index] = clamped_height;
    m_prefix_dirty = true;
  }

  void set_content_width(float content_width) {
    m_content_width = std::max(0.0f, content_width);
  }

  void set_overscan(size_t overscan) { m_overscan = overscan; }

  void refresh(bool force = false) {
    if (m_document == nullptr || m_scroll_view == k_invalid_node_id) {
      return;
    }

    ensure_structure();
    rebuild_prefix_heights();

    const UIVirtualListRange next_range = compute_range();
    const float viewport_width = scroll_viewport_width();
    const float resolved_width =
        std::max(m_content_width, std::max(0.0f, viewport_width));
    const bool range_changed =
        next_range.valid != m_current_range.valid ||
        next_range.start != m_current_range.start ||
        next_range.end != m_current_range.end;

    ensure_slot_count(next_range.count());
    update_spacers(next_range, resolved_width);

    for (size_t slot_index = 0u; slot_index < m_slots.size(); ++slot_index) {
      Slot &slot = m_slots[slot_index];
      if (slot_index >= next_range.count()) {
        m_document->set_visible(slot.root, false);
        slot.item_index.reset();
        continue;
      }

      const size_t item_index = next_range.start + slot_index;
      const float item_height =
          item_index < m_item_heights.size() ? m_item_heights[item_index] : 0.0f;
      update_slot_node(slot.root, item_height, resolved_width);

      if (force || range_changed || !slot.item_index.has_value() ||
          *slot.item_index != item_index) {
        m_bind_slot(slot_index, slot.root, item_index);
        slot.item_index = item_index;
      }
    }

    m_current_range = next_range;
  }

  UIVirtualListRange current_range() const { return m_current_range; }
  size_t pool_size() const { return m_slots.size(); }
  UINodeId top_spacer() const { return m_top_spacer; }
  UINodeId bottom_spacer() const { return m_bottom_spacer; }

private:
  struct Slot {
    UINodeId root = k_invalid_node_id;
    std::optional<size_t> item_index;
  };

  void ensure_structure() {
    if (m_document == nullptr || m_scroll_view == k_invalid_node_id) {
      return;
    }

    if (m_top_spacer == k_invalid_node_id) {
      m_top_spacer = m_document->create_view();
      m_document->append_child(m_scroll_view, m_top_spacer);
      m_document->set_visible(m_top_spacer, false);
    }

    if (m_bottom_spacer == k_invalid_node_id) {
      m_bottom_spacer = m_document->create_view();
      m_document->append_child(m_scroll_view, m_bottom_spacer);
      m_document->set_visible(m_bottom_spacer, false);
    }
  }

  void ensure_slot_count(size_t slot_count) {
    if (m_document == nullptr || m_scroll_view == k_invalid_node_id) {
      return;
    }

    ensure_structure();
    while (m_slots.size() < slot_count) {
      if (m_bottom_spacer != k_invalid_node_id) {
        m_document->remove_child(m_bottom_spacer);
      }

      Slot slot;
      slot.root = m_create_slot(m_slots.size());
      m_document->append_child(m_scroll_view, slot.root);
      m_document->set_visible(slot.root, false);
      m_slots.push_back(slot);

      if (m_bottom_spacer != k_invalid_node_id) {
        m_document->append_child(m_scroll_view, m_bottom_spacer);
      }
    }
  }

  void rebuild_prefix_heights() {
    if (!m_prefix_dirty) {
      return;
    }

    m_prefix_heights.assign(m_item_heights.size() + 1u, 0.0f);
    for (size_t index = 0u; index < m_item_heights.size(); ++index) {
      m_prefix_heights[index + 1u] = m_prefix_heights[index] + m_item_heights[index];
    }
    m_prefix_dirty = false;
  }

  float vertical_gap() const {
    if (m_document == nullptr) {
      return 0.0f;
    }

    const auto *scroll_view = m_document->node(m_scroll_view);
    if (scroll_view == nullptr || scroll_view->style.flex_direction != FlexDirection::Column) {
      return 0.0f;
    }

    return std::max(0.0f, scroll_view->style.gap);
  }

  float scroll_offset_y() const {
    if (m_document == nullptr) {
      return 0.0f;
    }

    const auto *scroll_view = m_document->node(m_scroll_view);
    if (scroll_view == nullptr) {
      return 0.0f;
    }

    return std::max(0.0f, scroll_view->layout.scroll.offset.y);
  }

  float scroll_viewport_height() const {
    if (m_document == nullptr) {
      return 0.0f;
    }

    const auto *scroll_view = m_document->node(m_scroll_view);
    if (scroll_view == nullptr) {
      return 0.0f;
    }

    return std::max(0.0f, scroll_view->layout.scroll.viewport_size.y);
  }

  float scroll_viewport_width() const {
    if (m_document == nullptr) {
      return 0.0f;
    }

    const auto *scroll_view = m_document->node(m_scroll_view);
    if (scroll_view == nullptr) {
      return 0.0f;
    }

    return std::max(0.0f, scroll_view->layout.scroll.viewport_size.x);
  }

  float item_start(size_t index) const {
    const float gap = vertical_gap();
    return index < m_prefix_heights.size() ? m_prefix_heights[index] + gap * static_cast<float>(index)
                                           : 0.0f;
  }

  float item_end(size_t index) const {
    if (index >= m_item_heights.size()) {
      return 0.0f;
    }

    return item_start(index) + m_item_heights[index];
  }

  size_t first_item_after(float scroll_top) const {
    size_t left = 0u;
    size_t right = m_item_heights.size();
    while (left < right) {
      const size_t middle = left + (right - left) / 2u;
      if (item_end(middle) <= scroll_top) {
        left = middle + 1u;
      } else {
        right = middle;
      }
    }

    return std::min(left, m_item_heights.size() - 1u);
  }

  size_t last_item_before(float scroll_bottom) const {
    size_t left = 0u;
    size_t right = m_item_heights.size();
    while (left < right) {
      const size_t middle = left + (right - left) / 2u;
      if (item_start(middle) < scroll_bottom) {
        left = middle + 1u;
      } else {
        right = middle;
      }
    }

    return left == 0u ? 0u : left - 1u;
  }

  UIVirtualListRange compute_range() const {
    if (m_item_heights.empty()) {
      return UIVirtualListRange{};
    }

    const float viewport_height = scroll_viewport_height();
    if (viewport_height <= 0.0f) {
      const size_t count =
          std::min(m_item_heights.size(), std::max<size_t>(1u, m_overscan * 2u + 1u));
      return UIVirtualListRange{
          .start = 0u,
          .end = count - 1u,
          .valid = true,
      };
    }

    const float scroll_top = scroll_offset_y();
    const float scroll_bottom = scroll_top + viewport_height;
    const size_t first_visible = first_item_after(scroll_top);
    const size_t last_visible = last_item_before(scroll_bottom);

    return UIVirtualListRange{
        .start = first_visible > m_overscan ? first_visible - m_overscan : 0u,
        .end = std::min(m_item_heights.size() - 1u, last_visible + m_overscan),
        .valid = true,
    };
  }

  void update_slot_node(UINodeId node_id, float height, float width) {
    if (m_document == nullptr || node_id == k_invalid_node_id) {
      return;
    }

    const float resolved_height = std::max(0.0f, height);
    const UILength resolved_width =
        width > 0.0f ? UILength::pixels(width) : UILength::percent(1.0f);

    m_document->set_visible(node_id, true);

    const auto *node = m_document->node(node_id);
    if (node != nullptr &&
        node->style.flex_direction == FlexDirection::Column &&
        node->style.align_items == AlignItems::Stretch &&
        node->style.justify_content == JustifyContent::Start &&
        std::fabs(node->style.flex_shrink - 0.0f) < 0.001f &&
        node->style.height.unit == UILengthUnit::Pixels &&
        std::fabs(node->style.height.value - resolved_height) < 0.001f &&
        node->style.width.unit == resolved_width.unit &&
        std::fabs(node->style.width.value - resolved_width.value) < 0.001f) {
      return;
    }

    m_document->mutate_style(
        node_id, [resolved_height, resolved_width](UIStyle &style) {
          style.flex_direction = FlexDirection::Column;
          style.align_items = AlignItems::Stretch;
          style.justify_content = JustifyContent::Start;
          style.flex_shrink = 0.0f;
          style.height = UILength::pixels(resolved_height);
          style.width = resolved_width;
        }
    );
  }

  void update_spacer_node(UINodeId node_id, float height, bool visible, float width) {
    if (m_document == nullptr || node_id == k_invalid_node_id) {
      return;
    }

    const float resolved_height = std::max(0.0f, height);
    const UILength resolved_width =
        width > 0.0f ? UILength::pixels(width) : UILength::percent(1.0f);

    m_document->set_visible(node_id, visible);

    const auto *node = m_document->node(node_id);
    if (node != nullptr &&
        node->style.flex_direction == FlexDirection::Column &&
        std::fabs(node->style.flex_shrink - 0.0f) < 0.001f &&
        node->style.height.unit == UILengthUnit::Pixels &&
        std::fabs(node->style.height.value - resolved_height) < 0.001f &&
        node->style.width.unit == resolved_width.unit &&
        std::fabs(node->style.width.value - resolved_width.value) < 0.001f) {
      return;
    }

    m_document->mutate_style(
        node_id, [resolved_height, resolved_width](UIStyle &style) {
          style.flex_direction = FlexDirection::Column;
          style.flex_shrink = 0.0f;
          style.height = UILength::pixels(resolved_height);
          style.width = resolved_width;
        }
    );
  }

  void update_spacers(const UIVirtualListRange &range, float width) {
    if (m_item_heights.empty() || !range.valid) {
      update_spacer_node(m_top_spacer, 0.0f, false, width);
      update_spacer_node(m_bottom_spacer, 0.0f, false, width);
      return;
    }

    const float gap = vertical_gap();
    const float top_height =
        range.start > 0u
            ? m_prefix_heights[range.start] + gap * static_cast<float>(range.start - 1u)
            : 0.0f;
    const size_t trailing_count = m_item_heights.size() - range.end - 1u;
    const float trailing_height =
        trailing_count > 0u
            ? (m_prefix_heights.back() - m_prefix_heights[range.end + 1u]) +
                  gap * static_cast<float>(trailing_count - 1u)
            : 0.0f;

    update_spacer_node(m_top_spacer, top_height, range.start > 0u, width);
    update_spacer_node(m_bottom_spacer, trailing_height, trailing_count > 0u, width);
  }

  Ref<UIDocument> m_document = nullptr;
  UINodeId m_scroll_view = k_invalid_node_id;
  CreateSlotCallback m_create_slot;
  BindSlotCallback m_bind_slot;
  std::vector<float> m_item_heights;
  std::vector<float> m_prefix_heights;
  std::vector<Slot> m_slots;
  UINodeId m_top_spacer = k_invalid_node_id;
  UINodeId m_bottom_spacer = k_invalid_node_id;
  UIVirtualListRange m_current_range;
  size_t m_overscan = 3u;
  float m_content_width = 0.0f;
  bool m_prefix_dirty = true;
};

} // namespace astralix::ui
