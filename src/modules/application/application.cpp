#include "application.hpp"
#include "application-plugin-registry.hpp"
#include "console.hpp"
#include "engine.hpp"
#include "event-dispatcher.hpp"
#include "event-scheduler.hpp"
#include "managers/project-manager.hpp"
#include "managers/system-manager.hpp"
#include "managers/window-manager.hpp"
#include "systems/job-system/job-system.hpp"
#include "time.hpp"
#include "trace.hpp"
#if defined(ASTRA_RENDERER_HOT_RELOAD)
#include "module-handle.hpp"
#include "systems/render-system/render-system.hpp"
#endif

namespace astralix {

namespace {

constexpr size_t k_main_thread_job_drain_budget = 2u;

} // namespace

Application *Application::m_instance = nullptr;

Application *Application::init() {
  ASTRA_PROFILE_N("Application::init");
  m_instance = new Application;

  {
    ASTRA_PROFILE_N("EventDispatcher::init");
    EventDispatcher::init();
  }
  {
    ASTRA_PROFILE_N("EventScheduler::init");
    EventScheduler::init();
  }
  {
    ASTRA_PROFILE_N("Time::init");
    Time::init();
  }
  {
    ASTRA_PROFILE_N("ConsoleManager::init");
    ConsoleManager::get();
  }
  {
    ASTRA_PROFILE_N("ProjectManager::init");
    ProjectManager::init();
  }
  {
    ASTRA_PROFILE_N("Engine::init");
    Engine::init();
  }
  {
    ASTRA_PROFILE_N("SystemManager::init");
    SystemManager::init();
  }
  {
    ASTRA_PROFILE_N("WindowManager::init");
    WindowManager::init();
  }
  if (ApplicationPluginRegistry::get() == nullptr) {
    ASTRA_PROFILE_N("ApplicationPluginRegistry::init");
    ApplicationPluginRegistry::init();
  }

  return m_instance;
}

void Application::start() {
  ASTRA_PROFILE_N("Application::start");
  {
    ASTRA_PROFILE_N("WindowManager::start");
    WindowManager::get()->start();
  }
  {
    ASTRA_PROFILE_N("Engine::start");
    Engine::get()->start();
  }

  {
    ASTRA_PROFILE_N("ApplicationPluginRegistry::apply_plugins");
    ApplicationPluginContext plugin_context{
        .project = active_project(),
        .systems = SystemManager::get(),
    };
    application_plugin_registry()->apply_plugins(plugin_context);
  }

  {
    ASTRA_PROFILE_N("SystemManager::start");
    SystemManager::get()->start();
  }
}

void Application::run() {
  auto wm = WindowManager::get();
  Engine *engine = Engine::get();
  auto system = SystemManager::get();
  Time *time = Time::get();

  auto scheduler = EventScheduler::get();

#if defined(ASTRA_RENDERER_HOT_RELOAD)
  ModuleHandle render_passes_module(ModuleHandle::Config{
      .module_path = ASTRA_RENDER_PASSES_MODULE_PATH,
      .source_dir = ASTRA_RENDER_PASSES_SOURCE_DIR,
      .build_dir = ASTRA_RENDER_PASSES_BUILD_DIR,
      .build_target = "render_passes_live",
  });
  {
    ASTRA_PROFILE_N("RenderPassesLoader::initial_load");
    if (render_passes_module.load()) {
      auto *render_system = SystemManager::get()->get_system<RenderSystem>();
      if (render_system != nullptr) {
        render_system->load_passes_from_module(render_passes_module.api());
      }
    }
  }
#endif

  while (wm->is_open()) {
    if (m_pre_frame_callback) m_pre_frame_callback();
#if defined(ASTRA_RENDERER_HOT_RELOAD)
    if (render_passes_module.poll_changed()) {
      auto *render_system = system->get_system<RenderSystem>();
      if (render_system != nullptr) {
        render_system->prepare_for_pass_reload();
      }
      render_passes_module.api()->unload();
      if (render_passes_module.load() && render_system != nullptr) {
        render_system->load_passes_from_module(render_passes_module.api());
      }
    }
#endif
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
    {
      ASTRA_PROFILE_N("JobSystem::drain_main_queue_pre_update");
      if (auto *jobs = JobSystem::get(); jobs != nullptr) {
        jobs->drain_main_queue(k_main_thread_job_drain_budget);
      }
    }
    system->fixed_update(1 / 60.0f);
    system->update(Time::get()->get_deltatime());
    {
      ASTRA_PROFILE_N("JobSystem::drain_main_queue_post_update");
      if (auto *jobs = JobSystem::get(); jobs != nullptr) {
        jobs->drain_main_queue(k_main_thread_job_drain_budget);
      }
    }
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
  if (auto system_manager = SystemManager::get(); system_manager != nullptr) {
    system_manager->remove_system<JobSystem>();
  }
  Engine::get()->end();
  Time::get()->end();
  WindowManager::get()->end();
}

} // namespace astralix
