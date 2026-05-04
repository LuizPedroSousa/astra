#include "terrain-recipe-data.hpp"
#include "adapters/file/file-stream-reader.hpp"
#include "assert.hpp"
#include "context-proxy.hpp"
#include "log.hpp"
#include "serialization-context.hpp"

namespace astralix::terrain {

namespace {

float read_number(ContextProxy ctx, float fallback = 0.0f) {
  switch (ctx.kind()) {
    case SerializationTypeKind::Float:
      return ctx.as<float>();
    case SerializationTypeKind::Int:
      return static_cast<float>(ctx.as<int>());
    default:
      return fallback;
  }
}

} // namespace

TerrainRecipeData parse_terrain_recipe(const std::string &recipe_path) {
  TerrainRecipeData data;

  FileStreamReader stream(recipe_path);
  stream.read();

  auto context = SerializationContext::create(SerializationFormat::Json,
                                              stream.get_buffer());

  auto root = (*context)["terrain"];
  const bool legacy_wrapped = root.kind() == SerializationTypeKind::Object;
  auto field = [&](const char *key) {
    return legacy_wrapped ? root[key] : (*context)[key];
  };

  if (field("version").kind() != SerializationTypeKind::Unknown) {
    ASTRA_ENSURE(
        field("version").kind() != SerializationTypeKind::Int,
        "Terrain recipe version must be an integer"
    );
    ASTRA_ENSURE(
        field("version").as<int>() != 1,
        "Unsupported terrain recipe version: ",
        field("version").as<int>()
    );
  }

  data.resolution = static_cast<uint32_t>(
      read_number(field("resolution"), 1025.0f));

  auto noise = field("noise");
  if (noise.kind() == SerializationTypeKind::Object) {
    if (noise["type"].kind() == SerializationTypeKind::String)
      data.noise.type = noise["type"].as<std::string>();
    data.noise.seed = static_cast<uint32_t>(read_number(noise["seed"], 42.0f));
    data.noise.octaves = static_cast<uint32_t>(read_number(noise["octaves"], 6.0f));
    data.noise.frequency = read_number(noise["frequency"], 0.005f);
    data.noise.lacunarity = read_number(noise["lacunarity"], 2.0f);
    data.noise.persistence = read_number(noise["persistence"], 0.5f);
    data.noise.amplitude = read_number(noise["amplitude"], 1.0f);
  }

  auto erosion = field("erosion");
  if (erosion.kind() == SerializationTypeKind::Object) {
    data.erosion.iterations = static_cast<uint32_t>(
        read_number(erosion["iterations"], 50000.0f));
    data.erosion.drop_lifetime = static_cast<uint32_t>(
        read_number(erosion["drop_lifetime"], 30.0f));
    data.erosion.inertia = read_number(erosion["inertia"], 0.05f);
    data.erosion.sediment_capacity =
        read_number(erosion["sediment_capacity"], 4.0f);
    data.erosion.min_sediment_capacity =
        read_number(erosion["min_sediment_capacity"], 0.01f);
    data.erosion.deposit_speed = read_number(erosion["deposit_speed"], 0.3f);
    data.erosion.erode_speed = read_number(erosion["erode_speed"], 0.3f);
    data.erosion.evaporate_speed =
        read_number(erosion["evaporate_speed"], 0.01f);
    data.erosion.gravity = read_number(erosion["gravity"], 4.0f);
    data.erosion.erode_radius = static_cast<uint32_t>(
        read_number(erosion["erode_radius"], 3.0f));
  }

  auto splat = field("splat");
  if (splat.kind() == SerializationTypeKind::Object) {
    auto layers = splat["layers"];
    auto layer_count = layers.size();

    for (int index = 0; index < static_cast<int>(layer_count); ++index) {
      auto layer = layers[index];

      SplatLayerConfig layer_config;

      if (layer["material_id"].kind() == SerializationTypeKind::String)
        layer_config.material_id = layer["material_id"].as<std::string>();

      if (layer["channel"].kind() == SerializationTypeKind::String) {
        std::string channel = layer["channel"].as<std::string>();
        if (channel != "r" && channel != "g" && channel != "b" && channel != "a") {
          LOG_WARN("[TERRAIN] unknown splat channel", channel, "skipping layer");
          continue;
        }
        layer_config.channel = channel;
      } else {
        continue;
      }

      layer_config.min_slope = read_number(layer["min_slope"], 0.0f);
      layer_config.max_slope = read_number(layer["max_slope"], 1.0f);
      layer_config.min_height = read_number(layer["min_height"], 0.0f);
      layer_config.max_height = read_number(layer["max_height"], 1.0f);

      data.splat.layers.push_back(std::move(layer_config));
    }
  }

  return data;
}

} // namespace astralix::terrain
