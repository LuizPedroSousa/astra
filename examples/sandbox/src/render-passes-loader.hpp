#pragma once

#include "module-handle.hpp"

class RenderPassesLoader {
public:
  struct Config {
    astralix::ModuleHandle::Config module;
  };

  explicit RenderPassesLoader(Config config);
  ~RenderPassesLoader();

  RenderPassesLoader(const RenderPassesLoader &) = delete;
  RenderPassesLoader &operator=(const RenderPassesLoader &) = delete;

  bool initial_load();
  void poll();
  bool is_loaded() const;

private:
  void activate();
  void deactivate();

  Config m_config;
  astralix::ModuleHandle m_handle;
};
