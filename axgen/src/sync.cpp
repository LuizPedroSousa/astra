#include "sync.hpp"

#include <fstream>
#include <sstream>

namespace axgen {

namespace {

bool write_text_file_atomic(const std::filesystem::path &path,
                            std::string_view content,
                            std::string *error = nullptr) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    if (error) {
      *error = "cannot create directory '" + path.parent_path().string() +
               "': " + ec.message();
    }
    return false;
  }

  auto tmp_path = path;
  tmp_path += ".tmp";

  std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    if (error) {
      *error = "cannot write to '" + tmp_path.string() + "'";
    }
    return false;
  }

  out << content;
  out.close();
  if (!out.good()) {
    if (error) {
      *error = "cannot write to '" + tmp_path.string() + "'";
    }
    std::filesystem::remove(tmp_path);
    return false;
  }

  std::filesystem::rename(tmp_path, path, ec);
  if (ec) {
    if (error) {
      *error = "cannot replace '" + path.string() + "': " + ec.message();
    }
    std::filesystem::remove(tmp_path);
    return false;
  }

  return true;
}

} // namespace

ArtifactPlanApplyResult
apply_shader_artifact_plan(const astralix::ShaderArtifactPlan &plan) {
  ArtifactPlanApplyResult result;

  for (const auto &path : plan.deletes) {
    if (!std::filesystem::exists(path)) {
      continue;
    }

    std::error_code ec;
    const auto removed = std::filesystem::remove(path, ec);
    if (ec) {
      result.failures.push_back(
          {path, "cannot remove '" + path.string() + "': " + ec.message()});
      continue;
    }

    if (removed) {
      ++result.removed_files;
    }
  }

  for (const auto &write : plan.writes) {
    std::string error;
    if (!write_text_file_atomic(write.path, write.content, &error)) {
      result.failures.push_back({write.path, std::move(error)});
      continue;
    }

    ++result.written_files;
  }

  return result;
}

std::string format_sync_summary(const astralix::ShaderArtifactPlan &plan,
                                const ArtifactPlanApplyResult &apply_result) {
  std::ostringstream out;
  out << "axgen sync-shaders: " << plan.total_shaders << " shader(s), "
      << plan.generated_shaders << " generated, " << plan.unchanged_shaders
      << " unchanged";

  if (apply_result.removed_files > 0) {
    out << ", " << apply_result.removed_files << " removed";
  }

  if (plan.failed_shaders > 0) {
    out << ", " << plan.failed_shaders << " failed";
  }

  if (!apply_result.ok()) {
    out << ", " << apply_result.failures.size() << " apply failed";
  }

  return out.str();
}

} // namespace axgen
