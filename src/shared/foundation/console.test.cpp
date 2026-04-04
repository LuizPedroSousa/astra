#include "console.hpp"
#include "assert.hpp"
#include "log.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace astralix {
namespace {

class LoggerConsoleTest : public ::testing::Test {
protected:
  void SetUp() override {
    Logger::get().set_terminal_output_enabled(false);
    Logger::get().clear();
    Logger::get().set_max_entries(500u);
    ConsoleManager::get().reset_for_testing();
    ConsoleManager::get().clear_entries();
    ConsoleManager::get().clear_history();
    ConsoleManager::get().set_max_entries(500u);
    ConsoleManager::get().set_max_history_entries(50u);
  }
};

TEST_F(LoggerConsoleTest, LoggerStoresDistinctEntriesAndNotifiesSinks) {
  std::vector<std::string> observed_messages;
  const auto sink_id = Logger::get().add_sink([&](const Logger::Log &log) {
    observed_messages.push_back(log.message);
  });

  Logger::get().log(LogLevel::INFO, "test", "/tmp/example.cpp", 12, "same");
  Logger::get().log(LogLevel::INFO, "test", "/tmp/example.cpp", 12, "same");

  const auto &logs = Logger::get().logs();
  ASSERT_EQ(logs.size(), 2u);
  EXPECT_EQ(logs[0].message, "same");
  EXPECT_EQ(logs[1].message, "same");
  ASSERT_EQ(observed_messages.size(), 2u);
  EXPECT_EQ(observed_messages[0], "same");
  EXPECT_EQ(observed_messages[1], "same");

  Logger::get().remove_sink(sink_id);
}

TEST_F(LoggerConsoleTest, LoggerRingBufferEvictsOldestEntries) {
  Logger::get().set_max_entries(2u);

  Logger::get().log(LogLevel::INFO, "test", "/tmp/example.cpp", 1, "one");
  Logger::get().log(LogLevel::INFO, "test", "/tmp/example.cpp", 2, "two");
  Logger::get().log(LogLevel::INFO, "test", "/tmp/example.cpp", 3, "three");

  const auto &logs = Logger::get().logs();
  ASSERT_EQ(logs.size(), 2u);
  EXPECT_EQ(logs[0].message, "two");
  EXPECT_EQ(logs[1].message, "three");
}

TEST_F(LoggerConsoleTest, ConsoleCoalescesRepeatedLoggerEntries) {
  Logger::get().log(
      LogLevel::WARNING, "test", "/tmp/example.cpp", 12, "same warning"
  );
  Logger::get().log(
      LogLevel::WARNING, "test", "/tmp/example.cpp", 12, "same warning"
  );

  const auto &logs = Logger::get().logs();
  ASSERT_EQ(logs.size(), 2u);

  const auto &entries = ConsoleManager::get().entries();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front().source, ConsoleEntrySource::Logger);
  EXPECT_EQ(entries.front().message, "same warning");
  EXPECT_EQ(entries.front().repeat_count, 2u);
}

TEST_F(LoggerConsoleTest, ConsoleExecutesCommandsAndKeepsBoundedHistory) {
  auto &console = ConsoleManager::get();
  console.set_max_history_entries(2u);
  console.register_command(
      "echo", "Echo arguments back to the console.",
      [](const ConsoleCommandInvocation &invocation) {
        ConsoleCommandResult result;
        result.success = true;
        std::string line;
        for (size_t index = 0u; index < invocation.arguments.size(); ++index) {
          if (index > 0u) {
            line += " ";
          }
          line += invocation.arguments[index];
        }
        result.lines.push_back(line);
        return result;
      });

  auto result = console.execute("EcHo \"hello world\" again");
  ASSERT_TRUE(result.executed);
  EXPECT_TRUE(result.success);
  ASSERT_EQ(result.lines.size(), 1u);
  EXPECT_EQ(result.lines[0], "hello world again");

  console.execute("echo first");
  console.execute("echo second");

  const auto &history = console.history();
  ASSERT_EQ(history.size(), 2u);
  EXPECT_EQ(history[0], "echo first");
  EXPECT_EQ(history[1], "echo second");
}

TEST_F(LoggerConsoleTest, CommandAliasesShareSingleCanonicalRegistration) {
  auto &console = ConsoleManager::get();
  console.register_command(
      "selection",
      "Inspect selection.",
      [](const ConsoleCommandInvocation &invocation) {
        ConsoleCommandResult result;
        result.success = true;
        result.lines.push_back(invocation.name);
        return result;
      },
      {"sel"}
  );

  ASSERT_TRUE(console.has_command("selection"));
  ASSERT_TRUE(console.has_command("sel"));

  const auto commands = console.commands();
  ASSERT_EQ(commands.size(), 3u);
  EXPECT_EQ(commands[2].name, "selection");
  ASSERT_EQ(commands[2].aliases.size(), 1u);
  EXPECT_EQ(commands[2].aliases[0], "sel");

  const auto result = console.execute("sel");
  ASSERT_TRUE(result.executed);
  EXPECT_TRUE(result.success);
  ASSERT_EQ(result.lines.size(), 1u);
  EXPECT_EQ(result.lines[0], "selection");
}

TEST_F(LoggerConsoleTest, UnknownAndClearCommandsProduceExpectedBufferState) {
  auto &console = ConsoleManager::get();

  auto result = console.execute("does_not_exist");
  ASSERT_TRUE(result.executed);
  EXPECT_FALSE(result.success);
  ASSERT_EQ(result.lines.size(), 1u);
  EXPECT_EQ(result.lines[0], "unknown command: does_not_exist");

  const auto &entries = console.entries();
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries.front().source, ConsoleEntrySource::Command);
  EXPECT_EQ(entries.back().source, ConsoleEntrySource::Output);
  EXPECT_EQ(entries.back().level, LogLevel::ERROR);

  result = console.execute("clear");
  ASSERT_TRUE(result.executed);
  EXPECT_TRUE(result.success);
  EXPECT_TRUE(console.entries().empty());
}

