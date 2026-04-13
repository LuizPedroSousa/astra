#pragma once

#include "framebuffer.hpp"

namespace astralix {

class VirtualFramebuffer : public Framebuffer {
public:
  explicit VirtualFramebuffer(const FramebufferSpecification &spec);
  ~VirtualFramebuffer() override = default;

  void bind(FramebufferBindType type = FramebufferBindType::Default,
            u_int32_t id = static_cast<u_int32_t>(-1)) override;
  void unbind() override;

  void resize(uint32_t width, uint32_t height) override;
  int read_pixel(uint32_t attachmentIndex, int x, int y) override;
  void clear_attachment(uint32_t attachmentIndex, int value) override;

  uint32_t get_color_attachment_id(uint32_t index = 0) const override;
  const std::vector<uint32_t> &get_color_attachments() const override;
  uint32_t get_depth_attachment_id() const override;

  void blit(uint32_t width, uint32_t height,
            FramebufferBlitType type = FramebufferBlitType::Color,
            FramebufferBlitFilter filter =
                FramebufferBlitFilter::Nearest) override;

  const FramebufferSpecification &get_specification() const override;

private:
  FramebufferSpecification m_spec;
  std::vector<uint32_t> m_color_attachments;
  uint32_t m_depth_attachment_id = 0;
};

} // namespace astralix
