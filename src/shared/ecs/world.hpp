#pragma once

#include "assert.hpp"
#include "base.hpp"
#include "guid.hpp"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astralix::ecs {

using ComponentTypeID = TypeID;

namespace detail {

inline ComponentTypeID next_component_type_id() {
  static ComponentTypeID counter = 0;
  return counter++;
}

template <typename T>
inline ComponentTypeID component_type_id() {
  static const ComponentTypeID id = next_component_type_id();
  return id;
}

template <typename T, typename... Args>
T make_component(Args &&...args) {
  return T{std::forward<Args>(args)...};
}

} // namespace detail

template <typename T>
inline ComponentTypeID component_type_id() {
  return detail::component_type_id<std::remove_cvref_t<T>>();
}

struct Signature {
  std::vector<uint64_t> words;

  void set(ComponentTypeID type_id) {
    const size_t word_index = type_id / 64u;
    if (word_index >= words.size()) {
      words.resize(word_index + 1u, 0u);
    }

    words[word_index] |= (uint64_t{1} << (type_id % 64u));
  }

  void reset(ComponentTypeID type_id) {
    const size_t word_index = type_id / 64u;
    if (word_index >= words.size()) {
      return;
    }

    words[word_index] &= ~(uint64_t{1} << (type_id % 64u));
    trim();
  }

  bool test(ComponentTypeID type_id) const {
    const size_t word_index = type_id / 64u;
    if (word_index >= words.size()) {
      return false;
    }

    return (words[word_index] & (uint64_t{1} << (type_id % 64u))) != 0u;
  }

  bool contains(const Signature &required) const {
    const size_t common = std::min(words.size(), required.words.size());
    for (size_t i = 0; i < common; ++i) {
      if ((words[i] & required.words[i]) != required.words[i]) {
        return false;
      }
    }

    for (size_t i = common; i < required.words.size(); ++i) {
      if (required.words[i] != 0u) {
        return false;
      }
    }

    return true;
  }

  bool empty() const {
    return std::all_of(words.begin(), words.end(), [](uint64_t word) { return word == 0u; });
  }

  friend bool operator==(const Signature &lhs, const Signature &rhs) {
    return lhs.words == rhs.words;
  }

private:
  void trim() {
    while (!words.empty() && words.back() == 0u) {
      words.pop_back();
    }
  }
};

struct SignatureHash {
  size_t operator()(const Signature &signature) const {
    size_t seed = 0u;
    for (uint64_t word : signature.words) {
      seed ^= std::hash<uint64_t>{}(word) + 0x9e3779b9 + (seed << 6u) +
              (seed >> 2u);
    }
    return seed;
  }
};

class World;

class EntityRef {
public:
  EntityRef() = default;

  EntityID id() const;
  std::string_view name() const;
  void set_name(std::string name);
  bool active() const;
  void set_active(bool active);
  bool exists() const;

  template <typename T>
  bool has() const;
  template <typename T>
  T *get();
  template <typename T, typename... Args>
  T &emplace(Args &&...args);
  template <typename T>
  void erase();

private:
  EntityRef(World *world, EntityID entity_id)
      : m_world(world), m_entity_id(entity_id) {}

  World *m_world = nullptr;
  std::optional<EntityID> m_entity_id;

  friend class World;
  friend class CommandBuffer;
};

class CommandBuffer {
public:
  explicit CommandBuffer(World *world) : m_world(world) {}

  EntityRef spawn(std::string name, bool active = true);
  void destroy(EntityID entity_id);

  template <typename T, typename... Args>
  void emplace(EntityID entity_id, Args &&...args);

  template <typename T>
  void erase(EntityID entity_id);
  void set_name(EntityID entity_id, std::string name);
  void set_active(EntityID entity_id, bool active);

  void apply(World &world);
  void apply();

private:
  World *m_world = nullptr;
  std::vector<std::function<void(World &)>> m_commands;
  std::vector<EntityID> m_reserved_ids;
};

