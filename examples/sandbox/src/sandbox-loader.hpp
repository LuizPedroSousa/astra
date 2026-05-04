#pragma once

#include "module-handle.hpp"

class SandboxLoader {
public:
  struct Config {
    astralix::ModuleHandle::Config module;
  };

  explicit SandboxLoader(Config config);
  ~SandboxLoader();

  SandboxLoader(const SandboxLoader &) = delete;
  SandboxLoader &operator=(const SandboxLoader &) = delete;

  bool initial_load();
  void poll();
  bool is_loaded() const;

private:
  void activate();
  void deactivate();

  Config m_config;
  astralix::ModuleHandle m_handle;
};
