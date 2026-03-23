#include "project-locator.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace axgen {

namespace {

namespace fs = std::filesystem;

struct GitignoreRule {
  std::string pattern;
  bool is_directory_only = false;
  bool is_negation = false;
};

std::vector<GitignoreRule>
parse_gitignore(const fs::path &gitignore_path) {
  std::vector<GitignoreRule> rules;

  std::ifstream file(gitignore_path);
  if (!file.is_open()) {
    return rules;
  }

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    while (!line.empty() &&
           (line.back() == ' ' || line.back() == '\t')) {
      line.pop_back();
    }

    if (line.empty()) {
      continue;
    }

    GitignoreRule rule;

    if (line[0] == '!') {
      rule.is_negation = true;
      line = line.substr(1);
    }

    if (!line.empty() && line.back() == '/') {
      rule.is_directory_only = true;
      line.pop_back();
    }

    while (!line.empty() && line.front() == '/') {
      line = line.substr(1);
    }

    if (!line.empty()) {
      rule.pattern = line;
      rules.push_back(rule);
    }
  }

  return rules;
}

bool matches_simple_pattern(const std::string &name,
                            const std::string &pattern) {
  if (pattern.find('*') == std::string::npos) {
    return name == pattern;
  }

  if (pattern.size() >= 2 && pattern[0] == '*' &&
      pattern.find('*', 1) == std::string::npos) {
    auto suffix = pattern.substr(1);
    return name.size() >= suffix.size() &&
           name.compare(name.size() - suffix.size(), suffix.size(), suffix) ==
               0;
  }

  if (pattern.back() == '*' &&
      pattern.find('*') == pattern.size() - 1) {
    auto prefix = pattern.substr(0, pattern.size() - 1);
    return name.size() >= prefix.size() &&
           name.compare(0, prefix.size(), prefix) == 0;
  }

  return name == pattern;
}

bool is_ignored(const fs::path &entry, bool is_directory,
                const std::vector<GitignoreRule> &rules) {
  auto name = entry.filename().string();
  bool ignored = false;

  for (const auto &rule : rules) {
    if (rule.is_directory_only && !is_directory) {
      continue;
    }

    if (matches_simple_pattern(name, rule.pattern)) {
      ignored = !rule.is_negation;
    }
  }

  return ignored;
}

std::optional<fs::path>
search_downward(const fs::path &directory,
                const std::vector<GitignoreRule> &parent_rules) {
  auto rules = parent_rules;
  auto local_gitignore = directory / ".gitignore";
  if (fs::exists(local_gitignore)) {
    auto local_rules = parse_gitignore(local_gitignore);
    rules.insert(rules.end(), local_rules.begin(), local_rules.end());
  }

  auto candidate = directory / "project.ax";
  if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
    return candidate.lexically_normal();
  }

  std::error_code error_code;
  for (const auto &entry :
       fs::directory_iterator(directory, error_code)) {
    if (error_code) {
      continue;
    }

    if (!entry.is_directory()) {
      continue;
    }

    auto dirname = entry.path().filename().string();

    if (!dirname.empty() && dirname[0] == '.') {
      continue;
    }

    if (is_ignored(entry.path(), true, rules)) {
      continue;
    }

    auto found = search_downward(entry.path(), rules);
    if (found) {
      return found;
    }
  }

  return std::nullopt;
}

} // namespace

std::optional<std::filesystem::path>
find_project_manifest(const std::filesystem::path &start_dir,
                      std::string *error) {
  auto current = fs::absolute(start_dir).lexically_normal();
  if (fs::is_regular_file(current)) {
    current = current.parent_path();
  }

  {
    auto search = current;
    while (true) {
      const auto candidate = search / "project.ax";
      if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
        return candidate.lexically_normal();
      }

      if (!search.has_parent_path() || search == search.parent_path()) {
        break;
      }

      search = search.parent_path();
    }
  }

  {
    std::vector<GitignoreRule> root_rules;

    auto ancestor = current;
    while (true) {
      auto gitignore = ancestor / ".gitignore";
      if (fs::exists(gitignore)) {
        root_rules = parse_gitignore(gitignore);
        break;
      }
      if (!ancestor.has_parent_path() ||
          ancestor == ancestor.parent_path()) {
        break;
      }
      ancestor = ancestor.parent_path();
    }

    auto found = search_downward(current, root_rules);
    if (found) {
      return found;
    }
  }

  if (error) {
    *error = "could not find project.ax by searching from '" +
             fs::absolute(start_dir).lexically_normal().string() +
             "'. Try '--manifest <path>'.";
  }
  return std::nullopt;
}

std::optional<std::filesystem::path>
resolve_manifest_path(const std::optional<std::filesystem::path> &manifest_path,
                      const std::filesystem::path &cwd, std::string *error) {
  if (manifest_path.has_value()) {
    auto resolved = manifest_path->is_absolute()
                        ? manifest_path->lexically_normal()
                        : (cwd / *manifest_path).lexically_normal();

    if (!fs::exists(resolved) || !fs::is_regular_file(resolved)) {
      if (error) {
        *error = "manifest path '" + resolved.string() + "' does not exist";
      }
      return std::nullopt;
    }

    return resolved;
  }

  return find_project_manifest(cwd, error);
}

} // namespace axgen
