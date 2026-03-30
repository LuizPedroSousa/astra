#include "adapters/toml/toml-serialization-context.hpp"
#include "arena.hpp"
#include "assert.hpp"
#include "base.hpp"
#include "context-proxy.hpp"
#include "stream-buffer.hpp"

#include <any>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace astralix {

namespace {

using Node = toml_detail::Node;
using NodeKind = toml_detail::NodeKind;

struct Line {
  size_t number = 0;
  std::string content;
};

enum class ArrayCategory {
  Scalar,
  Table,
  Unsupported,
};

Node make_node(NodeKind kind = NodeKind::Undefined) {
  Node node;
  node.kind = kind;
  return node;
}

void reset_node(Node &node, NodeKind kind) {
  node.kind = kind;
  node.string_value.clear();
  node.int_value = 0;
  node.float_value = 0.0f;
  node.bool_value = false;
  node.table_items.clear();
  node.array_items.clear();
}

Node clone_node(const Node &node) {
  Node clone = make_node(node.kind);
  clone.string_value = node.string_value;
  clone.int_value = node.int_value;
  clone.float_value = node.float_value;
  clone.bool_value = node.bool_value;

  for (const auto &entry : node.table_items) {
    clone.table_items.push_back(
        {entry.key, std::make_unique<Node>(clone_node(*entry.value))});
  }

  for (const auto &item : node.array_items) {
    clone.array_items.push_back(std::make_unique<Node>(clone_node(*item)));
  }

  return clone;
}

Node *find_table_child(Node &node, std::string_view key) {
  for (auto &entry : node.table_items) {
    if (entry.key == key) {
      return entry.value.get();
    }
  }

  return nullptr;
}

Node &ensure_table_container(Node &node) {
  if (node.kind == NodeKind::Undefined) {
    reset_node(node, NodeKind::Table);
  }

  ASTRA_ENSURE(node.kind != NodeKind::Table,
               "TOML node is not a table for string indexing");
  return node;
}

Node &ensure_array_container(Node &node) {
  if (node.kind == NodeKind::Undefined) {
    reset_node(node, NodeKind::Array);
  }

  ASTRA_ENSURE(node.kind != NodeKind::Array,
               "TOML node is not an array for integer indexing");
  return node;
}

Node &ensure_table_child(Node &table, const std::string &key) {
  ensure_table_container(table);

  if (auto *existing = find_table_child(table, key)) {
    return *existing;
  }

  table.table_items.push_back({key, std::make_unique<Node>(make_node())});
  return *table.table_items.back().value;
}

Node &ensure_array_child(Node &node, int index) {
  ASTRA_ENSURE(index < 0, "TOML array index cannot be negative");

  auto &array = ensure_array_container(node);
  while (static_cast<int>(array.array_items.size()) <= index) {
    array.array_items.push_back(std::make_unique<Node>(make_node()));
  }

  return *array.array_items[static_cast<size_t>(index)];
}

size_t current_size(const Node *node) {
  if (node == nullptr) {
    return 0;
  }

  switch (node->kind) {
  case NodeKind::Table:
    return node->table_items.size();
  case NodeKind::Array:
    return node->array_items.size();
  default:
    return 0;
  }
}

std::string trim_copy(std::string_view value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }

  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return std::string(value.substr(start, end - start));
}

std::vector<Line> preprocess_toml(std::string_view content) {
  std::vector<Line> lines;
  size_t position = 0;
  size_t line_number = 1;

  if (content.size() >= 3 &&
      static_cast<unsigned char>(content[0]) == 0xEF &&
      static_cast<unsigned char>(content[1]) == 0xBB &&
      static_cast<unsigned char>(content[2]) == 0xBF) {
    content.remove_prefix(3);
  }

  while (position <= content.size()) {
    const size_t line_end = content.find('\n', position);
    const bool has_newline = line_end != std::string_view::npos;
    std::string_view raw_line =
        has_newline ? content.substr(position, line_end - position)
                    : content.substr(position);

    if (!raw_line.empty() && raw_line.back() == '\r') {
      raw_line.remove_suffix(1);
    }

    bool in_single = false;
    bool in_double = false;
    std::string stripped;
    stripped.reserve(raw_line.size());

    for (size_t i = 0; i < raw_line.size(); ++i) {
      const char ch = raw_line[i];

      if (in_single) {
        stripped.push_back(ch);
        if (ch == '\'') {
          in_single = false;
        }
        continue;
      }

      if (in_double) {
        stripped.push_back(ch);
        if (ch == '\\' && i + 1 < raw_line.size()) {
          stripped.push_back(raw_line[++i]);
          continue;
        }
        if (ch == '"') {
          in_double = false;
        }
        continue;
      }

      if (ch == '#') {
        break;
      }

      stripped.push_back(ch);

      if (ch == '\'') {
        in_single = true;
      } else if (ch == '"') {
        in_double = true;
      }
    }

    const auto trimmed = trim_copy(stripped);
    if (!trimmed.empty()) {
      lines.push_back({.number = line_number, .content = trimmed});
    }

    if (!has_newline) {
      break;
    }

    position = line_end + 1;
    ++line_number;
  }

  return lines;
}

