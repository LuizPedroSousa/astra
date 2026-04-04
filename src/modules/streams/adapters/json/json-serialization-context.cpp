#include "adapters/json/json-serialization-context.hpp"
#include "arena.hpp"
#include "assert.hpp"
#include "base.hpp"
#include "context-proxy.hpp"
#include "stream-buffer.hpp"

#include <any>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace astralix {

namespace {

using Node = json_detail::Node;
using NodeKind = json_detail::NodeKind;

Node make_node(NodeKind kind = NodeKind::Null) {
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
  node.object_items.clear();
  node.array_items.clear();
}

Node clone_node(const Node &node) {
  Node clone = make_node(node.kind);
  clone.string_value = node.string_value;
  clone.int_value = node.int_value;
  clone.float_value = node.float_value;
  clone.bool_value = node.bool_value;

  for (const auto &entry : node.object_items) {
    clone.object_items.push_back(
        {entry.key, std::make_unique<Node>(clone_node(*entry.value))}
    );
  }

  for (const auto &item : node.array_items) {
    clone.array_items.push_back(std::make_unique<Node>(clone_node(*item)));
  }

  return clone;
}

Node *find_object_child(Node &node, std::string_view key) {
  for (auto &entry : node.object_items) {
    if (entry.key == key) {
      return entry.value.get();
    }
  }

  return nullptr;
}

void assign_object_value(Node &node, const std::string &key, Node value) {
  ASTRA_ENSURE(node.kind != NodeKind::Object, "Internal JSON parser expected an object node");

  if (auto *existing = find_object_child(node, key)) {
    *existing = std::move(value);
    return;
  }

  node.object_items.push_back({key, std::make_unique<Node>(std::move(value))});
}

Node &ensure_object_child(Node &node, const std::string &key) {
  if (node.kind == NodeKind::Null) {
    reset_node(node, NodeKind::Object);
  }

  ASTRA_ENSURE(node.kind != NodeKind::Object, "JSON node is not an object for string indexing");

  if (auto *existing = find_object_child(node, key)) {
    return *existing;
  }

  node.object_items.push_back({key, std::make_unique<Node>(make_node())});
  return *node.object_items.back().value;
}

Node &ensure_array_child(Node &node, int index) {
  ASTRA_ENSURE(index < 0, "JSON array index cannot be negative");

  if (node.kind == NodeKind::Null) {
    reset_node(node, NodeKind::Array);
  }

  ASTRA_ENSURE(node.kind != NodeKind::Array, "JSON node is not an array for integer indexing");

  while (static_cast<int>(node.array_items.size()) <= index) {
    node.array_items.push_back(std::make_unique<Node>(make_node()));
  }

  return *node.array_items[static_cast<size_t>(index)];
}

size_t current_size(const Node *node) {
  if (node == nullptr) {
    return 0;
  }

  switch (node->kind) {
    case NodeKind::Object:
      return node->object_items.size();
    case NodeKind::Array:
      return node->array_items.size();
    default:
      return 0;
  }
}

std::vector<std::string> current_object_keys(const Node *node) {
  std::vector<std::string> keys;

  if (node == nullptr || node->kind != NodeKind::Object) {
    return keys;
  }

  keys.reserve(node->object_items.size());
  for (const auto &entry : node->object_items) {
    keys.push_back(entry.key);
  }

  return keys;
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

  const char *begin = text.data();
  char *end = nullptr;
  value = std::strtof(begin, &end);

  return end == begin + text.size();
}

int parse_string_to_int(std::string_view value) {
  int parsed = 0;
  if (parse_int_strict(value, parsed)) {
    return parsed;
  }

  return 0;
}

std::string_view normalize_json_input(std::string_view input) {
  constexpr std::string_view utf8_bom = "\xEF\xBB\xBF";

  if (input.substr(0, utf8_bom.size()) == utf8_bom) {
    input.remove_prefix(utf8_bom.size());
  }

  while (!input.empty() && input.back() == '\0') {
    input.remove_suffix(1);
  }

  return input;
}

float parse_string_to_float(std::string_view value) {
  float parsed = 0.0f;
  if (parse_float_strict(value, parsed)) {
    return parsed;
  }

  return 0.0f;
}

bool parse_string_to_bool(std::string_view value) { return value == "true"; }

std::string format_float(float value) {
  std::ostringstream stream;
  stream << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
  return stream.str();
}

void append_utf8(std::string &output, uint32_t code_point) {
  ASTRA_ENSURE(code_point > 0x10FFFF, "Invalid Unicode code point in JSON");
  ASTRA_ENSURE(code_point >= 0xD800 && code_point <= 0xDFFF, "Invalid surrogate code point in JSON");

  if (code_point <= 0x7F) {
    output.push_back(static_cast<char>(code_point));
    return;
  }

  if (code_point <= 0x7FF) {
    output.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
    output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    return;
  }

  if (code_point <= 0xFFFF) {
    output.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
    output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    return;
  }

  output.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
  output.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
  output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
  output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
}

char hex_digit(uint32_t value) {
  return value < 10 ? static_cast<char>('0' + value)
                    : static_cast<char>('A' + (value - 10));
}

void append_unicode_escape(std::string &output, uint32_t code_point) {
  output += "\\u";
  output.push_back(hex_digit((code_point >> 12) & 0xF));
  output.push_back(hex_digit((code_point >> 8) & 0xF));
  output.push_back(hex_digit((code_point >> 4) & 0xF));
  output.push_back(hex_digit(code_point & 0xF));
}

std::string escape_json_string(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 2);

