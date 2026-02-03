#pragma once
#include "guid.hpp"

namespace astralix {

#define RESOURCE_INIT_PARAMS const ResourceHandle &id
#define RESOURCE_INIT() Resource(id)

class Resource {
public:
  Resource(RESOURCE_INIT_PARAMS) { m_id = id; }
  Resource() {}

  inline const ResourceHandle id() const noexcept { return m_id; };

protected:
  ResourceHandle m_id;
};

} // namespace astralix
