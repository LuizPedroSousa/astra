#pragma once
#include "assert.hpp"
#include "base.hpp"
#include "event-dispatcher.hpp"
#include "event.hpp"
#include "guid.hpp"
#include "log.hpp"
#include "vector"

#include "unordered_map"
#include <algorithm>
namespace astralix {

  enum SchedulerType {
    FRAME = 0,      // Executes once per frame (classic update loop)
    TICK = 1,       // Executes at a fixed timestep (good for physics updates)
    REALTIME = 2,   // Executes based on real-world clock time
    INTERVAL = 3,   // Executes every X milliseconds
    DEFERRED = 4,   // Executes at the end of the frame
    IMMEDIATE = 4,  // Executes instantly when scheduled
    BACKGROUND = 5, // Runs at a lower priority, for non-essential tasks
    POST_FRAME = 6, // Executes after rendering but before swap
    PRE_FRAME = 7,  // Executes before the frame starts
    IDLE = 8,       // Executes when there’s nothing else to do
  };

  struct Scheduler {
    SchedulerID id;
    SchedulerType type;
    Ref<Event> event;
  };

  class EventScheduler {
  public:
    static void init();

    static EventScheduler* get();

    bool has_schedulers() { return m_schedulers.size() > 0; }

    template <typename E, typename... Args>
    SchedulerID schedule(SchedulerType type, Args &&...args) {
      auto event = create_ref<E>(std::forward<Args>(args)...);

      auto it = m_type_schedulers.find(type);

      Guid scheduler_id;

      Scheduler scheduler = {
          .id = scheduler_id, .type = type, .event = std::move(event) };

      if (it != m_type_schedulers.end()) {

        auto emplaced_event = m_schedulers.emplace(scheduler_id, scheduler);

        ASTRA_ENSURE(!emplaced_event.second,
          "Error creating new Event Scheduler! Type: " +
          std::to_string(static_cast<int>(type)));

        it->second.push_back(scheduler_id);

        return scheduler_id;
      }

      auto emplaced_event = m_schedulers.emplace(scheduler_id, scheduler);

      ASTRA_ENSURE(!emplaced_event.second,
        "Error creating new Event Scheduler!");

      std::vector<SchedulerID> type_schedulers = { scheduler_id };

      auto emplaced_scheduler_type =
        m_type_schedulers.emplace(type, type_schedulers);

      ASTRA_ENSURE(!emplaced_scheduler_type.second,
        "Error creating new Event Scheduler!");

      return scheduler_id;
    }

    void bind(SchedulerType type) {
      auto type_scheduler = m_type_schedulers.find(type);

      if (type_scheduler == m_type_schedulers.end()) {
        return;
      }

      auto dispatcher = EventDispatcher::get();
      auto scheduler_ids = type_scheduler->second;

      for (const auto &schedule_id : scheduler_ids) {
        auto id_scheduler = m_schedulers.find(schedule_id);

        if (id_scheduler != m_schedulers.end()) {
          dispatcher->dispatch(id_scheduler->second.event.get());
        }
      }
    }

    void destroy(SchedulerID scheduler_id) {
      auto scheduler = m_schedulers.find(scheduler_id);

      if (scheduler == m_schedulers.end()) {
        return;
      }

      const auto scheduler_type = scheduler->second.type;
      m_schedulers.erase(scheduler);

      auto type_schedulers = m_type_schedulers.find(scheduler_type);

      if (type_schedulers == m_type_schedulers.end()) {
        return;
      }

      auto &scheduler_ids = type_schedulers->second;
      auto it =
          std::find(scheduler_ids.begin(), scheduler_ids.end(), scheduler_id);

      if (it == scheduler_ids.end()) {
        return;
      }

      scheduler_ids.erase(it);

      if (scheduler_ids.empty()) {
        m_type_schedulers.erase(type_schedulers);
      }
    }

    std::unordered_map<SchedulerID, Scheduler> m_schedulers;
    std::unordered_map<SchedulerType, std::vector<SchedulerID>> m_type_schedulers;

  private:
    EventScheduler() = default;
    static EventScheduler* m_instance;
  };
} // namespace astralix
