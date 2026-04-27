#pragma once
#include "stream-reader.hpp"
#include <filesystem>
#include <fstream>

namespace astralix {
class FileStreamReader : public StreamReader {
public:
  FileStreamReader(const std::filesystem::path &path);

  ~FileStreamReader();

  void read() override;

private:
  std::ifstream m_file;
  std::filesystem::path m_path;
  size_t m_total_size = 0;
};

} // namespace astralix
