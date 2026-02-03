#pragma once
#include "cstddef"
#include "string"
#include <cstddef>
#include <iostream>
#include <stdint.h>
#include <variant>

namespace astralix {

using SerializableValue = std::variant<int, float, std::string, bool>;
using SerializableKey = std::variant<int, float, std::string, bool>;

using DeserializableValue = std::variant<int, float, std::string, bool>;
using DeserializableKey = std::variant<int, float, std::string, bool>;

class Guid {
public:
  Guid();
  Guid(uint64_t value);
  Guid(const Guid &) = default;

  operator uint64_t() const { return m_value; }
  operator std::string() const { return std::to_string(m_value); }

private:
  uint64_t m_value;
};

using ObjectID = Guid;
using SceneID = Guid;
using TypeID = size_t;
using ListenerId = Guid;
using MeshID = std::size_t;
using MeshGroupID = std::size_t;

using EntityID = Guid;
using EntityFamilyID = size_t;
using EntityTypeID = TypeID;

using ComponentID = Guid;
using ComponentTypeID = TypeID;
using SchedulerID = Guid;

using SystemID = Guid;

using SystemTypeID = TypeID;

struct Handle {
  uint32_t index;
  uint32_t generation;

  bool operator==(const Handle &other) const {
    return index == other.index && generation == other.generation;
  }

  bool is_valid() const { return generation != 0; }
};

using ResourceHandle = Handle;

inline std::ostream &operator<<(std::ostream &os, const ResourceHandle &id) {
  if (!id.is_valid()) {
    os << "RID{invalid}";
  } else {
    os << "RID{" << id.index << ":" << id.generation << "}";
  }
  return os;
}

using ResourceDescriptorID = std::string;
using ProjectID = Guid;
using PathID = Guid;
using WindowID = std::string;

template <typename T> class FamilyTypeID {
private:
  static TypeID s_count;

public:
  template <typename U> static TypeID get() {
    static const TypeID STATIC_TYPE_ID{s_count++};
    return STATIC_TYPE_ID;
  }

  static TypeID get() { return s_count; }
};

template <typename T> class FamilyObjectID {
private:
  FamilyObjectID() {}
  static TypeID s_count;

public:
  constexpr static TypeID get() { return s_count++; }
};

} // namespace astralix

namespace std {
template <typename T> struct hash;

template <> struct hash<astralix::Guid> {
  std::size_t operator()(const astralix::Guid &guid) const {
    return (uint64_t)guid;
  }
};
}; // namespace std
