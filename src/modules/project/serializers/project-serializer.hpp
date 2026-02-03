#pragma once
#include "base.hpp"
#include "context-proxy.hpp"
#include "path.hpp"
#include "project.hpp"
#include "serialization-context.hpp"
#include "serializer.hpp"

namespace astralix {

class Project;

class ProjectSerializer : public Serializer {

public:
  ProjectSerializer(Ref<Project> project, Ref<SerializationContext> ctx);

  ProjectSerializer();

  void serialize() override;
  void deserialize() override;

private:
  Ref<Project> m_project = nullptr;

  Ref<Path> parse_path(ContextProxy ctx);
};

} // namespace astralix
