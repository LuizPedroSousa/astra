#include "adapters/xml/xml-serialization-context.hpp"
#include "arena.hpp"
#include "assert.hpp"
#include "base.hpp"
#include "context-proxy.hpp"
#include "stream-buffer.hpp"
#include <any>
#include <charconv>
#include <sstream>
#include <string>
#include <variant>

namespace astralix {

XmlSerializationContext::XmlSerializationContext() {
  auto root = m_document.append_child("root");
  m_current.push(root);
}

ContextProxy
XmlSerializationContext::operator[](const SerializableKey &key) {
  pugi::xml_node current_node = m_current.top();

  auto parsed_key = std::visit(
      [](auto &&key) -> std::variant<int, std::string> {
        using T = std::decay_t<decltype(key)>;

        if constexpr (std::is_same_v<T, int>) {
          return key;
        } else {
          return convert_key_to_string(key);
        }
      },
      key);

  auto sub_ctx = create_scope<XmlSerializationContext>();

  if (std::holds_alternative<std::string>(parsed_key)) {
    auto str_key = std::get<std::string>(parsed_key);

    // Attribute access via @-prefix
    if (str_key.size() > 1 && str_key[0] == '@') {
      std::string attribute_name = str_key.substr(1);

      if (!current_node.attribute(attribute_name.c_str())) {
        current_node.append_attribute(attribute_name.c_str());
      }

      sub_ctx->m_current.push(current_node);
      sub_ctx->m_is_attribute = true;
      sub_ctx->m_attribute_name = attribute_name;

      return ContextProxy(std::move(sub_ctx), str_key);
    }

    // Child element access
    auto child = current_node.child(str_key.c_str());

    if (!child) {
      child = current_node.append_child(str_key.c_str());
    }

    sub_ctx->m_current.push(child);

    return ContextProxy(std::move(sub_ctx), str_key);
  }

  // Integer index — nth child element
  auto int_key = std::get<int>(parsed_key);
  int index = 0;

  for (auto child = current_node.first_child(); child;
       child = child.next_sibling()) {
    if (child.type() == pugi::node_element) {
      if (index == int_key) {
        sub_ctx->m_current.push(child);
        return ContextProxy(std::move(sub_ctx), std::to_string(int_key));
      }
      index++;
    }
  }

  // If index doesn't exist, append a new child
  auto new_child =
      current_node.append_child(("item" + std::to_string(int_key)).c_str());
  sub_ctx->m_current.push(new_child);

  return ContextProxy(std::move(sub_ctx), std::to_string(int_key));
};

void XmlSerializationContext::set_value(const SerializableValue &value) {
  pugi::xml_node current_node = m_current.top();

  auto set_text = [&](const std::string &text) {
    if (m_is_attribute) {
      current_node.attribute(m_attribute_name.c_str()).set_value(text.c_str());
    } else {
      current_node.text().set(text.c_str());
    }
  };

  if (std::holds_alternative<int>(value)) {
    set_text(std::to_string(std::get<int>(value)));
  } else if (std::holds_alternative<float>(value)) {
    set_text(std::to_string(std::get<float>(value)));
  } else if (std::holds_alternative<std::string>(value)) {
    set_text(std::get<std::string>(value));
  } else if (std::holds_alternative<bool>(value)) {
    set_text(std::get<bool>(value) ? "true" : "false");
  }
}

void XmlSerializationContext::set_value(Ref<SerializationContext> ctx) {
  auto xml_ctx = static_cast<XmlSerializationContext *>(ctx.get());

  pugi::xml_node current_node = m_current.top();
  pugi::xml_node source_root = xml_ctx->m_current.top();

  // Copy all children from source into current
  for (auto child = source_root.first_child(); child;
       child = child.next_sibling()) {
    current_node.append_copy(child);
  }

  // Copy attributes
  for (auto attribute = source_root.first_attribute(); attribute;
       attribute = attribute.next_attribute()) {
    current_node.append_copy(attribute);
  }
}

ElasticArena::Block *XmlSerializationContext::to_buffer(ElasticArena &arena) {
  std::ostringstream stream;
  m_document.save(stream);

  std::string xml_string = stream.str();

  auto block = arena.allocate(xml_string.size());

  std::memcpy(block->data, xml_string.data(), xml_string.size());

  return block;
};

XmlSerializationContext::XmlSerializationContext(Scope<StreamBuffer> buffer) {
  from_buffer(std::move(buffer));
};

void XmlSerializationContext::from_buffer(Scope<StreamBuffer> buffer) {
  auto result = m_document.load_buffer(buffer->data(), buffer->size());

  ASTRA_ENSURE(!result, "Failed to parse XML");

  m_current = {};

  auto root = m_document.first_child();
  if (root) {
    m_current.push(root);
  } else {
    m_current.push(m_document.append_child("root"));
  }
};

size_t XmlSerializationContext::count_children(pugi::xml_node node) {
  size_t count = 0;
  for (auto child = node.first_child(); child; child = child.next_sibling()) {
    if (child.type() == pugi::node_element) {
      count++;
    }
  }
  return count;
}

size_t XmlSerializationContext::root_size() {
  return count_children(m_document.first_child());
}

size_t XmlSerializationContext::size() {
  return count_children(m_current.top());
}

bool XmlSerializationContext::has_text_only() const {
  auto node = m_current.top();
  return !node.text().empty() && !node.first_child().next_sibling();
}

std::string XmlSerializationContext::as_string() {
  if (m_is_attribute) {
    return m_current.top().attribute(m_attribute_name.c_str()).as_string();
  }
  return m_current.top().text().as_string();
}

int XmlSerializationContext::as_int() {
  if (m_is_attribute) {
    return m_current.top().attribute(m_attribute_name.c_str()).as_int();
  }
  return m_current.top().text().as_int();
}

float XmlSerializationContext::as_float() {
  if (m_is_attribute) {
    return m_current.top().attribute(m_attribute_name.c_str()).as_float();
  }
  return m_current.top().text().as_float();
}

bool XmlSerializationContext::as_bool() {
  if (m_is_attribute) {
    return m_current.top().attribute(m_attribute_name.c_str()).as_bool();
  }
  return m_current.top().text().as_bool();
}

std::vector<std::any> XmlSerializationContext::as_array() {
  std::vector<std::any> items;

  for (auto child = m_current.top().first_child(); child;
       child = child.next_sibling()) {
    if (child.type() == pugi::node_element) {
      items.push_back(child);
    }
  }

  return items;
}

bool XmlSerializationContext::is_string() {
  if (m_is_attribute) {
    return !std::string_view(
                m_current.top().attribute(m_attribute_name.c_str()).as_string())
                .empty();
  }
  return has_text_only();
}

bool XmlSerializationContext::is_int() {
  std::string text;
  if (m_is_attribute) {
    text = m_current.top().attribute(m_attribute_name.c_str()).as_string();
  } else {
    text = m_current.top().text().as_string();
  }

  if (text.empty())
    return false;

  int result;
  auto [pointer, error_code] =
      std::from_chars(text.data(), text.data() + text.size(), result);
  return error_code == std::errc{} && pointer == text.data() + text.size();
}

bool XmlSerializationContext::is_float() {
  std::string text;
  if (m_is_attribute) {
    text = m_current.top().attribute(m_attribute_name.c_str()).as_string();
  } else {
    text = m_current.top().text().as_string();
  }

  if (text.empty())
    return false;

  // Check if it contains a decimal point (otherwise is_int should be preferred)
  if (text.find('.') == std::string::npos)
    return false;

  try {
    std::stof(text);
    return true;
  } catch (...) {
    return false;
  }
}

bool XmlSerializationContext::is_bool() {
  std::string text;
  if (m_is_attribute) {
    text = m_current.top().attribute(m_attribute_name.c_str()).as_string();
  } else {
    text = m_current.top().text().as_string();
  }

  return text == "true" || text == "false";
}

bool XmlSerializationContext::is_array() {
  return count_children(m_current.top()) > 0;
}

bool XmlSerializationContext::is_object() {
  return count_children(m_current.top()) > 0;
}

} // namespace astralix
