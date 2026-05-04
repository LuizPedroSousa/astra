#pragma once

#include <functional>

namespace astralix {
class Application {

public:
  using PreFrameCallback = std::function<void()>;

  static Application *init();
  void start();
  void run();

  void set_pre_frame_callback(PreFrameCallback callback) {
    m_pre_frame_callback = std::move(callback);
  }

  static Application *get() { return m_instance; }
  static void end();

private:
  Application() {};
  ~Application();

  static Application *m_instance;
  PreFrameCallback m_pre_frame_callback;
};

} // namespace astralix
