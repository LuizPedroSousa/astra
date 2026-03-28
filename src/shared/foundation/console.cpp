#include "console.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <utility>

namespace astralix {
namespace {

ConsoleEntry make_output_entry(std::string message, LogLevel level) {
  return ConsoleEntry{
      .source = ConsoleEntrySource::Output,
      .level = level,
      .timestamp = Logger::timestamp_now(),
      .message = std::move(message),
  };
}

} // namespace

ConsoleManager::ConsoleManager() {
  m_logger_sink_id = Logger::get().add_sink(
      [this](const Logger::Log &log) { append_log_entry(log); });
  register_builtin_commands();
}

void ConsoleManager::register_command(std::string name, std::string description,
                                      CommandHandler handler) {
  const std::string normalized = normalize_command_name(name);
  if (normalized.empty()) {
    return;
  }

  m_commands.insert_or_assign(
      normalized, RegisteredCommand{.name = std::move(normalized),
                                    .description = std::move(description),
                                    .handler = std::move(handler)});
}

void ConsoleManager::unregister_command(std::string_view name) {
  m_commands.erase(normalize_command_name(name));
}

bool ConsoleManager::has_command(std::string_view name) const {
  return m_commands.contains(normalize_command_name(name));
}

std::vector<ConsoleManager::CommandInfo> ConsoleManager::commands() const {
  std::vector<CommandInfo> infos;
  infos.reserve(m_commands.size());

  for (const auto &[_, command] : m_commands) {
    infos.push_back(CommandInfo{
        .name = command.name,
        .description = command.description,
    });
  }

  return infos;
}

ConsoleCommandResult ConsoleManager::execute(std::string line) {
  ConsoleCommandResult result;
  result.executed = false;

  const std::string trimmed_line = trim(line);
  if (trimmed_line.empty()) {
    return result;
  }

  append_entry(ConsoleEntry{
      .source = ConsoleEntrySource::Command,
      .level = LogLevel::INFO,
      .timestamp = Logger::timestamp_now(),
      .message = trimmed_line,
  });
  push_history(trimmed_line);

  auto tokens = tokenize(trimmed_line);
  if (tokens.empty()) {
    result.executed = true;
    return result;
  }

  const std::string normalized_name = normalize_command_name(tokens.front());
  const auto command_it = m_commands.find(normalized_name);
  if (command_it == m_commands.end()) {
    result.executed = true;
    result.success = false;
    result.lines.push_back("unknown command: " + tokens.front());
    append_output(result.lines.front(), LogLevel::ERROR);
    return result;
  }

  ConsoleCommandInvocation invocation{
      .line = trimmed_line,
      .name = command_it->second.name,
      .arguments =
          std::vector<std::string>(std::make_move_iterator(tokens.begin() + 1u),
                                   std::make_move_iterator(tokens.end())),
  };

  result = command_it->second.handler(invocation);
  result.executed = true;

  if (result.clear_entries) {
    clear_entries();
  }

  if (!result.clear_entries) {
    const LogLevel output_level = result.success ? LogLevel::INFO : LogLevel::ERROR;
    for (const std::string &output_line : result.lines) {
      append_output(output_line, output_level);
    }
  }

  return result;
}

void ConsoleManager::append_output(std::string message, LogLevel level) {
  const std::string trimmed_message = trim(message);
  if (trimmed_message.empty()) {
    return;
  }

  append_entry(make_output_entry(trimmed_message, level));
}

void ConsoleManager::clear_entries() {
  m_entries.clear();
  ++m_entries_version;
}

void ConsoleManager::clear_history() { m_history.clear(); }

void ConsoleManager::set_max_entries(size_t max_entries) {
  m_max_entries = std::max<size_t>(1u, max_entries);
  trim_entries_to_capacity();
}

void ConsoleManager::set_max_history_entries(size_t max_entries) {
  m_max_history_entries = std::max<size_t>(1u, max_entries);
  trim_history_to_capacity();
}

void ConsoleManager::push_history(std::string line) {
  const std::string trimmed_line = trim(line);
  if (trimmed_line.empty()) {
    return;
  }

  m_history.push_back(trimmed_line);
  trim_history_to_capacity();
}

std::vector<std::string> ConsoleManager::tokenize(std::string_view line) const {
  std::vector<std::string> tokens;
  std::string current;
  bool in_quotes = false;

  for (const char character : line) {
    if (character == '"') {
      in_quotes = !in_quotes;
      continue;
    }

    if (std::isspace(static_cast<unsigned char>(character)) && !in_quotes) {
      if (!current.empty()) {
        tokens.push_back(std::move(current));
        current.clear();
      }
      continue;
    }

    current.push_back(character);
  }

  if (!current.empty()) {
    tokens.push_back(std::move(current));
  }

  return tokens;
}

void ConsoleManager::reset_for_testing() {
  m_open = false;
  m_entries_version = 0u;
  m_next_entry_id = 1u;
  m_max_entries = 500u;
  m_max_history_entries = 50u;
  m_entries.clear();
  m_history.clear();
  m_commands.clear();
  register_builtin_commands();
}

std::string ConsoleManager::normalize_command_name(std::string_view name) {
  std::string normalized;
  normalized.reserve(name.size());

  for (const char character : name) {
    normalized.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }

  return trim(normalized);
}

std::string ConsoleManager::trim(std::string_view text) {
  size_t begin = 0u;
  size_t end = text.size();

  while (begin < end &&
         std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }

  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1u]))) {
    --end;
  }

  return std::string(text.substr(begin, end - begin));
}

void ConsoleManager::register_builtin_commands() {
  register_command(
      "help", "List available console commands.",
      [this](const ConsoleCommandInvocation &) {
        ConsoleCommandResult result;
        result.success = true;
        for (const CommandInfo &command : commands()) {
          result.lines.push_back(command.name + " - " + command.description);
        }
        if (result.lines.empty()) {
          result.lines.push_back("no commands registered");
        }
        return result;
      });

  register_command(
      "clear", "Clear the console output buffer.",
      [](const ConsoleCommandInvocation &) {
        ConsoleCommandResult result;
        result.success = true;
        result.clear_entries = true;
        return result;
      });
}

void ConsoleManager::append_log_entry(const Logger::Log &log) {
  append_entry(ConsoleEntry{
      .source = ConsoleEntrySource::Logger,
      .level = log.level,
      .timestamp = log.timestamp,
      .message = log.message,
      .caller = log.caller,
      .file = log.file,
      .line = log.line,
  });
}

void ConsoleManager::append_entry(ConsoleEntry entry) {
  entry.id = m_next_entry_id++;
  m_entries.push_back(std::move(entry));
  trim_entries_to_capacity();
  ++m_entries_version;
}

void ConsoleManager::trim_entries_to_capacity() {
  while (m_entries.size() > m_max_entries) {
    m_entries.pop_front();
  }
}

void ConsoleManager::trim_history_to_capacity() {
  while (m_history.size() > m_max_history_entries) {
    m_history.pop_front();
  }
}

} // namespace astralix
