#include "resources/shader.hpp"

#include "exceptions/base-exception.hpp"
#include <gtest/gtest.h>
#include <limits>
#include <vector>

namespace astralix {

namespace {

class MockShader : public Shader {
public:
  struct UploadRecord {
    uint64_t binding_id = 0;
    ShaderValueKind kind = ShaderValueKind::Float;
  };

  MockShader() : Shader(ResourceHandle{0, 1}, "mock") {}

  void bind() const override {}
  void unbind() const override {}
  void attach() const override {}
  uint32_t renderer_id() const override { return 0; }

  mutable std::vector<UploadRecord> uploads;

protected:
  void set_typed_value(uint64_t binding_id, ShaderValueKind kind,
                       const void *) const override {
    uploads.push_back(UploadRecord{binding_id, kind});
  }
};

namespace generated_bindings {

struct EntityUniform {
  struct projection_t {
    using value_type = glm::mat4;
    static constexpr std::array<uint64_t, 1> binding_ids = {101ull};
  };

  struct lights__position_t {
    using value_type = std::array<glm::vec3, 2>;
    static constexpr std::array<uint64_t, 2> binding_ids = {201ull, 202ull};
  };

  struct enabled_t {
    using value_type = bool;
    static constexpr std::array<uint64_t, 1> binding_ids = {301ull};
  };

  static inline constexpr projection_t projection{};
  static inline constexpr lights__position_t lights__position{};
  static inline constexpr enabled_t enabled{};
};

struct MatrixUniform {
  struct values_t {
    using value_type = std::array<std::array<int, 2>, 2>;
    static constexpr std::array<uint64_t, 4> binding_ids = {1ull, 2ull, 3ull,
                                                            4ull};
  };

  static inline constexpr values_t values{};
};

struct LightParams {
  glm::vec3 position{};
};

struct EntityParams {
  glm::mat4 projection{};
  std::array<LightParams, 2> lights{};
  bool enabled = false;
};

inline bool validate_shader_params(const EntityParams &, std::string *) {
  return true;
}

inline void apply_shader_params(const astralix::Shader &shader,
                                const EntityParams &params) {
  shader.set(EntityUniform::projection, params.projection);

  {
    decltype(EntityUniform::lights__position)::value_type values{};
    for (size_t i0 = 0; i0 < values.size(); ++i0) {
      values[i0] = params.lights[i0].position;
    }
    shader.set(EntityUniform::lights__position, values);
  }

  shader.set(EntityUniform::enabled, params.enabled);
}

} // namespace generated_bindings

static_assert(requires(const MockShader &shader, const glm::mat4 &matrix) {
  shader.set(generated_bindings::EntityUniform::projection, matrix);
});

template <typename T>
concept AcceptsProjectionValue = requires(const MockShader &shader, T value) {
  shader.set(generated_bindings::EntityUniform::projection, value);
};

static_assert(!AcceptsProjectionValue<bool>);

TEST(ShaderTypedBindings, NestedArrayTagsFlattenBindingIdsInOrder) {
  MockShader shader;

  generated_bindings::MatrixUniform::values_t::value_type values = {
      std::array<int, 2>{1, 2}, std::array<int, 2>{3, 4}};

  shader.set(generated_bindings::MatrixUniform::values, values);

  ASSERT_EQ(shader.uploads.size(), 4u);
  EXPECT_EQ(shader.uploads[0].binding_id, 1ull);
  EXPECT_EQ(shader.uploads[1].binding_id, 2ull);
  EXPECT_EQ(shader.uploads[2].binding_id, 3ull);
  EXPECT_EQ(shader.uploads[3].binding_id, 4ull);

  for (const auto &upload : shader.uploads) {
    EXPECT_EQ(upload.kind, ShaderValueKind::Int);
  }
}

TEST(ShaderTypedBindings, ArrayFieldTagCanUploadSingleElementByIndex) {
  MockShader shader;

  shader.set(generated_bindings::EntityUniform::lights__position, 1,
             glm::vec3(4.0f, 5.0f, 6.0f));

  ASSERT_EQ(shader.uploads.size(), 1u);
  EXPECT_EQ(shader.uploads[0].binding_id, 202ull);
  EXPECT_EQ(shader.uploads[0].kind, ShaderValueKind::Vec3);
}

TEST(ShaderTypedBindings, ParamsApplyUsesGeneratedAdlHelper) {
  MockShader shader;
  generated_bindings::EntityParams params{};
  params.enabled = true;
  params.lights[0].position = glm::vec3(1.0f, 2.0f, 3.0f);
  params.lights[1].position = glm::vec3(4.0f, 5.0f, 6.0f);

  shader.set_all(params);

  ASSERT_EQ(shader.uploads.size(), 4u);
  EXPECT_EQ(shader.uploads[0].binding_id, 101ull);
  EXPECT_EQ(shader.uploads[0].kind, ShaderValueKind::Mat4);
  EXPECT_EQ(shader.uploads[1].binding_id, 201ull);
  EXPECT_EQ(shader.uploads[1].kind, ShaderValueKind::Vec3);
  EXPECT_EQ(shader.uploads[2].binding_id, 202ull);
  EXPECT_EQ(shader.uploads[2].kind, ShaderValueKind::Vec3);
  EXPECT_EQ(shader.uploads[3].binding_id, 301ull);
  EXPECT_EQ(shader.uploads[3].kind, ShaderValueKind::Bool);
}

namespace validating_generated_bindings {

struct MaterialUniform {
  struct diffuse_t {
    using value_type = int;
    static constexpr std::array<uint64_t, 1> binding_ids = {401ull};
  };

  static inline constexpr diffuse_t diffuse{};
};

struct MaterialParams {
  int diffuse = -1;
};

inline bool validate_shader_params(const MaterialParams &params,
                                   std::string *error = nullptr) {
  if (params.diffuse < 0) {
    if (error != nullptr) {
      *error = "MaterialParams is missing required field 'material.diffuse'";
    }
    return false;
  }

  return true;
}

inline void apply_shader_params(const astralix::Shader &shader,
                                const MaterialParams &params) {
  shader.set(MaterialUniform::diffuse, params.diffuse);
}

} // namespace validating_generated_bindings

TEST(ShaderTypedBindings, SetAllRejectsInvalidSnapshots) {
  MockShader shader;

  try {
    shader.set_all(validating_generated_bindings::MaterialParams{});
    FAIL() << "expected validation failure";
  } catch (const BaseException &error) {
    EXPECT_NE(std::string(error.what()).find("material.diffuse"),
              std::string::npos);
  }

  EXPECT_TRUE(shader.uploads.empty());
}

} // namespace

} // namespace astralix
