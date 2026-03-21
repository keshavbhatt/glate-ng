cmake_minimum_required(VERSION 3.21)
project(glate VERSION 4.1.0 LANGUAGES CXX)

# Single source of truth for app versioning reused by packaging/tooling.
set(GLATE_VERSION "${PROJECT_VERSION}")
set(GLATE_VERSION_SHORT "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

# Standalone reusable account & licence module (Qt6::Core + Qt6::Network only)
add_subdirectory(modules/AccountAndLicense)

add_subdirectory(src)
