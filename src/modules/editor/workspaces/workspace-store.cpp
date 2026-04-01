#include "workspaces/workspace-store.hpp"

#include "adapters/file/file-stream-reader.hpp"
#include "adapters/file/file-stream-writer.hpp"
#include "assert.hpp"
#include "exceptions/base-exception.hpp"
#include "serialization-context-readers.hpp"
#include "stream-buffer.hpp"
#include <cstring>

namespace astralix::editor {
namespace {

Scope<StreamBuffer> copy_to_stream_buffer(ElasticArena::Block *block) {
  auto buffer = create_scope<StreamBuffer>(block->size);
  std::memcpy(buffer->data(), block->data, block->size);
  return buffer;
}

Scope<StreamBuffer> buffer_from_string(std::string_view text) {
  auto buffer = create_scope<StreamBuffer>(text.size());
  if (!text.empty()) {
    std::memcpy(buffer->data(), text.data(), text.size());
  }
  return buffer;
}

void write_panel_frame(Ref<SerializationContext> ctx, const WorkspacePanelFrame &frame) {
  (*ctx)["x"] = frame.x;
  (*ctx)["y"] = frame.y;
  (*ctx)["width"] = frame.width;
  (*ctx)["height"] = frame.height;
}

std::optional<WorkspacePanelFrame> read_panel_frame(ContextProxy ctx) {
  auto x = serialization::context::read_float(ctx["x"]);
  auto y = serialization::context::read_float(ctx["y"]);
  auto width = serialization::context::read_float(ctx["width"]);
  auto height = serialization::context::read_float(ctx["height"]);
  if (!x.has_value() || !y.has_value() || !width.has_value() ||
      !height.has_value()) {
    return std::nullopt;
  }

  return WorkspacePanelFrame{
      .x = *x,
      .y = *y,
      .width = *width,
      .height = *height,
  };
}

ui::FlexDirection flex_direction_from_string(std::string_view value) {
  return value == "column" ? ui::FlexDirection::Column : ui::FlexDirection::Row;
}

std::string flex_direction_to_string(ui::FlexDirection direction) {
  return direction == ui::FlexDirection::Column ? "column" : "row";
}

LayoutNodeKind layout_kind_from_string(std::string_view value) {
  if (value == "split") {
    return LayoutNodeKind::Split;
  }

  if (value == "tabs") {
    return LayoutNodeKind::Tabs;
  }

  return LayoutNodeKind::Leaf;
}

std::string layout_kind_to_string(LayoutNodeKind kind) {
  switch (kind) {
    case LayoutNodeKind::Split:
      return "split";
    case LayoutNodeKind::Tabs:
      return "tabs";
    case LayoutNodeKind::Leaf:
    default:
      return "leaf";
  }
}

} // namespace

WorkspaceStore::WorkspaceStore(Ref<Project> project) : m_project(std::move(project)) {}

SerializationFormat WorkspaceStore::storage_format() const {
  if (m_project == nullptr) {
    return SerializationFormat::Json;
  }

  return m_project->get_config().serialization.format;
}

std::string WorkspaceStore::storage_extension() const {
  return create_context()->extension();
}

Ref<SerializationContext> WorkspaceStore::create_context() const {
  return SerializationContext::create(storage_format());
}

std::optional<std::string> WorkspaceStore::load_active_workspace_id() const {
  auto ctx = read_context_file(session_path());
  if (!ctx.has_value()) {
    return std::nullopt;
  }

  return read_session(*ctx);
}

void WorkspaceStore::save_active_workspace_id(std::string_view workspace_id) {
  auto ctx = create_context();
  write_session(ctx, workspace_id);
  write_context_file(session_path(), ctx);
}

std::optional<WorkspaceSnapshot>
WorkspaceStore::load_snapshot(std::string_view workspace_id) const {
  auto ctx = read_context_file(snapshot_path(workspace_id));
  if (!ctx.has_value()) {
    return std::nullopt;
  }

  try {
    return read_snapshot(*ctx);
  } catch (const BaseException &) {
    return std::nullopt;
  }
}

void WorkspaceStore::save_snapshot(const WorkspaceSnapshot &snapshot) {
  auto ctx = create_context();
  write_snapshot(ctx, snapshot);
  write_context_file(snapshot_path(snapshot.workspace_id), ctx);
}

std::string WorkspaceStore::encode_panel_state(Ref<SerializationContext> state) const {
  if (state == nullptr) {
    return {};
  }

  ElasticArena arena(KB(32));
  auto *block = state->to_buffer(arena);
  return std::string(static_cast<const char *>(block->data), block->size);
}

std::optional<Ref<SerializationContext>>
WorkspaceStore::decode_panel_state(std::string_view state_blob) const {
  if (state_blob.empty()) {
    return std::nullopt;
  }

  try {
    return SerializationContext::create(
        storage_format(), buffer_from_string(state_blob)
    );
  } catch (const BaseException &) {
    return std::nullopt;
  }
}

std::filesystem::path WorkspaceStore::editor_root() const {
  const auto base_directory = m_project != nullptr
                                  ? std::filesystem::path(
                                        m_project->get_config().directory
                                    )
                                  : std::filesystem::current_path();

  return base_directory / ".astralix" / "editor";
}

std::filesystem::path WorkspaceStore::workspaces_root() const {
  return editor_root() / "workspaces";
}

std::filesystem::path WorkspaceStore::session_path() const {
  return editor_root() / ("session" + storage_extension());
}

std::filesystem::path
WorkspaceStore::snapshot_path(std::string_view workspace_id) const {
  return workspaces_root() /
         (std::string(workspace_id) + storage_extension());
}

std::optional<Ref<SerializationContext>>
WorkspaceStore::read_context_file(const std::filesystem::path &path) const {
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }

