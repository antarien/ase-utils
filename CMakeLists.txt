cmake_minimum_required(VERSION 3.20)

project(ase-utils
    VERSION 0.1.0
    DESCRIPTION "ASE Utility Library - dotenv, string utils, etc."
    LANGUAGES CXX
)

# Header-only interface library
add_library(ase-utils INTERFACE)
add_library(ase::utils ALIAS ase-utils)

target_include_directories(ase-utils
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ase-utils INTERFACE cxx_std_20)

# Install
install(TARGETS ase-utils EXPORT ase-utils-targets)
install(DIRECTORY include/ DESTINATION include)
