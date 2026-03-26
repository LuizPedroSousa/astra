#pragma once

#include "guid.hpp"

namespace astralix::rendering {

struct SkyboxBinding {
  ResourceDescriptorID cubemap;
  ResourceDescriptorID shader;
};

} // namespace astralix::rendering
