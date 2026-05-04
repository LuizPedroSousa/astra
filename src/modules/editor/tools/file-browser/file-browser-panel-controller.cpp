#include "tools/file-browser/file-browser-panel-controller.hpp"

#include "fnv1a.hpp"
#include "managers/path-manager.hpp"
#include "managers/project-manager.hpp"
#include "path.hpp"
#include "serialization-context-readers.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace astralix::editor {
namespace {

constexpr double k_filesystem_poll_interval_seconds = 0.5;
constexpr double k_double_click_threshold = 0.4;

std::string lowercase_ascii(std::string_view value) {
  std::string lower(value);
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return lower;
}

bool contains_case_insensitive(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }

  return lowercase_ascii(haystack).find(lowercase_ascii(needle)) !=
         std::string::npos;
}

bool case_insensitive_less(std::string_view lhs, std::string_view rhs) {
  const std::string lhs_lower = lowercase_ascii(lhs);
  const std::string rhs_lower = lowercase_ascii(rhs);
  if (lhs_lower == rhs_lower) {
    return lhs < rhs;
  }
  return lhs_lower < rhs_lower;
}

std::string format_file_size(std::uintmax_t bytes) {
  if (bytes < 1024u) {
    return std::to_string(bytes) + " B";
  }

  if (bytes < 1024u * 1024u) {
    return std::to_string(bytes / 1024u) + " KB";
  }

  return std::to_string(bytes / (1024u * 1024u)) + " MB";
}

std::string
format_modified_time(
    const std::optional<std::filesystem::file_time_type> &value
) {
  if (!value.has_value()) {
    return {};
  }

  const auto system_time = std::chrono::time_point_cast<
      std::chrono::system_clock::duration>(
      *value - std::filesystem::file_time_type::clock::now() +
      std::chrono::system_clock::now()
  );
  const std::time_t timestamp = std::chrono::system_clock::to_time_t(system_time);

  std::tm local_time{};
#if defined(_WIN32)
  localtime_s(&local_time, &timestamp);
#else
  localtime_r(&timestamp, &local_time);
#endif

  std::ostringstream out;
  out << std::put_time(&local_time, "%Y-%m-%d %H:%M");
  return out.str();
}

bool same_directory_entry(
    const FileBrowserPanelController::DirectoryEntry &lhs,
    const FileBrowserPanelController::DirectoryEntry &rhs
) {
  return lhs.path_key == rhs.path_key && lhs.name == rhs.name &&
         lhs.extension == rhs.extension &&
         lhs.size_label == rhs.size_label &&
         lhs.modified_label == rhs.modified_label &&
         lhs.is_directory == rhs.is_directory;
}

bool same_directory_entries(
    const std::vector<FileBrowserPanelController::DirectoryEntry> &lhs,
    const std::vector<FileBrowserPanelController::DirectoryEntry> &rhs
) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), same_directory_entry);
}

bool same_tree_row(
    const FileBrowserPanelController::TreeVisibleRow &lhs,
    const FileBrowserPanelController::TreeVisibleRow &rhs
) {
  return lhs.path_key == rhs.path_key &&
         lhs.display_name == rhs.display_name &&
         lhs.depth == rhs.depth &&
         lhs.is_expanded == rhs.is_expanded &&
         lhs.is_selected == rhs.is_selected &&
         lhs.has_children == rhs.has_children &&
         lhs.height == rhs.height;
}

bool same_tree_rows(
    const std::vector<FileBrowserPanelController::TreeVisibleRow> &lhs,
    const std::vector<FileBrowserPanelController::TreeVisibleRow> &rhs
) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), same_tree_row);
}

} // namespace

void FileBrowserPanelController::invalidate_runtime_handles() {
  m_tree_widget = ui::im::k_invalid_widget_id;
  m_content_widget = ui::im::k_invalid_widget_id;
  m_split_widget = ui::im::k_invalid_widget_id;
}

