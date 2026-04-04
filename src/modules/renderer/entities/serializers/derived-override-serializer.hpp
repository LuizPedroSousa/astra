#pragma once

#include "base.hpp"
#include "entities/derived-override.hpp"

namespace astralix {

class SerializationContext;

void serialize_derived_state(
    SerializationContext &ctx, const DerivedState &state
);

DerivedState deserialize_derived_state(const Ref<SerializationContext> &ctx);

} // namespace astralix
