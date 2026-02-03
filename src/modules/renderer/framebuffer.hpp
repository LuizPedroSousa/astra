#pragma once

#include "base.hpp"
#include "vector"

namespace astralix {

enum class RendererBackend;

enum class FramebufferTextureFormat {
  None = 0,

  // Color
  RGBA8,
  RGBA16F,
  RGBA32F,
  DEPTH_ONLY,
  RED_INTEGER,

  // Depth/stencil
  DEPTH24STENCIL8,

  // Defaults
  Depth = DEPTH24STENCIL8
};

struct FramebufferTextureSpecification {
  FramebufferTextureSpecification() = default;
  FramebufferTextureSpecification(FramebufferTextureFormat format)
      : format(format) {}

  FramebufferTextureFormat format = FramebufferTextureFormat::None;
};

struct FramebufferAttachmentSpecification {
  FramebufferAttachmentSpecification() = default;
  FramebufferAttachmentSpecification(
      std::initializer_list<FramebufferTextureSpecification> attachments)
      : attachments(attachments) {}

  std::vector<FramebufferTextureSpecification> attachments;
};

struct FramebufferSpecification {
  uint32_t width = 0, height = 0;
  FramebufferAttachmentSpecification attachments;
  uint32_t samples = 1;

  //  bool swap_chain_target = false;
};

enum class FramebufferBindType { Default = 0, Read = 1, Draw = 2 };

class Framebuffer {
public:
  virtual ~Framebuffer() = default;

  virtual void bind(FramebufferBindType = FramebufferBindType::Default,
                    u_int32_t id = -1) = 0;
  virtual void unbind() = 0;

  virtual void resize(uint32_t width, uint32_t height) = 0;
  virtual int read_pixel(uint32_t attachmentIndex, int x, int y) = 0;

  virtual void clear_attachment(uint32_t attachmentIndex, int value) = 0;

  virtual uint32_t get_color_attachment_id(uint32_t index = 0) const = 0;

  virtual void blit(uint32_t width, uint32_t height) = 0;

  virtual const FramebufferSpecification &get_specification() const = 0;

  static Ref<Framebuffer> create(RendererBackend backend,
                                 const FramebufferSpecification &spec);
};

} // namespace astralix
