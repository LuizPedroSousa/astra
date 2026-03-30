#include "log-serializer.hpp"
#include "base.hpp"
#include "context-proxy.hpp"
#include "log.hpp"
#include "serialization-context.hpp"

namespace astralix {

LogSerializer::LogSerializer() {}

void LogSerializer::serialize() {
  SerializationContext &ctx = *m_ctx.get();

  const auto &logs = Logger::get().logs();

  for (int i = 0; i < static_cast<int>(logs.size()); i++) {
    const auto &log = logs.at(static_cast<size_t>(i));

    std::string levels[] = {"info", "warning", "error", "debug"};

    ctx[i]["level"] = levels[static_cast<int>(log.level)];
    ctx[i]["message"] = log.message;
    ctx[i]["line"] = log.line;
    ctx[i]["timestamp"] = log.timestamp;
    ctx[i]["caller"] = log.caller;
    ctx[i]["file"] = log.file;
  }
}

} // namespace astralix
