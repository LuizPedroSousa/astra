#pragma once

#include "vertex-buffer.hpp"
#include "systems/render-system/core/render-types.hpp"
#include <vulkan/vulkan.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace astralix {

class VulkanDevice;
class VulkanShaderProgram;

class VulkanPipelineCache {
public:
  explicit VulkanPipelineCache(const VulkanDevice &device);
  ~VulkanPipelineCache();

  VulkanPipelineCache(const VulkanPipelineCache &) = delete;
  VulkanPipelineCache &operator=(const VulkanPipelineCache &) = delete;

  VkPipeline get_or_create_graphics_pipeline(
      const VulkanShaderProgram &program,
      const RenderPipelineDesc &desc,
      const BufferLayout *vertex_layout,
      const std::vector<VkFormat> &color_formats,
      VkFormat depth_format);

private:
  uint64_t compute_pipeline_key(
      const VulkanShaderProgram &program,
      const RenderPipelineDesc &desc,
      const BufferLayout *vertex_layout,
      const std::vector<VkFormat> &color_formats,
      VkFormat depth_format) const;

  VkPipeline create_graphics_pipeline(
      const VulkanShaderProgram &program,
      const RenderPipelineDesc &desc,
      const BufferLayout *vertex_layout,
      const std::vector<VkFormat> &color_formats,
      VkFormat depth_format);

  const VulkanDevice &m_device;
  VkPipelineCache m_vk_cache = VK_NULL_HANDLE;
  std::unordered_map<uint64_t, VkPipeline> m_pipelines;
};

} // namespace astralix
