#pragma once
#include "arena.hpp"
#include "event.hpp"
#include "glad/glad.h"
#include "listener.hpp"
#include "managers/scene-manager.hpp"
#include "serializers/log-serializer.hpp"
#include "trace.hpp"
#include "websocket-server.hpp"

namespace astralix {

#define WIDTH 1920
#define HEIGHT 1080
#define PIXEL_SIZE 4 // RGBA

#define FRAMEBUFFER_SIZE WIDTH *HEIGHT *PIXEL_SIZE

#define SCENE_SIZE 1024

class ViewportEvent : public Event {
public:
  ViewportEvent() = default;

  EVENT_TYPE(Viewport)
};

class ViewportEventListener : public BaseListener {
public:
  ViewportEventListener(ElasticArena &arena) : m_arena(arena) {}
  ~ViewportEventListener() {}

  void dispatch(Event *event) override {
    ASTRA_PROFILE_N("Viewport listener");

    auto framebuffer = m_arena.allocate(FRAMEBUFFER_SIZE);

    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE,
                 framebuffer->data);

    auto socket = WebsocketServer::get();
    socket->publish("framebuffer", (const char *)framebuffer->data);

    auto serializer = SceneManager::get()->get_active_scene()->get_serializer();

    serializer->serialize();

    auto ctx = serializer->get_ctx();
    auto scene = ctx->to_buffer(m_arena);

    socket->publish("scene", (const char *)scene->data);

    m_log_serializer.serialize();

    m_arena.release(framebuffer);
    m_arena.release(scene);

    // auto logs = m_log_serializer.get_ctx()->to_buffer(m_arena);
    // socket->publish("logs", (const char *)logs->data);
    // m_arena.release(logs);
  }

  LISTENER_CLASS_TYPE(ViewportListener)

private:
  ElasticArena &m_arena;
  LogSerializer m_log_serializer;
};

} // namespace astralix
