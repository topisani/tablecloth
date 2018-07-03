#pragma once

#include <memory>
#include <variant>

/// No-function macro to mark implicit conversions
#define implicit

/// Generate conversions to Base
#define CLOTH_BIND_BASE(Class, BaseClass)                                                          \
public:                                                                                            \
  using Base = struct ::BaseClass;                                                                          \
  Class(::cloth::util::raw_or_unique_ptr<Base>&& base) noexcept : _base(std::move(base)) {}                         \
                                                                                                   \
  Base* base() noexcept                                                                            \
  {                                                                                                \
    return &*_base;                                                                            \
  }                                                                                                \
                                                                                                   \
  implicit operator Base*() noexcept                                                               \
  {                                                                                                \
    return base();                                                                                 \
  }                                                                                                \
                                                                                                   \
  Base const* base() const noexcept                                                                \
  {                                                                                                \
    return &*_base;                                                                            \
  }                                                                                                \
                                                                                                   \
  implicit operator const Base*() const noexcept                                                   \
  {                                                                                                \
    return base();                                                                                 \
  }                                                                                                \
                                                                                                   \
  Class& from_voidptr(void* data) noexcept                                                         \
  {                                                                                                \
    return *static_cast<Class*>(data);                                                             \
  }                                                                                                \
                                                                                                   \
private:                                                                                           \
  ::cloth::util::raw_or_unique_ptr<Base> _base;                                                                     \
                                                                                                   \
public:

namespace cloth::util {

  /// A simple function pointer type alias
  template<typename Ret, typename... Args>
  using function_ptr = Ret (*)(Args...);

  template<typename T, typename Del = std::default_delete<T>>
  struct raw_or_unique_ptr {

    using value_type = T;
    using raw_ptr = value_type*;
    using unique_ptr = std::unique_ptr<value_type, Del>;

    implicit raw_or_unique_ptr(raw_ptr ptr) noexcept
     : _data(ptr)
    {}

    implicit raw_or_unique_ptr(unique_ptr&& ptr) noexcept
     : _data(std::move(ptr))
    {}

    implicit raw_or_unique_ptr(value_type& data) noexcept
     : _data(&data)
    {}

    implicit raw_or_unique_ptr(value_type&& data) noexcept
     : _data(std::make_unique<value_type, Del>(std::move(data)))
    {}

    raw_or_unique_ptr(raw_or_unique_ptr const&) = delete;

    raw_or_unique_ptr(raw_or_unique_ptr&& rhs) noexcept
    {
      std::swap(_data, rhs._data);
    }

    void operator=(raw_or_unique_ptr const&) = delete;
    void operator=(raw_or_unique_ptr&& rhs) noexcept 
    {
      std::swap(_data, rhs._data);
    }


    T& operator *() noexcept {
      if (std::holds_alternative<raw_ptr>(_data)) {
        return *std::get<raw_ptr>(_data);
      } else {
        return *std::get<unique_ptr>(_data);
      }
    }

    T const& operator *() const noexcept {
      if (std::holds_alternative<raw_ptr>(_data)) {
        return *std::get<raw_ptr>(_data);
      } else {
        return *std::get<unique_ptr>(_data);
      }
    }

    T* get() noexcept {
      if (std::holds_alternative<raw_ptr>(_data)) {
        return std::get<raw_ptr>(_data);
      } else {
        return std::get<unique_ptr>(_data).get();
      }
    }
    T const* get() const noexcept {
      if (std::holds_alternative<raw_ptr>(_data)) {
        return std::get<raw_ptr>(_data);
      } else {
        return std::get<unique_ptr>(_data).get();
      }
    }
  private:
    std::variant<raw_ptr, unique_ptr> _data;
  };
} // namespace cloth::util
