#pragma once

#include <util/noncopyable.hh>

#include <type_traits>
#include <utility>

namespace bull {

template <typename Func>
class raii_guard : noncopyable {
  static_assert(std::is_nothrow_copy_constructible<Func>::value && std::is_nothrow_move_constructible<Func>::value,
                "Throwing an exception during the Func copy or move construction cause an unexpected behavior.");

public:
  raii_guard() = delete;

  explicit raii_guard(Func func) noexcept : func_(func), active_(true) {}

  raii_guard(raii_guard&& other) noexcept
      : func_(std::move(other.func_)),
        active_(std::exchange(other.active_, false)) {}

  raii_guard& operator=(raii_guard&& other) noexcept {
    if (this != &other) {
      this->~raii_guard();
      new (this) raii_guard(std::move(other));
    }
    return *this;
  }

  ~raii_guard() {
    if (active_) {
      func_();
    }
  }

  void dismiss() { active_ = false; }

private:
  Func func_;
  bool active_;
};

template <typename Func>
raii_guard<Func> make_defer(Func func) {
  return raii_guard<Func>(func);
}

}  // namespace bull
