#include <core/shared_ptr.hh>

namespace bull {

const char* bad_weak_ptr::what() const noexcept { return "bad_weak_ptr"; }

}  // namespace bull
