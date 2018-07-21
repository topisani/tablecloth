#pragma once

#include <memory>
#include <variant>

namespace cloth {
  template<typename T>
  struct owner {
    constexpr explicit owner(T* t) noexcept : _val(t) {}
    constexpr explicit operator T*() noexcept
    {
      return _val;
    }

  private:
    T* _val;
  };

} // namespace cloth

/// Generate conversions to Base
#define CLOTH_BIND_BASE(Class, BaseClass)                                                          \
public:                                                                                            \
  using Base = BaseClass;                                                                          \
  using deleter = cloth::util::deleter<Class>;                                                     \
  Class(Base* base = nullptr) noexcept : _base(base) {}                                            \
                                                                                                   \
  Class(cloth::owner<Base> base) noexcept : _base({static_cast<Base*>(base), deleter{*this}}) {}   \
                                                                                                   \
  Base* base() noexcept                                                                            \
  {                                                                                                \
    return &*_base;                                                                                \
  }                                                                                                \
                                                                                                   \
  operator Base*() noexcept                                                                        \
  {                                                                                                \
    return base();                                                                                 \
  }                                                                                                \
                                                                                                   \
  Base const* base() const noexcept                                                                \
  {                                                                                                \
    return &*_base;                                                                                \
  }                                                                                                \
                                                                                                   \
  operator const Base*() const noexcept                                                            \
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
  ::cloth::util::raw_or_unique_ptr<Base, Class::deleter> _base;                                    \
                                                                                                   \
public:

#define CLOTH_ENABLE_BITMASK_OPS(Enum)                                                             \
  template<>                                                                                       \
  struct ::cloth::enable_bitmask_ops<Enum> : std::true_type {};

namespace cloth {

  template<typename Enum>
  struct enum_or_bool {
    constexpr enum_or_bool(Enum e) : e(e){};
    constexpr enum_or_bool(bool e) : e(static_cast<Enum>(e)){};
    constexpr operator Enum&() noexcept
    {
      return e;
    }
    constexpr operator const Enum&() const noexcept
    {
      return e;
    }
    constexpr operator bool() const noexcept
    {
      return static_cast<bool>(e);
    }
    Enum e;
  };

  template<typename Enum>
  struct enable_bitmask_ops : std::false_type {};

  template<typename Enum>
  constexpr auto operator|(Enum lhs, Enum rhs)
    -> std::enable_if_t<cloth::enable_bitmask_ops<Enum>::value, Enum>
  {
    return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) |
                             static_cast<std::underlying_type_t<Enum>>(rhs));
  }

  template<typename Enum>
  constexpr auto operator&(Enum lhs, Enum rhs)
    -> std::enable_if_t<cloth::enable_bitmask_ops<Enum>::value, enum_or_bool<Enum>>
  {
    return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) &
                             static_cast<std::underlying_type_t<Enum>>(rhs));
  }

  template<typename Enum>
  constexpr auto operator^(Enum lhs, Enum rhs)
    -> std::enable_if_t<cloth::enable_bitmask_ops<Enum>::value, Enum>
  {
    return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) ^
                             static_cast<std::underlying_type_t<Enum>>(rhs));
  }

  template<typename Enum>
  constexpr auto operator~(Enum rhs)
    -> std::enable_if_t<cloth::enable_bitmask_ops<Enum>::value, Enum>
  {
    return static_cast<Enum>(~static_cast<std::underlying_type_t<Enum>>(rhs));
  }

  template<typename Enum>
  constexpr auto operator|=(Enum& lhs, Enum rhs)
    -> std::enable_if_t<cloth::enable_bitmask_ops<Enum>::value, Enum&>
  {
    lhs = static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) |
                            static_cast<std::underlying_type_t<Enum>>(rhs));

    return lhs;
  }

  template<typename Enum>
  constexpr auto operator&=(Enum& lhs, Enum rhs)
    -> std::enable_if_t<cloth::enable_bitmask_ops<Enum>::value, Enum&>
  {
    lhs = static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) &
                            static_cast<std::underlying_type_t<Enum>>(rhs));

    return lhs;
  }

  template<typename Enum>
  constexpr auto operator^=(Enum& lhs, Enum rhs)
    -> std::enable_if_t<cloth::enable_bitmask_ops<Enum>::value, Enum&>
  {
    lhs = static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) ^
                            static_cast<std::underlying_type_t<Enum>>(rhs));

    return lhs;
  }
} // namespace cloth

namespace cloth::util {

  template<typename T1, typename T2>
  constexpr auto dynamic_is(const T2& t) -> std::enable_if_t<!std::is_pointer_v<T2>, bool>
  {
    return dynamic_cast<T1*>(&t) != nullptr;
  }

  template<typename T1, typename T2>
  constexpr auto dynamic_is(T2* t) -> std::enable_if_t<!std::is_pointer_v<T2>, bool>
  {
    return dynamic_cast<T1*>(t) != nullptr;
  }

  template<typename E, typename T>
  constexpr auto enum_cast(E e) noexcept -> std::enable_if_t<std::is_enum_v<E>, E>
  {
    return static_cast<E>(e);
  }

  /// Cast scoped enums to their underlying numeric type
  template<typename E>
  constexpr auto enum_cast(E e) noexcept -> std::enable_if_t<!std::is_enum_v<E>, E>
  {
    return e;
  }

  template<typename E, typename = std::enable_if_t<std::is_enum_v<E>>>
  constexpr auto enum_cast(E e) noexcept
  {
    return static_cast<std::underlying_type_t<E>>(e);
  }

  template<typename T, typename = int>
  struct has_destroy : std::false_type {};

  template<typename T>
  struct has_destroy<T, decltype((void) std::declval<T>().destroy(), 0)> : std::true_type {};

  template<typename Class>
  struct deleter {
    Class& klass;
    using Base = typename Class::Base;

    void operator()(Base* ptr)
    {
      if constexpr (has_destroy<Class>::value) {
        klass.destroy();
      } else {
        delete ptr;
      }
    }
  };

  /// A simple function pointer type alias
  template<typename Ret, typename... Args>
  using function_ptr = Ret (*)(Args...);

  template<typename T, typename Del = std::default_delete<T>>
  struct raw_or_unique_ptr {
    using value_type = T;
    using raw_ptr = value_type*;
    using unique_ptr = std::unique_ptr<value_type, Del>;

    raw_or_unique_ptr(raw_ptr ptr) noexcept : _data(ptr) {}

    raw_or_unique_ptr(unique_ptr&& ptr) noexcept : _data(std::move(ptr)) {}

    raw_or_unique_ptr(value_type& data) noexcept : _data(&data) {}

    raw_or_unique_ptr(value_type&& data) noexcept
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


    T& operator*() noexcept
    {
      if (std::holds_alternative<raw_ptr>(_data)) {
        return *std::get<raw_ptr>(_data);
      } else {
        return *std::get<unique_ptr>(_data);
      }
    }

    T const& operator*() const noexcept
    {
      if (std::holds_alternative<raw_ptr>(_data)) {
        return *std::get<raw_ptr>(_data);
      } else {
        return *std::get<unique_ptr>(_data);
      }
    }

    T* get() noexcept
    {
      if (std::holds_alternative<raw_ptr>(_data)) {
        return std::get<raw_ptr>(_data);
      } else {
        return std::get<unique_ptr>(_data).get();
      }
    }
    T const* get() const noexcept
    {
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
