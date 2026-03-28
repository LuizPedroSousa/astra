#pragma once

#include "base.hpp"
#include <string>

struct GLFWwindow;

namespace astralix {

class ClipboardBackend {
public:
  virtual ~ClipboardBackend() = default;

  virtual std::string get(GLFWwindow *window) const = 0;
  virtual void set(GLFWwindow *window, const std::string &text) const = 0;
};

Ref<ClipboardBackend> clipboard_backend();
void set_clipboard_backend(Ref<ClipboardBackend> backend);

} // namespace astralix
