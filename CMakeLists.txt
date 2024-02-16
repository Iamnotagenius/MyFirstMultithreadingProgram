cmake_minimum_required(VERSION 3.28)

project(InfoTexTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)


file(GLOB_RECURSE FIRST_SRCFILES CONFIGURE_DEPENDS "src/first/*.cpp")
file(GLOB_RECURSE DEBUG_SRCFILES CONFIGURE_DEPENDS "src/debug/*.cpp")
add_library(debug_log ${DEBUG_SRCFILES})
add_executable(First ${FIRST_SRCFILES})
target_link_libraries(First debug debug_log)
target_compile_definitions(First PRIVATE $<$<CONFIG:Debug>:DEBUG>)
