#pragma once

#include "base.hpp"
#include "document/document.hpp"
#include "dsl.hpp"
#include "serialization-context.hpp"

namespace astralix::editor {

struct PanelMountContext {
  Ref<ui::UIDocument> document = nullptr;
  ResourceDescriptorID default_font_id;
  float default_font_size = 16.0f;
};

struct PanelUpdateContext {
  double dt = 0.0;
};

class PanelController {
public:
  virtual ~PanelController() = default;

  virtual ui::dsl::NodeSpec build() = 0;
  virtual void mount(const PanelMountContext &context) = 0;
  virtual void unmount() {}
  virtual void update(const PanelUpdateContext &) {}
  virtual void load_state(Ref<SerializationContext>) {}
  virtual void save_state(Ref<SerializationContext>) const {}
};

struct PanelProviderDescriptor {
  std::string id;
  std::string title;
  bool singleton = true;
  std::function<Scope<PanelController>()> factory;
};

} // namespace astralix::editor
