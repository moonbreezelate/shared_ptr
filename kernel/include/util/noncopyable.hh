#pragma once

namespace bull {

class noncopyable {
public:
  constexpr noncopyable()                    = default;
  ~noncopyable()                             = default;
  noncopyable(const noncopyable&)            = delete;
  noncopyable& operator=(const noncopyable&) = delete;
};

}  // namespace bull
