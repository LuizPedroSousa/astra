#include "sandbox-loader.hpp"

#include "astralix/modules/renderer/managers/scene-manager.hpp"
#include "exceptions/base-exception.hpp"
#include "log.hpp"

using namespace astralix;

SandboxLoader::SandboxLoader(Config config)
    : m_config(std::move(config)), m_handle(m_config.module) {}

SandboxLoader::~SandboxLoader() {
  if (m_handle.is_loaded()) {
    deactivate();
  }
}

bool SandboxLoader::initial_load() {
  if (!m_handle.load()) {
    if (auto scene_manager = SceneManager::get();
        scene_manager != nullptr) {
      scene_manager->set_scene_activation_enabled(false);
    }
    return false;
  }

  activate();
  return true;
}

void SandboxLoader::poll() {
  if (!m_handle.poll_changed()) return;

  LOG_INFO("SandboxLoader: detected new module, reloading...");
  if (m_handle.is_loaded()) {
    deactivate();
  }

  if (!m_handle.load()) {
    LOG_ERROR("SandboxLoader: reload failed, sandbox scenes are unloaded");
    return;
  }

  try {
    activate();
    LOG_INFO("SandboxLoader: reload complete, generation",
             m_handle.generation());
  } catch (const BaseException &exception) {
    LOG_ERROR("SandboxLoader: activation failed: ", exception.what());
    m_handle.unload();
  } catch (const std::exception &exception) {
    LOG_ERROR("SandboxLoader: activation failed: ", exception.what());
    m_handle.unload();
  }
}

bool SandboxLoader::is_loaded() const { return m_handle.is_loaded(); }

void SandboxLoader::activate() { m_handle.api()->load(nullptr, 0u); }

void SandboxLoader::deactivate() {
  if (!m_handle.is_loaded()) {
    return;
  }

  m_handle.api()->unload();
  m_handle.unload();
}
