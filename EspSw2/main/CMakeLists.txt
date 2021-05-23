set(_custom_headers_dir "${CMAKE_BINARY_DIR}/customHeaders/main")
file(MAKE_DIRECTORY ${_custom_headers_dir})

idf_component_register(
                SRCS
                    "main.cpp"
                    "RICSysTypes.cpp"
                    "PiCoProcessor/PiCoProcessor.cpp"
                INCLUDE_DIRS
                    "."
                    ${_custom_headers_dir}
                    "PiCoProcessor"
                )

# Make sure the SysTypes header is (re)generated before compiling this component (main) if needed
set(_systypes_header "${_custom_headers_dir}/SysTypes.h")
set(_systypes_json "${RIC_CONFIG_DIR}/SysTypes.json")
add_custom_command(OUTPUT ${_systypes_header}
                    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/json2header.py ${_systypes_json} ${_systypes_header}
                    DEPENDS ${_systypes_json}
                    COMMENT "Generating SysTypes.h"
                    )
add_custom_target(generateSysTypesHeader
                   DEPENDS ${_systypes_header}
                   COMMENT "Checking SysTypes.h"
                   )
# https://docs.espressif.com/projects/esp-idf/en/v4.0.1/api-guides/build-system.html#preset-component-variables
add_dependencies(${COMPONENT_LIB} generateSysTypesHeader)

# Clean up
unset(_custom_headers_dir)
unset(_systypes_header)
unset(_systypes_json)

# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'spiffs'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
if(FS_TYPE STREQUAL "spiffs")
    message(STATUS "Configuring SPIFFS file system")
    spiffs_create_partition_image(spiffs ${FS_IMAGE_PATH} FLASH_IN_PROJECT)
endif()
