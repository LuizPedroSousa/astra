#include "output.hpp"

#include <fstream>

bool write_outputs(const astralix::CompileResult &result,
                   const std::filesystem::path &input_file,
                   const std::filesystem::path &output_dir,
                   std::string *error,
                   std::vector<std::filesystem::path> *written_paths) {
  static const std::pair<astralix::StageKind, const char *> stages[] = {
      {astralix::StageKind::Vertex, "vert"},
      {astralix::StageKind::Fragment, "frag"},
      {astralix::StageKind::Geometry, "geom"},
      {astralix::StageKind::Compute, "comp"},
  };

  const auto input_stem = input_file.stem();

  for (auto [kind, extension] : stages) {
    auto it = result.stages.find(kind);
    if (it == result.stages.end()) {
      continue;
    }

    const auto path = output_dir / (input_stem.string() + "." + extension + ".glsl");
    std::ofstream out(path);
    if (!out) {
      if (error) {
        *error = "cannot write to '" + path.string() + "'";
      }
      return false;
    }

    out << it->second;
    if (written_paths) {
      written_paths->push_back(path);
    }
  }

  if (!result.reflection_ir) {
    if (error) {
      *error = "compile result is missing emitted reflection IR";
    }
    return false;
  }

  const auto reflection_path =
      output_dir / (input_stem.string() + ".reflection" +
                    result.reflection_ir->extension);
  std::ofstream reflection_out(reflection_path, std::ios::binary | std::ios::trunc);
  if (!reflection_out) {
    if (error) {
      *error = "cannot write to '" + reflection_path.string() + "'";
    }
    return false;
  }

  reflection_out << result.reflection_ir->content;
  if (!reflection_out.good()) {
    if (error) {
      *error = "cannot write to '" + reflection_path.string() + "'";
    }
    return false;
  }

  if (written_paths) {
    written_paths->push_back(reflection_path);
  }

  if (!result.binding_cpp_header) {
    if (error) {
      *error = "compile result is missing emitted C++ shader bindings";
    }
    return false;
  }

  const auto header_path = output_dir / (input_stem.string() + ".reflection.hpp");
  std::ofstream header_out(header_path);
  if (!header_out) {
    if (error) {
      *error = "cannot write to '" + header_path.string() + "'";
    }
    return false;
  }

  header_out << *result.binding_cpp_header;
  if (written_paths) {
    written_paths->push_back(header_path);
  }

  return true;
}
