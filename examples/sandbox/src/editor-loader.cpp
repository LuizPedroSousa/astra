#include "editor-loader.hpp"
#include "exceptions/base-exception.hpp"
#include "log.hpp"

using namespace astralix;

EditorLoader::EditorLoader(Config config)
    : m_config(std::move(config)), m_handle(m_config.module) {}

EditorLoader::~EditorLoader() {
  if (m_handle.is_loaded()) {
    deactivate();
  }
}

bool EditorLoader::initial_load() {
  if (!m_handle.load()) {
    return false;
  }
  activate();
  return true;
}

void EditorLoader::poll() {
  if (!m_handle.poll_changed()) return;

  LOG_INFO("EditorLoader: detected new module, reloading...");
  deactivate();
  if (!m_handle.load()) {
    LOG_ERROR("EditorLoader: reload failed, editor is unloaded");
    return;
  }

  try {
    activate();
    LOG_INFO("EditorLoader: reload complete, generation",
             m_handle.generation());
  } catch (const BaseException &exception) {
    LOG_ERROR("EditorLoader: activation failed: ", exception.what());
    m_handle.unload();
  } catch (const std::exception &exception) {
    LOG_ERROR("EditorLoader: activation failed: ", exception.what());
    m_handle.unload();
  }
}

bool EditorLoader::is_loaded() const { return m_handle.is_loaded(); }

void EditorLoader::activate() {
  m_handle.api()->load(&m_config.shell_config,
                       sizeof(m_config.shell_config));
}

void EditorLoader::deactivate() {
  m_handle.api()->unload();
  m_handle.unload();
}
