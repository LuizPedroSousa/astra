#pragma once

#include "module-handle.hpp"
#include "systems/workspace-shell-system.hpp"

class EditorLoader {
public:
  struct Config {
    astralix::ModuleHandle::Config module;
    astralix::editor::WorkspaceShellSystemConfig shell_config;
  };

  explicit EditorLoader(Config config);
  ~EditorLoader();

  EditorLoader(const EditorLoader &) = delete;
  EditorLoader &operator=(const EditorLoader &) = delete;

  bool initial_load();
  void poll();
  bool is_loaded() const;

private:
  void activate();
  void deactivate();

  Config m_config;
  astralix::ModuleHandle m_handle;
};