  for (unsigned char ch : value) {
    switch (ch) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
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
        if (ch < 0x20) {
          append_unicode_escape(escaped, ch);
        } else {
          escaped.push_back(static_cast<char>(ch));
        }
        break;
    }
  }

  return escaped;
}

std::string emit_scalar(const Node &node) {
  switch (node.kind) {
    case NodeKind::Null:
      return "null";
    case NodeKind::String:
      return "\"" + escape_json_string(node.string_value) + "\"";
    case NodeKind::Int:
      return std::to_string(node.int_value);
    case NodeKind::Float:
      return format_float(node.float_value);
    case NodeKind::Bool:
      return node.bool_value ? "true" : "false";
    case NodeKind::Object:
      return "{}";
    case NodeKind::Array:
      return "[]";
    default:
      return "null";
  }
}

void append_indent(std::string &output, int indent) {
  output.append(static_cast<size_t>(indent), ' ');
}

void emit_node(const Node &node, int indent, std::string &output);

void emit_object(const Node &node, int indent, std::string &output) {
  if (node.object_items.empty()) {
    output += "{}";
    return;
  }

  output += "{\n";

  for (size_t i = 0; i < node.object_items.size(); ++i) {
    append_indent(output, indent + 2);
    output += "\"";
    output += escape_json_string(node.object_items[i].key);
    output += "\": ";
    emit_node(*node.object_items[i].value, indent + 2, output);

    if (i + 1 != node.object_items.size()) {
      output += ",";
    }

    output += "\n";
  }

  append_indent(output, indent);
  output += "}";
}

void emit_array(const Node &node, int indent, std::string &output) {
  if (node.array_items.empty()) {
    output += "[]";
    return;
  }

  output += "[\n";

  for (size_t i = 0; i < node.array_items.size(); ++i) {
    append_indent(output, indent + 2);
    emit_node(*node.array_items[i], indent + 2, output);

    if (i + 1 != node.array_items.size()) {
      output += ",";
    }

    output += "\n";
  }

  append_indent(output, indent);
  output += "]";
}

void emit_node(const Node &node, int indent, std::string &output) {
  switch (node.kind) {
    case NodeKind::Object:
      emit_object(node, indent, output);
      break;
    case NodeKind::Array:
      emit_array(node, indent, output);
      break;
    default:
      output += emit_scalar(node);
      break;
  }
}

class JsonParser {
public:
  explicit JsonParser(std::string_view input) : m_input(input) {}

  Node parse_document() {
    skip_whitespace();
    ASTRA_ENSURE(m_position >= m_input.size(), "JSON buffer is empty");

    Node root = parse_value();
    skip_whitespace();

    ASTRA_ENSURE(m_position != m_input.size(), "Unexpected trailing JSON content at position ", m_position);

    return root;
  }

private:
  void skip_whitespace() {
    while (m_position < m_input.size() &&
           std::isspace(static_cast<unsigned char>(m_input[m_position]))) {
      ++m_position;
    }
  }

  bool consume(char expected) {
    if (m_position < m_input.size() && m_input[m_position] == expected) {
      ++m_position;
      return true;
    }

    return false;
  }

  void expect_sequence(std::string_view sequence) {
    ASTRA_ENSURE(m_position + sequence.size() > m_input.size() || m_input.substr(m_position, sequence.size()) != sequence, "Expected '", sequence, "' at position ", m_position);
    m_position += sequence.size();
  }