void FileBrowserPanelController::mount(const PanelMountContext &context) {
  m_runtime = context.runtime;
  m_default_font_id = context.default_font_id;
  m_default_font_size = context.default_font_size;
  invalidate_runtime_handles();
  m_tree_expanded["."] = true;
  m_split_drag_active = false;
  m_split_drag_origin_ratio = m_split_ratio;
  m_elapsed_time = 0.0;
  m_filesystem_poll_elapsed = 0.0;
  refresh(true);
}

void FileBrowserPanelController::unmount() {
  m_runtime = nullptr;
  m_root_path.clear();
  m_root_label = "Resources";
  m_current_directory.clear();
  m_tree_rows.clear();
  m_directory_contents.clear();
  m_selected_entry_key.reset();
  m_last_clicked_entry_key.reset();
  m_directory_read_failed = false;
  m_split_drag_active = false;
  m_split_drag_origin_ratio = m_split_ratio;
  m_reset_content_scroll = false;
  m_last_click_time = 0.0;
  m_elapsed_time = 0.0;
  m_filesystem_poll_elapsed = 0.0;
  invalidate_runtime_handles();
  mark_render_dirty();
}

void FileBrowserPanelController::update(const PanelUpdateContext &context) {
  m_elapsed_time += context.dt;
  m_filesystem_poll_elapsed += context.dt;

  if (m_filesystem_poll_elapsed >= k_filesystem_poll_interval_seconds) {
    refresh();
  }
}

std::optional<uint64_t> FileBrowserPanelController::render_version() const {
  uint64_t hash = k_fnv1a64_offset_basis;
  hash = fnv1a64_append_value(hash, m_render_revision);
  hash = fnv1a64_append_string(m_current_directory_key, hash);
  hash = fnv1a64_append_string(m_search_query, hash);
  hash = fnv1a64_append_value(hash, static_cast<uint8_t>(m_view_mode));
  hash = fnv1a64_append_value(hash, static_cast<uint8_t>(m_root_scope));
  hash = fnv1a64_append_value(hash, m_split_ratio);
  hash = fnv1a64_append_value(hash, m_tree_widget.value);
  hash = fnv1a64_append_value(hash, m_content_widget.value);
  hash = fnv1a64_append_value(hash, m_split_widget.value);
  hash = fnv1a64_append_value(hash, m_split_drag_active);

  if (m_runtime != nullptr && m_tree_widget != ui::im::k_invalid_widget_id) {
    const auto state = m_runtime->virtual_list_state(m_tree_widget);
    hash = fnv1a64_append_value(hash, state.scroll_offset.x);
    hash = fnv1a64_append_value(hash, state.scroll_offset.y);
    hash = fnv1a64_append_value(hash, state.viewport_width);
    hash = fnv1a64_append_value(hash, state.viewport_height);
  }

  if (m_runtime != nullptr && m_content_widget != ui::im::k_invalid_widget_id) {
    const auto state = m_runtime->virtual_list_state(m_content_widget);
    hash = fnv1a64_append_value(hash, state.scroll_offset.x);
    hash = fnv1a64_append_value(hash, state.scroll_offset.y);
    hash = fnv1a64_append_value(hash, state.viewport_width);
    hash = fnv1a64_append_value(hash, state.viewport_height);
  }

  if (m_runtime != nullptr && m_split_widget != ui::im::k_invalid_widget_id) {
    if (const auto bounds = m_runtime->layout_bounds(m_split_widget);
        bounds.has_value()) {
      hash = fnv1a64_append_value(hash, bounds->width);
      hash = fnv1a64_append_value(hash, bounds->height);
    }
  }

  return hash;
}

