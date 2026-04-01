#pragma once
#include "arena.hpp"
#include "event.hpp"
#include "glad/glad.h"
#include "listener.hpp"
#include "log.hpp"
#include "serializers/log-serializer.hpp"
#include "trace.hpp"
#include "websocket-server.hpp"

namespace astralix {

#define WIDTH 1920
#define HEIGHT 1080
#define PIXEL_SIZE 4 // RGBA

#define FRAMEBUFFER_SIZE WIDTH *HEIGHT *PIXEL_SIZE

#define SCENE_SIZE 1024

class LogsEvent : public Event {
public:
  LogsEvent() = default;

  EVENT_TYPE(Logs)
};

class LogsEventListener : public BaseListener {
public:
  LogsEventListener(ElasticArena &arena) : m_arena(arena) {}
  ~LogsEventListener() {}

  void dispatch(Event *event) override {
    ASTRA_PROFILE_N("Logs listener");

    auto socket = WebsocketServer::get();

    m_log_serializer.serialize();

    auto logs = m_log_serializer.get_ctx()->to_buffer(m_arena);

    socket->publish("logs", (const char *)logs->data);

    LOG_DEBUG(logs->data);
    m_arena.release(logs);
  }

  LISTENER_CLASS_TYPE(LogsListener)

private:
  ElasticArena &m_arena;
  LogSerializer m_log_serializer;
};

} // namespace astralix
