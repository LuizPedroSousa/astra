#pragma once
#include "base-manager.hpp"
#include "base.hpp"
#include "guid.hpp"
#include "memory"
#include "project.hpp"
#include "unordered_map"
#include "vector"

namespace astralix {

class ProjectManager : public BaseManager<ProjectManager> {
public:
  ProjectManager() = default;

  Ref<Project> get_active_project();
  Ref<Project> add_project(Ref<Project> project);
  ProjectID get_active_project_id() const;
  std::vector<Ref<Project>> get_projects() const;

private:
  std::unordered_map<ProjectID, Ref<Project>> m_projects;

  ProjectID m_active_project_id;
};

inline Ref<ProjectManager> project_manager() { return ProjectManager::get(); }
inline Ref<Project> active_project() {
  return ProjectManager::get()->get_active_project();
}
} // namespace astralix
