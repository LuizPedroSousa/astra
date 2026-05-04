#include "module-handle.hpp"
#include "build-log-store.hpp"
#include "log.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <sstream>
#include <unistd.h>

namespace astralix {

ModuleHandle::ModuleHandle(Config config) : m_config(std::move(config)) {
  LOG_INFO("ModuleHandle: initialized with", "module_path=", m_config.module_path.string(), "source_dir=", m_config.source_dir.string(), "build_dir=", m_config.build_dir.string(), "build_target=", m_config.build_target);
}

ModuleHandle::~ModuleHandle() {
  stop_watcher();
  if (m_handle != nullptr) {
    dlclose(m_handle);
    m_handle = nullptr;
  }
  cleanup_temp_files();
}

bool ModuleHandle::load() {
  std::error_code error_code;
  if (!std::filesystem::exists(m_config.module_path, error_code)) {
    LOG_ERROR("ModuleHandle: module file does not exist:", m_config.module_path.string());
    return false;
  }

  auto temp_path = copy_to_temp();
  if (temp_path.empty()) {
    return false;
  }

  void *handle = dlopen(temp_path.c_str(), RTLD_NOW);
  if (handle == nullptr) {
    LOG_ERROR("ModuleHandle: dlopen failed:", dlerror());
    return false;
  }

  auto get_api =
      reinterpret_cast<AstraModuleGetAPIFn>(dlsym(handle, "astra_get_module_api"));
  if (get_api == nullptr) {
    LOG_ERROR("ModuleHandle: dlsym(astra_get_module_api) failed:", dlerror());
    dlclose(handle);
    return false;
  }

  const AstraModuleAPI *api = get_api();
  if (api == nullptr || api->api_version != ASTRA_MODULE_API_VERSION) {
    LOG_ERROR("ModuleHandle: API version mismatch or null API");
    dlclose(handle);
    return false;
  }

  m_handle = handle;
  m_api = api;
  m_last_module_mtime =
      std::filesystem::last_write_time(m_config.module_path, error_code);
  m_generation++;

  LOG_INFO("ModuleHandle: loaded module", m_config.build_target, "generation", m_generation);

  if (m_config.auto_rebuild && !m_watcher_running.load()) {
    LOG_INFO("ModuleHandle: starting source watcher on", m_config.source_dir.string());
    start_watcher();
  }

  return true;
}

void ModuleHandle::unload() {
  if (m_handle == nullptr)
    return;

  dlclose(m_handle);
  m_handle = nullptr;
  m_api = nullptr;

  LOG_INFO("ModuleHandle: unloaded module", m_config.build_target);
}

bool ModuleHandle::poll_changed() {
  if (m_build_in_progress.load())
    return false;

  std::error_code error_code;
  if (!std::filesystem::exists(m_config.module_path, error_code)) {
    return false;
  }

  auto module_mtime =
      std::filesystem::last_write_time(m_config.module_path, error_code);
  if (error_code)
    return false;
  if (module_mtime == m_last_module_mtime)
    return false;

  LOG_INFO("ModuleHandle: module binary changed, triggering reload for", m_config.build_target);
  m_last_module_mtime = module_mtime;
  return true;
}

const AstraModuleAPI *ModuleHandle::api() const { return m_api; }

bool ModuleHandle::is_loaded() const { return m_handle != nullptr; }

uint64_t ModuleHandle::generation() const { return m_generation; }

std::filesystem::path ModuleHandle::copy_to_temp() {
  std::ostringstream filename;
  filename << "astra_module_" << getpid() << "_" << m_config.build_target << "_"
           << m_generation << ".so";

  auto dest = std::filesystem::temp_directory_path() / filename.str();

  std::error_code error_code;
  std::filesystem::copy_file(
      m_config.module_path, dest, std::filesystem::copy_options::overwrite_existing, error_code
  );
  if (error_code) {
    LOG_ERROR("ModuleHandle: failed to copy module to temp:", error_code.message());
    return {};
  }

  m_temp_files.push_back(dest);
  return dest;
}

void ModuleHandle::start_watcher() {
  if (m_config.source_dir.empty() || m_config.build_dir.empty()) {
    LOG_WARN("ModuleHandle: watcher not started, source_dir or build_dir is empty");
    return;
  }

  if (!std::filesystem::exists(m_config.source_dir)) {
    LOG_ERROR("ModuleHandle: source_dir does not exist:", m_config.source_dir.string());
    return;
  }

  m_watcher_running.store(true);

  std::error_code dir_error_code;
  std::filesystem::file_time_type max_mtime = std::filesystem::file_time_type::min();
  uint32_t file_count = 0;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(
           m_config.source_dir, dir_error_code
       )) {
    if (!entry.is_regular_file())
      continue;
    auto extension = entry.path().extension().string();
    if (extension != ".cpp" && extension != ".hpp" && extension != ".h")
      continue;
    file_count++;
    std::error_code mtime_error_code;
    auto file_mtime = std::filesystem::last_write_time(entry.path(), mtime_error_code);
    if (!mtime_error_code && file_mtime > max_mtime)
      max_mtime = file_mtime;
  }
  m_last_source_mtime = max_mtime;
  LOG_INFO("ModuleHandle: watcher tracking", file_count, "source files in", m_config.source_dir.string());

  m_watcher_thread = std::thread([this]() {
    while (m_watcher_running.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      if (!m_watcher_running.load())
        break;

      std::error_code iterator_error_code;
      std::filesystem::file_time_type max_source_mtime = std::filesystem::file_time_type::min();
      for (const auto &entry : std::filesystem::recursive_directory_iterator(
               m_config.source_dir, iterator_error_code
           )) {
        if (!entry.is_regular_file())
          continue;
        auto extension = entry.path().extension().string();
        if (extension != ".cpp" && extension != ".hpp" && extension != ".h")
          continue;
        std::error_code mtime_error_code;
        auto file_mtime = std::filesystem::last_write_time(entry.path(), mtime_error_code);
        if (!mtime_error_code && file_mtime > max_source_mtime)
          max_source_mtime = file_mtime;
      }

      if (max_source_mtime <= m_last_source_mtime)
        continue;

      m_last_source_mtime = max_source_mtime;

      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      m_build_in_progress.store(true);
      LOG_INFO("ModuleHandle: rebuilding", m_config.build_target);

      auto &build_log = BuildLogStore::get();
      build_log.begin_build(m_config.build_target);

      std::ostringstream command;
      command << "cmake --build " << m_config.build_dir.string() << " --target "
              << m_config.build_target << " -- -j$(nproc) 2>&1";

      FILE *pipe = popen(command.str().c_str(), "r");
      int result = -1;
      if (pipe != nullptr) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
          std::string line(buffer);
          while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
          }
          if (!line.empty()) {
            build_log.append_line(line);
          }
        }
        result = pclose(pipe);
      }

      bool success = (result == 0);
      build_log.finish_build(success);
      m_build_in_progress.store(false);

      if (!success) {
        LOG_ERROR("ModuleHandle: build failed for", m_config.build_target);
      } else {
        LOG_INFO("ModuleHandle: build succeeded for", m_config.build_target);
      }
    }
  });
}

void ModuleHandle::stop_watcher() {
  m_watcher_running.store(false);
  if (m_watcher_thread.joinable()) {
    m_watcher_thread.join();
  }
}

void ModuleHandle::cleanup_temp_files() {
  for (const auto &temp_file : m_temp_files) {
    std::error_code error_code;
    std::filesystem::remove(temp_file, error_code);
  }
  m_temp_files.clear();
}

} // namespace astralix
