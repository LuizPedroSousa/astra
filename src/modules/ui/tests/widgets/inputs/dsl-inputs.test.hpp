#include "document/document.hpp"
#include "dsl.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

TEST(UIFoundationsTest, DeclarativeDslAppliesInputAndLayoutRulesOnTopOfDefaults) {
  using namespace dsl;
  using namespace dsl::styles;

  auto document = UIDocument::create();
  UINodeId input_id = k_invalid_node_id;
  UINodeId scroll_id = k_invalid_node_id;
  UINodeId splitter_id = k_invalid_node_id;

  mount(
      *document,
      dsl::column().children(
          text_input("seed", "Placeholder")
              .bind(input_id)
              .style(
                  fill_x(),
                  focused(state().border(3.0f, rgba(0.9f, 0.8f, 0.7f, 1.0f)))
              ),
          scroll_view()
              .bind(scroll_id)
              .style(width(px(240.0f)), scroll_both()),
          splitter()
              .bind(splitter_id)
              .style(background(rgba(0.7f, 0.2f, 0.3f, 0.8f)))
      )
  );

  const auto *input_node = document->node(input_id);
  ASSERT_NE(input_node, nullptr);
  EXPECT_TRUE(input_node->focusable);
  EXPECT_EQ(input_node->style.width.unit, UILengthUnit::Percent);
  EXPECT_FLOAT_EQ(input_node->style.width.value, 1.0f);
  EXPECT_FLOAT_EQ(input_node->style.flex_shrink, 0.0f);
  EXPECT_FLOAT_EQ(input_node->style.border_radius, 10.0f);
  EXPECT_FALSE(input_node->style.cursor.has_value());
  ASSERT_TRUE(input_node->style.focused_style.border_width.has_value());
  EXPECT_FLOAT_EQ(*input_node->style.focused_style.border_width, 3.0f);

  const auto *scroll_node = document->node(scroll_id);
  ASSERT_NE(scroll_node, nullptr);
  EXPECT_EQ(scroll_node->style.overflow, Overflow::Hidden);
  EXPECT_EQ(scroll_node->style.width.unit, UILengthUnit::Pixels);
  EXPECT_FLOAT_EQ(scroll_node->style.width.value, 240.0f);
  EXPECT_EQ(scroll_node->style.scroll_mode, ScrollMode::Both);
  EXPECT_EQ(scroll_node->style.scrollbar_visibility, ScrollbarVisibility::Auto);

  const auto *splitter_node = document->node(splitter_id);
  ASSERT_NE(splitter_node, nullptr);
  EXPECT_EQ(splitter_node->style.align_self, AlignSelf::Stretch);
  EXPECT_EQ(
      splitter_node->style.background_color,
      rgba(0.7f, 0.2f, 0.3f, 0.8f)
  );
}

TEST(UIFoundationsTest, DeclarativeDslSupportsCheckboxSliderSelectAndCombobox) {
  using namespace dsl;
  using namespace dsl::styles;

  auto document = UIDocument::create();
  UINodeId checkbox_id = k_invalid_node_id;
  UINodeId slider_id = k_invalid_node_id;
  UINodeId select_id = k_invalid_node_id;
  UINodeId combobox_id = k_invalid_node_id;

  mount(
      *document,
      dsl::column().children(
          checkbox("Follow tail", true)
              .bind(checkbox_id)
              .style(
                  accent_color(rgba(0.4f, 0.7f, 0.9f, 1.0f)),
                  control_gap(12.0f),
                  control_indicator_size(20.0f)
              )
              .on_toggle([](bool) {}),
          slider(0.55f, 0.0f, 1.0f)
              .bind(slider_id)
              .step(0.25f)
              .style(
                  width(px(220.0f)),
                  accent_color(rgba(0.7f, 0.4f, 0.3f, 1.0f)),
                  slider_track_thickness(10.0f),
                  slider_thumb_radius(12.0f)
              )
              .on_value_change([](float) {}),
          select({"All", "Warn", "Error"}, 1u, "Severity")
              .bind(select_id)
              .style(
                  width(px(180.0f)),
                  accent_color(rgba(0.6f, 0.8f, 0.9f, 1.0f))
              )
              .on_select([](size_t, const std::string &) {}),
          combobox("help", "Command")
              .bind(combobox_id)
              .options({"help", "hello"})
              .style(
                  width(px(220.0f)),
                  accent_color(rgba(0.8f, 0.7f, 0.4f, 1.0f))
              )
              .on_change([](const std::string &) {})
              .on_select([](size_t, const std::string &) {})
      )
  );

  const auto *checkbox_node = document->node(checkbox_id);
  ASSERT_NE(checkbox_node, nullptr);
  EXPECT_TRUE(checkbox_node->checkbox.checked);
  EXPECT_EQ(checkbox_node->style.accent_color, rgba(0.4f, 0.7f, 0.9f, 1.0f));
  EXPECT_FLOAT_EQ(checkbox_node->style.control_gap, 12.0f);
  EXPECT_FLOAT_EQ(checkbox_node->style.control_indicator_size, 20.0f);
  EXPECT_TRUE(static_cast<bool>(checkbox_node->on_toggle));

  const auto *slider_node = document->node(slider_id);
  ASSERT_NE(slider_node, nullptr);
  EXPECT_FLOAT_EQ(slider_node->slider.value, 0.5f);
  EXPECT_FLOAT_EQ(slider_node->slider.step, 0.25f);
  EXPECT_EQ(slider_node->style.width.unit, UILengthUnit::Pixels);
  EXPECT_FLOAT_EQ(slider_node->style.width.value, 220.0f);
  EXPECT_FLOAT_EQ(slider_node->style.slider_track_thickness, 10.0f);
  EXPECT_FLOAT_EQ(slider_node->style.slider_thumb_radius, 12.0f);
  EXPECT_TRUE(static_cast<bool>(slider_node->on_value_change));

  const auto *select_node = document->node(select_id);
  ASSERT_NE(select_node, nullptr);
  EXPECT_EQ(select_node->select.options.size(), 3u);
  EXPECT_EQ(select_node->select.selected_index, 1u);
  EXPECT_EQ(select_node->placeholder, "Severity");
  EXPECT_EQ(select_node->style.width.unit, UILengthUnit::Pixels);
  EXPECT_FLOAT_EQ(select_node->style.width.value, 180.0f);
  EXPECT_EQ(select_node->style.accent_color, rgba(0.6f, 0.8f, 0.9f, 1.0f));
  EXPECT_TRUE(static_cast<bool>(select_node->on_select));

  const auto *combobox_node = document->node(combobox_id);
  ASSERT_NE(combobox_node, nullptr);
  EXPECT_EQ(combobox_node->type, NodeType::Combobox);
  EXPECT_EQ(combobox_node->text, "help");
  EXPECT_EQ(combobox_node->placeholder, "Command");
  ASSERT_EQ(combobox_node->combobox.options.size(), 2u);
  EXPECT_EQ(combobox_node->style.width.unit, UILengthUnit::Pixels);
  EXPECT_FLOAT_EQ(combobox_node->style.width.value, 220.0f);
  EXPECT_EQ(combobox_node->style.accent_color, rgba(0.8f, 0.7f, 0.4f, 1.0f));
  EXPECT_TRUE(static_cast<bool>(combobox_node->on_change));
  EXPECT_TRUE(static_cast<bool>(combobox_node->on_select));
}

} // namespace
} // namespace astralix::ui
