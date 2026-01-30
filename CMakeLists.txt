cmake_minimum_required(VERSION 3.20)
project(ase-utils VERSION 0.1.0 LANGUAGES CXX)

#[[
ASE FOUNDATION BUILD CONFIGURATION

@file        CMakeLists.txt
@brief       Build configuration for ase-utils
@description Utils library - dotenv, string utilities, hash functions.
             Header-only library for common utility functions.

@module      ase-utils
@layer       0 (Foundation)
@created     2025-01-01
@modified    2025-01-30
@version     1.0.0

CMAKE MODULE COMPLIANCE

[ ] cmake_minimum_required(VERSION 3.20) as FIRST line
[ ] project() with VERSION and LANGUAGES CXX
[ ] message(STATUS) for configuration output
[ ] add_library() with INTERFACE for header-only
[ ] add_library(ase::{module} ALIAS ...) for namespace
[ ] target_include_directories() with INTERFACE and generator expressions
[ ] target_compile_features() with cxx_std_20
[ ] NO external dependencies (Layer 0)
[ ] include(GNUInstallDirs) for installation
[ ] install(DIRECTORY include/ ...) for headers
]]

message(STATUS "Configuring ${PROJECT_NAME} v${PROJECT_VERSION}")

#[[
HEADER-ONLY LIBRARY
Utility functions: dotenv, string manipulation, UUID, hashing.
]]
add_library(${PROJECT_NAME} INTERFACE)
add_library(ase::utils ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

#[[ VERBOTEN: No dependencies allowed in Layer 0! ]]

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