  Node parse_value() {
    skip_whitespace();
    ASTRA_ENSURE(m_position >= m_input.size(), "Unexpected end of JSON input at position ", m_position);

    const char current = m_input[m_position];
    if (current == '{') {
      return parse_object();
    }

    if (current == '[') {
      return parse_array();
    }

    if (current == '"') {
      Node node = make_node(NodeKind::String);
      node.string_value = parse_string();
      return node;
    }

    if (current == 't') {
      expect_sequence("true");
      Node node = make_node(NodeKind::Bool);
      node.bool_value = true;
      return node;
    }

    if (current == 'f') {
      expect_sequence("false");
      Node node = make_node(NodeKind::Bool);
      node.bool_value = false;
      return node;
    }

    if (current == 'n') {
      expect_sequence("null");
      return make_node(NodeKind::Null);
    }

    if (current == '-' || std::isdigit(static_cast<unsigned char>(current))) {
      return parse_number();
    }

    ASTRA_EXCEPTION("Unexpected JSON token at position ", m_position);
  }

  Node parse_object() {
    ASTRA_ENSURE(!consume('{'), "Expected '{' at position ", m_position);

    Node object = make_node(NodeKind::Object);
    skip_whitespace();

    if (consume('}')) {
      return object;
    }

    while (true) {
      skip_whitespace();
      ASTRA_ENSURE(m_position >= m_input.size() || m_input[m_position] != '"', "Expected JSON object key at position ", m_position);

      const std::string key = parse_string();
      skip_whitespace();
      ASTRA_ENSURE(!consume(':'), "Expected ':' after JSON object key at position ", m_position);

      Node value = parse_value();
      assign_object_value(object, key, std::move(value));

      skip_whitespace();
      if (consume('}')) {
        return object;
      }

      ASTRA_ENSURE(!consume(','), "Expected ',' or '}' in JSON object at position ", m_position);
    }
  }

  Node parse_array() {
    ASTRA_ENSURE(!consume('['), "Expected '[' at position ", m_position);

    Node array = make_node(NodeKind::Array);
    skip_whitespace();

    if (consume(']')) {
      return array;
    }

    while (true) {
      array.array_items.push_back(
          std::make_unique<Node>(parse_value())
      );

      skip_whitespace();
      if (consume(']')) {
        return array;
      }

      ASTRA_ENSURE(!consume(','), "Expected ',' or ']' in JSON array at position ", m_position);
    }
  }

  int hex_to_int(char ch) const {
    if (ch >= '0' && ch <= '9') {
      return ch - '0';
    }

    if (ch >= 'a' && ch <= 'f') {
      return 10 + (ch - 'a');
    }

    if (ch >= 'A' && ch <= 'F') {
      return 10 + (ch - 'A');
    }

    return -1;
  }

  uint32_t parse_unicode_escape() {
    ASTRA_ENSURE(m_position + 4 > m_input.size(), "Incomplete JSON unicode escape at position ", m_position);

    uint32_t code_unit = 0;
    for (int i = 0; i < 4; ++i) {
      const int value = hex_to_int(m_input[m_position + static_cast<size_t>(i)]);
      ASTRA_ENSURE(value < 0, "Invalid JSON unicode escape at position ", m_position + static_cast<size_t>(i));
      code_unit = (code_unit << 4) | static_cast<uint32_t>(value);
    }

    m_position += 4;
    return code_unit;
  }

