#pragma once

#include "astralix/modules/ui/document.hpp"
#include "astralix/modules/ui/types.hpp"

using namespace astralix;

class Prologue;

constexpr const char *fps_text_prefix = "FPS: ";
constexpr const char *entities_count_text_prefix = "Entities Count: ";
constexpr const char *bodies_text_prefix = "Bodies: ";
constexpr float fps_update_interval = 0.25f;

struct HudDocumentState {
  Ref<ui::UIDocument> document;
  ui::UINodeId fps = ui::k_invalid_node_id;
  ui::UINodeId entities = ui::k_invalid_node_id;
  ui::UINodeId resizable_split_demo = ui::k_invalid_node_id;
  ui::UINodeId bodies = ui::k_invalid_node_id;
  ui::UINodeId console_root = ui::k_invalid_node_id;
  ui::UINodeId console_settings = ui::k_invalid_node_id;
  ui::UINodeId console_severity = ui::k_invalid_node_id;
  ui::UINodeId console_sources = ui::k_invalid_node_id;
  ui::UINodeId console_log_scroll = ui::k_invalid_node_id;
  ui::UINodeId console_input = ui::k_invalid_node_id;
};

HudDocumentState build_hud_document(Prologue *scene);