void FileBrowserPanelController::load_state(Ref<SerializationContext> state) {
  const std::string root_scope =
      serialization::context::read_string(state, "root_scope").value_or("assets");
  m_root_scope = root_scope == "files" ? RootScope::Files : RootScope::Assets;

  const std::string legacy_current_directory =
      serialization::context::read_string(state, "current_directory")
          .value_or(".");
  m_assets_directory_key =
      serialization::context::read_string(state, "assets_directory")
          .value_or(legacy_current_directory);
  m_files_directory_key =
      serialization::context::read_string(state, "files_directory")
          .value_or(
              m_root_scope == RootScope::Files ? legacy_current_directory : "."
          );
  if (m_assets_directory_key.empty()) {
    m_assets_directory_key = ".";
  }
  if (m_files_directory_key.empty()) {
    m_files_directory_key = ".";
  }
  m_current_directory_key = m_root_scope == RootScope::Files
                                ? m_files_directory_key
                                : m_assets_directory_key;

  const std::string view_mode =
      serialization::context::read_string(state, "view_mode").value_or("grid");
  m_view_mode =
      view_mode == "list" ? ContentViewMode::List : ContentViewMode::Grid;

  m_split_ratio = std::clamp(
      serialization::context::read_float(state, "split_ratio").value_or(0.26f),
      0.18f,
      0.45f
  );
  m_split_drag_origin_ratio = m_split_ratio;

  m_search_query =
      serialization::context::read_string(state, "search_query").value_or("");

  m_tree_expanded.clear();
  m_tree_expanded["."] = true;
  if (state != nullptr &&
      (*state)["tree_expanded"].kind() == SerializationTypeKind::Object) {
    for (const auto &key : (*state)["tree_expanded"].object_keys()) {
      const auto expanded =
          serialization::context::read_bool((*state)["tree_expanded"][key]);
      if (expanded.has_value()) {
        m_tree_expanded[key] = *expanded;
      }
    }
  }

  mark_render_dirty();
}

void FileBrowserPanelController::save_state(Ref<SerializationContext> state) const {
  if (state == nullptr) {
    return;
  }

  (*state)["root_scope"] = m_root_scope == RootScope::Files ? "files" : "assets";
  (*state)["current_directory"] = m_current_directory_key;
  (*state)["assets_directory"] = m_assets_directory_key;
  (*state)["files_directory"] = m_files_directory_key;
  (*state)["view_mode"] =
      m_view_mode == ContentViewMode::List ? "list" : "grid";
  (*state)["split_ratio"] = m_split_ratio;
  (*state)["search_query"] = m_search_query;
  for (const auto &[key, expanded] : m_tree_expanded) {
    (*state)["tree_expanded"][key] = expanded;
  }
}

void FileBrowserPanelController::resolve_root_path() {
  auto project = active_project();
  if (project == nullptr) {
    m_root_path.clear();
    m_root_label = "Resources";
    return;
  }

  if (m_root_scope == RootScope::Files) {
    m_root_path = std::filesystem::path(project->get_config().directory)
                      .lexically_normal();
    if (m_root_path.empty()) {
      m_root_path = project->manifest_path().parent_path().lexically_normal();
    }

    std::filesystem::path label_path = m_root_path;
    m_root_label = label_path.filename().string();
    if (m_root_label.empty()) {
      m_root_label = project->get_config().name;
    }
    if (m_root_label.empty()) {
      m_root_label = "Project";
    }
    return;
  }

  if (const auto manager = path_manager(); manager != nullptr) {
    m_root_path =
        manager->resolve(Path::create("", BaseDirectory::Project))
            .lexically_normal();
  } else {
    m_root_path = project->resolve_path(project->get_config().resources.directory)
                      .lexically_normal();
  }

  const std::string &configured_directory = project->get_config().resources.directory;
  std::filesystem::path label_path(configured_directory);
  m_root_label = label_path.filename().string();
  if (m_root_label.empty()) {
    m_root_label = configured_directory;
  }
  if (m_root_label.empty()) {
    m_root_label = "Resources";
  }
}

