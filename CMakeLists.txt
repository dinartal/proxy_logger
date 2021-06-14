#cmake_minimum_required(VERSION 2.8)
project(ProxyLogger)
add_executable(main main.cpp proxy_logger_linux.cpp)
target_compile_features(main PRIVATE cxx_std_17)
