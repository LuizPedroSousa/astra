#pragma once
#include "base.hpp"
#include "exceptions/base-exception.hpp"
#include "log.hpp"
#include <algorithm>
#include <numeric>
#include <vector>

namespace astralix {

static std::string to_lower(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}

static size_t levenshtein_distance(const std::string &str_value,
                                   const std::string &str_compare) {
  const size_t str_length = str_value.size(),
               str_compare_length = str_compare.size();
  std::vector<std::vector<size_t>> distance(
      str_length + 1, std::vector<size_t>(str_compare_length + 1));

  for (size_t i = 0; i <= str_length; ++i)
    distance[i][0] = i;
  for (size_t j = 0; j <= str_compare_length; ++j)
    distance[0][j] = j;

  for (size_t i = 1; i <= str_length; ++i) {
    for (size_t j = 1; j <= str_compare_length; ++j) {
      distance[i][j] =
          std::min({distance[i - 1][j] + 1, distance[i][j - 1] + 1,
                    distance[i - 1][j - 1] +
                        (str_value[i - 1] == str_compare[j - 1] ? 0 : 1)});
    }
  }
  return distance[str_length][str_compare_length];
}

#define ASTRA_ENSURE_WITH_SUGGESTIONS(EXPRESSION, option_table, item, itemKey, \
                                      sourceKey)                               \
  if (EXPRESSION) {                                                            \
    do {                                                                       \
      std::vector<std::string> suggestions;                                    \
      std::string item_lower = to_lower(item);                                 \
      for (const auto &[key, _] : option_table) {                              \
        std::string key_lower = to_lower(key);                                 \
        if (key_lower.find(item_lower) != std::string::npos ||                 \
            levenshtein_distance(key_lower, item_lower) < 3) {                 \
          suggestions.push_back(key);                                          \
        }                                                                      \
      }                                                                        \
      std::string suggestion_msg =                                             \
          suggestions.empty()                                                  \
              ? ""                                                             \
              : (" Maybe did you mean one of these? \n" + std::string(BOLD) +  \
                 std::string(" - ") +                                          \
                 std::accumulate(                                              \
                     suggestions.begin(), suggestions.end(), std::string{},    \
                     [](const std::string &a, const std::string &b) {          \
                       return a.empty() ? b : a + "\n - " + b;                 \
                     }));                                                      \
                                                                               \
      ASTRA_EXCEPTION(itemKey, " with ID ", BOLD, "[", item, "]", RESET,       \
                      " not found in ", BOLD, sourceKey, RESET,                \
                      suggestion_msg);                                         \
                                                                               \
    } while (0);                                                               \
  }

#define ASTRA_ASSERT(EXPRESSION, MESSAGE)                                      \
  if (EXPRESSION)                                                              \
  return BaseException(__FILE__, __LINE__, MESSAGE)

#define ASTRA_EXCEPTION(...)                                                   \
  throw BaseException(__FILE__, __FUNCTION__, __LINE__,                        \
                      build_exception_message(__VA_ARGS__));

#define ASTRA_ENSURE(EXPRESSION, ...)                                          \
  if (EXPRESSION) {                                                            \
    throw BaseException(__FILE__, __FUNCTION__, __LINE__,                      \
                        build_exception_message(__VA_ARGS__));                 \
  }

#define ASTRA_ASSERT_EITHER(EXPRESSION)                                        \
  if (EXPRESSION.isLeft())                                                     \
  return EXPRESSION.left()

#define ASTRA_ASSERT_EITHER_THROW(EXPRESSION)                                  \
  if (EXPRESSION.isLeft())                                                     \
  throw EXPRESSION.left()

} // namespace astralix
