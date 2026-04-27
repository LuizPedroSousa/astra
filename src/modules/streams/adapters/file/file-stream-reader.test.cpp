#include "file-stream-reader.hpp"

#include "exceptions/base-exception.hpp"
#include <filesystem>
#include <gtest/gtest.h>

using namespace astralix;

TEST(FileStreamReader, MissingFileThrowsBaseException) {
  const auto missing_path = std::filesystem::temp_directory_path() /
                            "astralix-missing-file-stream-reader.test";
  std::filesystem::remove(missing_path);

  EXPECT_THROW(
      {
        FileStreamReader reader(missing_path);
        reader.read();
      },
      BaseException
  );
}