void FileBrowserPanelController::refresh(bool force) {
  m_filesystem_poll_elapsed = 0.0;

  const std::filesystem::path previous_root_path = m_root_path;
  const std::string previous_root_label = m_root_label;
  const std::string previous_current_key = m_current_directory_key;
  const auto previous_tree_rows = m_tree_rows;
  const auto previous_contents = m_directory_contents;
  const auto previous_selection = m_selected_entry_key;
  const bool previous_read_failed = m_directory_read_failed;

  resolve_root_path();
  m_tree_expanded["."] = true;

  if (m_root_path.empty()) {
    m_current_directory.clear();
    m_current_directory_key = ".";
    m_tree_rows.clear();
    m_directory_contents.clear();
    m_directory_read_failed = false;
    m_selected_entry_key.reset();
  } else {
    ensure_current_directory_is_valid();
    auto tree = scan_directory_tree();
    auto next_tree_rows = build_tree_rows(tree);
    auto next_contents = scan_directory_contents(m_current_directory);

    m_tree_rows = std::move(next_tree_rows);
    m_directory_contents = std::move(next_contents);
    clear_selection_if_missing();
  }

  const bool changed =
      previous_root_path != m_root_path ||
      previous_root_label != m_root_label ||
      previous_current_key != m_current_directory_key ||
      previous_read_failed != m_directory_read_failed ||
      previous_selection != m_selected_entry_key ||
      !same_tree_rows(previous_tree_rows, m_tree_rows) ||
      !same_directory_entries(previous_contents, m_directory_contents);

  if (force || changed) {
    mark_render_dirty();
  }
}

void FileBrowserPanelController::refresh_now() { refresh(true); }

void FileBrowserPanelController::assign_current_directory(
    std::filesystem::path path,
    bool clear_selection
) {
  if (m_root_path.empty()) {
    return;
  }

  std::filesystem::path next = sanitize_directory_path(path);
  if (next.empty()) {
    return;
  }

  if (m_current_directory == next && !clear_selection) {
    return;
  }

  m_current_directory = std::move(next);
  m_current_directory_key = path_key_for(m_current_directory);
  if (m_root_scope == RootScope::Files) {
    m_files_directory_key = m_current_directory_key;
  } else {
    m_assets_directory_key = m_current_directory_key;
  }
  ensure_tree_expanded_for(m_current_directory);
  if (clear_selection) {
    m_selected_entry_key.reset();
  }
  m_reset_content_scroll = true;
}

void FileBrowserPanelController::ensure_current_directory_is_valid() {
  if (m_root_path.empty()) {
    m_current_directory.clear();
    m_current_directory_key = ".";
    return;
  }

  const std::string &scope_directory_key =
      m_root_scope == RootScope::Files ? m_files_directory_key
                                       : m_assets_directory_key;
  if (!scope_directory_key.empty()) {
    m_current_directory_key = scope_directory_key;
  }

  std::filesystem::path candidate = path_for_key(m_current_directory_key);
  if (candidate.empty()) {
    candidate = sanitize_directory_path(m_current_directory);
  }
  if (candidate.empty()) {
    candidate = sanitize_directory_path(m_root_path);
  }

  m_current_directory = std::move(candidate);
  m_current_directory_key = path_key_for(m_current_directory);
  if (m_root_scope == RootScope::Files) {
    m_files_directory_key = m_current_directory_key;
  } else {
    m_assets_directory_key = m_current_directory_key;
  }
  ensure_tree_expanded_for(m_current_directory);
}

void FileBrowserPanelController::ensure_tree_expanded_for(
    const std::filesystem::path &path
) {
  if (m_root_path.empty()) {
    return;
  }

  m_tree_expanded["."] = true;
  const auto relative =
      path.lexically_normal().lexically_relative(m_root_path.lexically_normal());
  if (relative.empty() || relative == ".") {
    return;
  }

  std::filesystem::path cursor = m_root_path;
  for (const auto &part : relative) {
    cursor /= part;
    m_tree_expanded[path_key_for(cursor)] = true;
  }
}

void FileBrowserPanelController::clear_selection_if_missing() {
  if (!m_selected_entry_key.has_value()) {
    return;
  }

  const bool found = std::any_of(
      m_directory_contents.begin(),
      m_directory_contents.end(),
      [selected = *m_selected_entry_key](const DirectoryEntry &entry) {
        return entry.path_key == selected;
      }
  );
  if (!found) {
    m_selected_entry_key.reset();
  }
}

