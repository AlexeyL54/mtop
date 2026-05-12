cmake_minimum_required(VERSION 3.10)
project(mtop)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -pthread")

# Find libmicrohttpd
find_package(PkgConfig REQUIRED)
pkg_check_modules(MICROHTTPD REQUIRED libmicrohttpd)

# Include directories
include_directories(${MICROHTTPD_INCLUDE_DIRS})
include_directories(src)

# Source files
set(SOURCES
    src/Monitor.cpp
    src/MonitorServer.cpp
    main.cpp
)

# Executable
add_executable(mtop ${SOURCES})

# Link libraries
target_link_libraries(mtop 
    ${MICROHTTPD_LIBRARIES}
    pthread
)

# Compile options
target_compile_options(mtop PRIVATE -Wall -Wextra)
