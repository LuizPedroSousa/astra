#include "layers/systems/layer-system.hpp"
#include "base.hpp"
#include "entities/layer.hpp"

#include "event-scheduler.hpp"
#include "log.hpp"
#include "overlays/dockspace-overlay.hpp"

#include "layers/console-layer.hpp"
#include "layers/content-browser-layer.hpp"
#include "layers/properties-layer.hpp"
#include "layers/scene-hierarchy-layer.hpp"
#include "layers/viewport-layer.hpp"

#include "layers/managers/layer-manager.hpp"

namespace astralix {

LayerSystem::LayerSystem() { LayerManager::init(); }

void LayerSystem::start() {

  auto layer_manager = LayerManager::get();

  layer_manager->add_overlay<DockspaceOverlay>();

  // layer_manager->add_layer<SceneHierarchyLayer>();
  layer_manager->add_layer<ViewportLayer>();
  // layer_manager->add_layer<ConsoleLayer>();
  // layer_manager->add_layer<ContentBrowserLayer>();
  // layer_manager->add_layer<PropertiesLayer>();

  layer_manager->for_each([](Overlay *overlay) { overlay->start(); });
  layer_manager->for_each([](Layer *layer) { layer->start(); });
};

void LayerSystem::fixed_update(double fixed_dt) {};

void LayerSystem::pre_update(double dt) {
  auto layer_manager = LayerManager::get();

  layer_manager->for_each([](Overlay *overlay) { overlay->pre_update(); });
};

void LayerSystem::update(double dt) {
  auto layer_manager = LayerManager::get();

  layer_manager->for_each([](Overlay *overlay) { overlay->update(); });
  layer_manager->for_each([](Layer *layer) { layer->update(); });

  ImGui::Begin("Light");
  ImGui::DragFloat("Distance", &distance, 0.1f, 0.0f, 0.0f, "%.2f");
  ImGui::DragFloat("near", &near_plane, 0.1f, 0.0f, 0.0f, "%.2f");
  ImGui::DragFloat("far", &far_plane, 0.1f, 0.0f, 0.0f, "%.2f");
  ImGui::End();

  layer_manager->for_each([](Overlay *overlay) { overlay->post_update(); });
};

LayerSystem::~LayerSystem() {}

} // namespace astralix
