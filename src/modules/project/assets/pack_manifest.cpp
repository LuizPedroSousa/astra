#include "assets/pack_manifest.hpp"

#include "adapters/file/file-stream-writer.hpp"
#include "arena.hpp"
#include "serialization-context.hpp"
#include "stream-buffer.hpp"

namespace astralix {

void PackManifest::write(const std::filesystem::path &path) const {
  auto ctx = SerializationContext::create(SerializationFormat::Json);
  (*ctx)["version"] = static_cast<int>(version);

  for (size_t asset_index = 0; asset_index < assets.size(); ++asset_index) {
    const auto &asset = assets[asset_index];
    auto asset_ctx = (*ctx)["assets"][static_cast<int>(asset_index)];
    asset_ctx["descriptor_id"] = asset.descriptor_id;
    asset_ctx["kind"] = std::string(asset_kind_name(asset.kind));
    asset_ctx["source_asset"] = asset.source_asset;

    for (size_t dependency_index = 0;
         dependency_index < asset.dependency_ids.size();
         ++dependency_index) {
      asset_ctx["dependency_ids"][static_cast<int>(dependency_index)] =
          asset.dependency_ids[dependency_index];
    }

    for (size_t artifact_index = 0; artifact_index < asset.artifacts.size();
         ++artifact_index) {
      asset_ctx["artifacts"][static_cast<int>(artifact_index)] =
          asset.artifacts[artifact_index];
    }
  }

  std::filesystem::create_directories(path.parent_path());

  ElasticArena arena(KB(64));
  auto *block = ctx->to_buffer(arena);
  auto writer = FileStreamWriter(path, clone_stream_buffer(block));
  writer.write();
}

} // namespace astralix