  std::string parse_string() {
    ASTRA_ENSURE(!consume('"'), "Expected JSON string at position ", m_position);

    std::string result;

    while (true) {
      ASTRA_ENSURE(m_position >= m_input.size(), "Unterminated JSON string at position ", m_position);

      const char ch = m_input[m_position++];
      if (ch == '"') {
        return result;
      }

      ASTRA_ENSURE(static_cast<unsigned char>(ch) < 0x20, "Control character in JSON string at position ", m_position - 1);

      if (ch != '\\') {
        result.push_back(ch);
        continue;
      }

      ASTRA_ENSURE(m_position >= m_input.size(), "Invalid JSON escape at position ", m_position);

      const char escaped = m_input[m_position++];
      switch (escaped) {
        case '"':
          result.push_back('"');
          break;
        case '\\':
          result.push_back('\\');
          break;
        case '/':
          result.push_back('/');
          break;
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
        case 'u': {
          uint32_t code_point = parse_unicode_escape();

          // JSON encodes code points above BMP as UTF-16 surrogate pairs.
          if (code_point >= 0xD800 && code_point <= 0xDBFF) {
            ASTRA_ENSURE(m_position + 2 > m_input.size() || m_input[m_position] != '\\' || m_input[m_position + 1] != 'u', "Missing low surrogate in JSON string at position ", m_position);

            m_position += 2;
            const uint32_t low_surrogate = parse_unicode_escape();
            ASTRA_ENSURE(low_surrogate < 0xDC00 || low_surrogate > 0xDFFF, "Invalid low surrogate in JSON string at position ", m_position - 4);

            code_point =
                0x10000 + (((code_point - 0xD800) << 10) |
                           (low_surrogate - 0xDC00));
          } else {
            ASTRA_ENSURE(code_point >= 0xDC00 && code_point <= 0xDFFF, "Unexpected low surrogate in JSON string at position ", m_position - 4);
          }

          append_utf8(result, code_point);
          break;
        }
        default:
          ASTRA_EXCEPTION("Invalid JSON escape sequence at position ", m_position - 1);
      }
    }
  }

  Node parse_number() {
    const size_t start = m_position;

    consume('-');

    ASTRA_ENSURE(m_position >= m_input.size(), "Invalid JSON number at position ", start);

    if (m_input[m_position] == '0') {
      ++m_position;
      ASTRA_ENSURE(m_position < m_input.size() && std::isdigit(static_cast<unsigned char>(m_input[m_position])), "Invalid JSON number with leading zero at position ", m_position);
    } else {
      ASTRA_ENSURE(
          !std::isdigit(static_cast<unsigned char>(m_input[m_position])),
          "Invalid JSON number at position ",
          m_position
      );

      while (m_position < m_input.size() &&
             std::isdigit(static_cast<unsigned char>(m_input[m_position]))) {
        ++m_position;
      }
    }

    bool is_float = false;

    if (consume('.')) {
      is_float = true;
      ASTRA_ENSURE(m_position >= m_input.size() || !std::isdigit(static_cast<unsigned char>(m_input[m_position])), "Invalid JSON fractional number at position ", m_position);

      while (m_position < m_input.size() &&
             std::isdigit(static_cast<unsigned char>(m_input[m_position]))) {
        ++m_position;
      }
    }

    if (m_position < m_input.size() &&
        (m_input[m_position] == 'e' || m_input[m_position] == 'E')) {
      is_float = true;
      ++m_position;

      if (m_position < m_input.size() &&
          (m_input[m_position] == '+' || m_input[m_position] == '-')) {
        ++m_position;
      }

      ASTRA_ENSURE(m_position >= m_input.size() || !std::isdigit(static_cast<unsigned char>(m_input[m_position])), "Invalid JSON exponent at position ", m_position);

      while (m_position < m_input.size() &&
             std::isdigit(static_cast<unsigned char>(m_input[m_position]))) {
        ++m_position;
      }
    }

    const std::string_view token = m_input.substr(start, m_position - start);

    if (!is_float) {
      long long int_value = 0;
      const char *begin = token.data();
      const char *end = token.data() + token.size();
      auto result = std::from_chars(begin, end, int_value);

      if (result.ec == std::errc{} && result.ptr == end &&
          int_value >= std::numeric_limits<int>::min() &&
          int_value <= std::numeric_limits<int>::max()) {
        Node node = make_node(NodeKind::Int);
        node.int_value = static_cast<int>(int_value);
        return node;
      }
    }

    const std::string token_string(token);
    char *end = nullptr;
    const double parsed = std::strtod(token_string.c_str(), &end);
    ASTRA_ENSURE(end == token_string.c_str() || static_cast<size_t>(end - token_string.c_str()) != token_string.size(), "Invalid JSON number at position ", start);

    Node node = make_node(NodeKind::Float);
    node.float_value = static_cast<float>(parsed);
    return node;
  }

  std::string_view m_input;
  size_t m_position = 0;
};

} // namespace

JsonSerializationContext::JsonSerializationContext()
    : m_root(std::make_shared<Node>(make_node())), m_current(m_root.get()) {}

JsonSerializationContext::JsonSerializationContext(Scope<StreamBuffer> buffer)
    : JsonSerializationContext() {
  from_buffer(std::move(buffer));
}

JsonSerializationContext::JsonSerializationContext(std::shared_ptr<Node> root, Node *current)
    : m_root(std::move(root)), m_current(current) {}