std::pair<std::string, size_t>
parse_quoted_string_at(std::string_view text, size_t offset,
                       size_t line_number) {
  ASTRA_ENSURE(offset >= text.size(), "Invalid TOML string on line ",
               line_number);

  const char quote = text[offset];
  ASTRA_ENSURE(quote != '"' && quote != '\'',
               "TOML string must begin with a quote on line ", line_number);

  ASTRA_ENSURE(offset + 2 < text.size() && text[offset + 1] == quote &&
                   text[offset + 2] == quote,
               "Multiline TOML strings are not supported on line ",
               line_number);

  std::string result;
  result.reserve(text.size() - offset);
  bool closed = false;

  size_t i = offset + 1;
  for (; i < text.size(); ++i) {
    const char ch = text[i];

    if (quote == '\'') {
      if (ch == '\'') {
        closed = true;
        ++i;
        break;
      }

      result.push_back(ch);
      continue;
    }

    if (ch == '\\') {
      ASTRA_ENSURE(i + 1 >= text.size(),
                   "Invalid TOML escape sequence on line ", line_number);

      const char escaped = text[++i];
      switch (escaped) {
      case 'b':
        result.push_back('\b');
        break;
      case 'f':
        result.push_back('\f');
        break;
      case 'n':
        result.push_back('\n');
        break;
      case 'r':
        result.push_back('\r');
        break;
      case 't':
        result.push_back('\t');
        break;
      case '"':
        result.push_back('"');
        break;
      case '\\':
        result.push_back('\\');
        break;
      default:
        ASTRA_EXCEPTION("Unsupported TOML escape sequence on line ",
                        line_number);
      }

      continue;
    }

    if (ch == '"') {
      closed = true;
      ++i;
      break;
    }

    result.push_back(ch);
  }

  ASTRA_ENSURE(!closed, "Unterminated TOML string on line ", line_number);
  return {std::move(result), i};
}

bool parse_int_strict(std::string_view text, int &value) {
  if (text.empty()) {
    return false;
  }

  const char *begin = text.data();
  const char *end = text.data() + text.size();
  auto result = std::from_chars(begin, end, value);
  return result.ec == std::errc{} && result.ptr == end;
}

bool parse_float_strict(std::string_view text, float &value) {
  if (text.empty()) {
    return false;
  }

  if (text.find_first_of(".eE") == std::string_view::npos) {
    return false;
  }

  const char *begin = text.data();
  char *end = nullptr;
  value = std::strtof(begin, &end);
  return end == begin + text.size();
}

std::vector<std::string>
parse_key_path(std::string_view text, size_t line_number, bool allow_dotted) {
  std::vector<std::string> segments;
  size_t position = 0;

  while (position < text.size()) {
    while (position < text.size() &&
           std::isspace(static_cast<unsigned char>(text[position]))) {
      ++position;
    }

    ASTRA_ENSURE(position >= text.size(), "TOML key is empty on line ",
                 line_number);

    if (text[position] == '"' || text[position] == '\'') {
      auto [segment, next] =
          parse_quoted_string_at(text, position, line_number);
      segments.push_back(std::move(segment));
      position = next;
    } else {
      const size_t start = position;
      while (position < text.size() && text[position] != '.' &&
             !std::isspace(static_cast<unsigned char>(text[position]))) {
        ++position;
      }

      auto segment = trim_copy(text.substr(start, position - start));
      ASTRA_ENSURE(segment.empty(), "TOML key is empty on line ", line_number);
      segments.push_back(std::move(segment));
    }

    while (position < text.size() &&
           std::isspace(static_cast<unsigned char>(text[position]))) {
      ++position;
    }

    if (position >= text.size()) {
      break;
    }

    ASTRA_ENSURE(text[position] != '.',
                 allow_dotted ? "Invalid TOML dotted key on line "
                              : "Dotted TOML keys are not supported on line ",
                 line_number);
    ASTRA_ENSURE(!allow_dotted, "Dotted TOML keys are not supported on line ",
                 line_number);

    ++position;
  }

  ASTRA_ENSURE(segments.empty(), "TOML key is empty on line ", line_number);
  return segments;
}

