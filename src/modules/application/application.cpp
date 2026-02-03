#include "application.hpp"
#include "engine.hpp"
#include "event-dispatcher.hpp"
#include "event-scheduler.hpp"
#include "managers/project-manager.hpp"
#include "managers/system-manager.hpp"
#include "managers/window-manager.hpp"
#include "time.hpp"

namespace astralix {
Application *Application::m_instance = nullptr;

Application *Application::init() {
  m_instance = new Application;

  EventDispatcher::init();
  EventScheduler::init();
  Time::init();
  ProjectManager::init();
  Engine::init();
  SystemManager::init();
  WindowManager::init();

  return m_instance;
}

void Application::start() {
  WindowManager::get()->start();
  Engine::get()->start();
  SystemManager::get()->start();
}

void Application::run() {
  auto wm = WindowManager::get();
  Engine *engine = Engine::get();
  auto system = SystemManager::get();
  Time *time = Time::get();

  auto scheduler = EventScheduler::get();

  while (wm->is_open()) {
    time->update();
    wm->update();
    scheduler->bind(SchedulerType::POST_FRAME);
    system->fixed_update(1 / 60.0f);
    system->update(Time::get()->get_deltatime());
    scheduler->bind(SchedulerType::IMMEDIATE);
    wm->swap();
  }

  delete m_instance;
}

void Application::end() { delete m_instance; }

Application::~Application() {
  Engine::get()->end();
  Time::get()->end();
  WindowManager::get()->end();
}

} // namespace astralix
