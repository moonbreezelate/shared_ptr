// simple non-atomic shared_ptr and weak_ptr suits implementation
// ref stl shared_ptr
#pragma once

#include <util/raii_guard.hh>

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <utility>

namespace bull {

class bad_weak_ptr : public std::exception {
public:
  const char* what() const noexcept override;
};

class control_block {
public:
  control_block() noexcept = default;

  void use_add() {
    if (use_cnt_ == 0) {
      throw bad_weak_ptr();
    }
    ++use_cnt_;
  }

  void weak_add() noexcept { ++weak_cnt_; }

  void use_release() {
    if (--use_cnt_ == 0) {
      do_release();
      if (--weak_cnt_ == 0) {
        delete this;
      }
    }
  }

  void weak_release() noexcept {
    if (--weak_cnt_ == 0 && use_cnt_ == 0) {
      delete this;
    }
  }

  std::size_t use_count() const noexcept { return use_cnt_; }

protected:
  virtual void do_release() = 0;

private:
  std::size_t use_cnt_ {1};
  std::size_t weak_cnt_ {1};
};

template <typename T>
class weak_ptr;

template <typename T>
class shared_ptr;

template <typename T>
class enable_shared_from_this {
  template <typename U>
  friend class shared_ptr;

  friend enable_shared_from_this* enable_shared_from_this_base(control_block* /* unused */,
                                                               enable_shared_from_this* ptr) {
    return ptr;
  }

public:
  shared_ptr<T> shared_from_this() { return weak_this_.lock(); }

  shared_ptr<const T> shared_from_this() const { return weak_this_.lock(); }

private:
  template <typename U>
  void weak_assign(U* ptr, control_block* cb) noexcept {
    weak_this_.assign(ptr, cb);
  }

  weak_ptr<T> weak_this_;
};

// shared_ptr internal class.
// for make_shared do alloc memory once time;
template <typename T>
class control_block_impl final : public control_block {
  template <typename U>
  friend class shared_ptr;

  // public:
  template <typename... Args>
  explicit control_block_impl(Args&&... args) : obj_(std::forward<Args>(args)...) {}

  T* object() noexcept { return &obj_; }

  T* object() const noexcept { return &obj_; }

  void do_release() final { obj_.~T(); }

  T obj_;
};

// shared_ptr internal class.
// for raw pointer
template <typename T>
class raw_ptr_guarder final : public control_block {
  template <typename U>
  friend class shared_pt;

  raw_ptr_guarder(T* ptr) noexcept : ptr_(ptr) {}

  void do_release() final { delete ptr_; }

  T* ptr_;
};

template <typename T>
class shared_ptr {
  template <typename U>
  friend class weak_ptr;

public:
  using element_type = T;

  shared_ptr() noexcept = default;

  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  explicit shared_ptr(U* ptr) noexcept {
    if (ptr) {
      cb_  = new raw_ptr_guarder(ptr);
      ptr_ = ptr;
      enable_shared_from_this_with(ptr_);
    }
  }

  // new --> malloc + placement new
  template <typename... Args>
  explicit shared_ptr(Args&&... args) {
    auto* mem = std::malloc(sizeof(control_block_impl<T>));
    if (!mem) {
      throw std::bad_alloc();
    }
    auto  guard = make_defer([mem] { std::free(mem); });
    auto* cb    = new (mem) control_block_impl<T>(std::forward<Args>(args)...);
    guard.dismiss();
    ptr_ = cb->object();
    cb_  = cb;
    enable_shared_from_this_with(ptr_);
  }

  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  shared_ptr(const shared_ptr<U>& other) : cb_(other.cb_),
                                           ptr_(other.ptr_) {
    do_add_use();
  }

  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  shared_ptr(shared_ptr<U>&& other) noexcept
      : cb_(std::exchange(other.cb_, nullptr)),
        ptr_(std::exchange(other.ptr_, nullptr)) {}

  shared_ptr(const weak_ptr<T>& other) : cb_(other.cb_), ptr_(other.ptr_) { do_add_use(); }

  ~shared_ptr() { reset(); }

  shared_ptr& operator=(const shared_ptr& other) noexcept {
    if (this != &other) {
      this->~shared_ptr();
      new (this) shared_ptr(other);
    }
    return *this;
  }

  shared_ptr& operator=(shared_ptr&& other) noexcept {
    if (this != &other) {
      this->~shared_ptr();
      new (this) shared_ptr(std::move(other));
    }
    return *this;
  }

  void reset() noexcept {
    if (cb_) {
      cb_->use_release();
      cb_  = nullptr;
      ptr_ = nullptr;
    }
  }

  T* get() const noexcept { return ptr_; }

  T& operator*() const noexcept { return *ptr_; }

  T* operator->() const noexcept { return ptr_; }

  std::size_t use_count() const noexcept { return cb_ ? cb_->use_count() : 0; }

  explicit operator bool() const noexcept { return cb_ != nullptr; }

private:
  /* begin traits enable_shared_from_this::enable_shared_from_this_base  */
  template <typename Yp>
  using esft_base_t = decltype(enable_shared_from_this_base(std::declval<control_block*>(), std::declval<Yp*>()));

