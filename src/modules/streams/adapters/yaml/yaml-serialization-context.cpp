#include "adapters/yaml/yaml-serialization-context.hpp"
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
#include <variant>
#include <vector>

namespace astralix {

namespace {

using Node = yaml_detail::Node;
using NodeKind = yaml_detail::NodeKind;

struct Line {
  int indent = 0;
  size_t number = 0;
  std::string content;
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
  node.map_items.clear();
  node.sequence_items.clear();
}

Node clone_node(const Node &node) {
  Node clone = make_node(node.kind);
  clone.string_value = node.string_value;
  clone.int_value = node.int_value;
  clone.float_value = node.float_value;
  clone.bool_value = node.bool_value;

  for (const auto &entry : node.map_items) {
    clone.map_items.push_back(
        {entry.key, std::make_unique<Node>(clone_node(*entry.value))});
  }

  for (const auto &item : node.sequence_items) {
    clone.sequence_items.push_back(std::make_unique<Node>(clone_node(*item)));
  }

  return clone;
}

Node *find_map_child(Node &node, std::string_view key) {
  for (auto &entry : node.map_items) {
    if (entry.key == key) {
      return entry.value.get();
    }
  }

  return nullptr;
}

Node &ensure_map_child(Node &node, const std::string &key) {
  if (node.kind == NodeKind::Undefined) {
    reset_node(node, NodeKind::Map);
  }

  ASTRA_ENSURE(node.kind != NodeKind::Map,
               "YAML node is not a map for string indexing");

  if (auto *existing = find_map_child(node, key)) {
    return *existing;
  }

  node.map_items.push_back({key, std::make_unique<Node>(make_node())});
  return *node.map_items.back().value;
}

Node &ensure_sequence_child(Node &node, int index) {
  ASTRA_ENSURE(index < 0, "YAML sequence index cannot be negative");

  if (node.kind == NodeKind::Undefined) {
    reset_node(node, NodeKind::Sequence);
  }

  ASTRA_ENSURE(node.kind != NodeKind::Sequence,
               "YAML node is not a sequence for integer indexing");

  while (static_cast<int>(node.sequence_items.size()) <= index) {
    node.sequence_items.push_back(std::make_unique<Node>(make_node()));
  }

  return *node.sequence_items[static_cast<size_t>(index)];
}

size_t current_size(const Node *node) {
  if (node == nullptr) {
    return 0;
  }

  switch (node->kind) {
  case NodeKind::Map:
    return node->map_items.size();
  case NodeKind::Sequence:
    return node->sequence_items.size();
  default:
    return 0;
  }
}

std::string trim_copy(std::string_view value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    start++;
  }

  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    end--;
  }

  return std::string(value.substr(start, end - start));
}

bool is_sequence_line(std::string_view content) {
  return content == "-" || content.starts_with("- ");
}

