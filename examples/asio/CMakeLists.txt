cmake_minimum_required(VERSION 3.16)
project(asio_example)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FetchContent for standalone ASIO
include(FetchContent)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG asio-1-30-2 # You can update this tag to the latest release if needed
)
FetchContent_MakeAvailable(asio)

add_executable(asio_example main.cpp)
add_compile_definitions(ASIO_STANDALONE)
target_include_directories(asio_example PRIVATE ${asio_SOURCE_DIR}/asio/include)
target_link_libraries(asio_example PRIVATE mgutility::task)
