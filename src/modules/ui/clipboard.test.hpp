#include "clipboard.hpp"

#include <gtest/gtest.h>

namespace astralix::ui {
namespace {

class MemoryClipboardBackend final : public ClipboardBackend {
public:
  std::string get(GLFWwindow *) const override { return text; }

  void set(GLFWwindow *, const std::string &value) const override {
    text = value;
  }

  mutable std::string text;
};

TEST(UIFoundationsTest, ClipboardBackendCanBeInjected) {
  const auto original = clipboard_backend();
  const auto backend = create_ref<MemoryClipboardBackend>();

  set_clipboard_backend(backend);
  clipboard_backend()->set(nullptr, "copied");
  EXPECT_EQ(clipboard_backend()->get(nullptr), "copied");

  set_clipboard_backend(original);
}

} // namespace
} // namespace astralix::ui
