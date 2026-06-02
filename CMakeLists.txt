cmake_minimum_required(VERSION 3.20)
project(MemoryAnalysisTool VERSION 1.0.0 DESCRIPTION "Process and memory usage tracker")

# ==============================================================================
# C++23 STANDARD AND COMPILER CONFIGURATION
# ==============================================================================

# Enforce the modern C++23 standard without compiler-specific extensions
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Some Extra settings
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)

# ==============================================================================
# AUTOMATIC SOURCE FILE DISCOVERY
# ==============================================================================

# Recursively discover all C++ source and header files within the src directory.
# CONFIGURE_DEPENDS ensures CMake re-evaluates the file tree automatically on build.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

# ==============================================================================
# AUTOMATIC ERROR MESSAGE CODE GENERATION (TEMPLATE CONFIGURATION)
# ==============================================================================

# Load the global string literals dictionary
include(cmake/error_messages.cmake)

# Define the isolated build directory where generated .hpp files will be stored
set(GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_include")

# СКАНИРУЕМ ПАПКУ SRC (потому что все шаблоны лежат там)
file(GLOB_RECURSE TEMPLATE_FILES 
     RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/src" 
     CONFIGURE_DEPENDS 
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp.in"
)

# Loop through the discovered templates to generate valid C++ header files
foreach(TMPL_REL_PATH ${TEMPLATE_FILES})
    set(IN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/${TMPL_REL_PATH}")
    string(REGEX REPLACE "\\.in$" "" HPP_REL_PATH "${TMPL_REL_PATH}")
    set(OUT_FILE "${GENERATED_INCLUDE_DIR}/${HPP_REL_PATH}")
    configure_file("${IN_FILE}" "${OUT_FILE}" @ONLY)
endforeach()

# ==============================================================================
# TARGET DEFINITION AND DEPENDENCIES
# ==============================================================================

# Locate required framework components (Qt6 Core)
find_package(Qt6 REQUIRED COMPONENTS Core)

# Define the main executable target using the discovered source files
add_executable(${PROJECT_NAME} ${SOURCES})

# Link external dependencies and logging tools
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core spdlog fmt)

# Configure compiler include search paths
target_include_directories(${PROJECT_NAME} PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src" # Main project source headers
    "${GENERATED_INCLUDE_DIR}"        # Automatically generated error string headers
)