namespace detail {

struct ColumnBase {
  virtual ~ColumnBase() = default;
  virtual Scope<ColumnBase> clone_empty() const = 0;
  virtual void append_from(const ColumnBase &other, size_t row) = 0;
  virtual void swap_remove(size_t row) = 0;
  virtual void *raw_at(size_t row) = 0;
  virtual const void *raw_at(size_t row) const = 0;
};

template <typename T>
struct Column final : ColumnBase {
  std::vector<T> data;

  Scope<ColumnBase> clone_empty() const override {
    return create_scope<Column<T>>();
  }

  void append_from(const ColumnBase &other, size_t row) override {
    const auto &typed_other = static_cast<const Column<T> &>(other);
    data.push_back(typed_other.data[row]);
  }

  void swap_remove(size_t row) override {
    ASTRA_ENSURE(row >= data.size(), "column row is out of bounds");

    const size_t last_row = data.size() - 1u;
    if (row != last_row) {
      data[row] = std::move(data[last_row]);
    }

    data.pop_back();
  }

  void *raw_at(size_t row) override {
    ASTRA_ENSURE(row >= data.size(), "column row is out of bounds");
    return &data[row];
  }

  const void *raw_at(size_t row) const override {
    ASTRA_ENSURE(row >= data.size(), "column row is out of bounds");
    return &data[row];
  }
};

struct ArchetypeStorage {
  Signature signature;
  std::vector<EntityID> entity_ids;
  std::unordered_map<ComponentTypeID, Scope<ColumnBase>> columns;
};

} // namespace detail

struct EntityRecord {
  Signature signature;
  size_t archetype_index = 0u;
  size_t row = 0u;
  bool active = true;
};

class World {
public:
  World();

  EntityRef spawn(std::string name, bool active = true);
  EntityRef ensure(EntityID entity_id, std::string name, bool active = true);
  void destroy(EntityID entity_id);

  EntityRef entity(EntityID entity_id) { return EntityRef(this, entity_id); }
  bool contains(EntityID entity_id) const {
    return m_entity_records.find(entity_id) != m_entity_records.end();
  }

  template <typename T>
  bool has(EntityID entity_id) const {
    auto it = m_entity_records.find(entity_id);
    return it != m_entity_records.end() &&
           it->second.signature.test(component_type_id<T>());
  }

  template <typename T>
  T *get(EntityID entity_id) {
    auto *record = find_record(entity_id);
    if (record == nullptr || !record->signature.test(component_type_id<T>())) {
      return nullptr;
    }

    auto &archetype = m_archetypes[record->archetype_index];
    auto column_it = archetype.columns.find(component_type_id<T>());
    if (column_it == archetype.columns.end()) {
      return nullptr;
    }

    return static_cast<T *>(column_it->second->raw_at(record->row));
  }

  template <typename T>
  const T *get(EntityID entity_id) const {
    const auto *record = find_record(entity_id);
    if (record == nullptr || !record->signature.test(component_type_id<T>())) {
      return nullptr;
    }

    const auto &archetype = m_archetypes[record->archetype_index];
    auto column_it = archetype.columns.find(component_type_id<T>());
    if (column_it == archetype.columns.end()) {
      return nullptr;
    }

    return static_cast<const T *>(column_it->second->raw_at(record->row));
  }

  template <typename T, typename... Args>
  T &emplace(EntityID entity_id, Args &&...args) {
    T component = detail::make_component<T>(std::forward<Args>(args)...);

    if (auto *existing = get<T>(entity_id); existing != nullptr) {
      *existing = std::move(component);
      touch();
      return *existing;
    }

    auto &record = require_record(entity_id);
    Signature new_signature = record.signature;
    new_signature.set(component_type_id<T>());

    migrate_entity(entity_id, new_signature, [&](detail::ArchetypeStorage &arch) {
      auto &column = ensure_column<T>(arch);
      column.data.push_back(std::move(component));
    });

    touch();
    return *get<T>(entity_id);
  }

  template <typename T>
  void erase(EntityID entity_id) {
    auto *record = find_record(entity_id);
    if (record == nullptr || !record->signature.test(component_type_id<T>())) {
      return;
    }

    Signature new_signature = record->signature;
    new_signature.reset(component_type_id<T>());
    migrate_entity(entity_id, new_signature, [](detail::ArchetypeStorage &) {});
    touch();
  }

