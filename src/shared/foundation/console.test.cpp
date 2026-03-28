#include "console.hpp"
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

} // namespace
} // namespace astralix
