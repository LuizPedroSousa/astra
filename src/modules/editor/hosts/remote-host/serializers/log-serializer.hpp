#pragma once
#include "serializer.hpp"

namespace astralix {

class LogSerializer : public Serializer {

public:
  LogSerializer();

  void serialize() override;
  void deserialize() override {};
};

} // namespace astralix
