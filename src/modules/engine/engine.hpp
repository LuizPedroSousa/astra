#pragma once

namespace astralix {

class Engine {
public:
  static void init();
  void end();

  static Engine *get() { return m_instance; }

  void start();
  void update();

private:
  Engine();
  ~Engine() {
    
  };

  static Engine *m_instance;
};

}; // namespace astralix
