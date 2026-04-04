#pragma once

#include "assert.hpp"
#include "components/tags.hpp"
#include "entities/derived-entity-id.hpp"
#include "entities/derived-override.hpp"
#include "serializers/scene-component-serialization.hpp"
#include "serializers/scene-snapshot-types.hpp"
#include "world.hpp"

#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace astralix {

class SceneBuildEntityBuilder {
public:
  explicit SceneBuildEntityBuilder(serialization::EntitySnapshot *snapshot)
      : m_snapshot(snapshot) {}

  template <typename T>
  SceneBuildEntityBuilder &component(const T &component) {
    ASTRA_ENSURE(m_snapshot == nullptr, "Build entity builder is detached");
    m_snapshot->components.push_back(serialization::snapshot_component(component));
    return *this;
  }

private:
  serialization::EntitySnapshot *m_snapshot = nullptr;
};

class SceneBuildPassBuilder {
public:
  struct Request {
    std::string stable_key;
    serialization::EntitySnapshot snapshot;
  };

  struct Pass {
    std::string generator_id;
    std::unordered_set<std::string> stable_keys;
    std::vector<Request> requests;
  };

  explicit SceneBuildPassBuilder(Pass *pass) : m_pass(pass) {}

  SceneBuildEntityBuilder entity(
      std::string stable_key, std::string name, bool active = true
  ) {
    ASTRA_ENSURE(m_pass == nullptr, "Build pass builder is detached");
    ASTRA_ENSURE(stable_key.empty(), "Build entity stable key is required");
    ASTRA_ENSURE(!m_pass->stable_keys.insert(stable_key).second, "Duplicate build entity stable key in generator ", m_pass->generator_id, ": ", stable_key);

    auto &request = m_pass->requests.emplace_back(Request{
        .stable_key = stable_key,
        .snapshot = serialization::EntitySnapshot{
            .name = std::move(name),
            .active = active,
        },
    });
    request.snapshot.components.push_back(
        serialization::snapshot_component(scene::SceneEntity{})
    );
    request.snapshot.components.push_back(
        serialization::snapshot_component(scene::DerivedEntity{})
    );
    request.snapshot.components.push_back(
        serialization::snapshot_component(scene::MetaEntityOwner{
            .generator_id = m_pass->generator_id,
            .stable_key = request.stable_key,
        })
    );

    return SceneBuildEntityBuilder(&request.snapshot);
  }

private:
  Pass *m_pass = nullptr;
};

class SceneBuildContext {
public:
  explicit SceneBuildContext(
      ecs::World &world, const DerivedState *derived_state = nullptr
  )
      : m_world(world), m_derived_state(derived_state) {}

  SceneBuildPassBuilder begin_pass(std::string generator_id) {
    ASTRA_ENSURE(generator_id.empty(), "Build generator id is required");
    ASTRA_ENSURE(!m_generator_ids.insert(generator_id).second, "Duplicate build generator id: ", generator_id);
    auto &pass = m_passes.emplace_back(
        SceneBuildPassBuilder::Pass{.generator_id = std::move(generator_id)}
    );
    return SceneBuildPassBuilder(&pass);
  }

  void apply() {
    using ExistingEntities =
        std::unordered_map<std::string, std::unordered_map<std::string, EntityID>>;

    ExistingEntities existing_entities;
    m_world.each<scene::MetaEntityOwner>(
        [&](EntityID entity_id, const scene::MetaEntityOwner &owner) {
          existing_entities[owner.generator_id].insert_or_assign(
              owner.stable_key, entity_id
          );
        }
    );

    for (auto &pass : m_passes) {
      const auto existing_it = existing_entities.find(pass.generator_id);
      const auto existing =
          existing_it != existing_entities.end() ? existing_it->second
                                                 : std::unordered_map<std::string, EntityID>{};
      std::unordered_set<std::string> touched_keys;

      for (auto request : pass.requests) {
        if (m_derived_state != nullptr &&
            is_derived_suppressed(
                *m_derived_state, pass.generator_id, request.stable_key
            )) {
          continue;
        }

        if (m_derived_state != nullptr) {
          if (const auto *override_record = find_derived_override(
                  *m_derived_state, pass.generator_id, request.stable_key
              );
              override_record != nullptr) {
            apply_derived_override(request.snapshot, *override_record);
          }
        }

        EntityID entity_id;
        if (const auto matched = existing.find(request.stable_key);
            matched != existing.end()) {
          entity_id = matched->second;
          m_world.destroy(entity_id);
          (void)m_world.ensure(
              entity_id, request.snapshot.name, request.snapshot.active
          );
        } else {
          entity_id = deterministic_derived_entity_id(
              pass.generator_id, request.stable_key, m_world
          );
          (void)m_world.ensure(
              entity_id, request.snapshot.name, request.snapshot.active
          );
        }

        request.snapshot.id = entity_id;

        auto entity = m_world.entity(entity_id);
        for (const auto &component : request.snapshot.components) {
          serialization::apply_component_snapshot(entity, component);
        }

        touched_keys.insert(request.stable_key);
      }

      if (existing_it == existing_entities.end()) {
        continue;
      }

      for (const auto &[stable_key, entity_id] : existing_it->second) {
        if (!touched_keys.contains(stable_key)) {
          m_world.destroy(entity_id);
        }
      }
    }
  }

private:
  ecs::World &m_world;
  const DerivedState *m_derived_state = nullptr;
  std::deque<SceneBuildPassBuilder::Pass> m_passes;
  std::unordered_set<std::string> m_generator_ids;
};

} // namespace astralix