std::filesystem::path FileBrowserPanelController::sanitize_directory_path(
    const std::filesystem::path &path
) const {
  if (m_root_path.empty()) {
    return {};
  }

  std::filesystem::path candidate =
      path.empty() ? m_root_path : path.lexically_normal();
  if (!is_directory_under_root(candidate)) {
    return m_root_path;
  }

  std::error_code error_code;
  if (std::filesystem::exists(candidate, error_code) &&
      std::filesystem::is_directory(candidate, error_code) && !error_code) {
    return candidate;
  }

  std::filesystem::path probe = candidate;
  while (is_directory_under_root(probe)) {
    error_code.clear();
    if (std::filesystem::exists(probe, error_code) &&
        std::filesystem::is_directory(probe, error_code) && !error_code) {
      return probe.lexically_normal();
    }

    if (probe == m_root_path || probe == probe.parent_path()) {
      break;
    }
    probe = probe.parent_path();
  }

  return m_root_path;
}

bool FileBrowserPanelController::is_directory_under_root(
    const std::filesystem::path &path
) const {
  if (m_root_path.empty()) {
    return false;
  }

  const auto normalized_root = m_root_path.lexically_normal();
  const auto normalized_path = path.lexically_normal();
  if (normalized_path == normalized_root) {
    return true;
  }

  const auto relative = normalized_path.lexically_relative(normalized_root);
  if (relative.empty()) {
    return false;
  }

  const auto first = *relative.begin();
  return first != "..";
}

bool FileBrowserPanelController::is_tree_expanded(std::string_view key) const {
  if (key.empty() || key == ".") {
    return true;
  }

  const auto it = m_tree_expanded.find(std::string(key));
  return it != m_tree_expanded.end() ? it->second : false;
}

bool FileBrowserPanelController::tree_query_matches(std::string_view value) const {
  return contains_case_insensitive(value, m_search_query);
}

bool FileBrowserPanelController::content_query_matches(
    std::string_view value
) const {
  return contains_case_insensitive(value, m_search_query);
}

std::string
FileBrowserPanelController::path_key_for(const std::filesystem::path &path) const {
  if (m_root_path.empty()) {
    return ".";
  }

  const auto normalized_root = m_root_path.lexically_normal();
  const auto normalized_path = path.lexically_normal();
  if (normalized_path == normalized_root) {
    return ".";
  }

  const auto relative = normalized_path.lexically_relative(normalized_root);
  if (relative.empty() || relative == ".") {
    return ".";
  }

  return relative.generic_string();
}

std::filesystem::path
FileBrowserPanelController::path_for_key(std::string_view key) const {
  if (m_root_path.empty()) {
    return {};
  }

  if (key.empty() || key == ".") {
    return sanitize_directory_path(m_root_path);
  }

  return sanitize_directory_path(m_root_path / std::filesystem::path(key));
}

std::string FileBrowserPanelController::display_name_for(
    const std::filesystem::path &path
) const {
  if (path_key_for(path) == ".") {
    return m_root_label;
  }

  const std::string filename = path.filename().string();
  return filename.empty() ? path.generic_string() : filename;
}

FileBrowserPanelController::DirectoryTreeNode
FileBrowserPanelController::scan_directory_tree() const {
  const auto build_node = [&](const auto &self,
                              const std::filesystem::path &path)
      -> DirectoryTreeNode {
    DirectoryTreeNode node{
        .absolute_path = path,
        .path_key = path_key_for(path),
        .name = display_name_for(path),
    };

    std::error_code error_code;
    std::vector<std::filesystem::path> child_directories;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::directory_iterator it(path, options, error_code), end;
         !error_code && it != end;
         it.increment(error_code)) {
      const auto child_path = it->path();
      std::error_code child_error;
      if (!it->is_directory(child_error) || child_error) {
        continue;
      }
      child_directories.push_back(child_path.lexically_normal());
    }

    std::sort(
        child_directories.begin(),
        child_directories.end(),
        [](const std::filesystem::path &lhs, const std::filesystem::path &rhs) {
          return case_insensitive_less(
              lhs.filename().string(), rhs.filename().string()
          );
        }
    );

    node.children.reserve(child_directories.size());
    for (const auto &child_directory : child_directories) {
      node.children.push_back(self(self, child_directory));
    }

    return node;
  };

  return build_node(build_node, m_root_path);
}

