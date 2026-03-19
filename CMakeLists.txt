cmake_minimum_required(VERSION 3.21)
project(glate VERSION 4.0 LANGUAGES CXX)

# Standalone reusable account & licence module (Qt6::Core + Qt6::Network only)
add_subdirectory(modules/AccountAndLicense)

add_subdirectory(src)
