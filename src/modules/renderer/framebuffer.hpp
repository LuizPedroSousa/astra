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

  RGB8,
  RGB16F,
  RGB32F,

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

  FramebufferTextureSpecification(std::string name, FramebufferTextureFormat format)
      : name(name), format(format) {}

  std::string name;
  FramebufferTextureFormat format = FramebufferTextureFormat::None;
};

struct FramebufferAttachmentSpecification {
  FramebufferAttachmentSpecification() = default;
  FramebufferAttachmentSpecification(
      std::initializer_list<FramebufferTextureSpecification> attachments
  )
      : attachments(attachments) {}

  std::vector<FramebufferTextureSpecification> attachments;
};

enum class RenderExtentMode {
  Absolute,
  WindowRelative,
};

struct RenderExtent {
  RenderExtentMode mode = RenderExtentMode::Absolute;
  uint32_t width = 0;
  uint32_t height = 0;
  float scale_x = 1.0f;
  float scale_y = 1.0f;
};

struct FramebufferSpecification {
  uint32_t width = 0, height = 0;
  FramebufferAttachmentSpecification attachments;
  uint32_t samples = 1;
  RenderExtent extent;

  //  bool swap_chain_target = false;
};

enum class FramebufferBindType { Default = 0,
                                 Read = 1,
                                 Draw = 2 };

enum class FramebufferBlitType : uint32_t {
  None = 0,
  Color = 1 << 0,
  Depth = 1 << 1,
  Stencil = 1 << 2,
  All = Color | Depth | Stencil
};

inline FramebufferBlitType operator|(FramebufferBlitType a, FramebufferBlitType b) {
  return static_cast<FramebufferBlitType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline FramebufferBlitType operator&(FramebufferBlitType a, FramebufferBlitType b) {
  return static_cast<FramebufferBlitType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline FramebufferBlitType operator^(FramebufferBlitType a, FramebufferBlitType b) {
  return static_cast<FramebufferBlitType>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b));
}

inline FramebufferBlitType operator~(FramebufferBlitType a) {
  return static_cast<FramebufferBlitType>(~static_cast<uint32_t>(a));
}

inline FramebufferBlitType &operator|=(FramebufferBlitType &a, FramebufferBlitType b) {
  return a = a | b;
}

inline FramebufferBlitType &operator&=(FramebufferBlitType &a, FramebufferBlitType b) {
  return a = a & b;
}

inline FramebufferBlitType &operator^=(FramebufferBlitType &a, FramebufferBlitType b) {
  return a = a ^ b;
}

enum class FramebufferBlitFilter {
  Nearest = 0,
  Linear = 1
};

class Framebuffer {
public:
  virtual ~Framebuffer() = default;

  virtual void bind(FramebufferBindType = FramebufferBindType::Default, u_int32_t id = -1) = 0;
  virtual void unbind() = 0;

  virtual void resize(uint32_t width, uint32_t height) = 0;
  virtual int read_pixel(uint32_t attachmentIndex, int x, int y) = 0;

  virtual void clear_attachment(uint32_t attachmentIndex, int value) = 0;

  virtual uint32_t get_color_attachment_id(uint32_t index = 0) const = 0;
  virtual const std::vector<uint32_t> &get_color_attachments() const = 0;
  virtual uint32_t get_depth_attachment_id() const = 0;

  virtual void blit(uint32_t width, uint32_t height, FramebufferBlitType type = FramebufferBlitType::Color, FramebufferBlitFilter filter = FramebufferBlitFilter::Nearest) = 0;

  virtual const FramebufferSpecification &get_specification() const = 0;

  static Ref<Framebuffer> create(RendererBackend backend, const FramebufferSpecification &spec);
};

} // namespace astralix
