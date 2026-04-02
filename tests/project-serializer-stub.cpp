#include "serializers/project-serializer.hpp"

namespace astralix {

ProjectSerializer::ProjectSerializer(Ref<Project> project,
                                     Ref<SerializationContext> ctx)
    : Serializer(ctx), m_project(project) {}

ProjectSerializer::ProjectSerializer() = default;

void ProjectSerializer::serialize() {}

void ProjectSerializer::deserialize() {}

Ref<Path> ProjectSerializer::parse_path(ContextProxy ctx) {
  (void)ctx;
  return nullptr;
}

} // namespace astralix
