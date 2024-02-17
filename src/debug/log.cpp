#include <iostream>
#include <syncstream>
#include <string>

#include "log.hpp"

namespace debug {
    std::osyncstream debug_log(const std::string& category) {
        return std::osyncstream(std::cerr) << "[" << category << "] ";
    }
}
