#include "adapters/xml/xml-serialization-context.hpp"
#include "arena.hpp"
#include "assert.hpp"
#include "base.hpp"
#include "context-proxy.hpp"
#include "stream-buffer.hpp"

#include <any>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace astralix::xml_detail {

struct Attribute {
  std::string name;
  std::string value;
};

struct Node {
  std::string name;
  std::string text;
  std::vector<Attribute> attributes;
  std::vector<std::unique_ptr<Node>> children;
};

} // namespace astralix::xml_detail

namespace astralix {

namespace {

using Attribute = xml_detail::Attribute;
using Node = xml_detail::Node;

Node make_node(std::string name = "root") {
  Node node;
  node.name = std::move(name);
  return node;
}

Node clone_node(const Node &node) {
  Node clone = make_node(node.name);
  clone.text = node.text;
  clone.attributes = node.attributes;

  for (const auto &child : node.children) {
    clone.children.push_back(std::make_unique<Node>(clone_node(*child)));
  }

  return clone;
}

Attribute *find_attribute(Node &node, std::string_view name) {
  for (auto &attribute : node.attributes) {
    if (attribute.name == name) {
      return &attribute;
    }
  }

  return nullptr;
}

const Attribute *find_attribute(const Node &node, std::string_view name) {
  for (const auto &attribute : node.attributes) {
    if (attribute.name == name) {
      return &attribute;
    }
  }

  return nullptr;
}

Attribute &ensure_attribute(Node &node, const std::string &name) {
  if (auto *attribute = find_attribute(node, name)) {
    return *attribute;
  }

  node.attributes.push_back({name, {}});
  return node.attributes.back();
}

Node *find_first_child(Node &node, std::string_view name) {
  for (auto &child : node.children) {
    if (child->name == name) {
      return child.get();
    }
  }

  return nullptr;
}

Node &ensure_named_child(Node &node, const std::string &name) {
  if (auto *child = find_first_child(node, name)) {
    return *child;
  }

  node.text.clear();
  node.children.push_back(std::make_unique<Node>(make_node(name)));
  return *node.children.back();
}

Node &ensure_index_child(Node &node, int index) {
  ASTRA_ENSURE(index < 0, "XML sequence index cannot be negative");

  node.text.clear();

  if (static_cast<size_t>(index) < node.children.size()) {
    return *node.children[static_cast<size_t>(index)];
  }

  node.children.push_back(
      std::make_unique<Node>(make_node("item" + std::to_string(index))));
  return *node.children.back();
}

size_t child_count(const Node *node) {
  return node == nullptr ? 0u : node->children.size();
}

std::string_view trim_view(std::string_view value) {
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

  return value.substr(start, end - start);
}

bool has_non_whitespace(std::string_view value) {
  for (const char ch : value) {
    if (!std::isspace(static_cast<unsigned char>(ch))) {
      return true;
    }
  }

  return false;
}

bool is_name_start_char(char ch) {
  return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_' ||
         ch == ':';
}

bool is_name_char(char ch) {
  return is_name_start_char(ch) || std::isdigit(static_cast<unsigned char>(ch)) ||
         ch == '-' || ch == '.';
}

std::string decode_entities(std::string_view input) {
  std::string decoded;
  decoded.reserve(input.size());

  for (size_t index = 0; index < input.size(); ++index) {
    if (input[index] != '&') {
      decoded.push_back(input[index]);
      continue;
    }

    const size_t end = input.find(';', index);
    ASTRA_ENSURE(end == std::string_view::npos, "Unterminated XML entity");

    const auto entity = input.substr(index, end - index + 1);

    if (entity == "&amp;") {
      decoded.push_back('&');
    } else if (entity == "&lt;") {
      decoded.push_back('<');
    } else if (entity == "&gt;") {
      decoded.push_back('>');
    } else if (entity == "&quot;") {
      decoded.push_back('"');
    } else if (entity == "&apos;") {
      decoded.push_back('\'');
    } else {
      ASTRA_EXCEPTION("Unsupported XML entity ", std::string(entity));
    }

    index = end;
  }

  return decoded;
}

void append_escaped(std::string_view value, std::string &output) {
  for (const char ch : value) {
    switch (ch) {
    case '&':
      output += "&amp;";
      break;
    case '<':
      output += "&lt;";
      break;
    case '>':
      output += "&gt;";
      break;
    case '"':
      output += "&quot;";
      break;
    case '\'':
      output += "&apos;";
      break;
    default:
      output.push_back(ch);
      break;
    }
  }
}

void emit_node(const Node &node, std::string &output) {
  output.push_back('<');
  output += node.name;

  for (const auto &attribute : node.attributes) {
    output.push_back(' ');
    output += attribute.name;
    output += "=\"";
    append_escaped(attribute.value, output);
    output.push_back('"');
  }

  if (node.children.empty() && node.text.empty()) {
    output += "/>";
    return;
  }

  output.push_back('>');

  if (node.children.empty()) {
    append_escaped(node.text, output);
  } else {
    for (const auto &child : node.children) {
      emit_node(*child, output);
    }
  }

  output += "</";
  output += node.name;
  output.push_back('>');
}

bool try_parse_int(std::string_view value, int &result) {
  const auto trimmed = trim_view(value);
  if (trimmed.empty()) {
    return false;
  }

  auto [ptr, error] =
      std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), result);
  return error == std::errc{} && ptr == trimmed.data() + trimmed.size();
}

