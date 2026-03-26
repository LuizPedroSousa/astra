#include "guid.hpp"
#include "random"

static std::random_device s_random_device;
static std::mt19937_64 s_engine(s_random_device());

static std::uniform_int_distribution<uint64_t> s_uniform_distribution;
namespace astralix {

Guid::Guid() : m_value(s_uniform_distribution(s_engine)) {}

Guid::Guid(uint64_t value) : m_value(value) {}

class ISystem;

// Specialize the static members using 'template<>'
template <> TypeID FamilyTypeID<ISystem>::s_count = 0u;

// Explicitly instantiate the specialized members
template class FamilyTypeID<ISystem>;

// Specialize the static members using 'template<>'
template <> TypeID FamilyObjectID<ISystem>::s_count = 0u;

// Explicitly instantiate the specialized members
template class FamilyObjectID<ISystem>;
} // namespace astralix
