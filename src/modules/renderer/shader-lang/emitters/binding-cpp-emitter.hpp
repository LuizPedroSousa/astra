#pragma once

#include "shader-lang/reflection.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace astralix {

class BindingCppEmitter {
public:
  std::optional<std::string> emit(const ShaderReflection &reflection,
                                  std::string_view input_path,
                                  std::string *error = nullptr);

  static std::string sanitize_namespace(std::string_view input_path);
};

} // namespace astralix