std::string strip_comment(std::string_view value) {
  bool in_single = false;
  bool in_double = false;

  for (size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];

    if (in_single) {
      if (ch == '\'' && i + 1 < value.size() && value[i + 1] == '\'') {
        ++i;
        continue;
      }

      if (ch == '\'') {
        in_single = false;
      }

      continue;
    }

    if (in_double) {
      if (ch == '\\' && i + 1 < value.size()) {
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

    if (ch == '#' &&
        (i == 0 ||
         std::isspace(static_cast<unsigned char>(value[i - 1])))) {
      return trim_copy(value.substr(0, i));
    }
  }

  return trim_copy(value);
}

std::optional<size_t> find_mapping_separator(std::string_view value) {
  bool in_single = false;
  bool in_double = false;

  for (size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];

    if (in_single) {
      if (ch == '\'' && i + 1 < value.size() && value[i + 1] == '\'') {
        ++i;
        continue;
      }

      if (ch == '\'') {
        in_single = false;
      }

      continue;
    }

    if (in_double) {
      if (ch == '\\' && i + 1 < value.size()) {
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

    if (ch == ':' &&
        (i + 1 == value.size() ||
         std::isspace(static_cast<unsigned char>(value[i + 1])))) {
      return i;
    }
  }

  return std::nullopt;
}

std::string parse_quoted_string(std::string_view text, size_t line_number) {
  ASTRA_ENSURE(text.size() < 2, "Invalid quoted YAML scalar on line ",
               line_number);

  const char quote = text.front();
  ASTRA_ENSURE(quote != '"' && quote != '\'',
               "Quoted YAML scalar must start with a quote on line ",
               line_number);

  std::string result;
  result.reserve(text.size());
  bool closed = false;

  size_t i = 1;
  for (; i < text.size(); ++i) {
    const char ch = text[i];

    if (quote == '\'') {
      if (ch == '\'' && i + 1 < text.size() && text[i + 1] == '\'') {
        result.push_back('\'');
        ++i;
        continue;
      }

      if (ch == '\'') {
        ++i;
        closed = true;
        break;
      }

      result.push_back(ch);
      continue;
    }

    if (ch == '\\') {
      ASTRA_ENSURE(i + 1 >= text.size(),
                   "Invalid escape sequence in YAML string on line ",
                   line_number);

      const char escaped = text[++i];
      switch (escaped) {
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
        result.push_back(escaped);
        break;
      }

      continue;
    }

    if (ch == '"') {
      ++i;
      closed = true;
      break;
    }

    result.push_back(ch);
  }

  ASTRA_ENSURE(!closed,
               "Unterminated quoted YAML scalar on line ", line_number);

  ASTRA_ENSURE(trim_copy(text.substr(i)).size() != 0,
               "Unexpected trailing content after YAML scalar on line ",
               line_number);

  return result;
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

Node parse_inline_value(std::string_view value, size_t line_number) {
  const auto trimmed = trim_copy(value);

  if (trimmed == "{}") {
    return make_node(NodeKind::Map);
  }

  if (trimmed == "[]") {
    return make_node(NodeKind::Sequence);
  }

  ASTRA_ENSURE((!trimmed.empty() &&
                (trimmed.front() == '{' || trimmed.front() == '[')),
               "Unsupported YAML flow collection on line ", line_number);

  if (!trimmed.empty() && (trimmed.front() == '"' || trimmed.front() == '\'')) {
    Node node = make_node(NodeKind::String);
    node.string_value = parse_quoted_string(trimmed, line_number);
    return node;
  }

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

  Node node = make_node(NodeKind::String);
  node.string_value = trimmed;
  return node;
}

std::string parse_mapping_key(std::string_view value, size_t line_number) {
  const auto trimmed = trim_copy(value);

  ASTRA_ENSURE(trimmed.empty(), "YAML map key cannot be empty on line ",
               line_number);

  if (trimmed.front() == '"' || trimmed.front() == '\'') {
    return parse_quoted_string(trimmed, line_number);
  }

  return trimmed;
}

void assign_map_value(Node &map, const std::string &key, Node value) {
  ASTRA_ENSURE(map.kind != NodeKind::Map,
               "Internal YAML parser expected a map node");

  if (auto *existing = find_map_child(map, key)) {
    *existing = std::move(value);
    return;
  }

  map.map_items.push_back({key, std::make_unique<Node>(std::move(value))});
}

Node parse_block(const std::vector<Line> &lines, size_t &index, int indent);

void parse_map_entries(const std::vector<Line> &lines, size_t &index, int indent,
                       Node &map) {
  ASTRA_ENSURE(map.kind != NodeKind::Map,
               "Internal YAML parser expected a map node");

  while (index < lines.size()) {
    const auto &line = lines[index];

    if (line.indent < indent) {
      return;
    }

    ASTRA_ENSURE(line.indent > indent, "Unexpected YAML indentation on line ",
                 line.number);
    ASTRA_ENSURE(is_sequence_line(line.content),
                 "Unexpected YAML sequence entry in map on line ",
                 line.number);

    const auto separator = find_mapping_separator(line.content);
    ASTRA_ENSURE(!separator.has_value(),
                 "Expected YAML mapping entry on line ", line.number);

    const auto key = parse_mapping_key(line.content.substr(0, *separator),
                                       line.number);
    const auto remainder =
        trim_copy(line.content.substr(*separator + 1));

    ++index;

    if (!remainder.empty()) {
      assign_map_value(map, key, parse_inline_value(remainder, line.number));
      continue;
    }

    Node value = make_node();
    if (index < lines.size() && lines[index].indent > indent) {
      value = parse_block(lines, index, lines[index].indent);
    }

    assign_map_value(map, key, std::move(value));
  }
}

Node parse_sequence_entries(const std::vector<Line> &lines, size_t &index,
                            int indent) {
  Node sequence = make_node(NodeKind::Sequence);

  while (index < lines.size()) {
    const auto &line = lines[index];

    if (line.indent < indent) {
      return sequence;
    }

    ASTRA_ENSURE(line.indent > indent, "Unexpected YAML indentation on line ",
                 line.number);

    if (!is_sequence_line(line.content)) {
      return sequence;
    }

    const std::string remainder =
        line.content == "-" ? "" : trim_copy(line.content.substr(2));

    ++index;

    if (remainder.empty()) {
      Node item = make_node();
      if (index < lines.size() && lines[index].indent > indent) {
        item = parse_block(lines, index, lines[index].indent);
      }

      sequence.sequence_items.push_back(
          std::make_unique<Node>(std::move(item)));
      continue;
    }

    if (const auto separator = find_mapping_separator(remainder);
        separator.has_value()) {
      Node item = make_node(NodeKind::Map);

      const auto key =
          parse_mapping_key(std::string_view(remainder).substr(0, *separator),
                            line.number);
      const auto first_value =
          trim_copy(std::string_view(remainder).substr(*separator + 1));

      if (!first_value.empty()) {
        assign_map_value(item, key, parse_inline_value(first_value, line.number));
      } else {
        Node value = make_node();
        if (index < lines.size() && lines[index].indent > indent) {
          value = parse_block(lines, index, lines[index].indent);
        }
        assign_map_value(item, key, std::move(value));
      }

      if (index < lines.size() && lines[index].indent > indent) {
        parse_map_entries(lines, index, lines[index].indent, item);
      }

      sequence.sequence_items.push_back(
          std::make_unique<Node>(std::move(item)));
      continue;
    }

    sequence.sequence_items.push_back(
        std::make_unique<Node>(parse_inline_value(remainder, line.number)));
  }

  return sequence;
}

Node parse_block(const std::vector<Line> &lines, size_t &index, int indent) {
  ASTRA_ENSURE(index >= lines.size(), "Unexpected end of YAML document");
  ASTRA_ENSURE(lines[index].indent != indent,
               "Unexpected YAML indentation on line ", lines[index].number);

  if (is_sequence_line(lines[index].content)) {
    return parse_sequence_entries(lines, index, indent);
  }

  Node map = make_node(NodeKind::Map);
  parse_map_entries(lines, index, indent, map);
  return map;
}

std::vector<Line> preprocess_yaml(std::string_view content) {
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
    std::string_view raw_line = has_newline
                                    ? content.substr(position, line_end - position)
                                    : content.substr(position);

    if (!raw_line.empty() && raw_line.back() == '\r') {
      raw_line.remove_suffix(1);
    }

    size_t indent = 0;
    while (indent < raw_line.size() && raw_line[indent] == ' ') {
      ++indent;
    }

    ASTRA_ENSURE(indent < raw_line.size() && raw_line[indent] == '\t',
                 "Tabs are not supported in YAML indentation on line ",
                 line_number);

    const auto content_without_comments = strip_comment(raw_line.substr(indent));
    if (!content_without_comments.empty()) {
      lines.push_back(
          {.indent = static_cast<int>(indent),
           .number = line_number,
           .content = content_without_comments});
    }

    if (!has_newline) {
      break;
    }

    position = line_end + 1;
    ++line_number;
  }

  return lines;
}

std::string escape_double_quoted(std::string_view value) {
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

std::string emit_scalar(const Node &node) {
  switch (node.kind) {
  case NodeKind::String:
    return "\"" + escape_double_quoted(node.string_value) + "\"";
  case NodeKind::Int:
    return std::to_string(node.int_value);
  case NodeKind::Float:
    return std::to_string(node.float_value);
  case NodeKind::Bool:
    return node.bool_value ? "true" : "false";
  case NodeKind::Map:
    return "{}";
  case NodeKind::Sequence:
    return "[]";
  case NodeKind::Undefined:
  default:
    return "\"\"";
  }
}

bool emits_inline(const Node &node) {
  return node.kind != NodeKind::Map && node.kind != NodeKind::Sequence ||
         current_size(&node) == 0;
}

void append_indent(std::string &output, int indent) {
  output.append(static_cast<size_t>(indent), ' ');
}

void emit_node(const Node &node, int indent, std::string &output);

void emit_map(const Node &node, int indent, std::string &output) {
  if (node.map_items.empty()) {
    output += "{}";
    return;
  }

  for (size_t i = 0; i < node.map_items.size(); ++i) {
    if (i != 0) {
      output.push_back('\n');
    }

    append_indent(output, indent);
    output += "\"";
    output += escape_double_quoted(node.map_items[i].key);
    output += "\":";

    const Node &value = *node.map_items[i].value;
    if (emits_inline(value)) {
      output.push_back(' ');
      output += emit_scalar(value);
    } else {
      output.push_back('\n');
      emit_node(value, indent + 2, output);
    }
  }
}

void emit_sequence(const Node &node, int indent, std::string &output) {
  if (node.sequence_items.empty()) {
    output += "[]";
    return;
  }

  for (size_t i = 0; i < node.sequence_items.size(); ++i) {
    if (i != 0) {
      output.push_back('\n');
    }

    append_indent(output, indent);
    output += "-";

    const Node &item = *node.sequence_items[i];
    if (emits_inline(item)) {
      output.push_back(' ');
      output += emit_scalar(item);
    } else {
      output.push_back('\n');
      emit_node(item, indent + 2, output);
    }
  }
}

void emit_node(const Node &node, int indent, std::string &output) {
  switch (node.kind) {
  case NodeKind::Map:
    emit_map(node, indent, output);
    break;
  case NodeKind::Sequence:
    emit_sequence(node, indent, output);
    break;
  default:
    append_indent(output, indent);
    output += emit_scalar(node);
    break;
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

YamlSerializationContext::YamlSerializationContext()
    : m_root(std::make_shared<Node>(make_node())), m_current(m_root.get()) {}

YamlSerializationContext::YamlSerializationContext(Scope<StreamBuffer> buffer)
    : YamlSerializationContext() {
  from_buffer(std::move(buffer));
}

YamlSerializationContext::YamlSerializationContext(std::shared_ptr<Node> root,
                                                   Node *current)
    : m_root(std::move(root)), m_current(current) {}

size_t YamlSerializationContext::node_size(const Node *node) {
  return current_size(node);
}

size_t YamlSerializationContext::root_size() { return node_size(m_root.get()); }

size_t YamlSerializationContext::size() { return node_size(m_current); }

ContextProxy YamlSerializationContext::operator[](const SerializableKey &key) {
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
    auto *child = &ensure_map_child(*m_current, str_key);

    return ContextProxy(
        Scope<SerializationContext>(
            new YamlSerializationContext(m_root, child)),
        str_key);
  }

  const int index = std::get<int>(parsed_key);
  auto *child = &ensure_sequence_child(*m_current, index);

  return ContextProxy(
      Scope<SerializationContext>(new YamlSerializationContext(m_root, child)),
      std::to_string(index));
}

void YamlSerializationContext::set_value(const SerializableValue &value) {
  ASTRA_ENSURE(m_current == nullptr, "YAML context has no current node");

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

void YamlSerializationContext::set_value(Ref<SerializationContext> ctx) {
  auto yaml_ctx = static_cast<YamlSerializationContext *>(ctx.get());
  ASTRA_ENSURE(yaml_ctx == nullptr || yaml_ctx->m_root == nullptr,
               "Invalid YAML serialization context");

  *m_current = clone_node(*yaml_ctx->m_root);
}

ElasticArena::Block *YamlSerializationContext::to_buffer(ElasticArena &arena) {
  std::string yaml_string;
  if (m_root->kind != NodeKind::Undefined) {
    emit_node(*m_root, 0, yaml_string);
  }

  auto block = arena.allocate(yaml_string.size());
  if (!yaml_string.empty()) {
    std::memcpy(block->data, yaml_string.data(), yaml_string.size());
  }

  return block;
}

void YamlSerializationContext::from_buffer(Scope<StreamBuffer> buffer) {
  const std::string content(buffer->data(), buffer->size());
  const auto lines = preprocess_yaml(content);

  *m_root = make_node();

  if (lines.empty()) {
    m_current = m_root.get();
    return;
  }

  ASTRA_ENSURE(lines.front().indent != 0,
               "Top-level YAML content must start at indentation 0");

  size_t index = 0;
  *m_root = parse_block(lines, index, 0);

  ASTRA_ENSURE(index != lines.size(),
               "Unexpected trailing YAML content on line ",
               lines[index].number);

  m_current = m_root.get();
}

std::string YamlSerializationContext::as_string() {
  ASTRA_ENSURE(m_current == nullptr, "YAML context has no current node");

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

int YamlSerializationContext::as_int() {
  ASTRA_ENSURE(m_current == nullptr, "YAML context has no current node");

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

float YamlSerializationContext::as_float() {
  ASTRA_ENSURE(m_current == nullptr, "YAML context has no current node");

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

bool YamlSerializationContext::as_bool() {
  ASTRA_ENSURE(m_current == nullptr, "YAML context has no current node");

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

std::vector<std::any> YamlSerializationContext::as_array() {
  std::vector<std::any> items;

  if (m_current == nullptr || m_current->kind != NodeKind::Sequence) {
    return items;
  }

  items.reserve(m_current->sequence_items.size());
  for (const auto &item : m_current->sequence_items) {
    items.push_back(item.get());
  }

  return items;
}

SerializationTypeKind YamlSerializationContext::kind() {
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
  case NodeKind::Map:
    return SerializationTypeKind::Object;
  case NodeKind::Sequence:
    return SerializationTypeKind::Array;
  case NodeKind::Undefined:
  default:
    return SerializationTypeKind::Unknown;
  }
}

bool YamlSerializationContext::is_string() {
  return m_current != nullptr && m_current->kind == NodeKind::String;
}

bool YamlSerializationContext::is_int() {
  return m_current != nullptr && m_current->kind == NodeKind::Int;
}

bool YamlSerializationContext::is_float() {
  return m_current != nullptr && m_current->kind == NodeKind::Float;
}

bool YamlSerializationContext::is_bool() {
  return m_current != nullptr && m_current->kind == NodeKind::Bool;
}

bool YamlSerializationContext::is_array() {
  return m_current != nullptr && m_current->kind == NodeKind::Sequence;
}

bool YamlSerializationContext::is_object() {
  return m_current != nullptr && m_current->kind == NodeKind::Map;
}

} // namespace astralix