  template <typename... Ts, typename Fn>
  void each(Fn &&fn) {
    Signature required;
    (required.set(component_type_id<Ts>()), ...);

    for (auto &archetype : m_archetypes) {
      if (!archetype.signature.contains(required) ||
          archetype.entity_ids.empty()) {
        continue;
      }

      for (size_t row = 0; row < archetype.entity_ids.size(); ++row) {
        if constexpr (sizeof...(Ts) == 0u) {
          fn(archetype.entity_ids[row]);
        } else {
          fn(archetype.entity_ids[row],
             *static_cast<Ts *>(archetype.columns.at(component_type_id<Ts>())
                                    ->raw_at(row))...);
        }
      }
    }
  }

  template <typename... Ts, typename Fn>
  void each(Fn &&fn) const {
    Signature required;
    (required.set(component_type_id<Ts>()), ...);

    for (const auto &archetype : m_archetypes) {
      if (!archetype.signature.contains(required) ||
          archetype.entity_ids.empty()) {
        continue;
      }

      for (size_t row = 0; row < archetype.entity_ids.size(); ++row) {
        if constexpr (sizeof...(Ts) == 0u) {
          fn(archetype.entity_ids[row]);
        } else {
          fn(archetype.entity_ids[row],
             *static_cast<const Ts *>(
                 archetype.columns.at(component_type_id<Ts>())
                     ->raw_at(row)
             )...);
        }
      }
    }
  }

  template <typename... Ts>
  size_t count() const {
    size_t total = 0u;
    each<Ts...>([&](auto &&...) { total++; });
    return total;
  }

  size_t size() const { return m_entity_records.size(); }
  bool empty() const { return m_entity_records.empty(); }
  uint64_t revision() const { return m_revision; }

  std::string_view name(EntityID entity_id) const {
    return require_name(entity_id);
  }

  void set_name(EntityID entity_id, std::string name) {
    require_record(entity_id);
    m_entity_names[entity_id] = std::move(name);
    touch();
  }

  bool active(EntityID entity_id) const {
    return require_record(entity_id).active;
  }

  void set_active(EntityID entity_id, bool active) {
    require_record(entity_id).active = active;
    touch();
  }

  CommandBuffer commands() { return CommandBuffer(this); }
  void touch() { ++m_revision; }

private:
  EntityRecord *find_record(EntityID entity_id) {
    auto it = m_entity_records.find(entity_id);
    return it != m_entity_records.end() ? &it->second : nullptr;
  }

  const EntityRecord *find_record(EntityID entity_id) const {
    auto it = m_entity_records.find(entity_id);
    return it != m_entity_records.end() ? &it->second : nullptr;
  }

  EntityRecord &require_record(EntityID entity_id) {
    auto *record = find_record(entity_id);
    ASTRA_ENSURE(record == nullptr, "entity not found");
    return *record;
  }

  const EntityRecord &require_record(EntityID entity_id) const {
    auto *record = find_record(entity_id);
    ASTRA_ENSURE(record == nullptr, "entity not found");
    return *record;
  }

  std::string_view require_name(EntityID entity_id) const {
    auto it = m_entity_names.find(entity_id);
    ASTRA_ENSURE(it == m_entity_names.end(), "entity name not found");
    return it->second;
  }

  template <typename T>
  detail::Column<T> &ensure_column(detail::ArchetypeStorage &archetype) {
    auto [it, inserted] =
        archetype.columns.emplace(component_type_id<T>(), nullptr);
    if (inserted) {
      it->second = create_scope<detail::Column<T>>();
    }

    return static_cast<detail::Column<T> &>(*it->second);
  }

  detail::ColumnBase &
  ensure_compatible_column(detail::ArchetypeStorage &archetype, ComponentTypeID type_id, const detail::ColumnBase &source_column) {
    auto [it, inserted] = archetype.columns.emplace(type_id, nullptr);
    if (inserted) {
      it->second = source_column.clone_empty();
    }

    return *it->second;
  }