std::optional<size_t> find_assignment_separator(std::string_view text) {
  bool in_single = false;
  bool in_double = false;

  for (size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];

    if (in_single) {
      if (ch == '\'') {
        in_single = false;
      }
      continue;
    }

    if (in_double) {
      if (ch == '\\' && i + 1 < text.size()) {
        ++i;
        continue;
      }
      if (ch == '"') {
        in_double = false;
      }
      continue;
    }

    if (ch == '\'') {
      in_single = true;
      continue;
    }

    if (ch == '"') {
      in_double = true;
      continue;
    }

    if (ch == '=') {
      return i;
    }
  }

  return std::nullopt;
}

Node parse_inline_value(std::string_view value, size_t line_number);

Node parse_array_value(std::string_view value, size_t line_number) {
  ASTRA_ENSURE(value.size() < 2 || value.front() != '[' || value.back() != ']',
               "Invalid TOML array on line ", line_number);

  Node array = make_node(NodeKind::Array);
  std::string_view inner = value.substr(1, value.size() - 2);
  size_t position = 0;

  while (position < inner.size()) {
    while (position < inner.size() &&
           std::isspace(static_cast<unsigned char>(inner[position]))) {
      ++position;
    }

    if (position >= inner.size()) {
      break;
    }

    const size_t start = position;
    bool in_single = false;
    bool in_double = false;
    int bracket_depth = 0;

    while (position < inner.size()) {
      const char ch = inner[position];

      if (in_single) {
        if (ch == '\'') {
          in_single = false;
        }
        ++position;
        continue;
      }

      if (in_double) {
        if (ch == '\\' && position + 1 < inner.size()) {
          position += 2;
          continue;
        }
        if (ch == '"') {
          in_double = false;
        }
        ++position;
        continue;
      }

      if (ch == '\'') {
        in_single = true;
        ++position;
        continue;
      }

      if (ch == '"') {
        in_double = true;
        ++position;
        continue;
      }

      if (ch == '[') {
        ++bracket_depth;
        ++position;
        continue;
      }

      if (ch == ']') {
        if (bracket_depth == 0) {
          break;
        }
        --bracket_depth;
        ++position;
        continue;
      }

      if (ch == ',' && bracket_depth == 0) {
        break;
      }

      ++position;
    }

    const auto token = trim_copy(inner.substr(start, position - start));
    ASTRA_ENSURE(token.empty(), "TOML array element is empty on line ",
                 line_number);

    Node element = parse_inline_value(token, line_number);
    ASTRA_ENSURE(element.kind == NodeKind::Table ||
                     element.kind == NodeKind::Array,
                 "Only scalar TOML arrays are supported on line ",
                 line_number);
    array.array_items.push_back(std::make_unique<Node>(std::move(element)));

    while (position < inner.size() &&
           std::isspace(static_cast<unsigned char>(inner[position]))) {
      ++position;
    }

    if (position < inner.size()) {
      ASTRA_ENSURE(inner[position] != ',', "Invalid TOML array syntax on line ",
                   line_number);
      ++position;
    }
  }

  return array;
}

