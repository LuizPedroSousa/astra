#include "event-dispatcher.hpp"
#include "event-scheduler.hpp"
#include "events/logs-event.hpp"
#include "events/mouse.hpp"
#include "glad/glad.h"

#include "editor.hpp"

#include "editor.hpp"
#include "engine.hpp"
#include "events/viewport-event.hpp"
#include "layers/systems/layer-system.hpp"
#include "managers/system-manager.hpp"
#include "time.hpp"
#include "websocket-server.hpp"
#include "window.hpp"

namespace astralix {

#ifdef ASTRA_EDITOR_USOCKET
#define OFFSECREN 1
#else
#define OFFSECREN 0
#endif

Editor *Editor::m_instance = nullptr;

Editor *Editor::init() {
  if (m_instance == nullptr) {
    m_instance = new Editor;
  }

  return m_instance;
}

void Editor::end() { delete m_instance; }

Editor::Editor() : m_arena(KB(1)) {
  EventDispatcher::init();
  EventScheduler::init();
  WebsocketServer::init();
  Time::init();
  SystemManager::init();
  Engine::init();
  Window::init();
}

void Editor::start() {
  Window::get()->open("FVM Editor", 1920, 1080, OFFSECREN);
  Engine::get()->start();
  auto system_manager = SystemManager::get();

#ifdef ASTRA_EDITOR_USOCKET

  system_manager->start();
  WebsocketServer::get()->start();

  auto scheduler = EventScheduler::get();
  auto dispatcher = EventDispatcher::get();
  scheduler->schedule<ViewportEvent>(SchedulerType::REALTIME);
  scheduler->schedule<LogsEvent>(SchedulerType::REALTIME);
  dispatcher->subscribe<ViewportEventListener, ViewportEvent>(m_arena);
#else
  system_manager->add_system<LayerSystem>();
  system_manager->start();
#endif

  //
  // dispatcher->subscribe<LogsEventListener, LogsEvent>(m_arena);
}

void Editor::run() {

#ifdef ASTRA_EDITOR_USOCKET
  auto server = WebsocketServer::get();
  auto delayTimer = server->get_timer();

  us_timer_set(
      delayTimer,
      [](struct us_timer_t *t) {
        ASTRA_PROFILE_N("Editor run");

        Engine *engine = Engine::get();
        auto system = SystemManager::get();
        Time *time = Time::get();

        Window *window = Window::get();

        auto scheduler = EventScheduler::get();

        time->update();
        window->update();
        scheduler->bind(SchedulerType::POST_FRAME);
        system->fixed_update(1 / 60.0f);
        system->update(Time::get()->get_deltatime());
        scheduler->bind(SchedulerType::IMMEDIATE);
        window->swap();
      },
      16, 16);

  server->update();

#else
  Window *window = Window::get();
  Engine *engine = Engine::get();
  auto system = SystemManager::get();
  Time *time = Time::get();

  auto scheduler = EventScheduler::get();

  while (window->is_open()) {
    time->update();
    window->update();
    scheduler->bind(SchedulerType::POST_FRAME);
    system->fixed_update(1 / 60.0f);
    system->update(Time::get()->get_deltatime());
    scheduler->bind(SchedulerType::IMMEDIATE);
    window->swap();
  }

  delete m_instance;
#endif
}
} // namespace astralix