std::vector<FileBrowserPanelController::DirectoryEntry>
FileBrowserPanelController::scan_directory_contents(
    const std::filesystem::path &directory
) {
  m_directory_read_failed = false;

  std::vector<DirectoryEntry> directories;
  std::vector<DirectoryEntry> files;

  std::error_code error_code;
  const auto options = std::filesystem::directory_options::skip_permission_denied;
  for (std::filesystem::directory_iterator it(directory, options, error_code), end;
       !error_code && it != end;
       it.increment(error_code)) {
    DirectoryEntry entry;
    entry.absolute_path = it->path().lexically_normal();
    entry.path_key = path_key_for(entry.absolute_path);
    entry.name = entry.absolute_path.filename().string();

    std::error_code entry_error;
    entry.is_directory = it->is_directory(entry_error);
    if (entry_error) {
      continue;
    }

    if (!content_query_matches(entry.name)) {
      continue;
    }

    if (!entry.is_directory) {
      entry.extension = lowercase_ascii(entry.absolute_path.extension().string());
      entry.file_size = it->file_size(entry_error);
      if (!entry_error && entry.file_size.has_value()) {
        entry.size_label = format_file_size(*entry.file_size);
      } else {
        entry.file_size.reset();
      }
    }

    entry_error.clear();
    const auto write_time = it->last_write_time(entry_error);
    if (!entry_error) {
      entry.last_modified = write_time;
      entry.modified_label = format_modified_time(entry.last_modified);
    }

    if (entry.is_directory) {
      directories.push_back(std::move(entry));
    } else {
      files.push_back(std::move(entry));
    }
  }

  if (error_code) {
    m_directory_read_failed = true;
  }

  const auto comparator = [](const DirectoryEntry &lhs, const DirectoryEntry &rhs) {
    return case_insensitive_less(lhs.name, rhs.name);
  };
  std::sort(directories.begin(), directories.end(), comparator);
  std::sort(files.begin(), files.end(), comparator);

  directories.reserve(directories.size() + files.size());
  directories.insert(
      directories.end(),
      std::make_move_iterator(files.begin()),
      std::make_move_iterator(files.end())
  );
  return directories;
}

std::vector<FileBrowserPanelController::TreeVisibleRow>
FileBrowserPanelController::build_tree_rows(const DirectoryTreeNode &root) const {
  std::vector<TreeVisibleRow> rows;
  rows.reserve(64u);
  append_tree_rows(rows, root, 0);
  return rows;
}

bool FileBrowserPanelController::append_tree_rows(
    std::vector<TreeVisibleRow> &rows,
    const DirectoryTreeNode &node,
    int depth
) const {
  const bool self_match = m_search_query.empty() || tree_query_matches(node.name) ||
                          tree_query_matches(node.path_key);

  bool keep_current_path = false;
  if (!m_current_directory.empty()) {
    const auto relative =
        m_current_directory.lexically_normal().lexically_relative(
            node.absolute_path.lexically_normal()
        );
    keep_current_path =
        m_current_directory.lexically_normal() == node.absolute_path.lexically_normal() ||
        (!relative.empty() && *relative.begin() != "..");
  }

  std::vector<TreeVisibleRow> child_rows;
  bool descendant_match = false;
  for (const auto &child : node.children) {
    descendant_match |= append_tree_rows(child_rows, child, depth + 1);
  }

  const bool include =
      depth == 0 || m_search_query.empty() || self_match || descendant_match ||
      keep_current_path;
  if (!include) {
    return self_match || descendant_match;
  }

  const bool expanded =
      depth == 0 ? true
                 : (m_search_query.empty()
                        ? is_tree_expanded(node.path_key)
                        : (self_match || descendant_match || keep_current_path));

  rows.push_back(TreeVisibleRow{
      .absolute_path = node.absolute_path,
      .path_key = node.path_key,
      .display_name = node.name,
      .depth = depth,
      .is_expanded = expanded,
      .is_selected = node.path_key == m_current_directory_key,
      .has_children = !node.children.empty(),
      .height = 28.0f,
  });

  if (expanded) {
    rows.insert(
        rows.end(),
        std::make_move_iterator(child_rows.begin()),
        std::make_move_iterator(child_rows.end())
    );
  }

  return self_match || descendant_match || keep_current_path;
}

