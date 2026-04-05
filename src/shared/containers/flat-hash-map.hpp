#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace astralix {

template <typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class FlatHashMap {
  static constexpr uint8_t k_empty_distance = 0u;
  static constexpr float k_max_load_factor = 0.75f;
  static constexpr size_t k_min_capacity = 64u;

  struct Slot {
    Key key{};
    Value value{};
    uint8_t distance = k_empty_distance;
  };

public:
  struct Proxy {
    const Key &first;
    Value &second;

    Proxy *operator->() { return this; }
    const Proxy *operator->() const { return this; }
  };

  struct ConstProxy {
    const Key &first;
    const Value &second;

    const ConstProxy *operator->() const { return this; }
  };

  struct Iterator {
    Slot *m_slot = nullptr;
    Slot *m_end = nullptr;

    Proxy operator*() const { return {m_slot->key, m_slot->value}; }
    Proxy operator->() const { return {m_slot->key, m_slot->value}; }

    bool operator==(const Iterator &other) const {
      return m_slot == other.m_slot;
    }

    bool operator!=(const Iterator &other) const {
      return m_slot != other.m_slot;
    }

    Iterator &operator++() {
      ++m_slot;
      skip_empty();
      return *this;
    }

    void skip_empty() {
      while (m_slot != m_end && m_slot->distance == k_empty_distance) {
        ++m_slot;
      }
    }
  };

  struct ConstIterator {
    const Slot *m_slot = nullptr;
    const Slot *m_end = nullptr;

    ConstProxy operator*() const { return {m_slot->key, m_slot->value}; }
    ConstProxy operator->() const { return {m_slot->key, m_slot->value}; }

    bool operator==(const ConstIterator &other) const {
      return m_slot == other.m_slot;
    }

    bool operator!=(const ConstIterator &other) const {
      return m_slot != other.m_slot;
    }

    ConstIterator &operator++() {
      ++m_slot;
      skip_empty();
      return *this;
    }

    void skip_empty() {
      while (m_slot != m_end && m_slot->distance == k_empty_distance) {
        ++m_slot;
      }
    }
  };

  FlatHashMap() = default;

  Iterator find(const Key &key) {
    if (m_slots.empty()) {
      return end();
    }
    const size_t hash = m_hash(key);
    size_t index = hash & m_mask;
    uint8_t distance = 1u;

    while (true) {
      Slot &slot = m_slots[index];
      if (slot.distance == k_empty_distance || distance > slot.distance) {
        return end();
      }
      if (slot.distance == distance && m_equal(slot.key, key)) {
        return {&slot, m_slots.data() + m_slots.size()};
      }
      ++distance;
      index = (index + 1u) & m_mask;
    }
  }

  ConstIterator find(const Key &key) const {
    if (m_slots.empty()) {
      return end();
    }
    const size_t hash = m_hash(key);
    size_t index = hash & m_mask;
    uint8_t distance = 1u;

    while (true) {
      const Slot &slot = m_slots[index];
      if (slot.distance == k_empty_distance || distance > slot.distance) {
        return end();
      }
      if (slot.distance == distance && m_equal(slot.key, key)) {
        return {&slot, m_slots.data() + m_slots.size()};
      }
      ++distance;
      index = (index + 1u) & m_mask;
    }
  }

  Value &operator[](const Key &key) {
    auto it = find(key);
    if (it != end()) {
      return it->second;
    }

    if (m_slots.empty()) {
      grow(k_min_capacity);
    } else if (should_grow()) {
      grow(m_slots.size() * 2u);
    }

    insert_entry(key, Value{});
    return find(key)->second;
  }

  void erase(const Key &key) {
    if (m_slots.empty()) {
      return;
    }

    const size_t hash = m_hash(key);
    size_t index = hash & m_mask;
    uint8_t distance = 1u;

    while (true) {
      Slot &slot = m_slots[index];
      if (slot.distance == k_empty_distance || distance > slot.distance) {
        return;
      }
      if (slot.distance == distance && m_equal(slot.key, key)) {
        slot.distance = k_empty_distance;
        --m_size;
        backward_shift(index);
        return;
      }
      ++distance;
      index = (index + 1u) & m_mask;
    }
  }

  Iterator begin() {
    Iterator it{m_slots.data(), m_slots.data() + m_slots.size()};
    it.skip_empty();
    return it;
  }

  Iterator end() {
    Slot *data_end = m_slots.data() + m_slots.size();
    return {data_end, data_end};
  }

  ConstIterator begin() const {
    ConstIterator it{m_slots.data(), m_slots.data() + m_slots.size()};
    it.skip_empty();
    return it;
  }

  ConstIterator end() const {
    const Slot *data_end = m_slots.data() + m_slots.size();
    return {data_end, data_end};
  }

  size_t size() const { return m_size; }
  bool empty() const { return m_size == 0u; }

  void clear() {
    for (auto &slot : m_slots) {
      slot.distance = k_empty_distance;
    }
    m_size = 0u;
  }

  void reserve(size_t count) {
    const size_t required =
        static_cast<size_t>(static_cast<float>(count) / k_max_load_factor) + 1u;
    size_t capacity = k_min_capacity;
    while (capacity < required) {
      capacity *= 2u;
    }
    if (capacity > m_slots.size()) {
      grow(capacity);
    }
  }

private:
  bool should_grow() const {
    return static_cast<float>(m_size + 1u) >
           static_cast<float>(m_slots.size()) * k_max_load_factor;
  }

  void grow(size_t new_capacity) {
    std::vector<Slot> old_slots = std::move(m_slots);
    m_slots.resize(new_capacity);
    m_mask = new_capacity - 1u;
    m_size = 0u;

    for (auto &slot : old_slots) {
      if (slot.distance != k_empty_distance) {
        insert_entry(std::move(slot.key), std::move(slot.value));
      }
    }
  }

  void insert_entry(Key key, Value value) {
    const size_t hash = m_hash(key);
    size_t index = hash & m_mask;
    uint8_t distance = 1u;

    while (true) {
      Slot &slot = m_slots[index];
      if (slot.distance == k_empty_distance) {
        slot.key = std::move(key);
        slot.value = std::move(value);
        slot.distance = distance;
        ++m_size;
        return;
      }
      if (distance > slot.distance) {
        std::swap(key, slot.key);
        std::swap(value, slot.value);
        std::swap(distance, slot.distance);
      }
      ++distance;
      index = (index + 1u) & m_mask;
    }
  }

  void backward_shift(size_t erased_index) {
    size_t current = erased_index;
    size_t next = (current + 1u) & m_mask;

    while (m_slots[next].distance > 1u) {
      m_slots[current] = std::move(m_slots[next]);
      m_slots[current].distance -= 1u;
      m_slots[next].distance = k_empty_distance;
      current = next;
      next = (next + 1u) & m_mask;
    }
  }

  std::vector<Slot> m_slots;
  size_t m_size = 0u;
  size_t m_mask = 0u;
  Hash m_hash;
  KeyEqual m_equal;
};

} // namespace astralix
