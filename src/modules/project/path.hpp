#pragma once
#include "base.hpp"
#include "guid.hpp"
#include "serializers/path-serializer.hpp"
#include <string>

namespace astralix {

#define RESOURCE_INIT_PARAMS const ResourceHandle &id

enum class BaseDirectory : uint8_t { Unknown = 0, Engine = 1, Project = 2 };

class Path {
public:
  Path(std::string relative_path, BaseDirectory m_base_directory);

  PathID get_path_id() const { return m_path_id; }
  BaseDirectory get_base_directory() const { return m_base_directory; }
  std::string get_relative_path() const { return m_relative_path; }

  static Ref<Path>
  create(std::string relative_path,
         BaseDirectory base_directory = BaseDirectory::Project);

  ~Path() = default;

private:
  PathID m_path_id;
  BaseDirectory m_base_directory;
  std::string m_relative_path;
};

inline Ref<Path> operator"" _engine(const char *str, size_t) {
  return Path::create(str, BaseDirectory::Engine);
}

inline Ref<Path> operator"" _project(const char *str, size_t) {
  return Path::create(str, BaseDirectory::Project);
}

} // namespace astralix
