#include "shader-lang/compiler.hpp"
#include "shader-lang/pipeline-layout-serializer.hpp"
#include "shader-lang/reflection-serializer.hpp"
#include "renderer/platform/Vulkan/shaderc-compiler.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace astralix {

struct SnapshotCase {
  std::string label;
  std::filesystem::path source_path;
  bool use_shared_layout = false;
};

struct SnapshotArtifacts {
  std::map<StageKind, std::string> glsl;
  std::map<StageKind, std::string> vulkan_glsl;
  std::string reflection_json;
  std::string layout_json;
};

namespace {

std::string stage_kind_name(StageKind stage) {
  switch (stage) {
    case StageKind::Vertex:
      return "vertex";
    case StageKind::Fragment:
      return "fragment";
    case StageKind::Geometry:
      return "geometry";
    case StageKind::Compute:
      return "compute";
  }

  return "vertex";
}

std::string shell_quote(std::string_view value) {
  std::string quoted = "'";
  for (char ch : value) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted += "'";
  return quoted;
}

std::string sanitize_filename(std::string_view value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (char ch : value) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') {
      sanitized.push_back(ch);
    } else {
      sanitized.push_back('_');
    }
  }
  return sanitized;
}

std::string read_text_file(const std::filesystem::path &path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("failed to open " + path.string());
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void write_text_file(const std::filesystem::path &path, std::string_view content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  if (!file) {
    throw std::runtime_error("failed to write " + path.string());
  }
  file << content;
}

void write_spirv_binary(
    const std::filesystem::path &path, const std::vector<uint32_t> &spirv
) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to write " + path.string());
  }
  file.write(
      reinterpret_cast<const char *>(spirv.data()),
      static_cast<std::streamsize>(spirv.size() * sizeof(uint32_t))
  );
}

std::optional<std::filesystem::path> find_executable_in_path(std::string_view name) {
  const char *path_env = std::getenv("PATH");
  if (path_env == nullptr) {
    return std::nullopt;
  }

  std::stringstream path_stream(path_env);
  std::string entry;
  while (std::getline(path_stream, entry, ':')) {
    if (entry.empty()) {
      continue;
    }

    std::filesystem::path candidate = std::filesystem::path(entry) / name;
    std::error_code ec;
    const auto status = std::filesystem::status(candidate, ec);
    if (!ec && std::filesystem::exists(status) &&
        (status.permissions() & std::filesystem::perms::owner_exec) !=
            std::filesystem::perms::none) {
      return candidate;
    }
  }

  return std::nullopt;
}

std::vector<std::filesystem::path>
find_axsl_files(const std::filesystem::path &root) {
  std::vector<std::filesystem::path> paths;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension() == ".axsl") {
      paths.push_back(entry.path());
    }
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

std::vector<SnapshotCase>
build_snapshot_cases(const std::filesystem::path &root) {
  std::vector<SnapshotCase> cases;
  const std::filesystem::path shader_root = root / "src/assets/shaders";

  for (const auto &path : find_axsl_files(shader_root)) {
    const auto relative_to_shader_root = path.lexically_relative(shader_root);
    if (relative_to_shader_root.empty()) {
      continue;
    }
    const auto relative_shader_path = relative_to_shader_root.generic_string();
    if (relative_shader_path.rfind("helpers/", 0) == 0) {
      continue;
    }

    cases.push_back(SnapshotCase{
        .label = path.lexically_relative(root).generic_string(),
        .source_path = path,
        .use_shared_layout = false,
    });
  }

  cases.push_back(SnapshotCase{
      .label = "shared-layout/ui-quad",
      .source_path = root / "src/assets/shaders/ui/quad.axsl",
      .use_shared_layout = true,
  });
  cases.push_back(SnapshotCase{
      .label = "shared-layout/ui-image",
      .source_path = root / "src/assets/shaders/ui/image.axsl",
      .use_shared_layout = true,
  });

  return cases;
}

std::string compile_errors_str(const CompileResult &result) {
  std::string errors;
  for (const auto &error : result.errors) {
    errors += error;
    errors += '\n';
  }
  return errors;
}

SnapshotArtifacts capture_snapshot(
    Compiler &compiler,
    const SnapshotCase &snapshot_case,
    bool emit_vulkan_glsl
) {
  CompileOptions options;
  options.emit_vulkan_glsl = emit_vulkan_glsl;

  const std::string source = read_text_file(snapshot_case.source_path);
  auto result = snapshot_case.use_shared_layout
                    ? compiler.compile_with_shared_layout_state(
                          source,
                          snapshot_case.source_path.parent_path().string(),
                          snapshot_case.source_path.string(),
                          options
                      )
                    : compiler.compile(
                          source,
                          snapshot_case.source_path.parent_path().string(),
                          snapshot_case.source_path.string(),
                          options
                      );

  if (!result.ok()) {
    throw std::runtime_error(
        "compile failed for " + snapshot_case.source_path.string() + "\n" +
        compile_errors_str(result)
    );
  }

  std::string reflection_error;
  auto reflection_json = serialize_shader_reflection(
      result.reflection, SerializationFormat::Json, &reflection_error
  );
  if (!reflection_json.has_value()) {
    throw std::runtime_error(reflection_error);
  }

  std::string layout_error;
  auto layout_json = serialize_shader_pipeline_layout(
      result.merged_layout, SerializationFormat::Json, &layout_error
  );
  if (!layout_json.has_value()) {
    throw std::runtime_error(layout_error);
  }

  return SnapshotArtifacts{
      .glsl = result.stages,
      .vulkan_glsl = result.vulkan_glsl_stages,
      .reflection_json = std::move(*reflection_json),
      .layout_json = std::move(*layout_json),
  };
}

