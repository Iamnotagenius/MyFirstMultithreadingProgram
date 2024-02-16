cmake_minimum_required(VERSION 3.28)

project(InfoTexTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

file(GLOB_RECURSE FIRST_SRCFILES CONFIGURE_DEPENDS "src/first/*.cpp")
add_executable(First ${FIRST_SRCFILES})
