#include "clipboard.hpp"
#include "document.hpp"
#include "dsl.hpp"
#include "foundations.hpp"

#include <gtest/gtest.h>
#include <limits>

namespace astralix::ui {
    namespace {

        class MemoryClipboardBackend final : public ClipboardBackend {
        public:
            std::string get(GLFWwindow*) const override { return text; }

            void set(GLFWwindow*, const std::string& value) const override {
                text = value;
            }

            mutable std::string text;
        };

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
                resolve_style(style, UIPaintState{ .focused = true }, true);
            EXPECT_EQ(focused.background_color, glm::vec4(0.2f, 0.3f, 0.4f, 1.0f));

            const UIResolvedStyle hovered = resolve_style(
                style, UIPaintState{ .hovered = true, .focused = true }, true
            );
            EXPECT_EQ(hovered.background_color, glm::vec4(0.3f, 0.4f, 0.5f, 1.0f));

            const UIResolvedStyle pressed = resolve_style(
                style, UIPaintState{ .hovered = true, .pressed = true, .focused = true },
                true
            );
            EXPECT_EQ(pressed.background_color, glm::vec4(0.4f, 0.5f, 0.6f, 1.0f));

            const UIResolvedStyle disabled = resolve_style(
                style, UIPaintState{ .hovered = true, .pressed = true, .focused = true },
                false
            );
            EXPECT_EQ(disabled.background_color, glm::vec4(0.8f, 0.7f, 0.6f, 0.9f));
            EXPECT_FLOAT_EQ(disabled.opacity, 0.35f);
        }

