
cmake_minimum_required(VERSION 3.17)
project(os_1 C)

set(CMAKE_C_STANDARD 11)
add_executable(os_1 main.c)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(os_1 PRIVATE Threads::Threads)