bool try_parse_float(std::string_view value, float &result) {
  const auto trimmed = trim_view(value);
  if (trimmed.empty()) {
    return false;
  }

  std::string buffer(trimmed);
  char *end = nullptr;
  errno = 0;

  const float parsed = std::strtof(buffer.c_str(), &end);
  if (end != buffer.c_str() + buffer.size() || errno == ERANGE ||
      !std::isfinite(parsed)) {
    return false;
  }

  result = parsed;
  return true;
}

bool try_parse_bool(std::string_view value, bool &result) {
  const auto trimmed = trim_view(value);

  if (trimmed == "true") {
    result = true;
    return true;
  }

  if (trimmed == "false") {
    result = false;
    return true;
  }

  return false;
}

bool is_array_node(const Node *node) {
  if (node == nullptr || node->children.empty() || !node->attributes.empty()) {
    return false;
  }

  for (size_t index = 0; index < node->children.size(); ++index) {
    if (node->children[index]->name != "item" + std::to_string(index)) {
      return false;
    }
  }

  return true;
}

SerializationTypeKind scalar_kind(std::string_view value) {
  if (value.empty()) {
    return SerializationTypeKind::Unknown;
  }

  bool bool_value = false;
  if (try_parse_bool(value, bool_value)) {
    return SerializationTypeKind::Bool;
  }

  int int_value = 0;
  if (try_parse_int(value, int_value)) {
    return SerializationTypeKind::Int;
  }

  const auto trimmed = trim_view(value);
  float float_value = 0.0f;
  if (trimmed.find_first_of(".eE") != std::string_view::npos &&
      try_parse_float(trimmed, float_value)) {
    return SerializationTypeKind::Float;
  }

  return SerializationTypeKind::String;
}

SerializationTypeKind node_kind(const Node *node,
                                const std::optional<std::string> &attribute) {
  if (node == nullptr) {
    return SerializationTypeKind::Unknown;
  }

  if (attribute.has_value()) {
    const auto *current_attribute = find_attribute(*node, *attribute);
    return current_attribute == nullptr
               ? SerializationTypeKind::Unknown
               : scalar_kind(current_attribute->value);
  }

  if (is_array_node(node)) {
    return SerializationTypeKind::Array;
  }

  if (!node->children.empty()) {
    return SerializationTypeKind::Object;
  }

  if (!node->attributes.empty()) {
    return SerializationTypeKind::Object;
  }

  return scalar_kind(node->text);
}

const std::string *current_scalar_value(
    const Node *node, const std::optional<std::string> &attribute_name) {
  if (node == nullptr) {
    return nullptr;
  }

  if (attribute_name.has_value()) {
    const auto *attribute = find_attribute(*node, *attribute_name);
    return attribute == nullptr ? nullptr : &attribute->value;
  }

  return &node->text;
}

std::string *current_scalar_value(
    Node *node, const std::optional<std::string> &attribute_name) {
  if (node == nullptr) {
    return nullptr;
  }

  if (attribute_name.has_value()) {
    auto *attribute = find_attribute(*node, *attribute_name);
    return attribute == nullptr ? nullptr : &attribute->value;
  }

  return &node->text;
}