        TEST(UIFoundationsTest, ClampsScrollOffsetsPerAxis) {
            EXPECT_EQ(
                clamp_scroll_offset(
                    glm::vec2(120.0f, -8.0f), glm::vec2(64.0f, 24.0f),
                    ScrollMode::Horizontal
                ),
                glm::vec2(64.0f, 0.0f)
            );
            EXPECT_EQ(
                clamp_scroll_offset(
                    glm::vec2(-5.0f, 200.0f), glm::vec2(64.0f, 48.0f),
                    ScrollMode::Vertical
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

        TEST(UIFoundationsTest, RemLengthsPreserveUnitAndDocumentRootFontSize) {
            using namespace dsl;
            using namespace dsl::styles;

            auto document = UIDocument::create();
            UINodeId panel = k_invalid_node_id;

            mount(
                *document,
                column("root").style(fill()).children(
                    view("panel").bind(panel).style(width(rem(2.0f)), height(rem(1.5f)))
                )
            );

            document->set_root_font_size(20.0f);

            const auto* panel_node = document->node(panel);
            ASSERT_NE(panel_node, nullptr);
            EXPECT_EQ(panel_node->style.width.unit, UILengthUnit::Rem);
            EXPECT_EQ(panel_node->style.height.unit, UILengthUnit::Rem);
            EXPECT_FLOAT_EQ(panel_node->style.width.value, 2.0f);
            EXPECT_FLOAT_EQ(panel_node->style.height.value, 1.5f);
            EXPECT_FLOAT_EQ(document->root_font_size(), 20.0f);
        }

        TEST(UIFoundationsTest, FreeStyleHelpersCanChainFromAnyEntryPoint) {
            using namespace dsl;
            using namespace dsl::styles;

            auto document = UIDocument::create();
            UINodeId panel = k_invalid_node_id;

            mount(
                *document,
                column("root").children(
                    view("panel")
                    .bind(panel)
                    .style(fill_x()
                        .padding(14.0f)
                        .gap(12.0f)
                        .height(px(40.0f))
                        .radius(6.0f))
                )
            );

            const auto* panel_node = document->node(panel);
            ASSERT_NE(panel_node, nullptr);
            EXPECT_EQ(panel_node->style.width.unit, UILengthUnit::Percent);
            EXPECT_FLOAT_EQ(panel_node->style.width.value, 1.0f);
            EXPECT_EQ(panel_node->style.height.unit, UILengthUnit::Pixels);
            EXPECT_FLOAT_EQ(panel_node->style.height.value, 40.0f);
            EXPECT_FLOAT_EQ(panel_node->style.gap, 12.0f);
            EXPECT_FLOAT_EQ(panel_node->style.padding.left, 14.0f);
            EXPECT_FLOAT_EQ(panel_node->style.padding.right, 14.0f);
            EXPECT_FLOAT_EQ(panel_node->style.padding.top, 14.0f);
            EXPECT_FLOAT_EQ(panel_node->style.padding.bottom, 14.0f);
            EXPECT_FLOAT_EQ(panel_node->style.border_radius, 6.0f);
            EXPECT_FLOAT_EQ(panel_node->style.flex_shrink, 0.0f);
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

        TEST(UIFoundationsTest, CollectsFocusableOrderAndNearestAncestors) {
            auto document = UIDocument::create();
            const UINodeId root = document->create_view("root");
            const UINodeId button = document->create_pressable("button");
            const UINodeId button_label = document->create_text("press", "label");
            const UINodeId custom_focusable = document->create_view("custom");
            const UINodeId hidden_button = document->create_pressable("hidden");
            const UINodeId disabled_button = document->create_pressable("disabled");
            const UINodeId scroll_container = document->create_view("scroll");
            const UINodeId leaf = document->create_text("leaf", "leaf");

            document->set_root(root);
            document->append_child(root, button);
            document->append_child(button, button_label);
            document->append_child(root, custom_focusable);
            document->append_child(root, hidden_button);
            document->append_child(root, disabled_button);
            document->append_child(custom_focusable, scroll_container);
            document->append_child(scroll_container, leaf);

            document->set_focusable(custom_focusable, true);
            document->set_visible(hidden_button, false);
            document->set_enabled(disabled_button, false);
            document->mutate_style(scroll_container, [](UIStyle& style) {
                style.scroll_mode = ScrollMode::Vertical;
                });

            const std::vector<UINodeId> order = collect_focusable_order(*document);
            ASSERT_EQ(order.size(), 2u);
            EXPECT_EQ(order[0], button);
            EXPECT_EQ(order[1], custom_focusable);

            EXPECT_EQ(find_nearest_focusable_ancestor(*document, button_label), button);
            EXPECT_EQ(find_nearest_pressable_ancestor(*document, button_label), button);
            EXPECT_EQ(
                find_nearest_scrollable_ancestor(*document, leaf), scroll_container
            );
            EXPECT_EQ(find_nearest_focusable_ancestor(*document, leaf), custom_focusable);
            EXPECT_FALSE(find_nearest_focusable_ancestor(*document, root).has_value());
        }

        TEST(UIFoundationsTest, DocumentStateMutationsUpdateFocusCaretAndScrollState) {
            auto document = UIDocument::create();
            const UINodeId root = document->create_view("root");
            const UINodeId node =
                document->create_text_input("stateful", "placeholder", "node");

            document->set_root(root);
            document->append_child(root, node);
            document->clear_dirty();

            document->set_hot_node(node);
            document->set_active_node(node);
            document->set_focused_node(node);
            document->set_text_selection(
                node, UITextSelection{ .anchor = 4u, .focus = 1u }
            );
            document->set_caret(node, 3u, true);
            document->set_scroll_offset(node, glm::vec2(12.0f, 24.0f));

            const auto* ui_node = document->node(node);
            ASSERT_NE(ui_node, nullptr);
            EXPECT_TRUE(ui_node->paint_state.hovered);
            EXPECT_TRUE(ui_node->paint_state.pressed);
            EXPECT_TRUE(ui_node->paint_state.focused);
            EXPECT_EQ(ui_node->selection.start(), 1u);
            EXPECT_EQ(ui_node->selection.end(), 4u);
            EXPECT_TRUE(ui_node->caret.active);
            EXPECT_TRUE(ui_node->caret.visible);
            EXPECT_EQ(ui_node->caret.index, 3u);
            ASSERT_NE(document->scroll_state(node), nullptr);
            EXPECT_EQ(document->scroll_state(node)->offset, glm::vec2(12.0f, 24.0f));
            EXPECT_TRUE(document->layout_dirty());

            document->set_enabled(node, false);
            EXPECT_EQ(document->hot_node(), k_invalid_node_id);
            EXPECT_EQ(document->active_node(), k_invalid_node_id);
            EXPECT_EQ(document->focused_node(), k_invalid_node_id);
            EXPECT_FALSE(ui_node->paint_state.hovered);
            EXPECT_FALSE(ui_node->paint_state.pressed);
            EXPECT_FALSE(ui_node->paint_state.focused);
        }

        TEST(UIFoundationsTest, FocusabilityAndButtonsExposeExpectedDefaults) {
            auto document = UIDocument::create();
            const UINodeId button =
                document->create_button("Spawn", [] {}, "spawn_button");

            const auto* button_node = document->node(button);
            ASSERT_NE(button_node, nullptr);
            EXPECT_TRUE(button_node->focusable);
            EXPECT_TRUE(button_node->style.hovered_style.background_color.has_value());
            EXPECT_TRUE(button_node->style.pressed_style.background_color.has_value());
            EXPECT_TRUE(button_node->style.focused_style.border_color.has_value());
            EXPECT_TRUE(button_node->style.disabled_style.opacity.has_value());

            document->set_focused_node(button);
            document->set_focusable(button, false);
            EXPECT_EQ(document->focused_node(), k_invalid_node_id);
        }

        TEST(UIFoundationsTest, SegmentedControlChipGroupAndIconButtonExposeDefaults) {
            auto document = UIDocument::create();
            const UINodeId segmented = document->create_segmented_control(
                { "All", "Info", "Warn" }, 5u, "segmented");
            const UINodeId chips = document->create_chip_group(
                { "Logs", "Commands", "Output" }, {}, "chips");
            const UINodeId icon_button =
                document->create_icon_button("icons::clear", [] {}, "icon_button");

            const auto* segmented_node = document->node(segmented);
            const auto* chip_node = document->node(chips);
            const auto* icon_button_node = document->node(icon_button);
            ASSERT_NE(segmented_node, nullptr);
            ASSERT_NE(chip_node, nullptr);
            ASSERT_NE(icon_button_node, nullptr);

            EXPECT_EQ(segmented_node->type, NodeType::SegmentedControl);
            EXPECT_TRUE(segmented_node->focusable);
            ASSERT_EQ(segmented_node->segmented_control.options.size(), 3u);
            EXPECT_EQ(segmented_node->segmented_control.selected_index, 2u);

            EXPECT_EQ(chip_node->type, NodeType::ChipGroup);
            ASSERT_EQ(chip_node->chip_group.options.size(), 3u);
            ASSERT_EQ(chip_node->chip_group.selected.size(), 3u);
            EXPECT_TRUE(chip_node->chip_group.selected[0]);
            EXPECT_TRUE(chip_node->chip_group.selected[1]);
            EXPECT_TRUE(chip_node->chip_group.selected[2]);

            EXPECT_EQ(icon_button_node->type, NodeType::Pressable);
            EXPECT_TRUE(icon_button_node->focusable);
            ASSERT_EQ(icon_button_node->children.size(), 1u);
            const auto* icon_node = document->node(icon_button_node->children.front());
            ASSERT_NE(icon_node, nullptr);
            EXPECT_EQ(icon_node->type, NodeType::Image);
            EXPECT_EQ(icon_node->texture_id, "icons::clear");
        }

        TEST(UIFoundationsTest,
            SegmentedControlAndChipGroupDocumentStateCanBeUpdated) {
            auto document = UIDocument::create();
            const UINodeId segmented = document->create_segmented_control(
                { "All", "Info", "Warn" }, 1u, "segmented");
            const UINodeId chips = document->create_chip_group(
                { "Logs", "Commands", "Output" }, { true, false, true }, "chips");

            document->set_segmented_selected_index(segmented, 2u);
            document->set_chip_selected(chips, 1u, true);
            document->set_chip_selected(chips, 2u, false);

            EXPECT_EQ(document->segmented_selected_index(segmented), 2u);
            EXPECT_TRUE(document->chip_selected(chips, 0u));
            EXPECT_TRUE(document->chip_selected(chips, 1u));
            EXPECT_FALSE(document->chip_selected(chips, 2u));
        }

        TEST(UIFoundationsTest, DocumentCanQueueAndConsumeFocusRequests) {
            auto document = UIDocument::create();
            const UINodeId root = document->create_view("root");
            const UINodeId input =
                document->create_text_input({}, "Placeholder", "input");

            document->set_root(root);
            document->append_child(root, input);

            document->request_focus(input);
            EXPECT_EQ(document->requested_focus(), input);
            EXPECT_EQ(document->consume_requested_focus(), input);
            EXPECT_EQ(document->requested_focus(), k_invalid_node_id);
        }

        TEST(UIFoundationsTest, DocumentCanSuppressNextCharacterInput) {
            auto document = UIDocument::create();

            document->suppress_next_character_input(static_cast<uint32_t>('`'));

            EXPECT_FALSE(
                document->consume_suppressed_character_input(static_cast<uint32_t>('a'))
            );
            EXPECT_TRUE(
                document->consume_suppressed_character_input(static_cast<uint32_t>('`'))
            );
            EXPECT_FALSE(
                document->consume_suppressed_character_input(static_cast<uint32_t>('`'))
            );
        }

        TEST(UIFoundationsTest, TextInputAndScrollViewExposeExpectedDefaults) {
            auto document = UIDocument::create();
            const UINodeId text_input =
                document->create_text_input("seed", "Placeholder", "text_input");
            const UINodeId scroll_view = document->create_scroll_view("scroll_view");
            const UINodeId splitter = document->create_splitter("splitter");

            document->set_placeholder(text_input, "Updated placeholder");
            document->set_read_only(text_input, true);
            document->set_select_all_on_focus(text_input, true);

            int on_change_calls = 0;
            int on_submit_calls = 0;
            document->set_on_change(text_input, [&](const std::string&) {
                ++on_change_calls;
                });
            document->set_on_submit(text_input, [&](const std::string&) {
                ++on_submit_calls;
                });

            const auto* text_input_node = document->node(text_input);
            ASSERT_NE(text_input_node, nullptr);
            EXPECT_EQ(text_input_node->type, NodeType::TextInput);
            EXPECT_TRUE(text_input_node->focusable);
            EXPECT_TRUE(text_input_node->read_only);
            EXPECT_TRUE(text_input_node->select_all_on_focus);
            EXPECT_EQ(text_input_node->placeholder, "Updated placeholder");
            EXPECT_EQ(text_input_node->style.overflow, Overflow::Hidden);
            EXPECT_FLOAT_EQ(text_input_node->style.flex_shrink, 0.0f);
            EXPECT_FALSE(text_input_node->style.focused_style.border_color.has_value());
            EXPECT_FALSE(text_input_node->style.focused_style.border_width.has_value());
            EXPECT_FALSE(text_input_node->style.hovered_style.border_color.has_value());
            EXPECT_TRUE(text_input_node->style.disabled_style.opacity.has_value());

            const auto* scroll_view_node = document->node(scroll_view);
            ASSERT_NE(scroll_view_node, nullptr);
            EXPECT_EQ(scroll_view_node->type, NodeType::ScrollView);
            EXPECT_FALSE(scroll_view_node->focusable);
            EXPECT_EQ(scroll_view_node->style.overflow, Overflow::Hidden);
            EXPECT_EQ(scroll_view_node->style.scroll_mode, ScrollMode::Vertical);
            EXPECT_EQ(
                scroll_view_node->style.scrollbar_visibility, ScrollbarVisibility::Auto
            );

            const auto* splitter_node = document->node(splitter);
            ASSERT_NE(splitter_node, nullptr);
            EXPECT_EQ(splitter_node->type, NodeType::Splitter);
            EXPECT_FLOAT_EQ(splitter_node->style.flex_shrink, 0.0f);
            EXPECT_EQ(splitter_node->style.align_self, AlignSelf::Stretch);

            document->set_text(text_input, "changed by code");
            document->flush_callbacks();
            EXPECT_EQ(on_change_calls, 0);
            EXPECT_EQ(on_submit_calls, 0);
        }

        TEST(UIFoundationsTest, CheckboxSliderAndSelectExposeExpectedDefaults) {
            auto document = UIDocument::create();
            const UINodeId checkbox =
                document->create_checkbox("Follow tail", true, "checkbox");
            const UINodeId slider =
                document->create_slider(0.24f, 0.0f, 1.0f, 0.2f, "slider");
            const UINodeId select = document->create_select(
                { "All", "Info", "Warn" }, 7u, "Severity", "select"
            );
            const UINodeId second_select =
                document->create_select({ "A", "B" }, 0u, "Other", "second_select");

            bool toggled = false;
            float changed_value = 0.0f;
            size_t selected = 0u;
            std::string selected_label;
            document->set_on_toggle(checkbox, [&](bool value) { toggled = value; });
            document->set_on_value_change(slider, [&](float value) {
                changed_value = value;
                });
            document->set_on_select(select, [&](size_t index, const std::string& label) {
                selected = index;
                selected_label = label;
                });

            const auto* checkbox_node = document->node(checkbox);
            ASSERT_NE(checkbox_node, nullptr);
            EXPECT_EQ(checkbox_node->type, NodeType::Checkbox);
            EXPECT_TRUE(checkbox_node->focusable);
            EXPECT_TRUE(checkbox_node->checkbox.checked);
            EXPECT_TRUE(checkbox_node->style.hovered_style.background_color.has_value());
            EXPECT_TRUE(checkbox_node->style.focused_style.border_color.has_value());
            EXPECT_TRUE(checkbox_node->style.disabled_style.opacity.has_value());
            EXPECT_TRUE(static_cast<bool>(checkbox_node->on_toggle));

            const auto* slider_node = document->node(slider);
            ASSERT_NE(slider_node, nullptr);
            EXPECT_EQ(slider_node->type, NodeType::Slider);
            EXPECT_TRUE(slider_node->focusable);
            EXPECT_FLOAT_EQ(slider_node->slider.value, 0.2f);
            EXPECT_FLOAT_EQ(slider_node->slider.step, 0.2f);
            EXPECT_TRUE(slider_node->style.focused_style.border_color.has_value());
            EXPECT_TRUE(slider_node->style.disabled_style.opacity.has_value());
            EXPECT_TRUE(static_cast<bool>(slider_node->on_value_change));

            const auto* select_node = document->node(select);
            ASSERT_NE(select_node, nullptr);
            EXPECT_EQ(select_node->type, NodeType::Select);
            EXPECT_TRUE(select_node->focusable);
            EXPECT_EQ(select_node->select.selected_index, 2u);
            EXPECT_EQ(select_node->select.highlighted_index, 2u);
            EXPECT_EQ(select_node->placeholder, "Severity");
            EXPECT_TRUE(select_node->style.focused_style.border_color.has_value());
            EXPECT_TRUE(select_node->style.disabled_style.opacity.has_value());
            EXPECT_TRUE(static_cast<bool>(select_node->on_select));

            document->set_checked(checkbox, false);
            EXPECT_FALSE(document->checked(checkbox));

            document->set_slider_range(slider, 0.0f, 10.0f, 2.0f);
            document->set_slider_value(slider, 7.1f);
            EXPECT_FLOAT_EQ(document->slider_value(slider), 8.0f);

            document->set_selected_index(select, 1u);
            EXPECT_EQ(document->selected_index(select), 1u);

            document->set_select_open(select, true);
            EXPECT_TRUE(document->select_open(select));
            EXPECT_EQ(document->open_select_node(), select);

            document->set_select_open(second_select, true);
            EXPECT_FALSE(document->select_open(select));
            EXPECT_TRUE(document->select_open(second_select));
            EXPECT_EQ(document->open_select_node(), second_select);

            document->set_select_options(select, {});
            EXPECT_FALSE(document->select_open(select));
            const auto* options = document->select_options(select);
            ASSERT_NE(options, nullptr);
            EXPECT_TRUE(options->empty());

            EXPECT_FALSE(toggled);
            EXPECT_FLOAT_EQ(changed_value, 0.0f);
            EXPECT_EQ(selected, 0u);
            EXPECT_TRUE(selected_label.empty());
        }

        TEST(UIFoundationsTest, TextInputMutationsClampCaretSelectionAndScrollHelpers) {
            auto document = UIDocument::create();
            const UINodeId text_input =
                document->create_text_input("hello", "Placeholder", "text_input");

            document->set_text_selection(
                text_input, UITextSelection{ .anchor = 4u, .focus = 5u }
            );
            document->set_caret(text_input, 5u, true);

            auto* node = document->node(text_input);
            ASSERT_NE(node, nullptr);
            node->text_scroll_x = -18.0f;

            document->set_text(text_input, "hi");

            node = document->node(text_input);
            ASSERT_NE(node, nullptr);
            EXPECT_EQ(node->selection.start(), 2u);
            EXPECT_EQ(node->selection.end(), 2u);
            EXPECT_EQ(node->caret.index, 2u);
            EXPECT_FLOAT_EQ(node->text_scroll_x, 0.0f);

            EXPECT_FLOAT_EQ(clamp_text_scroll_x(80.0f, 120.0f, 48.0f), 72.0f);
            EXPECT_FLOAT_EQ(clamp_text_scroll_x(-5.0f, 120.0f, 48.0f), 0.0f);
            EXPECT_FLOAT_EQ(
                scroll_x_to_keep_range_visible(0.0f, 52.0f, 60.0f, 120.0f, 32.0f), 28.0f
            );
            EXPECT_FLOAT_EQ(
                scroll_x_to_keep_range_visible(24.0f, 8.0f, 16.0f, 120.0f, 32.0f), 8.0f
            );
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
            EXPECT_FALSE(is_resize_part(UIHitPart::Body));
            EXPECT_FALSE(is_slider_part(UIHitPart::Body));
            EXPECT_FALSE(is_select_part(UIHitPart::Body));
        }

        TEST(UIFoundationsTest, ResizableViewsAndSplittersExposeExpectedHelpers) {
            auto document = UIDocument::create();
            const UINodeId panel = document->create_view("panel");
            const UINodeId header = document->create_view("header");
            const UINodeId splitter = document->create_splitter("splitter");
            document->mutate_style(panel, [](UIStyle& style) {
                style.position_type = PositionType::Absolute;
                style.draggable = true;
                style.resize_mode = ResizeMode::Both;
                style.resize_edges = k_resize_edge_all;
                style.resize_handle_thickness = 8.0f;
                style.resize_corner_extent = 18.0f;
                });
            document->mutate_style(header, [](UIStyle& style) { style.drag_handle = true; });

            const auto* panel_node = document->node(panel);
            const auto* header_node = document->node(header);
            const auto* splitter_node = document->node(splitter);
            ASSERT_NE(panel_node, nullptr);
            ASSERT_NE(header_node, nullptr);
            ASSERT_NE(splitter_node, nullptr);
            EXPECT_TRUE(node_supports_panel_drag(*panel_node));
            EXPECT_TRUE(node_supports_panel_resize(*panel_node));
            EXPECT_TRUE(node_is_drag_handle(*header_node));
            EXPECT_FALSE(node_supports_panel_resize(*splitter_node));
            EXPECT_TRUE(resize_allows_horizontal(panel_node->style.resize_mode));
            EXPECT_TRUE(resize_allows_vertical(panel_node->style.resize_mode));
            EXPECT_TRUE(
                has_resize_edge(panel_node->style.resize_edges, k_resize_edge_left)
            );
            EXPECT_TRUE(
                has_resize_edge(panel_node->style.resize_edges, k_resize_edge_bottom)
            );
        }

        TEST(UIFoundationsTest, ClampsResizablePanelBoundsToParentContent) {
            const UIRect parent_bounds{
                .x = 32.0f, .y = 24.0f, .width = 640.0f, .height = 360.0f };
            const UIRect start_bounds{
                .x = 160.0f, .y = 96.0f, .width = 240.0f, .height = 180.0f };

            const UIRect top_drag = clamp_panel_resize_bounds(
                start_bounds,
                UIRect{
                    .x = start_bounds.x,
                    .y = -48.0f,
                    .width = start_bounds.width,
                    .height = 324.0f,
                },
                parent_bounds, UIHitPart::ResizeTop, 120.0f,
                std::numeric_limits<float>::infinity(), 80.0f,
                std::numeric_limits<float>::infinity()
                );
            EXPECT_FLOAT_EQ(top_drag.y, parent_bounds.y);
            EXPECT_FLOAT_EQ(top_drag.height, start_bounds.bottom() - parent_bounds.y);

            const UIRect bottom_right_drag = clamp_panel_resize_bounds(
                start_bounds,
                UIRect{
                    .x = start_bounds.x,
                    .y = start_bounds.y,
                    .width = 640.0f,
                    .height = 420.0f,
                },
                parent_bounds, UIHitPart::ResizeBottomRight, 120.0f,
                std::numeric_limits<float>::infinity(), 80.0f,
                std::numeric_limits<float>::infinity()
                );
            EXPECT_FLOAT_EQ(bottom_right_drag.x, start_bounds.x);
            EXPECT_FLOAT_EQ(bottom_right_drag.y, start_bounds.y);
            EXPECT_FLOAT_EQ(bottom_right_drag.width,
                parent_bounds.right() - start_bounds.x);
            EXPECT_FLOAT_EQ(bottom_right_drag.height,
                parent_bounds.bottom() - start_bounds.y);

            const UIRect left_drag = clamp_panel_resize_bounds(
                start_bounds,
                UIRect{
                    .x = 0.0f,
                    .y = start_bounds.y,
                    .width = 400.0f,
                    .height = start_bounds.height,
                },
                parent_bounds, UIHitPart::ResizeLeft, 120.0f,
                std::numeric_limits<float>::infinity(), 80.0f,
                std::numeric_limits<float>::infinity()
                );
            EXPECT_FLOAT_EQ(left_drag.x, parent_bounds.x);
            EXPECT_FLOAT_EQ(left_drag.width, start_bounds.right() - parent_bounds.x);
        }

        TEST(UIFoundationsTest, ClampsResolvedAbsolutePanelRectsAfterViewportShrink) {
            const UIRect viewport{ .x = 0.0f, .y = 0.0f, .width = 400.0f, .height = 300.0f };

            const UIRect shifted = clamp_rect_to_bounds(
                UIRect{
                    .x = 220.0f,
                    .y = 120.0f,
                    .width = 320.0f,
                    .height = 240.0f,
                },
                viewport
                );
            EXPECT_FLOAT_EQ(shifted.x, 80.0f);
            EXPECT_FLOAT_EQ(shifted.y, 60.0f);
            EXPECT_FLOAT_EQ(shifted.width, 320.0f);
            EXPECT_FLOAT_EQ(shifted.height, 240.0f);

            const UIRect shrunk = clamp_rect_to_bounds(
                UIRect{
                    .x = 140.0f,
                    .y = 20.0f,
                    .width = 540.0f,
                    .height = 340.0f,
                },
                viewport
                );
            EXPECT_FLOAT_EQ(shrunk.x, 0.0f);
            EXPECT_FLOAT_EQ(shrunk.y, 0.0f);
            EXPECT_FLOAT_EQ(shrunk.width, viewport.width);
            EXPECT_FLOAT_EQ(shrunk.height, viewport.height);
        }

        TEST(UIFoundationsTest, ClampsMovedPanelsToParentBounds) {
            const UIRect parent_bounds{
                .x = 32.0f, .y = 24.0f, .width = 640.0f, .height = 360.0f };

            const UIRect moved_left_top = clamp_rect_to_bounds(
                UIRect{ .x = -48.0f, .y = -12.0f, .width = 240.0f, .height = 180.0f },
                parent_bounds
            );
            EXPECT_FLOAT_EQ(moved_left_top.x, parent_bounds.x);
            EXPECT_FLOAT_EQ(moved_left_top.y, parent_bounds.y);
            EXPECT_FLOAT_EQ(moved_left_top.width, 240.0f);
            EXPECT_FLOAT_EQ(moved_left_top.height, 180.0f);

            const UIRect moved_right_bottom = clamp_rect_to_bounds(
                UIRect{ .x = 580.0f, .y = 260.0f, .width = 240.0f, .height = 180.0f },
                parent_bounds
            );
            EXPECT_FLOAT_EQ(
                moved_right_bottom.x, parent_bounds.right() - moved_right_bottom.width
            );
            EXPECT_FLOAT_EQ(
                moved_right_bottom.y, parent_bounds.bottom() - moved_right_bottom.height
            );
            EXPECT_FLOAT_EQ(moved_right_bottom.width, 240.0f);
            EXPECT_FLOAT_EQ(moved_right_bottom.height, 180.0f);
        }

        TEST(UIFoundationsTest, DeclarativeDslMountsAndAppendsTrees) {
            using namespace dsl;
            using namespace dsl::styles;

            auto document = UIDocument::create();
            UINodeId root = k_invalid_node_id;
            UINodeId title = k_invalid_node_id;
            UINodeId actions = k_invalid_node_id;
            UINodeId run_button = k_invalid_node_id;
            UINodeId help = k_invalid_node_id;

            const UINodeId mounted_root = mount(
                *document, column("root")
                .bind(root)
                .style(fill(), padding(8.0f), gap(6.0f))
                .children(
                    text("HUD", "title").bind(title),
                    row("actions").bind(actions).style(gap(4.0f)).children(
                        button("Run", [] {}, "run_button").bind(run_button)
                    )
                )
            );

            EXPECT_EQ(mounted_root, root);
            EXPECT_EQ(document->root(), root);

            const auto* root_node = document->node(root);
            ASSERT_NE(root_node, nullptr);
            ASSERT_EQ(root_node->children.size(), 2u);
            EXPECT_EQ(root_node->children[0], title);
            EXPECT_EQ(root_node->children[1], actions);

            const UINodeId appended =
                append(*document, actions, text("Help", "help").bind(help));
            EXPECT_EQ(appended, help);

            const auto* actions_node = document->node(actions);
            ASSERT_NE(actions_node, nullptr);
            ASSERT_EQ(actions_node->children.size(), 2u);
            EXPECT_EQ(actions_node->children[0], run_button);
            EXPECT_EQ(actions_node->children[1], help);
        }

        TEST(UIFoundationsTest, DeclarativeDslAppliesRulesOnTopOfDefaults) {
            using namespace dsl;
            using namespace dsl::styles;

            auto document = UIDocument::create();
            UINodeId button_id = k_invalid_node_id;
            UINodeId input_id = k_invalid_node_id;
            UINodeId scroll_id = k_invalid_node_id;
            UINodeId splitter_id = k_invalid_node_id;

            mount(
                *document,
                column("root").children(
                    button(
                        "Spawn", [] {}, "spawn_button"
                    )
                    .bind(button_id)
                    .style(
                        padding_xy(20.0f, 6.0f),
                        hover(state().background(rgba(0.2f, 0.3f, 0.5f, 1.0f))),
                        focused(state().border(3.0f, rgba(0.8f, 0.9f, 1.0f, 1.0f))),
                        disabled(state().opacity(0.25f))
                    ),
                    text_input("seed", "Placeholder", "text_input")
                    .bind(input_id)
                    .style(
                        fill_x(),
                        focused(state().border(3.0f, rgba(0.9f, 0.8f, 0.7f, 1.0f)))
                    ),
                    scroll_view("scroll_view")
                    .bind(scroll_id)
                    .style(width(px(240.0f)), scroll_both()),
                    splitter("splitter")
                    .bind(splitter_id)
                    .style(background(rgba(0.7f, 0.2f, 0.3f, 0.8f)))
                )
            );

            const auto* button_node = document->node(button_id);
            ASSERT_NE(button_node, nullptr);
            EXPECT_TRUE(button_node->focusable);
            EXPECT_FLOAT_EQ(button_node->style.padding.left, 20.0f);
            EXPECT_FLOAT_EQ(button_node->style.padding.top, 6.0f);
            ASSERT_TRUE(button_node->style.hovered_style.background_color.has_value());
            EXPECT_EQ(
                *button_node->style.hovered_style.background_color,
                rgba(0.2f, 0.3f, 0.5f, 1.0f)
            );
            ASSERT_TRUE(button_node->style.focused_style.border_width.has_value());
            EXPECT_FLOAT_EQ(*button_node->style.focused_style.border_width, 3.0f);
            ASSERT_TRUE(button_node->style.disabled_style.opacity.has_value());
            EXPECT_FLOAT_EQ(*button_node->style.disabled_style.opacity, 0.25f);

            const auto* input_node = document->node(input_id);
            ASSERT_NE(input_node, nullptr);
            EXPECT_TRUE(input_node->focusable);
            EXPECT_EQ(input_node->style.width.unit, UILengthUnit::Percent);
            EXPECT_FLOAT_EQ(input_node->style.width.value, 1.0f);
            EXPECT_FLOAT_EQ(input_node->style.flex_shrink, 0.0f);
            EXPECT_FLOAT_EQ(input_node->style.border_radius, 10.0f);
            ASSERT_TRUE(input_node->style.focused_style.border_width.has_value());
            EXPECT_FLOAT_EQ(*input_node->style.focused_style.border_width, 3.0f);

            const auto* scroll_node = document->node(scroll_id);
            ASSERT_NE(scroll_node, nullptr);
            EXPECT_EQ(scroll_node->style.overflow, Overflow::Hidden);
            EXPECT_EQ(scroll_node->style.width.unit, UILengthUnit::Pixels);
            EXPECT_FLOAT_EQ(scroll_node->style.width.value, 240.0f);
            EXPECT_EQ(scroll_node->style.scroll_mode, ScrollMode::Both);
            EXPECT_EQ(scroll_node->style.scrollbar_visibility, ScrollbarVisibility::Auto);

            const auto* splitter_node = document->node(splitter_id);
            ASSERT_NE(splitter_node, nullptr);
            EXPECT_EQ(splitter_node->style.align_self, AlignSelf::Stretch);
            EXPECT_EQ(
                splitter_node->style.background_color, rgba(0.7f, 0.2f, 0.3f, 0.8f)
            );
        }

        TEST(UIFoundationsTest, DeclarativeDslRowColumnAndSpacerSugarMapToStyle) {
            using namespace dsl;

            auto document = UIDocument::create();
            UINodeId root = k_invalid_node_id;
            UINodeId toolbar = k_invalid_node_id;
            UINodeId filler = k_invalid_node_id;

            mount(
                *document,
                column("root").bind(root).children(
                    row("toolbar").bind(toolbar).children(spacer("filler").bind(filler))
                )
            );

            const auto* root_node = document->node(root);
            const auto* toolbar_node = document->node(toolbar);
            const auto* filler_node = document->node(filler);
            ASSERT_NE(root_node, nullptr);
            ASSERT_NE(toolbar_node, nullptr);
            ASSERT_NE(filler_node, nullptr);

            EXPECT_EQ(root_node->style.flex_direction, FlexDirection::Column);
            EXPECT_EQ(toolbar_node->style.flex_direction, FlexDirection::Row);
            EXPECT_FLOAT_EQ(root_node->style.flex_shrink, 0.0f);
            EXPECT_FLOAT_EQ(toolbar_node->style.flex_shrink, 0.0f);
            EXPECT_FLOAT_EQ(filler_node->style.flex_grow, 1.0f);
            EXPECT_FLOAT_EQ(filler_node->style.flex_shrink, 0.0f);
        }

        TEST(UIFoundationsTest, DeclarativeDslFlexSugarMapsToReactiveDefaults) {
            using namespace dsl;
            using namespace dsl::styles;

            auto document = UIDocument::create();
            UINodeId root = k_invalid_node_id;
            UINodeId body = k_invalid_node_id;

            mount(
                *document,
                column("root").bind(root).children(
                    view("body").bind(body).style(flex(1.0f))
                )
            );

            const auto* root_node = document->node(root);
            const auto* body_node = document->node(body);
            ASSERT_NE(root_node, nullptr);
            ASSERT_NE(body_node, nullptr);

            EXPECT_FLOAT_EQ(root_node->style.flex_shrink, 0.0f);
            EXPECT_FLOAT_EQ(body_node->style.flex_grow, 1.0f);
            EXPECT_FLOAT_EQ(body_node->style.flex_shrink, 1.0f);
            EXPECT_EQ(body_node->style.flex_basis.unit, UILengthUnit::Pixels);
            EXPECT_FLOAT_EQ(body_node->style.flex_basis.value, 0.0f);
        }

        TEST(UIFoundationsTest, DeclarativeDslSupportsDraggablePanels) {
            using namespace dsl;
            using namespace dsl::styles;

            auto document = UIDocument::create();
            UINodeId panel = k_invalid_node_id;
            UINodeId header = k_invalid_node_id;

            mount(
                *document,
                column("root").children(
                    column("panel")
                    .bind(panel)
                    .style(absolute(), draggable(), left(px(24.0f)), top(px(32.0f)))
                    .children(
                        row("header").bind(header).style(drag_handle()).children(
                            text("Panel", "title")
                        )
                    )
                )
            );

            const auto* panel_node = document->node(panel);
            const auto* header_node = document->node(header);
            ASSERT_NE(panel_node, nullptr);
            ASSERT_NE(header_node, nullptr);
            EXPECT_TRUE(panel_node->style.draggable);
            EXPECT_TRUE(header_node->style.drag_handle);
        }

        TEST(UIFoundationsTest, DeclarativeDslSupportsCheckboxSliderAndSelect) {
            using namespace dsl;
            using namespace dsl::styles;

            auto document = UIDocument::create();
            UINodeId checkbox_id = k_invalid_node_id;
            UINodeId slider_id = k_invalid_node_id;
            UINodeId select_id = k_invalid_node_id;

            mount(
                *document,
                column("root").children(
                    checkbox("Follow tail", true, "checkbox")
                    .bind(checkbox_id)
                    .style(
                        accent_color(rgba(0.4f, 0.7f, 0.9f, 1.0f)),
                        control_gap(12.0f), control_indicator_size(20.0f)
                    )
                    .on_toggle([](bool) {}),
                    slider(0.55f, 0.0f, 1.0f, "slider")
                    .bind(slider_id)
                    .step(0.25f)
                    .style(
                        width(px(220.0f)), accent_color(rgba(0.7f, 0.4f, 0.3f, 1.0f)),
                        slider_track_thickness(10.0f), slider_thumb_radius(12.0f)
                    )
                    .on_value_change([](float) {}),
                    select({ "All", "Warn", "Error" }, 1u, "Severity", "select")
                    .bind(select_id)
                    .style(
                        width(px(180.0f)), accent_color(rgba(0.6f, 0.8f, 0.9f, 1.0f))
                    )
                    .on_select([](size_t, const std::string&) {})
                )
            );

            const auto* checkbox_node = document->node(checkbox_id);
            ASSERT_NE(checkbox_node, nullptr);
            EXPECT_TRUE(checkbox_node->checkbox.checked);
            EXPECT_EQ(checkbox_node->style.accent_color, rgba(0.4f, 0.7f, 0.9f, 1.0f));
            EXPECT_FLOAT_EQ(checkbox_node->style.control_gap, 12.0f);
            EXPECT_FLOAT_EQ(checkbox_node->style.control_indicator_size, 20.0f);
            EXPECT_TRUE(static_cast<bool>(checkbox_node->on_toggle));

            const auto* slider_node = document->node(slider_id);
            ASSERT_NE(slider_node, nullptr);
            EXPECT_FLOAT_EQ(slider_node->slider.value, 0.5f);
            EXPECT_FLOAT_EQ(slider_node->slider.step, 0.25f);
            EXPECT_EQ(slider_node->style.width.unit, UILengthUnit::Pixels);
            EXPECT_FLOAT_EQ(slider_node->style.width.value, 220.0f);
            EXPECT_FLOAT_EQ(slider_node->style.slider_track_thickness, 10.0f);
            EXPECT_FLOAT_EQ(slider_node->style.slider_thumb_radius, 12.0f);
            EXPECT_TRUE(static_cast<bool>(slider_node->on_value_change));

            const auto* select_node = document->node(select_id);
            ASSERT_NE(select_node, nullptr);
            EXPECT_EQ(select_node->select.options.size(), 3u);
            EXPECT_EQ(select_node->select.selected_index, 1u);
            EXPECT_EQ(select_node->placeholder, "Severity");
            EXPECT_EQ(select_node->style.width.unit, UILengthUnit::Pixels);
            EXPECT_FLOAT_EQ(select_node->style.width.value, 180.0f);
            EXPECT_EQ(select_node->style.accent_color, rgba(0.6f, 0.8f, 0.9f, 1.0f));
            EXPECT_TRUE(static_cast<bool>(select_node->on_select));
        }

        TEST(UIFoundationsTest, DeclarativeDslRejectsChildrenOnLeafNodes) {
            using namespace dsl;

            auto document = UIDocument::create();

            EXPECT_ANY_THROW(
                mount(*document, button("Broken", [] {}, "broken").child(text("Nope")))
            );
        }

        TEST(UIFoundationsTest, CallbackQueueCanBeMutatedDuringFlush) {
            auto document = UIDocument::create();
            int calls = 0;

            document->queue_callback([&]() {
                ++calls;
                document->queue_callback([&]() { ++calls; });
                });

            document->flush_callbacks();
            EXPECT_EQ(calls, 1);

            document->flush_callbacks();
            EXPECT_EQ(calls, 2);
        }

        TEST(UIFoundationsTest, ClipboardBackendCanBeInjected) {
            const auto original = clipboard_backend();
            const auto backend = create_ref<MemoryClipboardBackend>();

            set_clipboard_backend(backend);
            clipboard_backend()->set(nullptr, "copied");
            EXPECT_EQ(clipboard_backend()->get(nullptr), "copied");

            set_clipboard_backend(original);
        }

    } // namespace
} // namespace astralix::ui
