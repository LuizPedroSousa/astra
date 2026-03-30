#include "console.hpp"

#include <algorithm>
#include <cctype>
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

} // namespace

ConsoleManager::ConsoleManager() {
  m_logger_sink_id = Logger::get().add_sink(
      [this](const Logger::Log &log) { append_log_entry(log); }
  );
  register_builtin_commands();
}

void ConsoleManager::register_command(std::string name, std::string description, CommandHandler handler) {
  const std::string normalized = normalize_command_name(name);
  if (normalized.empty()) {
    return;
  }

  m_commands.insert_or_assign(
      normalized, RegisteredCommand{.name = std::move(normalized), .description = std::move(description), .handler = std::move(handler)}
  );
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
          std::vector<std::string>(std::make_move_iterator(tokens.begin() + 1u), std::make_move_iterator(tokens.end())),
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

void ConsoleManager::register_builtin_commands() {
  register_command(
      "help", "List available console commands.", [this](const ConsoleCommandInvocation &) {
        ConsoleCommandResult result;
        result.success = true;
        for (const CommandInfo &command : commands()) {
          result.lines.push_back(command.name + " - " + command.description);
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
  std::map<std::string, bool> known_commands;
  for (const auto &command : commands) {
    const std::string normalized = normalize_suggestion_text(command.name);
    if (!normalized.empty()) {
      known_commands[normalized] = true;
    }
  }

  for (size_t index = 0u; index < history.size(); ++index) {
    const std::string candidate = trim_copy(history[index]);
    if (candidate.empty()) {
      continue;
    }

    const std::string normalized =
        normalize_suggestion_text(first_history_token(candidate));
    if (!known_commands.contains(normalized)) {
      continue;
    }

    auto &stats = history_stats[normalized];
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
  auto register_candidate = [&](std::string_view display_text) {
    const std::string display = trim_copy(display_text);
    if (display.empty()) {
      return;
    }

    const std::string normalized = normalize_suggestion_text(display);
    const auto bucket = match_suggestion_bucket(normalized_query, normalized);
    if (!bucket.has_value()) {
      return;
    }

    auto [it, inserted] = candidates.try_emplace(
        normalized,
        RankedSuggestion{
            .display = display,
            .normalized = normalized,
            .bucket = *bucket,
        }
    );

    if (inserted || *bucket < it->second.bucket) {
      it->second.display = display;
      it->second.bucket = *bucket;
    }

    const auto stats_it = history_stats.find(normalized);
    if (stats_it != history_stats.end()) {
      it->second.frecency = stats_it->second.frecency;
      it->second.use_count = stats_it->second.use_count;
      it->second.last_seen_index = stats_it->second.last_seen_index;
    }
  };

  for (const auto &command : commands) {
    register_candidate(command.name);
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