void copy_node_contents(Node &target, const Node &source) {
  target.text = source.text;
  target.attributes = source.attributes;
  target.children.clear();

  for (const auto &child : source.children) {
    target.children.push_back(std::make_unique<Node>(clone_node(*child)));
  }
}

class XmlParser {
public:
  explicit XmlParser(std::string_view input) : m_input(input) {}

  Node parse_document() {
    skip_misc();

    if (starts_with("<?xml")) {
      parse_declaration();
    }

    skip_misc();
    ASTRA_ENSURE(eof(), "XML document is empty");

    Node root = parse_element();

    skip_misc();
    ASTRA_ENSURE(!eof(), "Unexpected trailing XML content at position ",
                 m_position);

    return root;
  }

private:
  bool eof() const { return m_position >= m_input.size(); }

  bool starts_with(std::string_view value) const {
    return m_input.substr(m_position, value.size()) == value;
  }

  char peek() const {
    ASTRA_ENSURE(eof(), "Unexpected end of XML document");
    return m_input[m_position];
  }

  void expect(char ch) {
    ASTRA_ENSURE(eof() || m_input[m_position] != ch, "Expected '", ch,
                 "' at position ", m_position);
    ++m_position;
  }

  bool consume(char ch) {
    if (!eof() && m_input[m_position] == ch) {
      ++m_position;
      return true;
    }

    return false;
  }

  std::string parse_name() {
    ASTRA_ENSURE(eof() || !is_name_start_char(m_input[m_position]),
                 "Expected XML name at position ", m_position);

    const size_t start = m_position++;
    while (!eof() && is_name_char(m_input[m_position])) {
      ++m_position;
    }

    return std::string(m_input.substr(start, m_position - start));
  }

  std::string parse_quoted_value() {
    ASTRA_ENSURE(eof(), "Expected quoted XML attribute value at position ",
                 m_position);

    const char quote = m_input[m_position];
    ASTRA_ENSURE(quote != '"' && quote != '\'',
                 "Expected quoted XML attribute value at position ",
                 m_position);

    ++m_position;
    const size_t start = m_position;

    while (!eof() && m_input[m_position] != quote) {
      ++m_position;
    }

    ASTRA_ENSURE(eof(), "Unterminated XML attribute value at position ",
                 start);

    std::string value =
        decode_entities(m_input.substr(start, m_position - start));
    ++m_position;
    return value;
  }

  void skip_spaces() {
    while (!eof() &&
           std::isspace(static_cast<unsigned char>(m_input[m_position]))) {
      ++m_position;
    }
  }

  void skip_comment() {
    ASTRA_ENSURE(!starts_with("<!--"), "Expected XML comment at position ",
                 m_position);

    const size_t end = m_input.find("-->", m_position + 4);
    ASTRA_ENSURE(end == std::string_view::npos, "Unterminated XML comment");
    m_position = end + 3;
  }

  void skip_misc() {
    while (true) {
      skip_spaces();

      if (starts_with("<!--")) {
        skip_comment();
        continue;
      }

      break;
    }
  }

  void parse_declaration() {
    ASTRA_ENSURE(!starts_with("<?xml"),
                 "Expected XML declaration at position ", m_position);

    const size_t end = m_input.find("?>", m_position + 5);
    ASTRA_ENSURE(end == std::string_view::npos,
                 "Unterminated XML declaration");
    m_position = end + 2;
  }

