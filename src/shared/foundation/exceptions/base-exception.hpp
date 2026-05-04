#pragma once
#include "exception"
#include "string"
#include <sstream>
#include <vector>
#include <utility>

namespace astralix {

using ExceptionMetadata = std::vector<std::pair<std::string, std::string>>;

class BaseException : public std::exception {
public:
  BaseException(const char *file, const char *function, int line,
                std::string message);

  BaseException(const char *file, const char *function, int line,
                std::string message, ExceptionMetadata metadata);

  const char *what() const noexcept override {
    return m_formatted_message.c_str();
  }
  const std::string &message() const noexcept { return m_message; }
  const char *file() const noexcept { return m_file; }
  const char *function() const noexcept { return m_function; }
  int line() const noexcept { return m_line; }
  const ExceptionMetadata &metadata() const noexcept { return m_metadata; }

private:
  void format();

  std::string m_message;
  int m_line;
  const char *m_file;
  const char *m_function;
  ExceptionMetadata m_metadata;

  std::string m_formatted_message;
};

template <typename... Args>
std::string build_exception_message(Args &&...args) {
  std::ostringstream oss;
  (oss << ... << std::forward<Args>(args));
  return oss.str();
}

} // namespace astralix
