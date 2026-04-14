#include <cstdio>
#include <gtest/gtest.h>

#define C_RESET "\033[0m"
#define C_BOLD "\033[1m"
#define C_DIM "\033[2m"
#define C_GREEN "\033[32m"
#define C_RED "\033[31m"
#define C_YELLOW "\033[33m"
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
    int skipped = suite.skipped_test_count();
    int total = suite.test_to_run_count();

    if (failed == 0 && skipped == 0) {
      std::printf(C_GRAY "    %d/%d passed" C_RESET "\n", passed, total);
    } else if (failed == 0) {
      std::printf(C_GRAY "    %d passed, " C_YELLOW "%d skipped" C_GRAY
                         ", %d total" C_RESET "\n",
                  passed, skipped, total);
    } else {
      std::printf(C_RED "    %d failed" C_RESET ", %d passed, " C_YELLOW
                        "%d skipped" C_RESET ", %d total\n",
                  failed, passed, skipped, total);
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
    const char *status = C_RED "✕";
    if (info.result()->Passed()) {
      status = C_GREEN "✓";
    } else if (info.result()->Skipped()) {
      status = C_YELLOW "↷";
    }

    std::printf("\r    %s" C_RESET " %s", status, info.name());
    if (info.result()->Skipped()) {
      std::printf(" " C_YELLOW "[skipped]" C_RESET);
    }
    std::printf(C_DIM " (%.0f ms)" C_RESET "\n",
                info.result()->elapsed_time() * 1.0);
  }

  void OnTestProgramEnd(const testing::UnitTest &unit) override {
    int passed = unit.successful_test_count();
    int failed = unit.failed_test_count();
    int skipped = unit.skipped_test_count();
    int total = unit.test_to_run_count();
    int ms = static_cast<int>(unit.elapsed_time());

    std::printf("\n" C_BOLD);
    if (failed == 0 && skipped == 0) {
      std::printf(C_GREEN "  Tests:  %d passed, %d total" C_RESET "\n", passed,
                  total);
    } else if (failed == 0) {
      std::printf(C_GREEN "  Tests:  %d passed" C_RESET ", " C_YELLOW
                          "%d skipped" C_RESET ", %d total\n",
                  passed, skipped, total);
    } else {
      std::printf(C_RED "  Tests:  %d failed" C_RESET ", %d passed, " C_YELLOW
                        "%d skipped" C_RESET ", %d total\n",
                  failed, passed, skipped, total);
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