size_t JsonSerializationContext::node_size(const Node *node) {
  return current_size(node);
}

size_t JsonSerializationContext::root_size() { return node_size(m_root.get()); }

size_t JsonSerializationContext::size() { return node_size(m_current); }

ContextProxy
JsonSerializationContext::operator[](const SerializableKey &key) {
  auto parsed_key = std::visit(
      [](auto &&value) -> std::variant<int, std::string> {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, int>) {
          return value;
        } else {
          return convert_key_to_string(value);
        }
      },
      key
  );

  if (std::holds_alternative<std::string>(parsed_key)) {
    const auto &str_key = std::get<std::string>(parsed_key);
    auto *child = &ensure_object_child(*m_current, str_key);

    return ContextProxy(
        Scope<SerializationContext>(
            new JsonSerializationContext(m_root, child)
        ),
        str_key
    );
  }

  const int index = std::get<int>(parsed_key);
  auto *child = &ensure_array_child(*m_current, index);

  return ContextProxy(
      Scope<SerializationContext>(new JsonSerializationContext(m_root, child)),
      std::to_string(index)
  );
}

void JsonSerializationContext::set_value(const SerializableValue &value) {
  ASTRA_ENSURE(m_current == nullptr, "JSON context has no current node");

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

void JsonSerializationContext::set_value(Ref<SerializationContext> ctx) {
  auto json_ctx = static_cast<JsonSerializationContext *>(ctx.get());
  ASTRA_ENSURE(json_ctx == nullptr || json_ctx->m_root == nullptr, "Invalid JSON serialization context");

  *m_current = clone_node(*json_ctx->m_root);
}

ElasticArena::Block *JsonSerializationContext::to_buffer(ElasticArena &arena) {
  std::string json_string;
  emit_node(*m_root, 0, json_string);

  auto block = arena.allocate(json_string.size());
  if (!json_string.empty()) {
    std::memcpy(block->data, json_string.data(), json_string.size());
  }

  return block;
}

void JsonSerializationContext::from_buffer(Scope<StreamBuffer> buffer) {
  const std::string_view input(buffer->data(), buffer->size());
  const std::string content(normalize_json_input(input));
  JsonParser parser(content);

  *m_root = parser.parse_document();
  m_current = m_root.get();
}

std::string JsonSerializationContext::as_string() {
  ASTRA_ENSURE(m_current == nullptr, "JSON context has no current node");

  switch (m_current->kind) {
    case NodeKind::String:
      return m_current->string_value;
    case NodeKind::Int:
      return std::to_string(m_current->int_value);
    case NodeKind::Float:
      return format_float(m_current->float_value);
    case NodeKind::Bool:
      return m_current->bool_value ? "true" : "false";
    default:
      return "";
  }
}

int JsonSerializationContext::as_int() {
  ASTRA_ENSURE(m_current == nullptr, "JSON context has no current node");

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

float JsonSerializationContext::as_float() {
  ASTRA_ENSURE(m_current == nullptr, "JSON context has no current node");

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

bool JsonSerializationContext::as_bool() {
  ASTRA_ENSURE(m_current == nullptr, "JSON context has no current node");

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

std::vector<std::any> JsonSerializationContext::as_array() {
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

std::vector<std::string> JsonSerializationContext::object_keys() {
  return current_object_keys(m_current);
}

SerializationTypeKind JsonSerializationContext::kind() {
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
    case NodeKind::Object:
      return SerializationTypeKind::Object;
    case NodeKind::Array:
      return SerializationTypeKind::Array;
    case NodeKind::Null:
    default:
      return SerializationTypeKind::Unknown;
  }
}

bool JsonSerializationContext::is_string() {
  return m_current != nullptr && m_current->kind == NodeKind::String;
}

bool JsonSerializationContext::is_int() {
  return m_current != nullptr && m_current->kind == NodeKind::Int;
}

bool JsonSerializationContext::is_float() {
  return m_current != nullptr && m_current->kind == NodeKind::Float;
}

bool JsonSerializationContext::is_bool() {
  return m_current != nullptr && m_current->kind == NodeKind::Bool;
}

bool JsonSerializationContext::is_array() {
  return m_current != nullptr && m_current->kind == NodeKind::Array;
}

bool JsonSerializationContext::is_object() {
  return m_current != nullptr && m_current->kind == NodeKind::Object;
}

} // namespace astralix
