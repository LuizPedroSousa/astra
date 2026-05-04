#pragma once

#include "base.hpp"
#include "immediate.hpp"
#include "serialization-context.hpp"

#include <optional>

namespace astralix::editor {

struct PanelMountContext {
  ui::im::Runtime *runtime = nullptr;
  ResourceDescriptorID default_font_id;
  float default_font_size = 16.0f;
};

struct PanelUpdateContext {
  double dt = 0.0;
};

struct PanelMinimumSize {
  float width = 320.0f;
  float height = 200.0f;
};

class PanelController {
public:
  virtual ~PanelController() = default;

  virtual PanelMinimumSize minimum_size() const { return {}; }
  virtual void render(ui::im::Frame &) = 0;
  virtual void mount(const PanelMountContext &context) = 0;
  virtual void unmount() {}
  virtual void update(const PanelUpdateContext &) {}
  virtual std::optional<uint64_t> render_version() const {
    return std::nullopt;
  }
  virtual void load_state(Ref<SerializationContext>) {}
  virtual void save_state(Ref<SerializationContext>) const {}
};

struct PanelProviderDescriptor {
  std::string id;
  std::string title;
  PanelMinimumSize minimum_size;
  bool singleton = true;
  bool show_shell_frame = true;
  bool toggleable = true;
  std::function<Scope<PanelController>()> factory;
};

} // namespace astralix::editor
