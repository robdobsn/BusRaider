# This is the project CMakeLists.txt file for the test subproject
cmake_minimum_required(VERSION 3.16)
include(FetchContent)

# Include the components directory of the main application:
#
set(EXTRA_COMPONENT_DIRS "../components")

# Set the components to include the tests for.
# This can be overriden from CMake cache:
# - when invoking CMake directly: cmake -D TEST_COMPONENTS="xxxxx" ..
# - when using idf.py: idf.py -T xxxxx build
#
set(TEST_COMPONENTS "comms core" CACHE STRING "Components to test")

# Configure ESP-IDF project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Fetch the littlefs library
FetchContent_Declare(
  esp_littlefs
  GIT_REPOSITORY https://github.com/joltwallet/esp_littlefs.git
  GIT_TAG        v1.1.0
)

# Fetch the littlefs image build tool
FetchContent_Declare(
  mklittlefs
  GIT_REPOSITORY https://github.com/earlephilhower/mklittlefs.git
  GIT_TAG        affa497fb0b2bf6a538390ca812c7e003828c2e7
)

# Populate first so we have a folder that we can add in EXTRA_COMPONENT_DIRS
FetchContent_Populate(esp_littlefs)
message(STATUS "esp_littlefs folder " ${esp_littlefs_SOURCE_DIR})
string(APPEND EXTRA_COMPONENT_DIRS " ${esp_littlefs_SOURCE_DIR}")
FetchContent_MakeAvailable(esp_littlefs mklittlefs)

# Define the project name
project(ric_unit_tests)