  try {
    auto reader = FileStreamReader(path);
    reader.read();
    return SerializationContext::create(storage_format(), reader.get_buffer());
  } catch (const BaseException &) {
    return std::nullopt;
  }
}

void WorkspaceStore::write_context_file(const std::filesystem::path &path, Ref<SerializationContext> ctx) const {
  std::filesystem::create_directories(path.parent_path());

  ElasticArena arena(KB(128));
  auto *block = ctx->to_buffer(arena);
  auto writer = FileStreamWriter(path, copy_to_stream_buffer(block));
  writer.write();
}

void WorkspaceStore::write_session(Ref<SerializationContext> ctx, std::string_view workspace_id) const {
  (*ctx)["version"] = 1;
  (*ctx)["active_workspace_id"] = std::string(workspace_id);
}

std::optional<std::string>
WorkspaceStore::read_session(Ref<SerializationContext> ctx) const {
  return serialization::context::read_string((*ctx)["active_workspace_id"]);
}

void WorkspaceStore::write_snapshot(Ref<SerializationContext> ctx, const WorkspaceSnapshot &snapshot) const {
  (*ctx)["version"] = snapshot.version;
  (*ctx)["workspace_id"] = snapshot.workspace_id;

  auto root_ctx = create_context();
  write_layout_node(root_ctx, snapshot.root);
  (*ctx)["root"] = root_ctx;

  size_t panel_index = 0u;
  for (const auto &[instance_id, panel] : snapshot.panels) {
    auto panel_ctx = create_context();
    (*panel_ctx)["instance_id"] = instance_id;
    (*panel_ctx)["provider_id"] = panel.provider_id;
    (*panel_ctx)["title"] = panel.title;
    (*panel_ctx)["open"] = panel.open;
    if (panel.floating_frame.has_value()) {
      auto frame_ctx = create_context();
      write_panel_frame(frame_ctx, *panel.floating_frame);
      (*panel_ctx)["floating_frame"] = frame_ctx;
    }
    (*panel_ctx)["state_blob"] = panel.state_blob;
    (*ctx)["panels"][static_cast<int>(panel_index++)] = panel_ctx;
  }
}

