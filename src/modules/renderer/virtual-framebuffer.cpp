#include "virtual-framebuffer.hpp"

namespace astralix {

namespace {

bool is_depth_attachment(FramebufferTextureFormat format) {
  return format == FramebufferTextureFormat::DEPTH24STENCIL8 ||
         format == FramebufferTextureFormat::DEPTH_ONLY;
}

} // namespace

VirtualFramebuffer::VirtualFramebuffer(const FramebufferSpecification &spec)
    : m_spec(spec) {
  uint32_t next_attachment_id = 1;
  for (const auto &attachment : m_spec.attachments.attachments) {
    if (is_depth_attachment(attachment.format)) {
      if (m_depth_attachment_id == 0) {
        m_depth_attachment_id = next_attachment_id++;
      }
      continue;
    }

    m_color_attachments.push_back(next_attachment_id++);
  }
}

void VirtualFramebuffer::bind(FramebufferBindType type, u_int32_t id) {
  (void)type;
  (void)id;
}

void VirtualFramebuffer::unbind() {}

void VirtualFramebuffer::resize(uint32_t width, uint32_t height) {
  m_spec.width = width;
  m_spec.height = height;
}

int VirtualFramebuffer::read_pixel(uint32_t attachment_index, int x, int y) {
  (void)attachment_index;
  (void)x;
  (void)y;
  return 0;
}

void VirtualFramebuffer::clear_attachment(uint32_t attachment_index, int value) {
  (void)attachment_index;
  (void)value;
}

uint32_t VirtualFramebuffer::get_color_attachment_id(uint32_t index) const {
  if (index >= m_color_attachments.size()) {
    return 0;
  }

  return m_color_attachments[index];
}

const std::vector<uint32_t> &VirtualFramebuffer::get_color_attachments() const {
  return m_color_attachments;
}

uint32_t VirtualFramebuffer::get_depth_attachment_id() const {
  return m_depth_attachment_id;
}

void VirtualFramebuffer::blit(uint32_t width, uint32_t height,
                              FramebufferBlitType type,
                              FramebufferBlitFilter filter) {
  (void)width;
  (void)height;
  (void)type;
  (void)filter;
}

const FramebufferSpecification &VirtualFramebuffer::get_specification() const {
  return m_spec;
}

} // namespace astralix
