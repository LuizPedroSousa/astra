#pragma once

#include <string>

// #include "imgui.h"
//
// #include "imgui_internal.h"
// #include "imgui_stdlib.h"

namespace astralix {
class Overlay {

public:
  Overlay(const std::string &name) : name(name) {}

  std::string name;

  virtual void start() = 0;
  virtual void pre_update() = 0;
  virtual void update() = 0;
  virtual void post_update() = 0;
};
} // namespace astralix
