#pragma once
#include "assert.hpp"
#include "framebuffer.hpp"
#include "glad/glad.h"

namespace astralix {

class OpenGLFramebuffer : public Framebuffer {
public:
  OpenGLFramebuffer(const FramebufferSpecification &spec);
  ~OpenGLFramebuffer() override;

  void bind(FramebufferBindType type = FramebufferBindType::Default,
            uint32_t id = -1) override;
  void unbind() override;

  void resize(uint32_t width, uint32_t height) override;
  int read_pixel(uint32_t attachment_index, int x, int y) override;

  void blit(uint32_t width, uint32_t height,
            FramebufferBlitType type = FramebufferBlitType::Color,
            FramebufferBlitFilter filter = FramebufferBlitFilter::Nearest) override;

  void clear_attachment(uint32_t attachment_index, int value) override;

  uint32_t get_color_attachment_id(uint32_t index = 0) const override {
    ASTRA_ENSURE(index >= m_color_attachments.size(),
                 "Invalid color attachment index: ", index);
    return m_color_attachments[index];
  };

  const std::vector<uint32_t> &get_color_attachments() const override {
    return m_color_attachments;
  };

  uint32_t get_depth_attachment_id() const override {
    return m_depth_attachment;
  };

  const FramebufferSpecification &get_specification() const override {
    return m_specification;
  };

private:
  FramebufferSpecification m_specification;
  uint32_t m_renderer_id = 0;
  std::vector<FramebufferTextureSpecification>
      m_color_attachment_specifications;
  std::vector<uint32_t> m_color_attachments;

  FramebufferTextureSpecification m_depth_attachment_specification =
      FramebufferTextureFormat::None;
  uint32_t m_depth_attachment = 0;

  void invalidate();
};
} // namespace astralix
