#include "application.hpp"
#include "application-plugin-registry.hpp"
#include "console.hpp"
#include "engine.hpp"
#include "event-dispatcher.hpp"
#include "event-scheduler.hpp"
#include "managers/project-manager.hpp"
#include "managers/system-manager.hpp"
#include "managers/window-manager.hpp"
#include "time.hpp"
#include "trace.hpp"

namespace astralix {
Application *Application::m_instance = nullptr;

Application *Application::init() {
  ASTRA_PROFILE_N("Application::init");
  m_instance = new Application;

  EventDispatcher::init();
  EventScheduler::init();
  Time::init();
  ConsoleManager::get();
  ProjectManager::init();
  Engine::init();
  SystemManager::init();
  WindowManager::init();
  if (ApplicationPluginRegistry::get() == nullptr) {
    ApplicationPluginRegistry::init();
  }

  return m_instance;
}

void Application::start() {
  ASTRA_PROFILE_N("Application::start");
  WindowManager::get()->start();
  Engine::get()->start();

  ApplicationPluginContext plugin_context{
      .project = active_project(),
      .systems = SystemManager::get(),
  };
  application_plugin_registry()->apply_plugins(plugin_context);

  SystemManager::get()->start();
}

void Application::run() {
  auto wm = WindowManager::get();
  Engine *engine = Engine::get();
  auto system = SystemManager::get();
  Time *time = Time::get();

  auto scheduler = EventScheduler::get();

  while (wm->is_open()) {
    ASTRA_FRAME_MARK;
    ASTRA_PROFILE_N("Application::frame");
    time->update();
    {
      ASTRA_PROFILE_N("WindowManager::update");
      wm->update();
    }
    {
      ASTRA_PROFILE_N("EventScheduler::POST_FRAME");
      scheduler->bind(SchedulerType::POST_FRAME);
    }
    system->fixed_update(1 / 60.0f);
    system->update(Time::get()->get_deltatime());
    {
      ASTRA_PROFILE_N("EventScheduler::IMMEDIATE");
      scheduler->bind(SchedulerType::IMMEDIATE);
    }
    {
      ASTRA_PROFILE_N("WindowManager::swap");
      wm->swap();
    }
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
