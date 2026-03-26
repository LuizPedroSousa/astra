#pragma once

#include "guid.hpp"
#include <vector>

namespace astralix::rendering {

struct ModelRef {
  std::vector<ResourceDescriptorID> resource_ids;
};

} // namespace astralix::rendering