TEST_F(LoggerConsoleTest, CommandHandlerExceptionsBecomeConsoleErrors) {
  auto &console = ConsoleManager::get();
  console.register_command(
      "explode",
      "Throw a command exception.",
      [](const ConsoleCommandInvocation &) -> ConsoleCommandResult {
        ASTRA_EXCEPTION("explode failed");
      }
  );

  const auto result = console.execute("explode");
  ASSERT_TRUE(result.executed);
  EXPECT_FALSE(result.success);
  ASSERT_EQ(result.lines.size(), 1u);
  EXPECT_EQ(result.lines[0], "explode failed");

  const auto &entries = console.entries();
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries.front().source, ConsoleEntrySource::Command);
  EXPECT_EQ(entries.back().source, ConsoleEntrySource::Output);
  EXPECT_EQ(entries.back().level, LogLevel::ERROR);
  EXPECT_EQ(entries.back().message, "explode failed");
}

TEST_F(LoggerConsoleTest, OutputSanitizesAnsiAndSplitsLines) {
  auto &console = ConsoleManager::get();
  console.append_output(
      std::string(BOLD) + RED + "first" + RESET + "\n\n" + CYAN + "second" +
          RESET,
      LogLevel::ERROR
  );

  const auto &entries = console.entries();
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].message, "first");
  EXPECT_EQ(entries[1].message, "second");
  EXPECT_EQ(entries[0].level, LogLevel::ERROR);
  EXPECT_EQ(entries[1].level, LogLevel::ERROR);
}

TEST_F(LoggerConsoleTest, CommandSuggestionsRankByMatchBucketAndSessionFrecency) {
  const std::vector<ConsoleManager::CommandInfo> commands{
      {.name = "help", .description = "List commands."},
      {.name = "hello_world", .description = "Example command."},
      {.name = "hide_errors", .description = "Hide error rows."},
  };
  const std::deque<std::string> history{
      "hide_errors",
      "hello_world sample",
      "help",
      "help",
  };

  const auto suggestions =
      build_console_command_suggestions("he", commands, history, 8u);

  ASSERT_GE(suggestions.size(), 3u);
  EXPECT_EQ(suggestions[0], "help");
  EXPECT_EQ(suggestions[1], "hello_world");
  EXPECT_EQ(suggestions[2], "hide_errors");
}

TEST_F(LoggerConsoleTest, CommandSuggestionsMatchAliasesWithoutDuplicatingEntries) {
  const std::vector<ConsoleManager::CommandInfo> commands{
      {.name = "selection",
       .description = "Inspect selection.",
       .aliases = {"sel"}},
      {.name = "stats", .description = "Print stats."},
  };
  const std::deque<std::string> history{
      "sel cube",
      "stats",
      "selection sphere",
  };

  const auto suggestions =
      build_console_command_suggestions("sel", commands, history, 5u);

  ASSERT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0], "selection");
}

TEST_F(LoggerConsoleTest, EmptyCommandQueryReturnsCommandsOnlyRankedByHistory) {
  const std::vector<ConsoleManager::CommandInfo> commands{
      {.name = "help", .description = "List commands."},
      {.name = "stats", .description = "Print stats."},
      {.name = "spawn_cube", .description = "Spawn a cube."},
      {.name = "toggle_physics", .description = "Toggle physics."},
  };
  const std::deque<std::string> history{
      "help",
      "stats",
      "help",
      "spawn_cube heavy",
      "help",
  };

  const auto suggestions =
      build_console_command_suggestions("", commands, history, 5u);

  ASSERT_EQ(suggestions.size(), 4u);
  EXPECT_EQ(suggestions[0], "help");
  EXPECT_EQ(suggestions[1], "spawn_cube");
  EXPECT_EQ(suggestions[2], "stats");
  EXPECT_EQ(suggestions[3], "toggle_physics");
}

TEST_F(LoggerConsoleTest, CommandSuggestionsIgnoreArgumentQueries) {
  const std::vector<ConsoleManager::CommandInfo> commands{
      {.name = "help", .description = "List commands."},
      {.name = "clear", .description = "Clear output."},
  };
  const std::deque<std::string> history{
      "HELP",
      "help",
      "clear",
      "clear",
  };

  const auto suggestions =
      build_console_command_suggestions("clear now", commands, history, 5u);

  EXPECT_TRUE(suggestions.empty());

  const auto trailing_space =
      build_console_command_suggestions("clear ", commands, history, 5u);

  EXPECT_TRUE(trailing_space.empty());
}

TEST_F(LoggerConsoleTest, HistoryAutocompleteUsesFrecencyAndKeepsUnknownEntries) {
  const std::deque<std::string> history{
      "echo hello",
      "ecoh test",
      "ecoh test",
  };

  const auto autocomplete =
      build_console_history_autocomplete("ec", history);

  ASSERT_TRUE(autocomplete.has_value());
  EXPECT_EQ(*autocomplete, "ecoh test");
}

TEST_F(LoggerConsoleTest, HistoryAutocompleteReturnsLongerPrefixMatchOnly) {
  const std::deque<std::string> history{
      "help",
      "help verbose",
      "help verbose",
  };

  const auto autocomplete =
      build_console_history_autocomplete("help", history);

  ASSERT_TRUE(autocomplete.has_value());
  EXPECT_EQ(*autocomplete, "help verbose");

  const auto none = build_console_history_autocomplete("help verbose", history);

  EXPECT_FALSE(none.has_value());
}

} // namespace
} // namespace astralix