void FileBrowserPanelController::handle_tree_row_click(const TreeVisibleRow &row) {
  assign_current_directory(row.absolute_path);
  refresh(true);
}

void FileBrowserPanelController::handle_content_click(const DirectoryEntry &entry) {
  const bool is_double_click =
      m_last_clicked_entry_key.has_value() &&
      *m_last_clicked_entry_key == entry.path_key &&
      (m_elapsed_time - m_last_click_time) < k_double_click_threshold;

  m_last_clicked_entry_key = entry.path_key;
  m_last_click_time = m_elapsed_time;
  m_selected_entry_key = entry.path_key;
  mark_render_dirty();

  if (is_double_click) {
    activate_content_entry(entry);
  }
}

void FileBrowserPanelController::activate_content_entry(
    const DirectoryEntry &entry
) {
  if (!entry.is_directory) {
    return;
  }

  assign_current_directory(entry.absolute_path);
  refresh(true);
}

void FileBrowserPanelController::set_view_mode(ContentViewMode mode) {
  if (m_view_mode == mode) {
    return;
  }

  m_view_mode = mode;
  m_reset_content_scroll = true;
  mark_render_dirty();
}

void FileBrowserPanelController::set_root_scope(RootScope scope) {
  if (m_root_scope == scope) {
    return;
  }

  if (m_root_scope == RootScope::Files) {
    m_files_directory_key = m_current_directory_key;
  } else {
    m_assets_directory_key = m_current_directory_key;
  }

  m_root_scope = scope;
  m_current_directory_key = m_root_scope == RootScope::Files
                                ? m_files_directory_key
                                : m_assets_directory_key;
  if (m_current_directory_key.empty()) {
    m_current_directory_key = ".";
  }

  m_current_directory.clear();
  m_selected_entry_key.reset();
  m_last_clicked_entry_key.reset();
  m_reset_content_scroll = true;
  refresh(true);
}

std::pair<std::string, std::string>
FileBrowserPanelController::content_empty_state() const {
  if (m_root_path.empty()) {
    return m_root_scope == RootScope::Files
               ? std::pair<std::string, std::string>{
                     "No project directory",
                     "Open a project to browse its files.",
                 }
               : std::pair<std::string, std::string>{
                     "No project resources",
                     "Open a project to browse its resource directory.",
                 };
  }

  std::error_code error_code;
  if (!std::filesystem::exists(m_root_path, error_code) || error_code) {
    return m_root_scope == RootScope::Files
               ? std::pair<std::string, std::string>{
                     "Project directory missing",
                     "The current project directory could not be found.",
                 }
               : std::pair<std::string, std::string>{
                     "Resource directory missing",
                     "The configured resource directory could not be found.",
                 };
  }

  if (m_directory_read_failed) {
    return {
        "Unable to read directory",
        "This directory is unavailable or permission was denied.",
    };
  }

  if (!m_search_query.empty()) {
    return {
        "No matching items",
        "Try a different search query.",
    };
  }

  return {
      "Folder is empty",
      "This directory does not contain any files or subdirectories.",
  };
}

const FileBrowserPanelController::DirectoryEntry *
FileBrowserPanelController::selected_entry() const {
  if (!m_selected_entry_key.has_value()) {
    return nullptr;
  }

  const auto it = std::find_if(
      m_directory_contents.begin(),
      m_directory_contents.end(),
      [selected = *m_selected_entry_key](const DirectoryEntry &entry) {
        return entry.path_key == selected;
      }
  );
  return it != m_directory_contents.end() ? &(*it) : nullptr;
}

} // namespace astralix::editor
