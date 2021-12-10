# Ensure CMake supports FetchContent
cmake_minimum_required(VERSION 3.14)
include(FetchContent)

set(SUPPORTED_TARGETS esp32)

# Check RIC revision name is valid
get_filename_component(_ric_revision_name ${CMAKE_BINARY_DIR} NAME)

# Check config dir exists
set(RIC_CONFIG_DIR "${CMAKE_SOURCE_DIR}/buildConfigs/${_ric_revision_name}")
if((NOT EXISTS ${RIC_CONFIG_DIR}) OR (NOT IS_DIRECTORY ${RIC_CONFIG_DIR}))
    message(FATAL_ERROR "Config directory ${RIC_CONFIG_DIR} not found.")
endif()

# Use sdkconfig for the selected RIC revision
set(SDKCONFIG_DEFAULTS "${RIC_CONFIG_DIR}/sdkconfig.defaults")
set(SDKCONFIG "${RIC_CONFIG_DIR}/sdkconfig")

# Configure revision-specific options, flags, etc.
include(${RIC_CONFIG_DIR}/featureFlags.cmake)

message(STATUS "Configuring a build system for ${_ric_revision_name}")

# Clean up helper variables
unset(_ric_revision_name)

# Fetch the littlefs library
FetchContent_Declare(
  esp_littlefs
  GIT_REPOSITORY https://github.com/joltwallet/esp_littlefs.git
)

# Fetch the littlefs image build tool
FetchContent_Declare(
  mklittlefs
  GIT_REPOSITORY https://github.com/earlephilhower/mklittlefs.git
)

# Fetch the RdWebServer library
FetchContent_Declare(
  rd_webserver
  GIT_REPOSITORY https://github.com/robdobsn/RdWebServer
)

# Fetch Micropython
# FetchContent_Declare(
#  micropython_release
#  GIT_REPOSITORY https://github.com/micropython/Micropython
#  GIT_TAG        v1.17
#  # GIT_SUBMODULES "docs"
#)

# Populate first so we have a folder that we can add in EXTRA_COMPONENT_DIRS
FetchContent_Populate(esp_littlefs)
FetchContent_Populate(rd_webserver)
# message(STATUS "fetching micropython - this can take some time, please wait ...")
# FetchContent_Populate(micropython_release)
# message(STATUS "micropython_source folder " ${micropython_release_SOURCE_DIR})
# set(MICROPYTHON_SOURCE_DIR ${micropython_release_SOURCE_DIR})
message(STATUS "esp_littlefs folder " ${esp_littlefs_SOURCE_DIR})
set(EXTRA_COMPONENT_DIRS ${esp_littlefs_SOURCE_DIR} ${rd_webserver_SOURCE_DIR})
FetchContent_MakeAvailable(esp_littlefs mklittlefs rd_webserver)

# Configure ESP-IDF project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project name
project(BusRaider DEPENDS esp_littlefs rd_webserver micropython_release)

# Update compile_commands.json for VS Code

# This makes it easy for VS Code to access the compile commands for the most
# recently built FW revision.
# TODO: A nicer solution might be to set up launch configurations properly and
#       have them find their respective launch_commands.json
add_custom_target(updateSharedCompileCommands ALL
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/compile_commands.json" "${CMAKE_BINARY_DIR}/.."
                    DEPENDS "${CMAKE_BINARY_DIR}/compile_commands.json"
                    COMMENT "Updating shared compile_commands.json"
                    )