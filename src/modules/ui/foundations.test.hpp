#include "foundations.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, ResolvesStateStylesInPriorityOrder) {
  UIStyle style;
  style.background_color = glm::vec4(0.1f, 0.2f, 0.3f, 1.0f);
  style.text_color = glm::vec4(0.4f, 0.5f, 0.6f, 1.0f);
  style.focused_style.background_color = glm::vec4(0.2f, 0.3f, 0.4f, 1.0f);
  style.hovered_style.background_color = glm::vec4(0.3f, 0.4f, 0.5f, 1.0f);
  style.pressed_style.background_color = glm::vec4(0.4f, 0.5f, 0.6f, 1.0f);
  style.disabled_style.background_color = glm::vec4(0.8f, 0.7f, 0.6f, 0.9f);
  style.disabled_style.opacity = 0.35f;

  const UIResolvedStyle focused =
      resolve_style(style, UIPaintState{.focused = true}, true);
  EXPECT_EQ(focused.background_color, glm::vec4(0.2f, 0.3f, 0.4f, 1.0f));

  const UIResolvedStyle hovered = resolve_style(
      style, UIPaintState{.hovered = true, .focused = true}, true
  );
  EXPECT_EQ(hovered.background_color, glm::vec4(0.3f, 0.4f, 0.5f, 1.0f));

  const UIResolvedStyle pressed = resolve_style(
      style, UIPaintState{.hovered = true, .pressed = true, .focused = true}, true
  );
  EXPECT_EQ(pressed.background_color, glm::vec4(0.4f, 0.5f, 0.6f, 1.0f));

  const UIResolvedStyle disabled = resolve_style(
      style, UIPaintState{.hovered = true, .pressed = true, .focused = true}, false
  );
  EXPECT_EQ(disabled.background_color, glm::vec4(0.8f, 0.7f, 0.6f, 0.9f));
  EXPECT_FLOAT_EQ(disabled.opacity, 0.35f);
}

TEST(UIFoundationsTest, ClampsScrollOffsetsPerAxis) {
  EXPECT_EQ(
      clamp_scroll_offset(
          glm::vec2(120.0f, -8.0f), glm::vec2(64.0f, 24.0f), ScrollMode::Horizontal
      ),
      glm::vec2(64.0f, 0.0f)
  );
  EXPECT_EQ(
      clamp_scroll_offset(
          glm::vec2(-5.0f, 200.0f), glm::vec2(64.0f, 48.0f), ScrollMode::Vertical
      ),
      glm::vec2(0.0f, 48.0f)
  );
  EXPECT_EQ(
      clamp_scroll_offset(
          glm::vec2(50.0f, 60.0f), glm::vec2(10.0f, 20.0f), ScrollMode::None
      ),
      glm::vec2(0.0f)
  );
}

TEST(UIFoundationsTest, MeasuresTextPrefixesAndNearestIndices) {
  const auto advance = [](char) { return 10.0f; };

  EXPECT_FLOAT_EQ(measure_text_prefix_advance("hello", 0u, advance), 0.0f);
  EXPECT_FLOAT_EQ(measure_text_prefix_advance("hello", 3u, advance), 30.0f);
  EXPECT_FLOAT_EQ(measure_text_prefix_advance("hello", 99u, advance), 50.0f);

  EXPECT_EQ(nearest_text_index("hello", -4.0f, advance), 0u);
  EXPECT_EQ(nearest_text_index("hello", 14.0f, advance), 1u);
  EXPECT_EQ(nearest_text_index("hello", 16.0f, advance), 2u);
  EXPECT_EQ(nearest_text_index("hello", 999.0f, advance), 5u);
}

