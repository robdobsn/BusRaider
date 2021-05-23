# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

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

# Configure ESP-IDF project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(BusRaider)

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