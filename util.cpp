#include "util.hpp"

namespace detail {
    std::thread::id main_thread_id = std::this_thread::get_id();
}