void write_snapshot_case(
    const std::filesystem::path &out_root,
    const SnapshotCase &snapshot_case,
    const SnapshotArtifacts &artifacts
) {
  const std::filesystem::path case_root = out_root / snapshot_case.label;

  for (const auto &[stage, glsl] : artifacts.glsl) {
    write_text_file(
        case_root / "glsl" / (stage_kind_name(stage) + ".glsl"), glsl
    );
  }

  for (const auto &[stage, glsl] : artifacts.vulkan_glsl) {
    write_text_file(
        case_root / "vulkan_glsl" / (stage_kind_name(stage) + ".glsl"), glsl
    );
  }

  write_text_file(case_root / "reflection.json", artifacts.reflection_json);
  write_text_file(case_root / "layout.json", artifacts.layout_json);
}

void validate_vulkan_stages(
    const SnapshotCase &snapshot_case,
    const SnapshotArtifacts &artifacts,
    const std::filesystem::path &out_root
) {
  const auto spirv_val = find_executable_in_path("spirv-val");

  for (const auto &[stage, glsl] : artifacts.vulkan_glsl) {
    auto spirv_result = compile_glsl_to_spirv(
        glsl, stage, snapshot_case.label + "/" + stage_kind_name(stage)
    );
    if (!spirv_result.ok()) {
      throw std::runtime_error("shaderc failed for " + snapshot_case.label);
    }

    if (!spirv_val.has_value()) {
      continue;
    }

    const std::filesystem::path module_path =
        out_root / ".spirv-val" /
        (sanitize_filename(snapshot_case.label) + "-" + stage_kind_name(stage) +
         ".spv");
    write_spirv_binary(module_path, spirv_result.spirv);

    const std::string command =
        shell_quote(spirv_val->string()) +
        " --relax-block-layout --target-env vulkan1.3 " +
        shell_quote(module_path.string()) + " >/dev/null";
    const int status = std::system(command.c_str());
    if (status != 0) {
      throw std::runtime_error(
          "spirv-val failed for " + snapshot_case.label + " [" +
          stage_kind_name(stage) + "]"
      );
    }
  }
}

} // namespace

} // namespace astralix

int main(int argc, char **argv) {
  try {
    using namespace astralix;

    std::optional<std::filesystem::path> root;
    std::optional<std::filesystem::path> out;
    bool emit_vulkan_glsl = false;
    bool compile_vulkan_spirv = false;

    for (int i = 1; i < argc; ++i) {
      const std::string_view arg = argv[i];
      if (arg == "--root") {
        if (i + 1 >= argc) {
          throw std::runtime_error("--root requires a value");
        }
        root = argv[++i];
      } else if (arg == "--out") {
        if (i + 1 >= argc) {
          throw std::runtime_error("--out requires a value");
        }
        out = argv[++i];
      } else if (arg == "--emit-vulkan-glsl") {
        emit_vulkan_glsl = true;
      } else if (arg == "--compile-vulkan-spirv") {
        compile_vulkan_spirv = true;
      } else {
        throw std::runtime_error("unknown argument: " + std::string(arg));
      }
    }

    if (!root.has_value() || !out.has_value()) {
      throw std::runtime_error(
          "usage: shader_glsl_snapshot --root <repo-root> --out <dir> "
          "[--emit-vulkan-glsl] [--compile-vulkan-spirv]"
      );
    }

    if (compile_vulkan_spirv) {
      emit_vulkan_glsl = true;
    }

    std::filesystem::remove_all(*out);
    std::filesystem::create_directories(*out);

    Compiler compiler;
    Compiler shared_layout_compiler;
    const auto cases = build_snapshot_cases(*root);

    for (const auto &snapshot_case : cases) {
      auto &active_compiler =
          snapshot_case.use_shared_layout ? shared_layout_compiler : compiler;
      const auto artifacts =
          capture_snapshot(active_compiler, snapshot_case, emit_vulkan_glsl);
      write_snapshot_case(*out, snapshot_case, artifacts);

      if (compile_vulkan_spirv) {
        validate_vulkan_stages(snapshot_case, artifacts, *out);
      }

      std::cout << snapshot_case.label << '\n';
    }

    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