  void migrate_entity(EntityID entity_id, const Signature &new_signature, const std::function<void(detail::ArchetypeStorage &)> &append_new_components) {
    EntityRecord previous = require_record(entity_id);
    ASTRA_ENSURE(previous.signature == new_signature, "migrate_entity called without a signature change");

    const size_t new_archetype_index = ensure_archetype(new_signature);
    detail::ArchetypeStorage &new_archetype = m_archetypes[new_archetype_index];
    const size_t new_row = new_archetype.entity_ids.size();

    const auto &old_archetype = m_archetypes[previous.archetype_index];
    for (const auto &[type_id, old_column] : old_archetype.columns) {
      if (!new_signature.test(type_id)) {
        continue;
      }

      auto &new_column =
          ensure_compatible_column(new_archetype, type_id, *old_column);
      new_column.append_from(*old_column, previous.row);
    }

    append_new_components(new_archetype);

    new_archetype.entity_ids.push_back(entity_id);

    auto &record = require_record(entity_id);
    record.signature = new_signature;
    record.archetype_index = new_archetype_index;
    record.row = new_row;

    swap_remove(previous.archetype_index, previous.row, entity_id);
  }

  size_t ensure_archetype(const Signature &signature) {
    auto it = m_archetype_lookup.find(signature);
    if (it != m_archetype_lookup.end()) {
      return it->second;
    }

    const size_t index = m_archetypes.size();
    m_archetypes.push_back(detail::ArchetypeStorage{.signature = signature});
    m_archetype_lookup.emplace(signature, index);
    return index;
  }

  EntityID allocate_entity_id() const {
    EntityID entity_id;
    while (contains(entity_id)) {
      entity_id = EntityID();
    }
    return entity_id;
  }

  EntityRef spawn_with_id(EntityID entity_id, std::string name, bool active) {
    ASTRA_ENSURE(contains(entity_id), "entity already exists");

    const size_t archetype_index = ensure_archetype(Signature{});
    auto &archetype = m_archetypes[archetype_index];
    const size_t row = archetype.entity_ids.size();
    archetype.entity_ids.push_back(entity_id);

    m_entity_records.emplace(entity_id, EntityRecord{
                                            .signature = Signature{},
                                            .archetype_index = archetype_index,
                                            .row = row,
                                            .active = active,
                                        });
    m_entity_names.emplace(entity_id, std::move(name));
    touch();

    return EntityRef(this, entity_id);
  }

  void swap_remove(size_t archetype_index, size_t row, EntityID) {
    auto &archetype = m_archetypes[archetype_index];
    ASTRA_ENSURE(row >= archetype.entity_ids.size(), "archetype row is out of bounds");

    const size_t last_row = archetype.entity_ids.size() - 1u;
    const EntityID moved_entity = archetype.entity_ids[last_row];

    for (auto &[_, column] : archetype.columns) {
      column->swap_remove(row);
    }

    if (row != last_row) {
      archetype.entity_ids[row] = moved_entity;
      require_record(moved_entity).row = row;
    }

    archetype.entity_ids.pop_back();
  }

  std::vector<detail::ArchetypeStorage> m_archetypes;
  std::unordered_map<Signature, size_t, SignatureHash> m_archetype_lookup;
  std::unordered_map<EntityID, EntityRecord> m_entity_records;
  std::unordered_map<EntityID, std::string> m_entity_names;
  uint64_t m_revision = 0u;

  friend class EntityRef;
  friend class CommandBuffer;
};

inline World::World() {
  m_archetypes.push_back(detail::ArchetypeStorage{});
  m_archetype_lookup.emplace(Signature{}, 0u);
}

inline EntityRef World::spawn(std::string name, bool active) {
  return spawn_with_id(allocate_entity_id(), std::move(name), active);
}

inline EntityRef World::ensure(EntityID entity_id, std::string name, bool active) {
  if (contains(entity_id)) {
    set_name(entity_id, std::move(name));
    set_active(entity_id, active);
    return EntityRef(this, entity_id);
  }

  return spawn_with_id(entity_id, std::move(name), active);
}