  Node parse_element() {
    expect('<');
    ASTRA_ENSURE(eof(), "Unexpected end of XML document after '<'");

    if (m_input[m_position] == '/') {
      ASTRA_EXCEPTION("Unexpected XML closing tag at position ", m_position);
    }

    if (starts_with("!DOCTYPE")) {
      ASTRA_EXCEPTION("DTD is not supported in XML");
    }

    if (starts_with("![CDATA[")) {
      ASTRA_EXCEPTION("CDATA is not supported in XML");
    }

    if (m_input[m_position] == '?') {
      ASTRA_EXCEPTION("Processing instructions are not supported inside XML");
    }

    Node node = make_node(parse_name());

    while (true) {
      skip_spaces();
      ASTRA_ENSURE(eof(), "Unexpected end of XML start tag for <", node.name,
                   ">");

      if (starts_with("/>")) {
        m_position += 2;
        return node;
      }

      if (consume('>')) {
        break;
      }

      const std::string attribute_name = parse_name();
      skip_spaces();
      expect('=');
      skip_spaces();

      auto &attribute = ensure_attribute(node, attribute_name);
      attribute.value = parse_quoted_value();
    }

    std::string text_buffer;

    while (true) {
      ASTRA_ENSURE(eof(), "Unterminated XML element <", node.name, ">");

      if (starts_with("</")) {
        m_position += 2;
        skip_spaces();

        const std::string closing_name = parse_name();
        ASTRA_ENSURE(closing_name != node.name, "Mismatched XML closing tag </",
                     closing_name, "> for <", node.name, ">");

        skip_spaces();
        expect('>');

        if (node.children.empty()) {
          node.text = decode_entities(text_buffer);
        } else {
          ASTRA_ENSURE(has_non_whitespace(text_buffer),
                       "Mixed XML text content is not supported in <",
                       node.name, ">");
        }

        return node;
      }

      if (starts_with("<!--")) {
        skip_comment();
        continue;
      }

      if (starts_with("<!DOCTYPE")) {
        ASTRA_EXCEPTION("DTD is not supported in XML");
      }

      if (starts_with("<![CDATA[")) {
        ASTRA_EXCEPTION("CDATA is not supported in XML");
      }

      if (starts_with("<?")) {
        ASTRA_EXCEPTION(
            "Processing instructions are not supported inside XML");
      }

      if (peek() == '<') {
        ASTRA_ENSURE(has_non_whitespace(text_buffer),
                     "Mixed XML text content is not supported in <", node.name,
                     ">");
        text_buffer.clear();
        node.children.push_back(std::make_unique<Node>(parse_element()));
        continue;
      }

      text_buffer.push_back(m_input[m_position++]);
    }
  }

  std::string_view m_input;
  size_t m_position = 0;
};

} // namespace

XmlSerializationContext::XmlSerializationContext()
    : m_root(std::make_shared<Node>(make_node())), m_current(m_root.get()) {}

XmlSerializationContext::XmlSerializationContext(Scope<StreamBuffer> buffer)
    : XmlSerializationContext() {
  from_buffer(std::move(buffer));
}

XmlSerializationContext::XmlSerializationContext(
    std::shared_ptr<Node> root, Node *current,
    std::optional<std::string> attribute_name)
    : m_root(std::move(root)), m_current(current),
      m_attribute_name(std::move(attribute_name)) {}

size_t XmlSerializationContext::node_size(const Node *node) {
  return child_count(node);
}

size_t XmlSerializationContext::root_size() { return node_size(m_root.get()); }

size_t XmlSerializationContext::size() { return node_size(m_current); }

ContextProxy XmlSerializationContext::operator[](const SerializableKey &key) {
  ASTRA_ENSURE(m_current == nullptr, "XML context has no current node");

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
    const auto &string_key = std::get<std::string>(parsed_key);

    if (string_key.size() > 1 && string_key.front() == '@') {
      auto attribute_name = string_key.substr(1);
      ensure_attribute(*m_current, attribute_name);

      return ContextProxy(
          Scope<SerializationContext>(new XmlSerializationContext(
              m_root, m_current, std::move(attribute_name))),
          string_key);
    }

    auto *child = &ensure_named_child(*m_current, string_key);

    return ContextProxy(
        Scope<SerializationContext>(
            new XmlSerializationContext(m_root, child)),
        string_key);
  }

  const int index = std::get<int>(parsed_key);
  auto *child = &ensure_index_child(*m_current, index);

  return ContextProxy(
      Scope<SerializationContext>(new XmlSerializationContext(m_root, child)),
      std::to_string(index));
}

void XmlSerializationContext::set_value(const SerializableValue &value) {
  ASTRA_ENSURE(m_current == nullptr, "XML context has no current node");

  auto assign = [&](std::string text) {
    if (m_attribute_name.has_value()) {
      ensure_attribute(*m_current, *m_attribute_name).value = std::move(text);
      return;
    }

    m_current->children.clear();
    m_current->text = std::move(text);
  };

  if (std::holds_alternative<int>(value)) {
    assign(std::to_string(std::get<int>(value)));
  } else if (std::holds_alternative<float>(value)) {
    assign(std::to_string(std::get<float>(value)));
  } else if (std::holds_alternative<std::string>(value)) {
    assign(std::get<std::string>(value));
  } else if (std::holds_alternative<bool>(value)) {
    assign(std::get<bool>(value) ? "true" : "false");
  }
}