WorkspaceSnapshot WorkspaceStore::read_snapshot(Ref<SerializationContext> ctx) const {
  WorkspaceSnapshot snapshot;
  snapshot.version = serialization::context::read_int_or(
      (*ctx)["version"], k_workspace_snapshot_version
  );
  snapshot.workspace_id =
      serialization::context::read_string_or((*ctx)["workspace_id"]);
  snapshot.root = read_layout_node((*ctx)["root"]);

  auto panels_proxy = (*ctx)["panels"];
  if (panels_proxy.kind() == SerializationTypeKind::Array) {
    for (size_t index = 0u; index < panels_proxy.size(); ++index) {
      auto panel_proxy = panels_proxy[static_cast<int>(index)];
      const std::string instance_id =
          serialization::context::read_string_or(panel_proxy["instance_id"]);
      if (instance_id.empty()) {
        continue;
      }

      snapshot.panels.insert_or_assign(
          instance_id, WorkspacePanelState{
                           .provider_id = serialization::context::read_string_or(panel_proxy["provider_id"]

                           ),
                           .title = serialization::context::read_string_or(panel_proxy["title"]),
                           .open = serialization::context::read_bool_or(panel_proxy["open"], true),
                           .floating_frame = read_panel_frame(panel_proxy["floating_frame"]),
                           .state_blob = serialization::context::read_string_or(panel_proxy["state_blob"]),
                       }
      );
    }
  }

  return snapshot;
}

void WorkspaceStore::write_layout_node(Ref<SerializationContext> ctx, const LayoutNode &node) const {
  (*ctx)["kind"] = layout_kind_to_string(node.kind);

  switch (node.kind) {
    case LayoutNodeKind::Split: {
      (*ctx)["axis"] = flex_direction_to_string(node.split_axis);
      (*ctx)["ratio"] = node.split_ratio;

      if (node.first != nullptr) {
        auto first_ctx = create_context();
        write_layout_node(first_ctx, *node.first);
        (*ctx)["first"] = first_ctx;
      }

      if (node.second != nullptr) {
        auto second_ctx = create_context();
        write_layout_node(second_ctx, *node.second);
        (*ctx)["second"] = second_ctx;
      }
      return;
    }

    case LayoutNodeKind::Tabs: {
      (*ctx)["active_tab_id"] = node.active_tab_id;
      for (size_t index = 0u; index < node.tabs.size(); ++index) {
        (*ctx)["tabs"][static_cast<int>(index)] = node.tabs[index];
      }
      return;
    }

    case LayoutNodeKind::Leaf:
    default:
      (*ctx)["panel_instance_id"] = node.panel_instance_id;
      return;
  }
}

LayoutNode WorkspaceStore::read_layout_node(ContextProxy ctx) const {
  LayoutNode node;
  node.kind = layout_kind_from_string(
      serialization::context::read_string_or(ctx["kind"], "leaf")
  );

  switch (node.kind) {
    case LayoutNodeKind::Split: {
      node.split_axis = flex_direction_from_string(
          serialization::context::read_string_or(ctx["axis"], "row")
      );
      node.split_ratio =
          serialization::context::read_float_or(ctx["ratio"], 0.5f);
      node.first = create_scope<LayoutNode>(read_layout_node(ctx["first"]));
      node.second = create_scope<LayoutNode>(read_layout_node(ctx["second"]));
      return node;
    }

    case LayoutNodeKind::Tabs: {
      node.active_tab_id =
          serialization::context::read_string_or(ctx["active_tab_id"]);
      auto tabs_proxy = ctx["tabs"];
      if (tabs_proxy.kind() == SerializationTypeKind::Array) {
        for (size_t index = 0u; index < tabs_proxy.size(); ++index) {
          auto value = serialization::context::read_string(
              tabs_proxy[static_cast<int>(index)]
          );
          if (value.has_value()) {
            node.tabs.push_back(*value);
          }
        }
      }
      return node;
    }

    case LayoutNodeKind::Leaf:
    default:
      node.panel_instance_id =
          serialization::context::read_string_or(ctx["panel_instance_id"]);
      return node;
  }
}

} // namespace astralix::editor
