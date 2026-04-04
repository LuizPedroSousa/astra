#include "console.hpp"

#include "exceptions/base-exception.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <iterator>
#include <optional>
#include <utility>

namespace astralix {
namespace {

enum class SuggestionBucket : uint8_t {
  Exact = 0u,
  Prefix = 1u,
  Subsequence = 2u,
};

struct SuggestionHistoryStats {
  double frecency = 0.0;
  size_t use_count = 0u;
  size_t last_seen_index = 0u;
};

std::string trim_copy(std::string_view text) {
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

std::string normalize_suggestion_text(std::string_view text) {
  std::string normalized = trim_copy(text);
  for (char &character : normalized) {
    character = static_cast<char>(
        std::tolower(static_cast<unsigned char>(character))
    );
  }

  return normalized;
}

std::string lowercase_copy(std::string_view text) {
  std::string normalized;
  normalized.reserve(text.size());
  for (const char character : text) {
    normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }

  return normalized;
}

std::string join_aliases(const std::vector<std::string> &aliases) {
  std::string joined;

  for (size_t index = 0u; index < aliases.size(); ++index) {
    if (index > 0u) {
      joined += ", ";
    }
    joined += aliases[index];
  }

  return joined;
}

std::string strip_ansi_escape_sequences(std::string_view text) {
  std::string sanitized;
  sanitized.reserve(text.size());

  for (size_t index = 0u; index < text.size(); ++index) {
    const char character = text[index];
    if (character != '\x1b') {
      sanitized.push_back(character);
      continue;
    }

    if (index + 1u >= text.size() || text[index + 1u] != '[') {
      continue;
    }

    index += 2u;
    while (index < text.size()) {
      const unsigned char escape_char =
          static_cast<unsigned char>(text[index]);
      if (escape_char >= 0x40u && escape_char <= 0x7eu) {
        break;
      }
      ++index;
    }
  }

  return sanitized;
}

std::vector<std::string> split_console_output_lines(std::string_view message) {
  std::vector<std::string> lines;
  size_t line_start = 0u;

  while (line_start <= message.size()) {
    const size_t line_end = message.find('\n', line_start);
    const size_t segment_end =
        line_end == std::string_view::npos ? message.size() : line_end;
    std::string line = strip_ansi_escape_sequences(
        message.substr(line_start, segment_end - line_start)
    );
    line = trim_copy(line);
    if (!line.empty()) {
      lines.push_back(std::move(line));
    }

    if (line_end == std::string_view::npos) {
      break;
    }

    line_start = line_end + 1u;
  }

  return lines;
}

std::string format_command_label(const ConsoleManager::CommandInfo &command) {
  if (command.aliases.empty()) {
    return command.name;
  }

  return command.name + " (aliases: " + join_aliases(command.aliases) + ")";
}

bool starts_with_case_insensitive(
    std::string_view text, std::string_view prefix
) {
  if (prefix.size() > text.size()) {
    return false;
  }

  for (size_t index = 0u; index < prefix.size(); ++index) {
    if (std::tolower(static_cast<unsigned char>(text[index])) !=
        std::tolower(static_cast<unsigned char>(prefix[index]))) {
      return false;
    }
  }

  return true;
}

std::string_view first_history_token(std::string_view line) {
  size_t begin = 0u;
  while (begin < line.size() &&
         std::isspace(static_cast<unsigned char>(line[begin]))) {
    ++begin;
  }

  size_t end = begin;
  while (end < line.size() &&
         !std::isspace(static_cast<unsigned char>(line[end]))) {
    ++end;
  }

  return line.substr(begin, end - begin);
}

bool is_subsequence_match(
    std::string_view needle, std::string_view haystack
) {
  if (needle.empty()) {
    return true;
  }

  size_t needle_index = 0u;
  for (char character : haystack) {
    if (needle_index < needle.size() && character == needle[needle_index]) {
      ++needle_index;
    }
  }

  return needle_index == needle.size();
}

std::optional<SuggestionBucket>
match_suggestion_bucket(
    std::string_view normalized_query, std::string_view normalized_candidate
) {
  if (normalized_query.empty()) {
    return SuggestionBucket::Prefix;
  }

  if (normalized_candidate == normalized_query) {
    return SuggestionBucket::Exact;
  }

  if (normalized_candidate.starts_with(normalized_query)) {
    return SuggestionBucket::Prefix;
  }

  if (is_subsequence_match(normalized_query, normalized_candidate)) {
    return SuggestionBucket::Subsequence;
  }

  return std::nullopt;
}

ConsoleEntry make_output_entry(std::string message, LogLevel level) {
  return ConsoleEntry{
      .source = ConsoleEntrySource::Output,
      .level = level,
      .timestamp = Logger::timestamp_now(),
      .message = std::move(message),
  };
}

bool can_coalesce_logger_entry(
    const ConsoleEntry &entry,
    const Logger::Log &log
) {
  return entry.source == ConsoleEntrySource::Logger &&
         entry.level == log.level && entry.message == log.message &&
         entry.caller == log.caller && entry.file == log.file &&
         entry.line == log.line;
}

} // namespace

ConsoleManager::ConsoleManager() {
  m_logger_sink_id = Logger::get().add_sink(
      [this](const Logger::Log &log) { append_log_entry(log); }
  );
  register_builtin_commands();
}

void ConsoleManager::register_command(
    std::string name,
    std::string description,
    CommandHandler handler,
    std::vector<std::string> aliases
) {
  const std::string normalized = normalize_command_name(name);
  if (normalized.empty()) {
    return;
  }

  if (const auto alias_it = m_command_aliases.find(normalized);
      alias_it != m_command_aliases.end()) {
    if (auto command_it = m_commands.find(alias_it->second);
        command_it != m_commands.end()) {
      auto &existing_aliases = command_it->second.aliases;
      existing_aliases.erase(
          std::remove(
              existing_aliases.begin(), existing_aliases.end(), normalized
          ),
          existing_aliases.end()
      );
    }

    m_command_aliases.erase(alias_it);
  }

  if (const auto existing_it = m_commands.find(normalized);
      existing_it != m_commands.end()) {
    for (const std::string &alias : existing_it->second.aliases) {
      m_command_aliases.erase(alias);
    }
  }

  std::vector<std::string> normalized_aliases;
  normalized_aliases.reserve(aliases.size());

  for (std::string &alias : aliases) {
    std::string normalized_alias = normalize_command_name(alias);
    if (normalized_alias.empty() || normalized_alias == normalized) {
      continue;
    }

    if (const auto command_it = m_commands.find(normalized_alias);
        command_it != m_commands.end() && command_it->first != normalized) {
      continue;
    }

    if (std::find(
            normalized_aliases.begin(),
            normalized_aliases.end(),
            normalized_alias
        ) != normalized_aliases.end()) {
      continue;
    }

    if (const auto alias_it = m_command_aliases.find(normalized_alias);
        alias_it != m_command_aliases.end() && alias_it->second != normalized) {
      if (auto command_it = m_commands.find(alias_it->second);
          command_it != m_commands.end()) {
        auto &existing_aliases = command_it->second.aliases;
        existing_aliases.erase(
            std::remove(
                existing_aliases.begin(),
                existing_aliases.end(),
                normalized_alias
            ),
            existing_aliases.end()
        );
      }
    }

    m_command_aliases.insert_or_assign(normalized_alias, normalized);
    normalized_aliases.push_back(std::move(normalized_alias));
  }

  m_commands.insert_or_assign(
      normalized,
      RegisteredCommand{
          .name = normalized,
          .description = std::move(description),
          .aliases = std::move(normalized_aliases),
          .handler = std::move(handler),
      }
  );
}

void ConsoleManager::unregister_command(std::string_view name) {
  const auto normalized = resolve_registered_command_name(name);
  if (!normalized.has_value()) {
    return;
  }

  const auto command_it = m_commands.find(*normalized);
  if (command_it == m_commands.end()) {
    return;
  }

  for (const std::string &alias : command_it->second.aliases) {
    m_command_aliases.erase(alias);
  }

  m_commands.erase(command_it);
}

bool ConsoleManager::has_command(std::string_view name) const {
  return resolve_registered_command_name(name).has_value();
}

std::vector<ConsoleManager::CommandInfo> ConsoleManager::commands() const {
  std::vector<CommandInfo> infos;
  infos.reserve(m_commands.size());

  for (const auto &[_, command] : m_commands) {
    infos.push_back(CommandInfo{
        .name = command.name,
        .description = command.description,
        .aliases = command.aliases,
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
  const auto resolved_name = resolve_registered_command_name(normalized_name);
  if (!resolved_name.has_value()) {
    result.executed = true;
    result.success = false;
    result.lines.push_back("unknown command: " + tokens.front());
    append_output(result.lines.front(), LogLevel::ERROR);
    return result;
  }

  const auto command_it = m_commands.find(*resolved_name);
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
          std::vector<std::string>(std::make_move_iterator(tokens.begin() + 1u), std::make_move_iterator(tokens.end())),
  };

  try {
    result = command_it->second.handler(invocation);
  } catch (const BaseException &error) {
    result.success = false;
    result.lines.push_back(error.message());
  } catch (const std::exception &error) {
    result.success = false;
    result.lines.push_back(error.what());
  } catch (...) {
    result.success = false;
    result.lines.push_back("unknown command error");
  }
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
  for (std::string &line : split_console_output_lines(message)) {
    append_entry(make_output_entry(std::move(line), level));
  }
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
  m_captures_input = false;
  m_entries_version = 0u;
  m_next_entry_id = 1u;
  m_max_entries = 500u;
  m_max_history_entries = 50u;
  m_entries.clear();
  m_history.clear();
  m_commands.clear();
  m_command_aliases.clear();
  register_builtin_commands();
}

std::string ConsoleManager::normalize_command_name(std::string_view name) {
  std::string normalized;
  normalized.reserve(name.size());

  for (const char character : name) {
    normalized.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(character)))
    );
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

std::optional<std::string> ConsoleManager::resolve_registered_command_name(
    std::string_view name
) const {
  const std::string normalized = normalize_command_name(name);
  if (normalized.empty()) {
    return std::nullopt;
  }

  if (m_commands.contains(normalized)) {
    return normalized;
  }

  const auto alias_it = m_command_aliases.find(normalized);
  if (alias_it == m_command_aliases.end()) {
    return std::nullopt;
  }

  return alias_it->second;
}

void ConsoleManager::register_builtin_commands() {
  register_command(
      "help", "List available console commands.", [this](const ConsoleCommandInvocation &) {
        ConsoleCommandResult result;
        result.success = true;
        for (const CommandInfo &command : commands()) {
          result.lines.push_back(
              format_command_label(command) + " - " + command.description
          );
        }
        if (result.lines.empty()) {
          result.lines.push_back("no commands registered");
        }
        return result;
      }
  );

  register_command(
      "clear", "Clear the console output buffer.", [](const ConsoleCommandInvocation &) {
        ConsoleCommandResult result;
        result.success = true;
        result.clear_entries = true;
        return result;
      }
  );
}

void ConsoleManager::append_log_entry(const Logger::Log &log) {
  if (!m_entries.empty() && can_coalesce_logger_entry(m_entries.back(), log)) {
    ConsoleEntry &entry = m_entries.back();
    entry.repeat_count += 1u;
    entry.timestamp = log.timestamp;
    ++m_entries_version;
    return;
  }

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
  entry.repeat_count = std::max(entry.repeat_count, 1u);
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

std::vector<std::string> build_console_command_suggestions(
    std::string_view query,
    const std::vector<ConsoleManager::CommandInfo> &commands,
    const std::deque<std::string> &history, size_t max_results
) {
  if (max_results == 0u) {
    return {};
  }

  size_t query_start = 0u;
  while (query_start < query.size() &&
         std::isspace(static_cast<unsigned char>(query[query_start]))) {
    ++query_start;
  }

  const std::string_view command_query = query.substr(query_start);
  const bool query_has_whitespace =
      std::any_of(command_query.begin(), command_query.end(), [](char character) {
        return std::isspace(static_cast<unsigned char>(character));
      });
  if (query_has_whitespace) {
    return {};
  }

  const std::string normalized_query =
      normalize_suggestion_text(command_query);

  std::map<std::string, SuggestionHistoryStats> history_stats;
  std::map<std::string, std::string> known_commands;
  for (const auto &command : commands) {
    const std::string normalized_name =
        normalize_suggestion_text(command.name);
    if (normalized_name.empty()) {
      continue;
    }

    known_commands[normalized_name] = normalized_name;
    for (const std::string &alias : command.aliases) {
      const std::string normalized_alias =
          normalize_suggestion_text(alias);
      if (!normalized_alias.empty()) {
        known_commands[normalized_alias] = normalized_name;
      }
    }
  }

  for (size_t index = 0u; index < history.size(); ++index) {
    const std::string candidate = trim_copy(history[index]);
    if (candidate.empty()) {
      continue;
    }

    const std::string normalized =
        normalize_suggestion_text(first_history_token(candidate));
    const auto known_it = known_commands.find(normalized);
    if (known_it == known_commands.end()) {
      continue;
    }

    auto &stats = history_stats[known_it->second];
    stats.use_count += 1u;
    stats.last_seen_index = index;
    stats.frecency +=
        1.0 / static_cast<double>((history.size() - index));
  }

  struct RankedSuggestion {
    std::string display;
    std::string normalized;
    SuggestionBucket bucket = SuggestionBucket::Subsequence;
    double frecency = 0.0;
    size_t use_count = 0u;
    size_t last_seen_index = 0u;
  };

  std::map<std::string, RankedSuggestion> candidates;
  for (const auto &command : commands) {
    const std::string display = trim_copy(command.name);
    if (display.empty()) {
      continue;
    }

    const std::string normalized = normalize_suggestion_text(display);
    std::optional<SuggestionBucket> best_bucket =
        match_suggestion_bucket(normalized_query, normalized);

    for (const std::string &alias : command.aliases) {
      const auto alias_bucket = match_suggestion_bucket(
          normalized_query, normalize_suggestion_text(alias)
      );
      if (!alias_bucket.has_value()) {
        continue;
      }

      if (!best_bucket.has_value() || *alias_bucket < *best_bucket) {
        best_bucket = alias_bucket;
      }
    }

    if (!best_bucket.has_value()) {
      continue;
    }

    auto [it, inserted] = candidates.try_emplace(
        normalized,
        RankedSuggestion{
            .display = display,
            .normalized = normalized,
            .bucket = *best_bucket,
        }
    );

    if (inserted || *best_bucket < it->second.bucket) {
      it->second.display = display;
      it->second.bucket = *best_bucket;
    }

    const auto stats_it = history_stats.find(normalized);
    if (stats_it != history_stats.end()) {
      it->second.frecency = stats_it->second.frecency;
      it->second.use_count = stats_it->second.use_count;
      it->second.last_seen_index = stats_it->second.last_seen_index;
    }
  }

  std::vector<RankedSuggestion> ranked;
  ranked.reserve(candidates.size());
  for (const auto &[_, candidate] : candidates) {
    ranked.push_back(candidate);
  }

  std::sort(
      ranked.begin(), ranked.end(), [](const RankedSuggestion &lhs, const RankedSuggestion &rhs) {
        if (lhs.bucket != rhs.bucket) {
          return lhs.bucket < rhs.bucket;
        }
        if (lhs.frecency != rhs.frecency) {
          return lhs.frecency > rhs.frecency;
        }
        if (lhs.last_seen_index != rhs.last_seen_index) {
          return lhs.last_seen_index > rhs.last_seen_index;
        }
        if (lhs.use_count != rhs.use_count) {
          return lhs.use_count > rhs.use_count;
        }
        return lhs.display < rhs.display;
      }
  );

  std::vector<std::string> suggestions;
  suggestions.reserve(std::min(max_results, ranked.size()));
  for (const auto &candidate : ranked) {
    suggestions.push_back(candidate.display);
    if (suggestions.size() >= max_results) {
      break;
    }
  }

  return suggestions;
}

std::optional<std::string> build_console_history_autocomplete(
    std::string_view query, const std::deque<std::string> &history
) {
  if (query.empty()) {
    return std::nullopt;
  }

  struct RankedHistoryCandidate {
    std::string display;
    double frecency = 0.0;
    size_t use_count = 0u;
    size_t last_seen_index = 0u;
  };

  std::map<std::string, RankedHistoryCandidate> candidates;
  for (size_t index = 0u; index < history.size(); ++index) {
    const std::string candidate = trim_copy(history[index]);
    if (candidate.size() <= query.size() ||
        !starts_with_case_insensitive(candidate, query)) {
      continue;
    }

    const std::string normalized = normalize_suggestion_text(candidate);
    auto [it, inserted] = candidates.try_emplace(
        normalized, RankedHistoryCandidate{.display = candidate}
    );
    if (!inserted && index >= it->second.last_seen_index) {
      it->second.display = candidate;
    }

    it->second.use_count += 1u;
    it->second.last_seen_index = index;
    it->second.frecency +=
        1.0 / static_cast<double>((history.size() - index));
  }

  if (candidates.empty()) {
    return std::nullopt;
  }

  std::vector<RankedHistoryCandidate> ranked;
  ranked.reserve(candidates.size());
  for (const auto &[_, candidate] : candidates) {
    ranked.push_back(candidate);
  }

  std::sort(
      ranked.begin(), ranked.end(), [](const RankedHistoryCandidate &lhs, const RankedHistoryCandidate &rhs) {
        if (lhs.frecency != rhs.frecency) {
          return lhs.frecency > rhs.frecency;
        }
        if (lhs.last_seen_index != rhs.last_seen_index) {
          return lhs.last_seen_index > rhs.last_seen_index;
        }
        if (lhs.use_count != rhs.use_count) {
          return lhs.use_count > rhs.use_count;
        }
        return lowercase_copy(lhs.display) < lowercase_copy(rhs.display);
      }
  );

  return ranked.front().display;
}

} // namespace astralix
