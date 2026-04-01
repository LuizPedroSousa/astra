#pragma once

#include "arena.hpp"
#include "event-scheduler.hpp"
#include "events/key-codes.hpp"
#include "events/key-event.hpp"
#include "events/keyboard.hpp"
#include "events/mouse.hpp"
#include "log.hpp"
#include "uwebsockets/App.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <json/reader.h>
#include <memory>
#include <string>
#include <uwebsockets/PerMessageDeflate.h>
#include <uwebsockets/WebSocketProtocol.h>

namespace astralix {

class WebsocketServer {
public:
  WebsocketServer();

  static void init();

  struct PerSocketData {
    /* Fill with user data */
  };

  void start() {
    m_app = new uWS::SSLApp(
        {/* There are example certificates in uWebSockets.js repo */
         .key_file_name = "misc/key.pem",
         .cert_file_name = "misc/cert.pem",
         .passphrase = "1234"});

    m_app
        ->ws<PerSocketData>(
            "/*",
            {/* Settings */
             .compression = uWS::DISABLED,
             .maxPayloadLength = 64 * 1024 * 1024,
             .idleTimeout = 8,
             .maxBackpressure = 5 * 1024 * 1024,
             .closeOnBackpressureLimit = false,
             .resetIdleTimeoutOnSend = true,
             .sendPingsAutomatically = false,
             /* Handlers */
             .open =
                 [](auto *ws) {
                   /* Open event here, you may access ws->getUserData() which
                    * points to a PerSocketData struct */
                   ws->subscribe("framebuffer");
                   ws->subscribe("scene");
                   ws->subscribe("logs");
                 },
             .message =
                 [](auto * /*ws*/, std::string_view message,
                    uWS::OpCode /*opCode*/) {
                   Json::Value root;

                   Json::CharReaderBuilder rbuilder;

                   rbuilder["collectComments"] = false;

                   std::unique_ptr<Json::CharReader> reader(
                       rbuilder.newCharReader());
                   std::string errs;

                   bool ok = reader->parse(message.data(),
                                           message.data() + message.size(),
                                           &root, &errs);

                   const auto &type = root["type"].asString();
                   const auto &action = root["action"].asString();

                   auto scheduler = EventScheduler::get();

                   auto mouse = Mouse::get();

                   if (type == "keyboard") {
                     auto keyCode = root["keyCode"].as<int>();

                     auto keyboard = Keyboard::get();

                     if (action == "press") {
                       auto event = KeyCode(keyCode);

                       auto scheduler_id = scheduler->schedule<KeyPressedEvent>(
                           SchedulerType::POST_FRAME, event);

                       keyboard->attach_key(
                           event, Keyboard::KeyState{
                                      .event = Keyboard::KeyEvent::KeyDown,
                                      .scheduler_id = scheduler_id});
                       return;
                     }

                     auto keycode = KeyCode(keyCode);

                     keyboard->release_key(keycode);

                     return;
                   }

                   const auto &current_x = root["x"].as<double>();
                   const auto &current_y = root["y"].as<double>();

                   auto position =
                       Mouse::Position{.x = current_x, .y = current_y};

                   Mouse::get()->apply_delta(position);
                 },
             .drain =
                 [](auto * /*ws*/) {
                   /* Check ws->getBufferedAmount() here */
                 },
             .ping =
                 [](auto * /*ws*/, std::string_view) {
                   /* Not implemented yet */
                 },
             .pong =
                 [](auto * /*ws*/, std::string_view) {
                   /* Not implemented yet */
                 },
             .close =
                 [](auto * /*ws*/, int /*code*/, std::string_view /*message*/) {
                   /* You may access ws->getUserData() here */
                 }})
        .listen(9001, [](auto *listen_socket) {
          if (listen_socket) {
            std::cout << "Listening on port " << 9001 << std::endl;
          }
        });

    if (!m_app) {
      std::cerr << "SSLApp is not initialized!" << std::endl;
    }
  }

  static WebsocketServer *get() { return m_instance; }

  void publish(std::string_view topic, std::string_view message,
               uWS::OpCode opCode, bool compress = true) {

    if (opCode == uWS::OpCode::BINARY) {

      m_app->publish(topic, message, opCode, compress);

      return;
    }

    m_app->publish(topic, message, opCode, compress);
  }

  uWS::SSLApp *get_app() { return m_app; }

  void publish(const char *topic, const char *message) {
    m_app->publish(topic, build_message(topic, message), uWS::OpCode::BINARY,
                   false);
  };

  inline std::string build_message(const char *topic, const char *message) {
    uint32_t topic_size = std::strlen(topic);

    char topic_size_bytes[4];
    topic_size_bytes[0] = (topic_size >> 24) & 0xFF; // Significant Byte
    topic_size_bytes[1] = (topic_size >> 16) & 0xFF;
    topic_size_bytes[2] = (topic_size >> 8) & 0xFF;
    topic_size_bytes[3] = topic_size & 0xFF; // Less Significant Byte

    std::string result;

    result.append(topic_size_bytes, 4);
    result += topic;
    result += message;

    return result;
  }

  struct us_timer_t *get_timer() {
    struct us_loop_t *loop = (struct us_loop_t *)uWS::Loop::get();
    struct us_timer_t *delayTimer = us_create_timer(loop, 0, 0);

    return delayTimer;
  };

  void update() { m_app->run(); }

  ~WebsocketServer() { delete m_app; }

private:
  int m_port;
  int m_threads_length;

  uWS::SSLApp *m_app = nullptr;

  static WebsocketServer *m_instance;
};
} // namespace astralix
