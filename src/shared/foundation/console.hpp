#pragma once

#include "log.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace astralix {

enum class ConsoleEntrySource : uint8_t {
  Logger,
  Command,
  Output,
};

struct ConsoleEntry {
  uint64_t id = 0u;
  ConsoleEntrySource source = ConsoleEntrySource::Logger;
  LogLevel level = LogLevel::INFO;
  uint32_t repeat_count = 1u;
  std::string timestamp;
  std::string message;
  std::string caller;
  std::string file;
  int line = 0;
};

struct ConsoleCommandInvocation {
  std::string line;
  std::string name;
  std::vector<std::string> arguments;
};

struct ConsoleCommandResult {
  bool executed = false;
  bool success = true;
  bool clear_entries = false;
  std::vector<std::string> lines;
};

class ConsoleManager {
public:
  using CommandHandler =
      std::function<ConsoleCommandResult(const ConsoleCommandInvocation &)>;

  struct CommandInfo {
    std::string name;
    std::string description;
    std::vector<std::string> aliases;
  };

  static ConsoleManager &get() {
    static ConsoleManager instance;
    return instance;
  }

  ConsoleManager(const ConsoleManager &) = delete;
  ConsoleManager &operator=(const ConsoleManager &) = delete;

  void set_open(bool open) {
    m_open = open;
    if (!m_open) {
      m_captures_input = false;
    }
  }
  bool is_open() const { return m_open; }
  void set_captures_input(bool captures_input) {
    m_captures_input = m_open && captures_input;
  }
  bool captures_input() const { return m_captures_input; }

  void register_command(
      std::string name,
      std::string description,
      CommandHandler handler,
      std::vector<std::string> aliases = {}
  );
  void unregister_command(std::string_view name);
  bool has_command(std::string_view name) const;
  std::vector<CommandInfo> commands() const;

  ConsoleCommandResult execute(std::string line);

  void append_output(std::string message, LogLevel level = LogLevel::INFO);
  void clear_entries();
  void clear_history();

  const std::deque<ConsoleEntry> &entries() const { return m_entries; }
  const std::deque<std::string> &history() const { return m_history; }

  uint64_t entries_version() const { return m_entries_version; }
  size_t max_entries() const { return m_max_entries; }
  void set_max_entries(size_t max_entries);

  size_t max_history_entries() const { return m_max_history_entries; }
  void set_max_history_entries(size_t max_entries);
  void push_history(std::string line);

  std::vector<std::string> tokenize(std::string_view line) const;

  void reset_for_testing();

private:
  struct RegisteredCommand {
    std::string name;
    std::string description;
    std::vector<std::string> aliases;
    CommandHandler handler;
  };

  ConsoleManager();

  static std::string normalize_command_name(std::string_view name);
  static std::string trim(std::string_view text);
  std::optional<std::string> resolve_registered_command_name(
      std::string_view name
  ) const;

  void register_builtin_commands();
  void append_log_entry(const Logger::Log &log);
  void append_entry(ConsoleEntry entry);
  void trim_entries_to_capacity();
  void trim_history_to_capacity();

  bool m_open = false;
  bool m_captures_input = false;
  uint64_t m_entries_version = 0u;
  uint64_t m_next_entry_id = 1u;
  size_t m_max_entries = 500u;
  size_t m_max_history_entries = 50u;
  Logger::SinkID m_logger_sink_id = 0u;
  std::deque<ConsoleEntry> m_entries;
  std::deque<std::string> m_history;
  std::map<std::string, RegisteredCommand> m_commands;
  std::map<std::string, std::string> m_command_aliases;
};

std::vector<std::string> build_console_command_suggestions(
    std::string_view query,
    const std::vector<ConsoleManager::CommandInfo> &commands,
    const std::deque<std::string> &history, size_t max_results = 8u
);

std::optional<std::string> build_console_history_autocomplete(
    std::string_view query, const std::deque<std::string> &history
);

inline ConsoleManager &console_manager() { return ConsoleManager::get(); }

} // namespace astralix
