# Ensure CMake supports FetchContent
cmake_minimum_required(VERSION 3.16)
include(FetchContent)

set(SUPPORTED_TARGETS esp32)

# Get build configuration folder
get_filename_component(buildConfigName ${CMAKE_BINARY_DIR} NAME)

# Check config dir exists
set(BUILD_CONFIG_DIR "${CMAKE_SOURCE_DIR}/buildConfigs/${buildConfigName}")
if((NOT EXISTS ${BUILD_CONFIG_DIR}) OR (NOT IS_DIRECTORY ${BUILD_CONFIG_DIR}))
    message(FATAL_ERROR "Config directory ${BUILD_CONFIG_DIR} not found.")
endif()

# Use sdkconfig for the selected build configuration
set(SDKCONFIG_DEFAULTS "${BUILD_CONFIG_DIR}/sdkconfig.defaults")
set(SDKCONFIG "${BUILD_CONFIG_DIR}/sdkconfig")

# Configure build config specific options, flags, etc.
include(${BUILD_CONFIG_DIR}/featureFlags.cmake)

message(STATUS "============ Using build config ${buildConfigName} from ${BUILD_CONFIG_DIR} FW_IMAGE_NAME ${FW_IMAGE_NAME}")

message(STATUS "Configuring a build system for ${buildConfigName}")

# Clean up helper variables
unset(buildConfigName)

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

# Fetch the RdWebServer library
FetchContent_Declare(
  rd_webserver
  GIT_REPOSITORY https://github.com/robdobsn/RdWebServer
)

# Populate first so we have a folder that we can add in EXTRA_COMPONENT_DIRS
FetchContent_Populate(esp_littlefs)
FetchContent_Populate(rd_webserver)

# Fetch esp_littlefs, mklittlefs, and rd_webserver
message(STATUS "esp_littlefs folder " ${esp_littlefs_SOURCE_DIR})
set(EXTRA_COMPONENT_DIRS ${esp_littlefs_SOURCE_DIR} ${rd_webserver_SOURCE_DIR})
FetchContent_MakeAvailable(esp_littlefs mklittlefs rd_webserver)

# Configure ESP-IDF project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Fetch Micropython if required
if(NOT MICROPYTHON_VERSION STREQUAL "")
    message(STATUS "Including Micropython " ${MICROPYTHON_VERSION})
    FetchContent_Declare(
      micropython_repo
      GIT_REPOSITORY https://github.com/micropython/micropython
      GIT_TAG         ${MICROPYTHON_VERSION}
    )
    FetchContent_Populate(micropython_repo)
    message(STATUS "micropython folder " ${micropython_repo_SOURCE_DIR})
    FetchContent_MakeAvailable(micropython_repo)
else()
  message(STATUS "Excluding Micropython")
endif()

# Define the project name
project(${FW_IMAGE_NAME} DEPENDS esp_littlefs rd_webserver)

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