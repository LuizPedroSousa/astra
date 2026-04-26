#pragma once

#include "assert.hpp"
#include "base.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "guid.hpp"
#include "path.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/resource.hpp"
#include <array>
#include <concepts>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

namespace astralix {

enum class ShaderValueKind : uint8_t {
  Bool,
  Int,
  Float,
  Vec2,
  Vec3,
  Vec4,
  Mat3,
  Mat4,
};

namespace shader_detail {

template <typename T>
struct is_std_array : std::false_type {};

template <typename T, size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T>
inline constexpr bool is_std_array_v =
    is_std_array<std::remove_cvref_t<T>>::value;

template <typename T>
struct array_value_type;

template <typename T, size_t N>
struct array_value_type<std::array<T, N>> {
  using type = T;
};

template <typename T>
using array_value_type_t = typename array_value_type<std::remove_cvref_t<T>>::type;

template <typename T>
struct flattened_value_count : std::integral_constant<size_t, 1> {};

template <typename T, size_t N>
struct flattened_value_count<std::array<T, N>>
    : std::integral_constant<size_t, N * flattened_value_count<T>::value> {
};

template <typename T>
struct value_kind;

template <>
struct value_kind<bool> {
  static constexpr ShaderValueKind kind = ShaderValueKind::Bool;
};

template <>
struct value_kind<int> {
  static constexpr ShaderValueKind kind = ShaderValueKind::Int;
};

template <>
struct value_kind<float> {
  static constexpr ShaderValueKind kind = ShaderValueKind::Float;
};

template <>
struct value_kind<glm::vec2> {
  static constexpr ShaderValueKind kind = ShaderValueKind::Vec2;
};

template <>
struct value_kind<glm::vec3> {
  static constexpr ShaderValueKind kind = ShaderValueKind::Vec3;
};

template <>
struct value_kind<glm::vec4> {
  static constexpr ShaderValueKind kind = ShaderValueKind::Vec4;
};

template <>
struct value_kind<glm::mat3> {
  static constexpr ShaderValueKind kind = ShaderValueKind::Mat3;
};

template <>
struct value_kind<glm::mat4> {
  static constexpr ShaderValueKind kind = ShaderValueKind::Mat4;
};

} // namespace shader_detail

class Shader : public Resource {
public:
  virtual void bind() const = 0;
  virtual void unbind() const = 0;
  virtual void attach() const = 0;

  void apply_binding_value(uint64_t binding_id, ShaderValueKind kind, const void *value) const {
    set_typed_value(binding_id, kind, value);
  }

  template <typename FieldTag, typename Value>
    requires std::same_as<std::remove_cvref_t<Value>, typename FieldTag::value_type>
  void set(FieldTag, Value &&value) const {
    upload_field_value<FieldTag>(value);
  }

  template <typename FieldTag, typename Value>
    requires shader_detail::is_std_array_v<typename FieldTag::value_type> &&
             (!shader_detail::is_std_array_v<
                 shader_detail::array_value_type_t<typename FieldTag::value_type>>) &&
             std::same_as<std::remove_cvref_t<Value>, shader_detail::array_value_type_t<typename FieldTag::value_type>>
  void set(FieldTag, size_t index, Value &&value) const {
    upload_field_element_value<FieldTag>(index, value);
  }

  template <typename Params>
  void set_all(const Params &params) const {
    if constexpr (requires(const Params &value, std::string *error) {
                    { validate_shader_params(value, error) } -> std::same_as<bool>;
                  }) {
      std::string error;
      ASTRA_ENSURE(!validate_shader_params(params, &error), error);
    }

    apply_shader_params(*this, params);
  }

  template <typename Params>
  void set(const Params &) const = delete;

  virtual uint32_t renderer_id() const = 0;

  inline ResourceDescriptorID descriptor_id() const noexcept {
    return m_descriptor_id;
  }

  static Ref<ShaderDescriptor> create(const ResourceDescriptorID &id, Ref<Path> fragment_path, Ref<Path> vertex_path, Ref<Path> geometry_path = nullptr);

  static Ref<ShaderDescriptor> define(const ResourceDescriptorID &id, Ref<Path> fragment_path, Ref<Path> vertex_path, Ref<Path> geometry_path = nullptr);

  static Ref<Shader> from_descriptor(const ResourceHandle &id, Ref<ShaderDescriptor> descriptor);

protected:
  Shader(const ResourceHandle &id, const ResourceDescriptorID &descriptor_id)
      : Resource(id), m_descriptor_id(descriptor_id) {
  }

  virtual void set_typed_value(uint64_t binding_id, ShaderValueKind kind, const void *value) const = 0;

  ResourceDescriptorID m_descriptor_id;

private:
  template <typename T>
  void upload_scalar_value(uint64_t binding_id, const T &value) const {
    set_typed_value(binding_id, shader_detail::value_kind<T>::kind, &value);
  }

  template <typename BindingIds, typename Value>
  size_t upload_value_recursive(const BindingIds &binding_ids, size_t index, const Value &value) const {
    if constexpr (shader_detail::is_std_array_v<Value>) {
      for (const auto &item : value) {
        index = upload_value_recursive(binding_ids, index, item);
      }

      return index;
    } else {
      upload_scalar_value(binding_ids[index], value);
      return index + 1;
    }
  }

  template <typename FieldTag, typename Value>
  void upload_field_value(const Value &value) const {
    static_assert(
        std::same_as<typename FieldTag::value_type, std::remove_cvref_t<Value>>,
        "field tag value type mismatch"
    );

    constexpr size_t k_binding_count =
        std::tuple_size_v<std::remove_cvref_t<decltype(FieldTag::binding_ids)>>;
    constexpr size_t k_value_count =
        shader_detail::flattened_value_count<std::remove_cvref_t<Value>>::value;

    static_assert(k_binding_count == k_value_count, "field tag binding count must match flattened value size");

    (void)upload_value_recursive(FieldTag::binding_ids, 0, value);
  }

  template <typename FieldTag, typename Value>
  void upload_field_element_value(size_t index, const Value &value) const {
    using field_value_type = typename FieldTag::value_type;
    using element_value_type = shader_detail::array_value_type_t<field_value_type>;

    static_assert(shader_detail::is_std_array_v<field_value_type>, "indexed field upload requires an array-valued field");
    static_assert(!shader_detail::is_std_array_v<element_value_type>, "indexed field upload only supports single-dimension arrays");
    static_assert(std::same_as<element_value_type, std::remove_cvref_t<Value>>, "indexed field upload value type mismatch");

    constexpr size_t k_binding_count =
        std::tuple_size_v<std::remove_cvref_t<decltype(FieldTag::binding_ids)>>;
    ASTRA_ENSURE(index >= k_binding_count, "shader field index out of range");

    upload_scalar_value(FieldTag::binding_ids[index], value);
  }
};

} // namespace astralix
