#include "render-passes-loader.hpp"
#include "exceptions/base-exception.hpp"
#include "log.hpp"
#include "managers/system-manager.hpp"
#include "systems/render-system/render-system.hpp"

using namespace astralix;

RenderPassesLoader::RenderPassesLoader(Config config)
    : m_config(std::move(config)), m_handle(m_config.module) {}

RenderPassesLoader::~RenderPassesLoader() {
  if (m_handle.is_loaded()) {
    deactivate();
  }
}

bool RenderPassesLoader::initial_load() {
  if (!m_handle.load()) {
    return false;
  }
  activate();
  return true;
}

void RenderPassesLoader::poll() {
  if (!m_handle.poll_changed()) return;

  LOG_INFO("RenderPassesLoader: detected new module, reloading...");
  deactivate();
  if (!m_handle.load()) {
    LOG_ERROR("RenderPassesLoader: reload failed, passes are unloaded");
    return;
  }

  try {
    activate();
    LOG_INFO("RenderPassesLoader: reload complete, generation",
             m_handle.generation());
  } catch (const BaseException &exception) {
    LOG_ERROR("RenderPassesLoader: activation failed: ", exception.what());
    m_handle.unload();
  } catch (const std::exception &exception) {
    LOG_ERROR("RenderPassesLoader: activation failed: ", exception.what());
    m_handle.unload();
  }
}

bool RenderPassesLoader::is_loaded() const { return m_handle.is_loaded(); }

void RenderPassesLoader::activate() {
  auto *render_system =
      SystemManager::get()->get_system<RenderSystem>();
  if (render_system == nullptr) return;

  render_system->load_passes_from_module(m_handle.api());
}

void RenderPassesLoader::deactivate() {
  auto *render_system =
      SystemManager::get()->get_system<RenderSystem>();
  if (render_system != nullptr) {
    render_system->prepare_for_pass_reload();
  }

  m_handle.api()->unload();
  m_handle.unload();
}