TEST(UIFoundationsTest, SanitizesAsciiTextAndSupportedCodepoints) {
  std::string raw = "A\tB";
  raw.push_back(static_cast<char>(0xC8));
  raw.push_back('!');

  EXPECT_TRUE(is_supported_text_codepoint('A'));
  EXPECT_FALSE(is_supported_text_codepoint('\n'));
  EXPECT_FALSE(is_supported_text_codepoint(0x80u));
  EXPECT_EQ(sanitize_ascii_text(raw), "AB!");
}

TEST(UIFoundationsTest, ScrollbarHitPartHelpersIdentifyParts) {
  EXPECT_TRUE(is_scrollbar_thumb_part(UIHitPart::VerticalScrollbarThumb));
  EXPECT_TRUE(is_scrollbar_thumb_part(UIHitPart::HorizontalScrollbarThumb));
  EXPECT_TRUE(is_scrollbar_track_part(UIHitPart::VerticalScrollbarTrack));
  EXPECT_TRUE(is_scrollbar_track_part(UIHitPart::HorizontalScrollbarTrack));
  EXPECT_TRUE(is_scrollbar_part(UIHitPart::VerticalScrollbarThumb));
  EXPECT_TRUE(is_scrollbar_part(UIHitPart::HorizontalScrollbarTrack));
  EXPECT_FALSE(is_scrollbar_part(UIHitPart::Body));
  EXPECT_FALSE(is_scrollbar_part(UIHitPart::TextInputText));
  EXPECT_TRUE(is_panel_resize_part(UIHitPart::ResizeLeft));
  EXPECT_TRUE(is_panel_resize_part(UIHitPart::ResizeBottomRight));
  EXPECT_TRUE(is_corner_resize_part(UIHitPart::ResizeTopLeft));
  EXPECT_TRUE(is_splitter_part(UIHitPart::SplitterBar));
  EXPECT_TRUE(is_resize_part(UIHitPart::SplitterBar));
  EXPECT_TRUE(is_resize_part(UIHitPart::ResizeTop));
  EXPECT_TRUE(is_slider_part(UIHitPart::SliderTrack));
  EXPECT_TRUE(is_slider_part(UIHitPart::SliderThumb));
  EXPECT_TRUE(is_select_part(UIHitPart::SelectField));
  EXPECT_TRUE(is_select_part(UIHitPart::SelectOption));
  EXPECT_TRUE(is_combobox_part(UIHitPart::ComboboxField));
  EXPECT_TRUE(is_combobox_part(UIHitPart::ComboboxOption));
  EXPECT_FALSE(is_resize_part(UIHitPart::Body));
  EXPECT_FALSE(is_slider_part(UIHitPart::Body));
  EXPECT_FALSE(is_select_part(UIHitPart::Body));
  EXPECT_FALSE(is_combobox_part(UIHitPart::Body));
}

TEST(UIFoundationsTest, DistributedJustificationKeepsGapAsMinimumGutter) {
  const UIFlowSpacing exact_fit = resolve_flow_spacing(
      JustifyContent::SpaceBetween, 10.0f, 0.0f, 2u
  );
  EXPECT_FLOAT_EQ(exact_fit.leading, 0.0f);
  EXPECT_FLOAT_EQ(exact_fit.between, 10.0f);

  const UIFlowSpacing extra_space = resolve_flow_spacing(
      JustifyContent::SpaceBetween, 10.0f, 40.0f, 2u
  );
  EXPECT_FLOAT_EQ(extra_space.leading, 0.0f);
  EXPECT_FLOAT_EQ(extra_space.between, 50.0f);
}

TEST(UIFoundationsTest, DetachedNodesDoNotAllowInteraction) {
  auto document = UIDocument::create();
  const UINodeId root = document->create_view();
  const UINodeId child = document->create_text_input("value", {});

  document->append_child(root, child);
  document->set_root(root);

  EXPECT_TRUE(node_chain_allows_interaction(*document, child));

  document->remove_child(child);

  EXPECT_FALSE(node_chain_allows_interaction(*document, child));
}

} // namespace
} // namespace astralix::ui
