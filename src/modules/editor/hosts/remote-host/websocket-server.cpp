#include "websocket-server.hpp"

namespace astralix {
WebsocketServer::WebsocketServer() {}

void WebsocketServer::init() {
  if (!m_instance)
    m_instance = new WebsocketServer;
}

WebsocketServer *WebsocketServer::m_instance = nullptr;
} // namespace astralix
