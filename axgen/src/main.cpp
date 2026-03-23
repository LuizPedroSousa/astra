#include "args.hpp"
#include "project-bootstrap.hpp"
#include "project-locator.hpp"
#include "shader-lang/compiler.hpp"
#include "sync.hpp"
#include "watch.hpp"

#include <filesystem>
#include <iostream>
#include <optional>

namespace axgen {

namespace {

void print_usage(const char *prog) {
  std::cout << "Usage:\n"
            << "  " << prog
            << " sync-shaders [--manifest <path>] [--root <dir>] [--watch]\n"
            << "  " << prog << " --help\n";
}

std::optional<std::filesystem::path> resolve_manifest(const Options &options,
                                                      std::string *error) {
  std::optional<std::filesystem::path> explicit_manifest;
  if (!options.manifest_path.empty()) {
    explicit_manifest = std::filesystem::path(options.manifest_path);
  }

  auto search_root = options.root_path.empty()
                         ? std::filesystem::current_path()
                         : std::filesystem::absolute(options.root_path);

  return resolve_manifest_path(explicit_manifest, search_root, error);
}

std::vector<astralix::ShaderArtifactInput>
build_artifact_inputs(const ProjectShaderDiscovery &discovery) {
  std::vector<astralix::ShaderArtifactInput> inputs;
  inputs.reserve(discovery.shaders.size());

  for (const auto &shader : discovery.shaders) {
    const bool is_engine_shader = shader.canonical_id.starts_with("engine/");
    inputs.push_back({
        .canonical_id = shader.canonical_id,
        .source_path = shader.source_path,
        .output_root =
            is_engine_shader ? discovery.engine_root : discovery.project_root,
        .umbrella_name =
            is_engine_shader ? "engine_shaders.hpp" : "project_shaders.hpp",
    });
  }

  return inputs;
}

RunContext run_sync_once(const Options &options) {
  RunContext context;

  std::string manifest_error;
  auto manifest_path = resolve_manifest(options, &manifest_error);
  if (!manifest_path) {
    std::cerr << "error: " << manifest_error << '\n';
    if (!options.manifest_path.empty()) {
      context.watch_paths.push_back(
          std::filesystem::absolute(options.manifest_path).lexically_normal());
    }
    return context;
  }

  std::string bootstrap_error;
  auto discovery = discover_project_shaders(*manifest_path, &bootstrap_error);
  if (!discovery) {
    std::cerr << "error: " << bootstrap_error << '\n';
    context.watch_paths.push_back(*manifest_path);
    return context;
  }

  astralix::Compiler compiler;
  auto artifact_plan =
      compiler.build_artifact_plan(build_artifact_inputs(*discovery));
  auto apply_result = apply_shader_artifact_plan(artifact_plan);

  std::cout << format_sync_summary(artifact_plan, apply_result) << '\n';

  for (const auto &failure : artifact_plan.failures) {
    std::cerr << "  [" << failure.canonical_id << "] " << failure.message
              << '\n';
  }

  for (const auto &failure : apply_result.failures) {
    std::cerr << "  [" << failure.path.string() << "] " << failure.message
              << '\n';
  }

  context.ok = artifact_plan.ok() && apply_result.ok();
  context.watch_paths = artifact_plan.watched_paths;
  context.watch_paths.push_back(discovery->manifest_path);
  return context;
}

} // namespace

} // namespace axgen

int main(int argc, char **argv) {
  axgen::Options options;
  if (!axgen::parse_args(argc, argv, options)) {
    std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
    return 1;
  }

  if (options.help) {
    axgen::print_usage(argv[0]);
    return 0;
  }

  try {
    if (options.watch) {
      return axgen::run_watch_loop(options, axgen::run_sync_once);
    }

    return axgen::run_sync_once(options).ok ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