Node parse_inline_value(std::string_view value, size_t line_number) {
  const auto trimmed = trim_copy(value);
  ASTRA_ENSURE(trimmed.empty(), "TOML value is empty on line ", line_number);

  if (trimmed.front() == '"' || trimmed.front() == '\'') {
    auto [string_value, next] = parse_quoted_string_at(trimmed, 0, line_number);
    ASTRA_ENSURE(trim_copy(trimmed.substr(next)).size() != 0,
                 "Unexpected trailing content after TOML string on line ",
                 line_number);

    Node node = make_node(NodeKind::String);
    node.string_value = std::move(string_value);
    return node;
  }

  if (trimmed.front() == '[') {
    return parse_array_value(trimmed, line_number);
  }

  ASTRA_ENSURE(trimmed.front() == '{',
               "Inline TOML tables are not supported on line ", line_number);

  if (trimmed == "true" || trimmed == "false") {
    Node node = make_node(NodeKind::Bool);
    node.bool_value = trimmed == "true";
    return node;
  }

  int int_value = 0;
  if (parse_int_strict(trimmed, int_value)) {
    Node node = make_node(NodeKind::Int);
    node.int_value = int_value;
    return node;
  }

  float float_value = 0.0f;
  if (parse_float_strict(trimmed, float_value)) {
    Node node = make_node(NodeKind::Float);
    node.float_value = float_value;
    return node;
  }

  ASTRA_EXCEPTION("Unsupported TOML value on line ", line_number);
}

Node &resolve_table_path(Node &root, const std::vector<std::string> &path) {
  ASTRA_ENSURE(path.empty(), "TOML table path cannot be empty");

  Node *current = &root;
  for (const auto &segment : path) {
    Node &child = ensure_table_child(*current, segment);
    if (child.kind == NodeKind::Undefined) {
      reset_node(child, NodeKind::Table);
    }

    ASTRA_ENSURE(child.kind != NodeKind::Table,
                 "TOML table path conflicts with a non-table value");
    current = &child;
  }

  return *current;
}

Node &append_table_to_array(Node &root, const std::vector<std::string> &path) {
  ASTRA_ENSURE(path.empty(), "TOML array-of-tables path cannot be empty");

  Node *current = &root;
  for (size_t i = 0; i + 1 < path.size(); ++i) {
    Node &child = ensure_table_child(*current, path[i]);
    if (child.kind == NodeKind::Undefined) {
      reset_node(child, NodeKind::Table);
    }

    ASTRA_ENSURE(child.kind != NodeKind::Table,
                 "TOML array-of-tables path conflicts with a non-table value");
    current = &child;
  }

  Node &array = ensure_table_child(*current, path.back());
  if (array.kind == NodeKind::Undefined) {
    reset_node(array, NodeKind::Array);
  }

  ASTRA_ENSURE(array.kind != NodeKind::Array,
               "TOML array-of-tables path conflicts with a non-array value");

  for (const auto &item : array.array_items) {
    ASTRA_ENSURE(item->kind != NodeKind::Table,
                 "TOML array-of-tables path conflicts with a scalar array");
  }

  array.array_items.push_back(std::make_unique<Node>(make_node(NodeKind::Table)));
  return *array.array_items.back();
}

std::string escape_basic_string(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 2);

  for (const char ch : value) {
    switch (ch) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\b':
      escaped += "\\b";
      break;
    case '\f':
      escaped += "\\f";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped.push_back(ch);
      break;
    }
  }

  return escaped;
}

bool is_bare_key(std::string_view key) {
  if (key.empty()) {
    return false;
  }

  for (const char ch : key) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (!std::isalnum(uch) && ch != '_' && ch != '-') {
      return false;
    }
  }

  return true;
}

std::string emit_key_segment(std::string_view key) {
  if (is_bare_key(key)) {
    return std::string(key);
  }

  return "\"" + escape_basic_string(key) + "\"";
}

std::string emit_header_path(const std::vector<std::string> &path) {
  std::string output;

  for (size_t i = 0; i < path.size(); ++i) {
    if (i != 0) {
      output.push_back('.');
    }

    output += emit_key_segment(path[i]);
  }

  return output;
}

std::string emit_scalar(const Node &node) {
  switch (node.kind) {
  case NodeKind::String:
    return "\"" + escape_basic_string(node.string_value) + "\"";
  case NodeKind::Int:
    return std::to_string(node.int_value);
  case NodeKind::Float:
    return std::to_string(node.float_value);
  case NodeKind::Bool:
    return node.bool_value ? "true" : "false";
  default:
    ASTRA_EXCEPTION("Unsupported TOML scalar emission");
  }
}

ArrayCategory classify_array(const Node &array) {
  ASTRA_ENSURE(array.kind != NodeKind::Array, "TOML node is not an array");

  bool saw_table = false;
  bool saw_scalar = false;

  for (const auto &item : array.array_items) {
    switch (item->kind) {
    case NodeKind::String:
    case NodeKind::Int:
    case NodeKind::Float:
    case NodeKind::Bool:
      saw_scalar = true;
      break;
    case NodeKind::Table:
      saw_table = true;
      break;
    case NodeKind::Undefined:
    case NodeKind::Array:
    default:
      return ArrayCategory::Unsupported;
    }

    if (saw_scalar && saw_table) {
      return ArrayCategory::Unsupported;
    }
  }

  if (saw_table) {
    return ArrayCategory::Table;
  }

  return ArrayCategory::Scalar;
}

