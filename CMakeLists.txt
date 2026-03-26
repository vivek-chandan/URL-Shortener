cmake_minimum_required(VERSION 3.10)
project(url-shortener)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use standalone ASIO for Crow
add_definitions(-DASIO_STANDALONE)

# Include directories
include_directories(include)
include_directories(third_party/crow/include)

# Dependencies
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(SQLite3 REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/server.cpp
    src/database.cpp
    src/cache.cpp
    src/shortener.cpp
    src/rate_limiter.cpp
)

add_executable(url-shortener ${SOURCES})

target_link_libraries(url-shortener
    Boost::system
    Boost::thread
    SQLite::SQLite3
    pthread
)