#pragma once
#include "assert.hpp"
#include "base.hpp"
#include "event.hpp"
#include "listener.hpp"
#include "vector"

#include "unordered_map"
#include <utility>

namespace astralix {
class EventDispatcher {
public:
  static void init();

  static EventDispatcher *get();

  template <typename L, typename E, typename... Args>
  void subscribe(Args &&...args) {
    Ref<L> listener = create_ref<L>(std::forward<Args>(args)...);

    auto listener_id = listener->get_id();
    auto emplaced_listener =
        m_listeners.emplace(listener_id, std::move(listener));
    ASTRA_ENSURE(!emplaced_listener.second,

                 "Error creating new Listener!");
    auto type = E::get_static_type();

    auto it = m_event_listeners.find(type);

    if (it != m_event_listeners.end()) {
      auto event = it->second;
      it->second.push_back(listener_id);
      return;
    }

    std::vector<ListenerId> listeners{listener_id};

    auto emplaced_event = m_event_listeners.emplace(type, listeners);

    ASTRA_ENSURE(!emplaced_event.second, "Error creating new Event Listener!");
  }

  // L -> listener
  // E -> an event
  // F -> callback function deduced by the compiler
  template <typename L, typename E, typename F>
  void attach(F callback) {
    auto typed_callback = [callback](Event *event) {
      // Ensure the event is of the expected type
      E *typed_event = static_cast<E *>(event);
      if (typed_event) {
        callback(typed_event);
      } else {
        ASTRA_EXCEPTION("Event type mismatch! Expected: " + std::string(typeid(E).name()));
      }
    };

    Ref<L> listener = create_ref<L>(typed_callback);
    auto listener_id = listener->get_id();
    auto emplaced_listener =
        m_listeners.emplace(listener_id, std::move(listener));
    ASTRA_ENSURE(!emplaced_listener.second,

                 "Error creating new Listener!");
    auto type = E::get_static_type();

    auto it = m_event_listeners.find(type);

    if (it != m_event_listeners.end()) {
      auto event = it->second;
      it->second.push_back(listener_id);
      return;
    }

    std::vector<ListenerId> listeners{listener_id};

    auto emplaced_event = m_event_listeners.emplace(type, listeners);

    ASTRA_ENSURE(!emplaced_event.second, "Error creating new Event Listener!");
  }

  template <typename E>
  void dispatch(E *event) {
    auto type = event->get_event_type();

    auto it = m_event_listeners.find(type);

    if (it != m_event_listeners.end()) {
      auto event_listeners = it->second;

      // #pragma omp parallel for
      for (auto &listener_id : event_listeners) {
        auto listener = m_listeners.at(listener_id);
        listener->dispatch(event);
      }
    }
  }

  template <typename E>
  void dispatch(EventType type, E *event) {
    auto it = m_event_listeners.find(type);

    if (it != m_event_listeners.end()) {
      auto event_listeners = it->second;

      // #pragma omp parallel for
      for (auto &listener_id : event_listeners) {
        auto listener = m_listeners.at(listener_id);
        listener->dispatch(event);
      }
    }
  }

  std::unordered_map<ListenerId, Ref<BaseListener>> m_listeners;
  std::unordered_map<EventType, std::vector<ListenerId>> m_event_listeners;

private:
  EventDispatcher() {};
  static EventDispatcher *m_instance;
};
} // namespace astralix
