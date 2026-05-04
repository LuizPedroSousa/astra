#include "system-manager.hpp"
#include "algorithm"
#include "map"
#include "time.hpp"
#include "trace.hpp"
#include <cstring>

namespace astralix {

SystemManager::SystemManager() {}

SystemManager::~SystemManager() {}

void SystemManager::start() {
  ASTRA_PROFILE_N("SystemManager::start");
  for (ISystem_ptr system : m_system_work_order) {
    if (system->m_enabled) {
      ASTRA_PROFILE_N("System::start");
      const char *name = system->get_system_type_name();
      ASTRA_PROFILE_TEXT(name, strlen(name));
      system->start();
    }
  }
}

void SystemManager::fixed_update(const double step_size) {
  ASTRA_PROFILE_N("SystemManager::fixed_update");
  m_accumulator_step += Time::get()->get_deltatime();

  while (m_accumulator_step >= step_size) {
    for (ISystem_ptr system : m_system_work_order) {
      if (system->m_enabled) {
        ASTRA_PROFILE_N("System::fixed_update");
        const char *name = system->get_system_type_name();
        ASTRA_PROFILE_TEXT(name, strlen(name));
        system->fixed_update(step_size);
      }
    }

    m_accumulator_step -= step_size;
  }
}

/**
 * \brief
 * \param dt_ms
 */
void SystemManager::update(const double dt_ms) {
  ASTRA_PROFILE_N("SystemManager::update");
  for (ISystem_ptr system : this->m_system_work_order) {
    system->m_time_since_last_update = dt_ms;

    system->m_needs_update =
        (system->m_updater_internal < 0.0f) ||
        ((system->m_updater_internal > 0.0f) &&
         (system->m_time_since_last_update > system->m_updater_internal));

    if (system->m_enabled && system->m_needs_update) {
      ASTRA_PROFILE_N("System::pre_update");
      const char *name = system->get_system_type_name();
      ASTRA_PROFILE_TEXT(name, strlen(name));
      system->pre_update(dt_ms);
    }
  }

  for (ISystem_ptr system : this->m_system_work_order) {
    if (system->m_enabled && system->m_needs_update) {
      ASTRA_PROFILE_N("System::update");
      const char *name = system->get_system_type_name();
      ASTRA_PROFILE_TEXT(name, strlen(name));
      system->update(dt_ms);
      system->m_time_since_last_update = dt_ms;
    }
  }
}

void SystemManager::update_system_work_order() {
  const size_t system_count = this->m_system_dependency_table.size();

  // create index array
  std::vector<int> indices(system_count);

  for (size_t i = 0; i < system_count; ++i)
    indices[i] = static_cast<int>(i);

  // determine vertex-groups
  std::vector<std::vector<SystemTypeID>> vertex_groups;
  std::vector<SystemPriority> group_priority;

  while (!indices.empty()) {
    SystemTypeID index = indices.back();
    indices.pop_back();

    if (index == -1)
      continue;

    std::vector<SystemTypeID> group;
    std::vector<SystemTypeID> member;

    SystemPriority current_group_priority = LOWEST_SYSTEM_PRIORITY;
    member.push_back(index);

    while (!member.empty()) {
      index = member.back();
      member.pop_back();

      for (int i = 0; i < static_cast<int>(indices.size()); ++i) {
        if (indices[i] != -1 &&
            (this->m_system_dependency_table[i][index] == true ||
             this->m_system_dependency_table[index][i] == true)) {
          member.push_back(i);
          indices[i] = -1;
        }
      }

      group.push_back(index);

      const ISystem_ptr sys = this->m_system_table[index];
      current_group_priority =
          std::max((sys != nullptr ? sys->m_priority : NORMAL_SYSTEM_PRIORITY), current_group_priority);
    }

    vertex_groups.push_back(group);
    group_priority.push_back(current_group_priority);
  }

  const size_t vertex_group_count = vertex_groups.size();

  // do a topological sort on groups w.r.t. groups priority!
  std::vector<int> vertex_states(system_count, 0);

  std::multimap<SystemPriority, std::vector<SystemTypeID>> sorted_vertex_groups;

  for (size_t i = 0; i < vertex_group_count; ++i) {
    auto g = vertex_groups[i];

    std::vector<SystemTypeID> order;

    for (size_t j = 0; j < g.size(); ++j) {
      if (vertex_states[g[j]] == 0)

        depth_first_search<SystemTypeID>(
            g[j], vertex_states, this->m_system_dependency_table, order
        );
    }

    std::reverse(order.begin(), order.end());

    sorted_vertex_groups.insert(
        std::pair<SystemPriority, std::vector<SystemTypeID>>(
            std::numeric_limits<SystemPriority>::max() - group_priority[i],
            order
        )
    );
  }

  // re-build system work order
  this->m_system_work_order.clear();
  for (auto group : sorted_vertex_groups) {
    for (auto m : group.second) {
      const ISystem_ptr sys = this->m_system_table[m];
      if (sys != nullptr) {
        this->m_system_work_order.push_back(sys);
      }
    }
  }
}

} // namespace astralix
