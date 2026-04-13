#include "shader-lang/artifacts/shader-artifact-pipeline.hpp"

#include "fnv1a.hpp"
#include "shader-lang/compiler.hpp"
#include "shader-lang/emitters/binding-cpp-emitter.hpp"
#include "shader-lang/emitters/reflection-ir-emitter.hpp"
#include "shader-lang/emitters/pipeline-layout-ir-emitter.hpp"
#include "shader-lang/emitters/umbrella-header-emitter.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace astralix {

  namespace {

    constexpr std::string_view k_generator_version = "axgen-v3";
    constexpr int k_reflection_version = 3;

    struct CacheEntry {
      std::string fingerprint;
      std::vector<std::filesystem::path> dependencies;
    };

    struct CompileMemoEntry {
      bool ok = false;
      CompileResult result;
      std::string error;
    };

    struct ArtifactPaths {
      std::optional<std::filesystem::path> binding_header;
      std::vector<std::pair<SerializationFormat, std::filesystem::path>>
        reflection_outputs;
      std::vector<std::pair<SerializationFormat, std::filesystem::path>>
        layout_outputs;
      std::optional<std::filesystem::path> cache_metadata;
    };

    std::filesystem::path normalize_path(const std::filesystem::path& path) {
      return path.lexically_normal();
    }

    std::optional<std::string> read_file(const std::filesystem::path& path, std::string* error = nullptr) {
      std::ifstream file(path, std::ios::binary);
      if (!file) {
        if (error) {
          *error = "cannot open '" + path.string() + "'";
        }
        return std::nullopt;
      }

      std::ostringstream buffer;
      buffer << file.rdbuf();
      return buffer.str();
    }

    std::vector<std::filesystem::path>
      unique_paths(std::vector<std::filesystem::path> paths) {
      std::sort(paths.begin(), paths.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.generic_string() < rhs.generic_string();
        });
      paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
      return paths;
    }

    std::string aggregate_errors(const CompileResult& result) {
      std::ostringstream out;
      for (size_t i = 0; i < result.errors.size(); ++i) {
        if (i != 0) {
          out << '\n';
        }
        out << result.errors[i];
      }
      return out.str();
    }

    bool has_unit_artifact(const ShaderArtifactBuildOptions& options, ShaderUnitArtifactKind kind) {
      return std::any_of(
        options.unit_artifacts.begin(), options.unit_artifacts.end(), [kind](const ShaderUnitArtifactSpec& spec) { return spec.kind == kind; }
      );
    }

    bool has_batch_artifact(const ShaderArtifactBuildOptions& options, ShaderBatchArtifactKind kind) {
      return std::any_of(options.batch_artifacts.begin(), options.batch_artifacts.end(), [kind](const ShaderBatchArtifactSpec& spec) {
        return spec.kind == kind;
        });
    }

    std::vector<SerializationFormat>
      requested_layout_formats(const ShaderArtifactBuildOptions& options) {
      std::vector<SerializationFormat> formats;

      for (const auto& spec : options.unit_artifacts) {
        if (spec.kind != ShaderUnitArtifactKind::PipelineLayoutIR) {
          continue;
        }

        if (std::find(formats.begin(), formats.end(), spec.format) ==
          formats.end()) {
          formats.push_back(spec.format);
        }
      }

      return formats;
    }

    std::vector<SerializationFormat>
      requested_reflection_formats(const ShaderArtifactBuildOptions& options) {
      std::vector<SerializationFormat> formats;

      for (const auto& spec : options.unit_artifacts) {
        if (spec.kind != ShaderUnitArtifactKind::ReflectionIR) {
          continue;
        }

        if (std::find(formats.begin(), formats.end(), spec.format) ==
          formats.end()) {
          formats.push_back(spec.format);
        }
      }

      return formats;
    }

    std::filesystem::path generated_root(const std::filesystem::path& output_root) {
      return output_root / ".astralix" / "generated";
    }

    std::filesystem::path
      generated_shader_dir(const std::filesystem::path& output_root) {
      return generated_root(output_root) / "shaders";
    }

    std::filesystem::path
      generated_reflection_dir(const std::filesystem::path& output_root) {
      return generated_root(output_root) / "reflections";
    }

    std::filesystem::path
      generated_layout_dir(const std::filesystem::path& output_root) {
      return generated_root(output_root) / "layouts";
    }

    std::filesystem::path cache_root(const std::filesystem::path& output_root) {
      return output_root / ".astralix" / "cache" / "shader-artifacts";
    }

    std::filesystem::path
      legacy_cache_root(const std::filesystem::path& output_root) {
      return output_root / ".astralix" / "cache" / "axgen";
    }

    std::filesystem::path header_path_for(const std::filesystem::path& output_root, std::string_view canonical_id) {
      return generated_shader_dir(output_root) /
        (sanitize_generated_shader_name(canonical_id) + ".hpp");
    }

    std::optional<std::filesystem::path>
      layout_path_for(const std::filesystem::path& output_root, std::string_view canonical_id, SerializationFormat format, std::string* error = nullptr) {
      try {
        auto extension = SerializationContext::create(format)->extension();
        return generated_layout_dir(output_root) /
          (sanitize_generated_shader_name(canonical_id) + ".layout" +
            extension);
      }
      catch (const std::exception& exception) {
        if (error) {
          *error = exception.what();
        }
        return std::nullopt;
      }
    }

    std::optional<std::filesystem::path>
      reflection_path_for(const std::filesystem::path& output_root, std::string_view canonical_id, SerializationFormat format, std::string* error = nullptr) {
      try {
        auto extension = SerializationContext::create(format)->extension();
        return generated_reflection_dir(output_root) /
          (sanitize_generated_shader_name(canonical_id) + ".reflection" +
            extension);
      }
      catch (const std::exception& exception) {
        if (error) {
          *error = exception.what();
        }
        return std::nullopt;
      }
    }

    std::filesystem::path cache_entry_path(const std::filesystem::path& output_root, std::string_view canonical_id) {
      return cache_root(output_root) /
        (sanitize_generated_shader_name(canonical_id) + ".meta");
    }

    std::filesystem::path
      legacy_cache_entry_path(const std::filesystem::path& output_root, std::string_view canonical_id) {
      return legacy_cache_root(output_root) /
        (sanitize_generated_shader_name(canonical_id) + ".meta");
    }

    std::filesystem::path
      umbrella_header_path(const std::filesystem::path& output_root, std::string_view umbrella_name) {
      return generated_root(output_root) / umbrella_name;
    }

    std::optional<std::string> compute_artifact_fingerprint(
      const std::filesystem::path& source_path,
      const std::vector<std::filesystem::path>& dependencies,
      std::string_view canonical_id, std::string* error = nullptr
    ) {
      uint64_t hash = k_fnv1a64_offset_basis;
      const auto append = [&](std::string_view value) {
        hash = fnv1a64_append_string(value, hash);
        };
      append("generator:");
      append(k_generator_version);
      append("\nreflection:");
      append(std::to_string(k_reflection_version));
      append("\ncanonical:");
      append(canonical_id);
      append("\n");

      auto append_file = [&](std::string_view role,
        const std::filesystem::path& path) -> bool {
          auto content = read_file(path, error);
          if (!content) {
            return false;
          }

          append(role);
          append(":");
          append(normalize_path(path).generic_string());
          append("\n");
          append(*content);
          append("\n");
          return true;
        };

      if (!append_file("source", source_path)) {
        return std::nullopt;
      }

      for (const auto& dependency : dependencies) {
        if (!append_file("dependency", dependency)) {
          return std::nullopt;
        }
      }

      return fnv1a64_hex_digest(hash);
    }

    std::optional<CacheEntry> read_cache_entry(const std::filesystem::path& path) {
      if (!std::filesystem::exists(path)) {
        return std::nullopt;
      }

      std::ifstream file(path);
      if (!file) {
        return std::nullopt;
      }

      CacheEntry entry;
      std::string line;
      while (std::getline(file, line)) {
        if (line.starts_with("fingerprint ")) {
          entry.fingerprint = line.substr(std::string("fingerprint ").size());
          continue;
        }

        if (line.starts_with("dependency ")) {
          entry.dependencies.push_back(
            normalize_path(line.substr(std::string("dependency ").size()))
          );
        }
      }

      if (entry.fingerprint.empty()) {
        return std::nullopt;
      }

      return entry;
    }

    std::optional<CacheEntry>
      read_cache_entry_with_legacy(const std::filesystem::path& primary_path, const std::filesystem::path& legacy_path) {
      auto primary = read_cache_entry(primary_path);
      if (primary) {
        return primary;
      }

      return read_cache_entry(legacy_path);
    }

    std::string serialize_cache_entry(const CacheEntry& entry) {
      std::ostringstream out;
      out << "fingerprint " << entry.fingerprint << '\n';
      for (const auto& dependency : entry.dependencies) {
        out << "dependency " << normalize_path(dependency).generic_string() << '\n';
      }
      return out.str();
    }

    CompileMemoEntry compile_shader(const std::filesystem::path& source_path) {
      CompileMemoEntry memo;

      auto source = read_file(source_path, &memo.error);
      if (!source) {
        return memo;
      }

      Compiler compiler;
      memo.result = compiler.compile(*source, source_path.parent_path().string(), source_path.string());

      if (!memo.result.ok()) {
        memo.error = aggregate_errors(memo.result);
        return memo;
      }

      memo.ok = true;
      return memo;
    }

    bool file_exists_and_matches(const std::filesystem::path& path, std::string_view expected_content) {
      auto existing = read_file(path);
      return existing && *existing == expected_content;
    }

    void push_planned_write_if_changed(ShaderArtifactPlan& plan, const std::filesystem::path& path, const std::string& content) {
      if (file_exists_and_matches(path, content)) {
        return;
      }

      plan.writes.push_back({ path, content });
    }

    bool contains_path(const std::vector<std::filesystem::path>& paths, const std::filesystem::path& target) {
      const auto normalized_target = normalize_path(target);
      return std::any_of(paths.begin(), paths.end(), [&](const auto& path) {
        return normalize_path(path) == normalized_target;
        });
    }

    bool path_will_exist_after_plan(const ShaderArtifactPlan& plan, const std::filesystem::path& path) {
      if (contains_path(plan.deletes, path)) {
        return false;
      }

      return std::any_of(plan.writes.begin(), plan.writes.end(), [&](const ShaderPlannedWrite& write) {
        return normalize_path(write.path) ==
          normalize_path(path);
        }) ||
        std::filesystem::exists(path);
    }

    void add_delete_if_exists(std::vector<std::filesystem::path>& deletes, const std::filesystem::path& path) {
      if (!std::filesystem::exists(path)) {
        return;
      }

      deletes.push_back(path);
    }

    ArtifactPaths planned_artifact_paths(const ShaderArtifactInput& input, const ShaderArtifactBuildOptions& options, std::string* error = nullptr) {
      ArtifactPaths paths;

      if (has_unit_artifact(options, ShaderUnitArtifactKind::BindingCppHeader)) {
        paths.binding_header =
          header_path_for(input.output_root, input.canonical_id);
      }

      if (has_unit_artifact(options, ShaderUnitArtifactKind::ReflectionIR)) {
        for (const auto format : requested_reflection_formats(options)) {
          auto reflection_path = reflection_path_for(
            input.output_root, input.canonical_id, format, error
          );
          if (!reflection_path) {
            return {};
          }
          paths.reflection_outputs.emplace_back(format, *reflection_path);
        }
      }

      if (has_unit_artifact(options, ShaderUnitArtifactKind::PipelineLayoutIR)) {
        for (const auto format : requested_layout_formats(options)) {
          auto layout_path = layout_path_for(
            input.output_root, input.canonical_id, format, error
          );
          if (!layout_path) {
            return {};
          }
          paths.layout_outputs.emplace_back(format, *layout_path);
        }
      }

      if (has_unit_artifact(options, ShaderUnitArtifactKind::CacheMetadata)) {
        paths.cache_metadata =
          cache_entry_path(input.output_root, input.canonical_id);
      }

      return paths;
    }

    bool all_requested_artifacts_exist(const ArtifactPaths& paths, bool cache_entry_available) {
      if (paths.binding_header && !std::filesystem::exists(*paths.binding_header)) {
        return false;
      }

      for (const auto& [format, path] : paths.reflection_outputs) {
        (void)format;
        if (!std::filesystem::exists(path)) {
          return false;
        }
      }

      for (const auto& [format, path] : paths.layout_outputs) {
        (void)format;
        if (!std::filesystem::exists(path)) {
          return false;
        }
      }

      if (paths.cache_metadata && !cache_entry_available) {
        return false;
      }

      return true;
    }

    std::string normalize_generic_string(const std::filesystem::path& path) {
      return normalize_path(path).generic_string();
    }

    void sort_and_unique_deletes(std::vector<std::filesystem::path>& deletes) {
      std::sort(
        deletes.begin(), deletes.end(), [](const auto& lhs, const auto& rhs) {
          return normalize_generic_string(lhs) < normalize_generic_string(rhs);
        }
      );
      deletes.erase(std::unique(deletes.begin(), deletes.end(), [](const auto& lhs, const auto& rhs) {
        return normalize_path(lhs) == normalize_path(rhs);
        }),
        deletes.end());
    }

    void sort_writes(std::vector<ShaderPlannedWrite>& writes) {
      std::sort(writes.begin(), writes.end(), [](const auto& lhs, const auto& rhs) {
        return normalize_generic_string(lhs.path) <
          normalize_generic_string(rhs.path);
        });
    }

  } // namespace

  std::string sanitize_generated_shader_name(std::string_view canonical_id) {
    return BindingCppEmitter::sanitize_namespace(canonical_id);
  }

  ShaderArtifactPlan ShaderArtifactPipeline::build_plan(
    const std::vector<ShaderArtifactInput>& inputs,
    const ShaderArtifactBuildOptions& options
  ) {
    ShaderArtifactPlan plan;

    std::map<std::string, ShaderArtifactInput> unique_inputs;
    std::unordered_map<std::string, std::string> generated_names;

    auto push_failure = [&](std::string canonical_id,
      std::filesystem::path source_path,
      std::string message) {
        plan.failures.push_back(
          { std::move(canonical_id), std::move(source_path), std::move(message) }
        );
        ++plan.failed_shaders;
      };

    for (const auto& input : inputs) {
      if (input.canonical_id.empty() || input.source_path.empty() ||
        input.source_path.extension() != ".axsl") {
        continue;
      }

      const auto normalized_source = normalize_path(input.source_path);
      const auto generated_name =
        sanitize_generated_shader_name(input.canonical_id);

      auto generated_name_it = generated_names.find(generated_name);
      if (generated_name_it != generated_names.end() &&
        generated_name_it->second != input.canonical_id) {
        push_failure(input.canonical_id, normalized_source, "generated header name collision with '" + generated_name_it->second + "'");
        continue;
      }
      generated_names.emplace(generated_name, input.canonical_id);

      auto [it, inserted] = unique_inputs.emplace(
        input.canonical_id,
        ShaderArtifactInput{ input.canonical_id, normalized_source, normalize_path(input.output_root), input.umbrella_name }
      );
      if (!inserted) {
        const bool same_source = it->second.source_path == normalized_source;
        const bool same_root =
          it->second.output_root == normalize_path(input.output_root);
        const bool same_umbrella =
          it->second.umbrella_name == input.umbrella_name;
        if (!same_source || !same_root || !same_umbrella) {
          push_failure(
            input.canonical_id, normalized_source, "canonical shader id resolves to multiple artifact inputs"
          );
        }
      }
    }

    plan.total_shaders = static_cast<int>(unique_inputs.size());

    const bool wants_binding_cpp =
      has_unit_artifact(options, ShaderUnitArtifactKind::BindingCppHeader);
    const bool wants_reflection_ir =
      has_unit_artifact(options, ShaderUnitArtifactKind::ReflectionIR);
    const bool wants_cache_metadata =
      has_unit_artifact(options, ShaderUnitArtifactKind::CacheMetadata);
    const bool wants_umbrella =
      has_batch_artifact(options, ShaderBatchArtifactKind::UmbrellaHeader);

    const bool wants_layout_ir =
      has_unit_artifact(options, ShaderUnitArtifactKind::PipelineLayoutIR);

    std::unordered_map<std::string, CompileMemoEntry> compile_memo;
    std::set<std::string> expected_generated_headers;
    std::set<std::string> expected_generated_reflections;
    std::set<std::string> expected_generated_layouts;
    std::set<std::string> expected_cache_entries;
    std::set<std::string> expected_umbrellas;
    std::set<std::string> managed_roots;

    struct GroupInfo {
      std::filesystem::path output_root;
      std::string umbrella_name;
      std::vector<std::pair<std::string, std::filesystem::path>> headers;
    };

    std::map<std::pair<std::string, std::string>, GroupInfo> umbrella_groups;

    for (const auto& [canonical_id, input] : unique_inputs) {
      managed_roots.insert(input.output_root.generic_string());

      std::string path_error;
      auto artifact_paths = planned_artifact_paths(input, options, &path_error);
      if (!path_error.empty()) {
        push_failure(canonical_id, input.source_path, path_error);
        continue;
      }

      if (artifact_paths.binding_header) {
        expected_generated_headers.insert(
          normalize_generic_string(*artifact_paths.binding_header)
        );
      }
      for (const auto& [format, path] : artifact_paths.reflection_outputs) {
        (void)format;
        expected_generated_reflections.insert(normalize_generic_string(path));
      }
      for (const auto& [format, path] : artifact_paths.layout_outputs) {
        (void)format;
        expected_generated_layouts.insert(normalize_generic_string(path));
      }
      if (artifact_paths.cache_metadata) {
        expected_cache_entries.insert(
          normalize_generic_string(*artifact_paths.cache_metadata)
        );
      }

      const auto metadata_path =
        cache_entry_path(input.output_root, canonical_id);
      const auto legacy_metadata_path =
        legacy_cache_entry_path(input.output_root, canonical_id);

      auto cached_entry =
        options.use_cache
        ? read_cache_entry_with_legacy(metadata_path, legacy_metadata_path)
        : std::nullopt;

      std::vector<std::filesystem::path> watched_dependencies;
      if (cached_entry) {
        watched_dependencies = cached_entry->dependencies;
      }

      bool treated_as_unchanged = false;
      if (options.use_cache && cached_entry &&
        all_requested_artifacts_exist(artifact_paths, true)) {
        std::string fingerprint_error;
        auto current_fingerprint = compute_artifact_fingerprint(
          input.source_path, cached_entry->dependencies, canonical_id, &fingerprint_error
        );

        if (current_fingerprint &&
          *current_fingerprint == cached_entry->fingerprint) {
          ++plan.unchanged_shaders;
          plan.watched_paths.push_back(input.source_path);
          plan.watched_paths.insert(plan.watched_paths.end(), cached_entry->dependencies.begin(), cached_entry->dependencies.end());
          treated_as_unchanged = true;

          if (wants_cache_metadata) {
            const CacheEntry updated_entry{ *current_fingerprint, cached_entry->dependencies };
            push_planned_write_if_changed(plan, metadata_path, serialize_cache_entry(updated_entry));
          }
        }
      }

      if (!treated_as_unchanged) {
        const auto memo_key = input.source_path.generic_string();
        CompileMemoEntry local_memo;
        CompileMemoEntry* memo = nullptr;

        if (options.memoize_compiles) {
          auto memo_it = compile_memo.find(memo_key);
          if (memo_it == compile_memo.end()) {
            memo_it =
              compile_memo.emplace(memo_key, compile_shader(input.source_path))
              .first;
          }
          memo = &memo_it->second;
        }
        else {
          local_memo = compile_shader(input.source_path);
          memo = &local_memo;
        }

        if (!memo->result.dependencies.empty()) {
          watched_dependencies = memo->result.dependencies;
        }

        plan.watched_paths.push_back(input.source_path);
        plan.watched_paths.insert(plan.watched_paths.end(), watched_dependencies.begin(), watched_dependencies.end());

        if (!memo->ok) {
          if (!options.preserve_last_good_outputs) {
            if (artifact_paths.binding_header) {
              add_delete_if_exists(plan.deletes, *artifact_paths.binding_header);
            }
            for (const auto& [format, path] : artifact_paths.reflection_outputs) {
              (void)format;
              add_delete_if_exists(plan.deletes, path);
            }
            for (const auto& [format, path] : artifact_paths.layout_outputs) {
              (void)format;
              add_delete_if_exists(plan.deletes, path);
            }
          }

          push_failure(canonical_id, input.source_path, memo->error);
          continue;
        }

        BindingCppEmitter binding_cpp_emitter;
        ReflectionIREmitter reflection_ir_emitter;

        std::optional<std::string> binding_header;
        if (wants_binding_cpp) {
          std::string binding_error;
          binding_header = binding_cpp_emitter.emit(memo->result.reflection, canonical_id, &binding_error);
          if (!binding_header) {
            push_failure(canonical_id, input.source_path, binding_error);
            continue;
          }
        }

        std::vector<std::pair<std::filesystem::path, std::string>>
          reflection_outputs;
        if (wants_reflection_ir) {
          bool reflection_failed = false;
          for (const auto& [format, path] : artifact_paths.reflection_outputs) {
            std::string reflection_error;
            auto reflection = reflection_ir_emitter.emit(
              memo->result.reflection, format, &reflection_error
            );
            if (!reflection) {
              push_failure(canonical_id, input.source_path, reflection_error);
              reflection_failed = true;
              break;
            }

            reflection_outputs.emplace_back(path, std::move(reflection->content));
          }
          if (reflection_failed) {
            continue;
          }
        }

        std::vector<std::pair<std::filesystem::path, std::string>>
          layout_outputs;
        if (wants_layout_ir) {
          PipelineLayoutIREmitter layout_ir_emitter;
          bool layout_failed = false;
          for (const auto& [format, path] : artifact_paths.layout_outputs) {
            std::string layout_error;
            auto layout = layout_ir_emitter.emit(
              memo->result.merged_layout, format, &layout_error
            );
            if (!layout) {
              push_failure(canonical_id, input.source_path, layout_error);
              layout_failed = true;
              break;
            }

            layout_outputs.emplace_back(path, std::move(layout->content));
          }
          if (layout_failed) {
            continue;
          }
        }

        std::string fingerprint_error;
        auto fingerprint =
          compute_artifact_fingerprint(input.source_path, watched_dependencies, canonical_id, &fingerprint_error);
        if (!fingerprint) {
          push_failure(canonical_id, input.source_path, fingerprint_error);
          continue;
        }

        if (artifact_paths.binding_header && binding_header) {
          push_planned_write_if_changed(plan, *artifact_paths.binding_header, *binding_header);
        }

        for (const auto& [path, content] : reflection_outputs) {
          push_planned_write_if_changed(plan, path, content);
        }

        for (const auto& [path, content] : layout_outputs) {
          push_planned_write_if_changed(plan, path, content);
        }

        if (artifact_paths.cache_metadata) {
          const CacheEntry updated_entry{ *fingerprint, watched_dependencies };
          push_planned_write_if_changed(plan, *artifact_paths.cache_metadata, serialize_cache_entry(updated_entry));
        }

        ++plan.generated_shaders;
      }

      if (wants_umbrella && wants_binding_cpp && !input.umbrella_name.empty() &&
        artifact_paths.binding_header &&
        path_will_exist_after_plan(plan, *artifact_paths.binding_header)) {
        const auto group_key = std::make_pair(input.output_root.generic_string(), input.umbrella_name);
        auto& group = umbrella_groups[group_key];
        group.output_root = input.output_root;
        group.umbrella_name = input.umbrella_name;
        group.headers.emplace_back(canonical_id, std::filesystem::path("shaders") / artifact_paths.binding_header->filename());
      }
    }

    if (wants_umbrella) {
      UmbrellaHeaderEmitter emitter;

      for (auto& [group_key, group] : umbrella_groups) {
        (void)group_key;
        std::sort(group.headers.begin(), group.headers.end(), [](const auto& lhs, const auto& rhs) {
          return lhs.first < rhs.first;
          });

        const auto path =
          umbrella_header_path(group.output_root, group.umbrella_name);
        expected_umbrellas.insert(normalize_generic_string(path));
        push_planned_write_if_changed(plan, path, emitter.emit(group.headers));
      }
    }

    if (options.prune_stale) {
      for (const auto& root_key : managed_roots) {
        const auto output_root = std::filesystem::path(root_key);

        if (wants_binding_cpp &&
          std::filesystem::exists(generated_shader_dir(output_root))) {
          for (const auto& entry : std::filesystem::directory_iterator(
            generated_shader_dir(output_root)
          )) {
            if (!entry.is_regular_file()) {
              continue;
            }

            const auto normalized = normalize_generic_string(entry.path());
            if (expected_generated_headers.count(normalized) == 0) {
              plan.deletes.push_back(entry.path());
            }
          }
        }

        if (wants_reflection_ir &&
          std::filesystem::exists(generated_reflection_dir(output_root))) {
          for (const auto& entry : std::filesystem::directory_iterator(
            generated_reflection_dir(output_root)
          )) {
            if (!entry.is_regular_file()) {
              continue;
            }

            const auto normalized = normalize_generic_string(entry.path());
            if (expected_generated_reflections.count(normalized) == 0) {
              plan.deletes.push_back(entry.path());
            }
          }
        }

        if (wants_layout_ir &&
          std::filesystem::exists(generated_layout_dir(output_root))) {
          for (const auto& entry : std::filesystem::directory_iterator(
            generated_layout_dir(output_root)
          )) {
            if (!entry.is_regular_file()) {
              continue;
            }

            const auto normalized = normalize_generic_string(entry.path());
            if (expected_generated_layouts.count(normalized) == 0) {
              plan.deletes.push_back(entry.path());
            }
          }
        }

        if (wants_cache_metadata &&
          std::filesystem::exists(cache_root(output_root))) {
          for (const auto& entry :
            std::filesystem::directory_iterator(cache_root(output_root))) {
            if (!entry.is_regular_file()) {
              continue;
            }

            const auto normalized = normalize_generic_string(entry.path());
            if (expected_cache_entries.count(normalized) == 0) {
              plan.deletes.push_back(entry.path());
            }
          }
        }

        if (wants_umbrella &&
          std::filesystem::exists(generated_root(output_root))) {
          for (const auto& entry :
            std::filesystem::directory_iterator(generated_root(output_root))) {
            if (!entry.is_regular_file()) {
              continue;
            }

            const auto filename = entry.path().filename().string();
            if (!std::string_view(filename).ends_with("_shaders.hpp")) {
              continue;
            }

            const auto normalized = normalize_generic_string(entry.path());
            if (expected_umbrellas.count(normalized) == 0) {
              plan.deletes.push_back(entry.path());
            }
          }
        }
      }
    }

    sort_and_unique_deletes(plan.deletes);
    sort_writes(plan.writes);
    plan.planned_removals = static_cast<int>(plan.deletes.size());
    plan.watched_paths = unique_paths(std::move(plan.watched_paths));
    return plan;
  }

} // namespace astralix
