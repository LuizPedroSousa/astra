#include "serializer.hpp"
#include "assert.hpp"
#include "filesystem"
#include "fstream"
#include "managers/project-manager.hpp"

namespace astralix {

Serializer::Serializer() {
  if (m_ctx == nullptr) {
    auto active_project = ProjectManager::get()->get_active_project();
    m_ctx = SerializationContext::create(
        active_project->get_config().serialization.format);
  }
}

// Json::Value Serializer::read(const std::filesystem::path &absolute_path) {
//   bool file_exists = std::filesystem::exists(absolute_path);
//
//   ASTRA_ENSURE(!file_exists,
//                   "File" + std::string(absolute_path) + " doesn't exists");
//   Json::Value root;
//
//   auto file = std::ifstream(absolute_path);
//
//   Json::CharReaderBuilder rbuilder;
//
//   rbuilder["collectComments"] = false;
//   std::string errs;
//   bool ok = Json::parseFromStream(rbuilder, file, &root, &errs);
//
//   ASTRA_ENSURE(!ok, "Canno parse" + std::string(absolute_path));
//
//   //
// }
//
// void Serializer::write(const std::filesystem::path &absolute_path,
//                        Json::Value &data) {
//
//   auto directory = absolute_path.parent_path();
//
//   bool directory_exists = std::filesystem::exists(directory) &&
//                           std::filesystem::is_directory(directory);
//
//   if (!directory_exists) {
//     ASTRA_ENSURE(!std::filesystem::create_directory(directory),
//                     "Cannot create directory");
//   }
//
//   Json::StreamWriterBuilder writer;
//   std::string json_string = Json::writeString(writer, data);
//
//   std::ofstream out_file(absolute_path);
//
//   if (out_file.is_open()) {
//     out_file << json_string;
//     out_file.close();
//   } else {
//     ASTRA_EXCEPTION("Canno write serialized" +
//     std::string(absolute_path))
//   }
// }

} // namespace astralix
