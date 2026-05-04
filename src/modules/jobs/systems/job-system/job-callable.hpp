#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace astralix {

class JobCallable {
public:
  static constexpr size_t INLINE_CAPACITY = 56u;

  JobCallable() noexcept = default;

  template <typename F>
    requires(!std::is_same_v<std::decay_t<F>, JobCallable> &&
             std::is_invocable_v<F>)
  JobCallable(F &&callable) {
    construct(std::forward<F>(callable));
  }

  ~JobCallable() { destroy(); }

  JobCallable(JobCallable &&other) noexcept {
    if (other.m_ops != nullptr) {
      other.m_ops->move_into(other.m_storage, m_storage);
      m_ops = other.m_ops;
      other.m_ops = nullptr;
    }
  }

  JobCallable &operator=(JobCallable &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    destroy();

    if (other.m_ops != nullptr) {
      other.m_ops->move_into(other.m_storage, m_storage);
      m_ops = other.m_ops;
      other.m_ops = nullptr;
    }

    return *this;
  }

  JobCallable(const JobCallable &) = delete;
  JobCallable &operator=(const JobCallable &) = delete;

  void operator()() {
    if (m_ops != nullptr) {
      m_ops->invoke(m_storage);
    }
  }

  explicit operator bool() const noexcept { return m_ops != nullptr; }

private:
  struct Ops {
    void (*invoke)(void *storage);
    void (*move_into)(void *source, void *destination);
    void (*destroy)(void *storage);
  };

  template <typename F, bool Inline>
  static const Ops &ops_for() {
    static const Ops ops{
        .invoke =
            [](void *storage) {
              if constexpr (Inline) {
                (*static_cast<F *>(storage))();
              } else {
                (*(*static_cast<F **>(storage)))();
              }
            },
        .move_into =
            [](void *source, void *destination) {
              if constexpr (Inline) {
                auto *source_value = static_cast<F *>(source);
                ::new (destination) F(std::move(*source_value));
                source_value->~F();
              } else {
                *static_cast<F **>(destination) = *static_cast<F **>(source);
                *static_cast<F **>(source) = nullptr;
              }
            },
        .destroy =
            [](void *storage) {
              if constexpr (Inline) {
                static_cast<F *>(storage)->~F();
              } else {
                F *pointer = *static_cast<F **>(storage);
                if (pointer != nullptr) {
                  pointer->~F();
                  ::operator delete(pointer);
                }
              }
            },
    };

    return ops;
  }

  template <typename F>
  void construct(F &&callable) {
    using Decayed = std::decay_t<F>;

    constexpr bool fits_inline =
        sizeof(Decayed) <= INLINE_CAPACITY &&
        alignof(Decayed) <= alignof(std::max_align_t) &&
        std::is_nothrow_move_constructible_v<Decayed>;

    if constexpr (fits_inline) {
      ::new (m_storage) Decayed(std::forward<F>(callable));
      m_ops = &ops_for<Decayed, true>();
    } else {
      auto *heap_value = static_cast<Decayed *>(::operator new(sizeof(Decayed)));
      ::new (heap_value) Decayed(std::forward<F>(callable));
      *reinterpret_cast<Decayed **>(m_storage) = heap_value;
      m_ops = &ops_for<Decayed, false>();
    }
  }

  void destroy() noexcept {
    if (m_ops != nullptr) {
      m_ops->destroy(m_storage);
      m_ops = nullptr;
    }
  }

  alignas(std::max_align_t) unsigned char m_storage[INLINE_CAPACITY]{};
  const Ops *m_ops = nullptr;
};

} // namespace astralix
