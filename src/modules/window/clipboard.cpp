#include "clipboard.hpp"

#include "GLFW/glfw3.h"

namespace astralix {
namespace {

class GlfwClipboardBackend final : public ClipboardBackend {
public:
  std::string get(GLFWwindow *window) const override {
    if (window == nullptr) {
      return {};
    }

    const char *text = glfwGetClipboardString(window);
    return text != nullptr ? std::string(text) : std::string{};
  }

  void set(GLFWwindow *window, const std::string &text) const override {
    if (window == nullptr) {
      return;
    }

    glfwSetClipboardString(window, text.c_str());
  }
};

Ref<ClipboardBackend> g_clipboard_backend =
    create_ref<GlfwClipboardBackend>();

} // namespace

Ref<ClipboardBackend> clipboard_backend() { return g_clipboard_backend; }

void set_clipboard_backend(Ref<ClipboardBackend> backend) {
  g_clipboard_backend =
      backend != nullptr ? std::move(backend) : create_ref<GlfwClipboardBackend>();
}

} // namespace astralix