void XmlSerializationContext::set_value(Ref<SerializationContext> ctx) {
  ASTRA_ENSURE(m_current == nullptr, "XML context has no current node");
  ASTRA_ENSURE(m_attribute_name.has_value(),
               "Cannot assign XML context into an XML attribute");

  auto *xml_ctx = static_cast<XmlSerializationContext *>(ctx.get());
  ASTRA_ENSURE(xml_ctx == nullptr || xml_ctx->m_current == nullptr,
               "Invalid XML serialization context");

  copy_node_contents(*m_current, *xml_ctx->m_current);
}

ElasticArena::Block *XmlSerializationContext::to_buffer(ElasticArena &arena) {
  std::string xml_string;
  if (m_root != nullptr) {
    emit_node(*m_root, xml_string);
  }

  auto *block = arena.allocate(xml_string.size());
  if (!xml_string.empty()) {
    std::memcpy(block->data, xml_string.data(), xml_string.size());
  }

  return block;
}

void XmlSerializationContext::from_buffer(Scope<StreamBuffer> buffer) {
  *m_root = make_node();
  m_attribute_name.reset();

  if (buffer == nullptr || buffer->size() == 0) {
    m_current = m_root.get();
    return;
  }

  const std::string content(buffer->data(), buffer->size());
  *m_root = XmlParser(content).parse_document();
  m_current = m_root.get();
}

std::string XmlSerializationContext::as_string() {
  const auto *value = current_scalar_value(m_current, m_attribute_name);
  return value == nullptr ? std::string() : *value;
}

int XmlSerializationContext::as_int() {
  const auto *value = current_scalar_value(m_current, m_attribute_name);
  if (value == nullptr) {
    return 0;
  }

  int int_value = 0;
  if (try_parse_int(*value, int_value)) {
    return int_value;
  }

  bool bool_value = false;
  if (try_parse_bool(*value, bool_value)) {
    return bool_value ? 1 : 0;
  }

  float float_value = 0.0f;
  if (try_parse_float(*value, float_value)) {
    return static_cast<int>(float_value);
  }

  return 0;
}

float XmlSerializationContext::as_float() {
  const auto *value = current_scalar_value(m_current, m_attribute_name);
  if (value == nullptr) {
    return 0.0f;
  }

  float float_value = 0.0f;
  if (try_parse_float(*value, float_value)) {
    return float_value;
  }

  int int_value = 0;
  if (try_parse_int(*value, int_value)) {
    return static_cast<float>(int_value);
  }

  bool bool_value = false;
  if (try_parse_bool(*value, bool_value)) {
    return bool_value ? 1.0f : 0.0f;
  }

  return 0.0f;
}

bool XmlSerializationContext::as_bool() {
  const auto *value = current_scalar_value(m_current, m_attribute_name);
  if (value == nullptr) {
    return false;
  }

  bool bool_value = false;
  if (try_parse_bool(*value, bool_value)) {
    return bool_value;
  }

  int int_value = 0;
  if (try_parse_int(*value, int_value)) {
    return int_value != 0;
  }

  float float_value = 0.0f;
  if (try_parse_float(*value, float_value)) {
    return float_value != 0.0f;
  }

  return false;
}

std::vector<std::any> XmlSerializationContext::as_array() {
  std::vector<std::any> items;

  if (m_attribute_name.has_value() || m_current == nullptr) {
    return items;
  }

  items.reserve(m_current->children.size());
  for (const auto &child : m_current->children) {
    items.push_back(child.get());
  }

  return items;
}

SerializationTypeKind XmlSerializationContext::kind() {
  return node_kind(m_current, m_attribute_name);
}

bool XmlSerializationContext::is_string() {
  return kind() == SerializationTypeKind::String;
}

bool XmlSerializationContext::is_int() {
  return kind() == SerializationTypeKind::Int;
}

bool XmlSerializationContext::is_float() {
  return kind() == SerializationTypeKind::Float;
}

bool XmlSerializationContext::is_bool() {
  return kind() == SerializationTypeKind::Bool;
}

bool XmlSerializationContext::is_array() {
  return kind() == SerializationTypeKind::Array;
}

bool XmlSerializationContext::is_object() {
  return kind() == SerializationTypeKind::Object;
}

} // namespace astralix