std::string emit_inline_value(const Node &node) {
  if (node.kind == NodeKind::Array) {
    ASTRA_ENSURE(classify_array(node) != ArrayCategory::Scalar,
                 "Only scalar TOML arrays can be emitted inline");

    std::string output = "[";
    for (size_t i = 0; i < node.array_items.size(); ++i) {
      if (i != 0) {
        output += ", ";
      }

      output += emit_scalar(*node.array_items[i]);
    }
    output += "]";
    return output;
  }

  ASTRA_ENSURE(node.kind == NodeKind::Table || node.kind == NodeKind::Undefined,
               "TOML table values cannot be emitted inline");
  return emit_scalar(node);
}

void ensure_blank_line_before_section(std::string &output) {
  if (output.empty()) {
    return;
  }

  if (output.size() >= 2 &&
      output[output.size() - 1] == '\n' &&
      output[output.size() - 2] == '\n') {
    return;
  }

  if (!output.empty() && output.back() == '\n') {
    output.push_back('\n');
    return;
  }

  output += "\n\n";
}

void emit_table(const Node &table, const std::vector<std::string> &path,
                std::string &output);

void emit_array_of_tables(const Node &array, std::vector<std::string> path,
                          std::string &output) {
  ASTRA_ENSURE(classify_array(array) != ArrayCategory::Table,
               "Only TOML arrays of tables can use array-of-table emission");

  for (const auto &item : array.array_items) {
    ensure_blank_line_before_section(output);
    output += "[[" + emit_header_path(path) + "]]\n";

    for (const auto &entry : item->table_items) {
      const Node &value = *entry.value;
      ASTRA_ENSURE(value.kind == NodeKind::Table ||
                       (value.kind == NodeKind::Array &&
                        classify_array(value) == ArrayCategory::Table),
                   "Nested tables inside TOML arrays of tables are not "
                   "supported");

      if (value.kind == NodeKind::Undefined) {
        continue;
      }

      output += emit_key_segment(entry.key);
      output += " = ";
      output += emit_inline_value(value);
      output.push_back('\n');
    }
  }
}

void emit_table(const Node &table, const std::vector<std::string> &path,
                std::string &output) {
  ASTRA_ENSURE(table.kind != NodeKind::Table,
               "Only TOML tables can be emitted as tables");

  if (!path.empty()) {
    ensure_blank_line_before_section(output);
    output += "[" + emit_header_path(path) + "]\n";
  }

  for (const auto &entry : table.table_items) {
    const Node &value = *entry.value;
    if (value.kind == NodeKind::Undefined || value.kind == NodeKind::Table) {
      continue;
    }

    if (value.kind == NodeKind::Array &&
        classify_array(value) == ArrayCategory::Table) {
      continue;
    }

    output += emit_key_segment(entry.key);
    output += " = ";
    output += emit_inline_value(value);
    output.push_back('\n');
  }

  for (const auto &entry : table.table_items) {
    const Node &value = *entry.value;
    if (value.kind != NodeKind::Table) {
      continue;
    }

    auto child_path = path;
    child_path.push_back(entry.key);
    emit_table(value, child_path, output);
  }

  for (const auto &entry : table.table_items) {
    const Node &value = *entry.value;
    if (value.kind != NodeKind::Array) {
      continue;
    }

    auto child_path = path;
    child_path.push_back(entry.key);

    const auto category = classify_array(value);
    if (category == ArrayCategory::Table) {
      emit_array_of_tables(value, std::move(child_path), output);
    } else if (category == ArrayCategory::Unsupported) {
      ASTRA_EXCEPTION("Unsupported TOML array emission");
    }
  }
}

int parse_string_to_int(std::string_view value) {
  int parsed = 0;
  if (parse_int_strict(value, parsed)) {
    return parsed;
  }

  return 0;
}

float parse_string_to_float(std::string_view value) {
  float parsed = 0.0f;
  if (parse_float_strict(value, parsed)) {
    return parsed;
  }

  int integer = 0;
  if (parse_int_strict(value, integer)) {
    return static_cast<float>(integer);
  }

  return 0.0f;
}