inline void World::destroy(EntityID entity_id) {
  auto it = m_entity_records.find(entity_id);
  if (it == m_entity_records.end()) {
    return;
  }

  const EntityRecord record = it->second;
  swap_remove(record.archetype_index, record.row, entity_id);

  m_entity_records.erase(entity_id);
  m_entity_names.erase(entity_id);
  touch();
}

inline EntityID EntityRef::id() const {
  ASTRA_ENSURE(m_entity_id == std::nullopt, "entity ref is empty");
  return *m_entity_id;
}

inline std::string_view EntityRef::name() const {
  ASTRA_ENSURE(m_world == nullptr, "entity ref is detached");
  return m_world->name(id());
}

inline void EntityRef::set_name(std::string name) {
  ASTRA_ENSURE(m_world == nullptr, "entity ref is detached");
  m_world->set_name(id(), std::move(name));
}

inline bool EntityRef::active() const {
  ASTRA_ENSURE(m_world == nullptr, "entity ref is detached");
  return exists() ? m_world->active(id()) : false;
}

inline void EntityRef::set_active(bool active) {
  ASTRA_ENSURE(m_world == nullptr, "entity ref is detached");
  m_world->set_active(id(), active);
}

inline bool EntityRef::exists() const {
  return m_world != nullptr && m_entity_id != std::nullopt &&
         m_world->contains(*m_entity_id);
}

template <typename T>
inline bool EntityRef::has() const {
  return exists() && m_world->has<T>(id());
}

template <typename T>
inline T *EntityRef::get() {
  return exists() ? m_world->get<T>(id()) : nullptr;
}

template <typename T, typename... Args>
inline T &EntityRef::emplace(Args &&...args) {
  ASTRA_ENSURE(!exists(), "cannot emplace on an invalid entity");
  return m_world->emplace<T>(id(), std::forward<Args>(args)...);
}

template <typename T>
inline void EntityRef::erase() {
  if (!exists()) {
    return;
  }

  m_world->erase<T>(id());
}

inline EntityRef CommandBuffer::spawn(std::string name, bool active) {
  ASTRA_ENSURE(m_world == nullptr, "command buffer is detached");

  EntityID entity_id = m_world->allocate_entity_id();
  while (std::find(m_reserved_ids.begin(), m_reserved_ids.end(), entity_id) !=
         m_reserved_ids.end()) {
    entity_id = m_world->allocate_entity_id();
  }

  m_reserved_ids.push_back(entity_id);
  m_commands.push_back(
      [entity_id, name = std::move(name), active](World &world) mutable {
        world.spawn_with_id(entity_id, std::move(name), active);
      }
  );

  return EntityRef(m_world, entity_id);
}

inline void CommandBuffer::destroy(EntityID entity_id) {
  m_commands.push_back([entity_id](World &world) { world.destroy(entity_id); });
}

template <typename T, typename... Args>
inline void CommandBuffer::emplace(EntityID entity_id, Args &&...args) {
  T component = detail::make_component<T>(std::forward<Args>(args)...);
  m_commands.push_back(
      [entity_id, component = std::move(component)](World &world) mutable {
        world.emplace<T>(entity_id, std::move(component));
      }
  );
}

template <typename T>
inline void CommandBuffer::erase(EntityID entity_id) {
  m_commands.push_back(
      [entity_id](World &world) { world.erase<T>(entity_id); }
  );
}

inline void CommandBuffer::set_name(EntityID entity_id, std::string name) {
  m_commands.push_back(
      [entity_id, name = std::move(name)](World &world) mutable {
        world.set_name(entity_id, std::move(name));
      }
  );
}

inline void CommandBuffer::set_active(EntityID entity_id, bool active) {
  m_commands.push_back([entity_id, active](World &world) {
    world.set_active(entity_id, active);
  });
}

inline void CommandBuffer::apply(World &world) {
  ASTRA_ENSURE(m_world != nullptr && m_world != &world, "command buffer cannot be applied to a different world");

  for (auto &command : m_commands) {
    command(world);
  }

  m_commands.clear();
  m_reserved_ids.clear();
}

inline void CommandBuffer::apply() {
  ASTRA_ENSURE(m_world == nullptr, "command buffer is detached");
  apply(*m_world);
}

} // namespace astralix::ecs
