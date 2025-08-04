cmake_minimum_required(VERSION 3.25)

project(
  Learning_OpenGL
  VERSION 0.1.0
  DESCRIPTION "Learning OpenGL with C++"
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Project Options
# ============================================================================

option(ENABLE_CLANG_TIDY "Enable clang-tidy for project targets" OFF)

# ============================================================================
# CMake Modules
# ============================================================================

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)

include(ProjectWarnings)
include(ProjectOptions)
include(ClangTidy)
include(ProjectTarget)

# ============================================================================
# Output Directories
# ============================================================================

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")

# ============================================================================
# Third-Party Dependencies
# ============================================================================

add_subdirectory(third_party)

# ============================================================================
# Project Sources
# ============================================================================

add_subdirectory(src)