  template <typename Yp, typename = void>
  struct has_esft_base : std::false_type {};

  template <typename Yp>
  struct has_esft_base<Yp, std::__void_t<esft_base_t<Yp>>> : std::true_type {};

  template <typename Yp, typename Yp2 = std::remove_cv_t<Yp>>
  typename std::enable_if_t<has_esft_base<Yp2>::value> enable_shared_from_this_with(Yp* ptr) noexcept {
    if (auto base = enable_shared_from_this_base(cb_, ptr)) {
      base->weak_assign(static_cast<Yp2*>(ptr), cb_);
    }
  }

  template <typename Yp, typename Yp2 = std::remove_cv_t<Yp>>
  typename std::enable_if_t<!has_esft_base<Yp2>::value> enable_shared_from_this_with(Yp* /*unused*/) noexcept {}

  /* end traits enable_shared_from_this::enable_shared_from_this_base  */

  void do_add_use() {
    if (cb_) {
      cb_->use_add();
    }
  }

  control_block* cb_ {};
  T*             ptr_ {};
};

// weak_ptr 定义
template <typename T>
class weak_ptr {
  template <typename U>
  friend class enable_shared_from_this;
  template <typename U>
  friend class shared_ptr;

  void assign(T* ptr, control_block* cb) noexcept {
    if (use_count() == 0) {
      ptr_ = ptr;
      cb_  = cb;
    }
  }

public:
  weak_ptr() noexcept = default;

  weak_ptr(const shared_ptr<T>& shared) noexcept : cb_(shared.cb_), ptr_(shared.ptr_) {
    if (cb_) {
      cb_->weak_add();
    }
  }

  weak_ptr(const weak_ptr& other) noexcept : cb_(other.cb_), ptr_(other.ptr_) {
    if (cb_) {
      cb_->weak_add();
    }
  }

  weak_ptr(weak_ptr&& other) noexcept
      : cb_(std::exchange(other.cb_, nullptr)),
        ptr_(std::exchange(other.ptr_, nullptr)) {}

  ~weak_ptr() { reset(); }

  weak_ptr& operator=(const weak_ptr& other) noexcept {
    if (this != &other) {
      this->~weak_ptr();
      new (this) weak_ptr(other);
    }
    return *this;
  }

  weak_ptr& operator=(weak_ptr&& other) noexcept {
    if (this != &other) {
      this->~weak_ptr();
      new (this) weak_ptr(std::move(other));
    }
    return *this;
  }

  void reset() noexcept {
    if (cb_) {
      cb_->weak_release();
      cb_  = nullptr;
      ptr_ = nullptr;
    }
  }

  shared_ptr<T> lock() const noexcept {
    if (cb_ && cb_->use_count() > 0) {
      return {*this};
    }
    return {};
  }

  std::size_t use_count() const noexcept { return cb_ ? cb_->use_count() : 0; }

private:
  control_block* cb_ {};
  T*             ptr_ {};
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
  return shared_ptr<T>(std::forward<Args>(args)...);
}

template <typename T, typename U>
bool operator==(const bull::shared_ptr<T>& lhs, const bull::shared_ptr<U>& rhs) noexcept {
  return lhs.get() == rhs.get();
}

/// shared_ptr comparison with nullptr
template <typename T>
bool operator==(const bull::shared_ptr<T>& lhs, std::nullptr_t /*unused*/) noexcept {
  return !lhs.get();
}

template <typename T>
bool operator==(std::nullptr_t /*unused*/, const bull::shared_ptr<T>& rhs) noexcept {
  return !rhs.get();
}

/// Inequality operator for shared_ptr objects, compares the stored pointers
template <typename T, typename U>
bool operator!=(const bull::shared_ptr<T>& lhs, const bull::shared_ptr<U>& rhs) noexcept {
  return lhs.get() != rhs.get();
}

/// shared_ptr comparison with nullptr
template <typename T>
bool operator!=(const bull::shared_ptr<T>& lhs, std::nullptr_t /*unused*/) noexcept {
  return static_cast<bool>(lhs.get());
}

/// shared_ptr comparison with nullptr
template <typename T>
bool operator!=(std::nullptr_t /*unused*/, const bull::shared_ptr<T>& rhs) noexcept {
  return static_cast<bool>(rhs.get());
}

template <typename T, typename U>
inline shared_ptr<T> static_pointer_cast(const shared_ptr<U>& from) noexcept {
  return {from, static_cast<typename shared_ptr<T>::element_type*>(from.get())};
}

template <typename T, typename U>
inline shared_ptr<T> const_pointer_cast(const shared_ptr<U>& from) noexcept {
  return {from, const_cast<typename shared_ptr<T>::element_type*>(from.get())};
}

template <typename T, typename U>
inline shared_ptr<T> dynamic_pointer_cast(const shared_ptr<U>& from) noexcept {
  if (auto* to = dynamic_cast<typename shared_ptr<T>::element_type*>(from.get())) {
    return {from, to};
  }
  return {};
}

}  // namespace bull
