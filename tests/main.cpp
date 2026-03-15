#include <cstdio>
#include <gtest/gtest.h>

#define C_RESET "\033[0m"
#define C_BOLD "\033[1m"
#define C_DIM "\033[2m"
#define C_GREEN "\033[32m"
#define C_RED "\033[31m"
#define C_GRAY "\033[90m"
#define C_CYAN "\033[36m"

class PrettyPrinter : public testing::EmptyTestEventListener {
public:
  void OnTestSuiteStart(const testing::TestSuite &suite) override {
    std::printf("\n" C_BOLD C_CYAN "  %s" C_RESET "\n", suite.name());
  }

  void OnTestSuiteEnd(const testing::TestSuite &suite) override {
    int passed = suite.successful_test_count();
    int failed = suite.failed_test_count();
    int total = suite.total_test_count();

    if (failed == 0) {
      std::printf(C_GRAY "    %d/%d passed" C_RESET "\n", passed, total);
    } else {
      std::printf(C_RED "    %d failed, %d passed, %d total" C_RESET "\n",
                  failed, passed, total);
    }
  }

  void OnTestStart(const testing::TestInfo &info) override {
    std::printf(C_DIM "    ○ %s" C_RESET, info.name());
    std::fflush(stdout);
  }

  void OnTestPartResult(const testing::TestPartResult &result) override {
    if (result.failed()) {
      std::printf("\n" C_RED "      ✕ %s" C_RESET C_DIM " (%s:%d)" C_RESET
                  "\n" C_RED "        %s" C_RESET "\n",
                  result.summary(),
                  result.file_name() ? result.file_name() : "?",
                  result.line_number(), result.message());
    }
  }

  void OnTestEnd(const testing::TestInfo &info) override {
    const char *status = info.result()->Passed() ? C_GREEN "✓" : C_RED "✕";
    std::printf("\r    %s" C_RESET " %s" C_DIM " (%.0f ms)" C_RESET "\n",
                status, info.name(), info.result()->elapsed_time() * 1.0);
  }

  void OnTestProgramEnd(const testing::UnitTest &unit) override {
    int passed = unit.successful_test_count();
    int failed = unit.failed_test_count();
    int total = unit.total_test_count();
    int ms = static_cast<int>(unit.elapsed_time());

    std::printf("\n" C_BOLD);
    if (failed == 0) {
      std::printf(C_GREEN "  Tests:  %d passed, %d total" C_RESET "\n", passed,
                  total);
    } else {
      std::printf(C_RED "  Tests:  %d failed" C_RESET ", %d passed, %d total\n",
                  failed, passed, total);
    }
    std::printf(C_GRAY "  Time:   %d ms" C_RESET "\n\n", ms);
  }
};

#define REPEAT 1

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);

  auto &listeners = testing::UnitTest::GetInstance()->listeners();
  delete listeners.Release(listeners.default_result_printer());
  listeners.Append(new PrettyPrinter);

  for (int i = 0; i < REPEAT; i++) {
    int result = RUN_ALL_TESTS();
    if (result != 0)
      return result;
  }

  return 0;
}
