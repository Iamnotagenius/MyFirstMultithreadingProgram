#ifndef __DEBUG_LOG_HPP
#define __DEBUG_LOG_HPP

#include <syncstream>
#include <string>

namespace debug {
    std::osyncstream debug_log(const std::string& category);
}
#endif