bool parse_string_to_bool(std::string_view value) {
  return value == "true";
}

} // namespace

TomlSerializationContext::TomlSerializationContext()
    : m_root(std::make_shared<Node>(make_node(NodeKind::Table))),
      m_current(m_root.get()) {}

TomlSerializationContext::TomlSerializationContext(Scope<StreamBuffer> buffer)
    : TomlSerializationContext() {
  from_buffer(std::move(buffer));
}

TomlSerializationContext::TomlSerializationContext(std::shared_ptr<Node> root,
                                                   Node *current)
    : m_root(std::move(root)), m_current(current) {}

size_t TomlSerializationContext::node_size(const Node *node) {
  return current_size(node);
}

size_t TomlSerializationContext::root_size() { return node_size(m_root.get()); }

size_t TomlSerializationContext::size() { return node_size(m_current); }

ContextProxy TomlSerializationContext::operator[](const SerializableKey &key) {
  auto parsed_key = std::visit(
      [](auto &&value) -> std::variant<int, std::string> {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, int>) {
          return value;
        } else {
          return convert_key_to_string(value);
        }
      },
      key);

  if (std::holds_alternative<std::string>(parsed_key)) {
    const auto &str_key = std::get<std::string>(parsed_key);
    auto *child = &ensure_table_child(*m_current, str_key);

    return ContextProxy(
        Scope<SerializationContext>(
            new TomlSerializationContext(m_root, child)),
        str_key);
  }

  const int index = std::get<int>(parsed_key);
  auto *child = &ensure_array_child(*m_current, index);

  return ContextProxy(
      Scope<SerializationContext>(new TomlSerializationContext(m_root, child)),
      std::to_string(index));
}

void TomlSerializationContext::set_value(const SerializableValue &value) {
  ASTRA_ENSURE(m_current == nullptr, "TOML context has no current node");

  if (std::holds_alternative<int>(value)) {
    reset_node(*m_current, NodeKind::Int);
    m_current->int_value = std::get<int>(value);
  } else if (std::holds_alternative<float>(value)) {
    reset_node(*m_current, NodeKind::Float);
    m_current->float_value = std::get<float>(value);
  } else if (std::holds_alternative<std::string>(value)) {
    reset_node(*m_current, NodeKind::String);
    m_current->string_value = std::get<std::string>(value);
  } else if (std::holds_alternative<bool>(value)) {
    reset_node(*m_current, NodeKind::Bool);
    m_current->bool_value = std::get<bool>(value);
  }
}

void TomlSerializationContext::set_value(Ref<SerializationContext> ctx) {
  auto toml_ctx = static_cast<TomlSerializationContext *>(ctx.get());
  ASTRA_ENSURE(toml_ctx == nullptr || toml_ctx->m_current == nullptr,
               "Invalid TOML serialization context");

  *m_current = clone_node(*toml_ctx->m_current);
}

ElasticArena::Block *TomlSerializationContext::to_buffer(ElasticArena &arena) {
  std::string toml_string;
  emit_table(*m_root, {}, toml_string);

  if (!toml_string.empty() && toml_string.back() == '\n') {
    toml_string.pop_back();
  }

  auto block = arena.allocate(toml_string.size());
  if (!toml_string.empty()) {
    std::memcpy(block->data, toml_string.data(), toml_string.size());
  }

  return block;
}

void TomlSerializationContext::from_buffer(Scope<StreamBuffer> buffer) {
  *m_root = make_node(NodeKind::Table);
  m_current = m_root.get();

  const std::string content(buffer->data(), buffer->size());
  const auto lines = preprocess_toml(content);

  Node *current_table = m_root.get();

  for (const auto &line : lines) {
    const auto &text = line.content;

    if (text.size() >= 4 && text.starts_with("[[") && text.ends_with("]]")) {
      const auto path = parse_key_path(
          trim_copy(std::string_view(text).substr(2, text.size() - 4)),
          line.number, true);
      current_table = &append_table_to_array(*m_root, path);
      continue;
    }

    if (text.size() >= 2 && text.front() == '[' && text.back() == ']') {
      const auto path = parse_key_path(
          trim_copy(std::string_view(text).substr(1, text.size() - 2)),
          line.number, true);
      current_table = &resolve_table_path(*m_root, path);
      continue;
    }

    const auto separator = find_assignment_separator(text);
    ASTRA_ENSURE(!separator.has_value(), "Invalid TOML assignment on line ",
                 line.number);

    const auto key_segments =
        parse_key_path(trim_copy(std::string_view(text).substr(0, *separator)),
                       line.number, false);
    ASTRA_ENSURE(key_segments.size() != 1,
                 "Dotted TOML keys are not supported on line ", line.number);

    auto &value_node = ensure_table_child(*current_table, key_segments.front());
    value_node =
        parse_inline_value(std::string_view(text).substr(*separator + 1),
                           line.number);
  }
}

