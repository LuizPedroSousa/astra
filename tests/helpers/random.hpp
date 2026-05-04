#pragma once

#include <random>
#include <string>

namespace astralix::testing {

inline std::mt19937 &rng() {
  static std::mt19937 generator{std::random_device{}()};
  return generator;
}

inline int random_integer(int min, int max) {
  std::uniform_int_distribution<int> distribution(min, max);
  return distribution(rng());
}

inline std::string random_alpha(int length) {
  static constexpr char alphabet[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  std::uniform_int_distribution<int> distribution(0, sizeof(alphabet) - 2);
  std::string result;
  result.reserve(static_cast<std::string::size_type>(length));
  for (int i = 0; i < length; ++i)
    result += alphabet[distribution(rng())];
  return result;
}

inline std::string random_sentences() {
  static constexpr const char *words[] = {
      "lorem",      "ipsum",   "dolor",    "sit",       "amet",
      "consectetur","adipiscing","elit",    "sed",       "do",
      "eiusmod",    "tempor",  "incididunt","ut",       "labore",
      "et",         "dolore",  "magna",    "aliqua",    "fusce",
      "posuere",    "morbi",   "tempus",   "iaculis",   "urna",
      "id",         "volutpat","lacus",    "laoreet",   "non",
      "curabitur",  "gravida", "arcu",     "ac",        "tortor",
      "dignissim"};
  int word_count = random_integer(30, 60);
  std::uniform_int_distribution<int> distribution(
      0, sizeof(words) / sizeof(words[0]) - 1);
  std::string result;
  for (int i = 0; i < word_count; ++i) {
    if (i > 0)
      result += ' ';
    result += words[distribution(rng())];
  }
  result += '.';
  return result;
}

} // namespace astralix::testing