std::string TomlSerializationContext::as_string() {
  ASTRA_ENSURE(m_current == nullptr, "TOML context has no current node");

  switch (m_current->kind) {
  case NodeKind::String:
    return m_current->string_value;
  case NodeKind::Int:
    return std::to_string(m_current->int_value);
  case NodeKind::Float:
    return std::to_string(m_current->float_value);
  case NodeKind::Bool:
    return m_current->bool_value ? "true" : "false";
  default:
    return "";
  }
}

int TomlSerializationContext::as_int() {
  ASTRA_ENSURE(m_current == nullptr, "TOML context has no current node");

  switch (m_current->kind) {
  case NodeKind::Int:
    return m_current->int_value;
  case NodeKind::Float:
    return static_cast<int>(m_current->float_value);
  case NodeKind::Bool:
    return m_current->bool_value ? 1 : 0;
  case NodeKind::String:
    return parse_string_to_int(m_current->string_value);
  default:
    return 0;
  }
}

float TomlSerializationContext::as_float() {
  ASTRA_ENSURE(m_current == nullptr, "TOML context has no current node");

  switch (m_current->kind) {
  case NodeKind::Float:
    return m_current->float_value;
  case NodeKind::Int:
    return static_cast<float>(m_current->int_value);
  case NodeKind::Bool:
    return m_current->bool_value ? 1.0f : 0.0f;
  case NodeKind::String:
    return parse_string_to_float(m_current->string_value);
  default:
    return 0.0f;
  }
}

bool TomlSerializationContext::as_bool() {
  ASTRA_ENSURE(m_current == nullptr, "TOML context has no current node");

  switch (m_current->kind) {
  case NodeKind::Bool:
    return m_current->bool_value;
  case NodeKind::Int:
    return m_current->int_value != 0;
  case NodeKind::Float:
    return m_current->float_value != 0.0f;
  case NodeKind::String:
    return parse_string_to_bool(m_current->string_value);
  default:
    return false;
  }
}

std::vector<std::any> TomlSerializationContext::as_array() {
  std::vector<std::any> items;

  if (m_current == nullptr || m_current->kind != NodeKind::Array) {
    return items;
  }

  items.reserve(m_current->array_items.size());
  for (const auto &item : m_current->array_items) {
    items.push_back(item.get());
  }

  return items;
}

SerializationTypeKind TomlSerializationContext::kind() {
  if (m_current == nullptr) {
    return SerializationTypeKind::Unknown;
  }

  switch (m_current->kind) {
  case NodeKind::String:
    return SerializationTypeKind::String;
  case NodeKind::Int:
    return SerializationTypeKind::Int;
  case NodeKind::Float:
    return SerializationTypeKind::Float;
  case NodeKind::Bool:
    return SerializationTypeKind::Bool;
  case NodeKind::Table:
    return SerializationTypeKind::Object;
  case NodeKind::Array:
    return SerializationTypeKind::Array;
  case NodeKind::Undefined:
  default:
    return SerializationTypeKind::Unknown;
  }
}

bool TomlSerializationContext::is_string() {
  return m_current != nullptr && m_current->kind == NodeKind::String;
}

bool TomlSerializationContext::is_int() {
  return m_current != nullptr && m_current->kind == NodeKind::Int;
}

bool TomlSerializationContext::is_float() {
  return m_current != nullptr && m_current->kind == NodeKind::Float;
}

bool TomlSerializationContext::is_bool() {
  return m_current != nullptr && m_current->kind == NodeKind::Bool;
}

bool TomlSerializationContext::is_array() {
  return m_current != nullptr && m_current->kind == NodeKind::Array;
}

bool TomlSerializationContext::is_object() {
  return m_current != nullptr && m_current->kind == NodeKind::Table;
}

} // namespace astralix
