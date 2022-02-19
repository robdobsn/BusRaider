# littlefs_create_partition_image
# (based on ESP IDF spiffs_create_partition_image) Rob Dobson
#
# Create a littlefs image of the specified directory on the host during build and optionally
# have the created image flashed using `idf.py flash`
function(littlefs_create_partition_image partition base_dir)

    # mklittlefs executable
    add_custom_target(build_mklittlefs ALL
        COMMAND echo "------------- Building mklittlefs ---------------"
        COMMAND cd ${mklittlefs_SOURCE_DIR}
        COMMAND make dist
    )

    set(options FLASH_IN_PROJECT)
    set(multi DEPENDS)
    cmake_parse_arguments(arg "${options}" "" "${multi}" "${ARGN}")

    idf_build_get_property(idf_path IDF_PATH)
    set(mklittlefs_exec ${mklittlefs_SOURCE_DIR}/mklittlefs)

    get_filename_component(base_dir_full_path ${base_dir} ABSOLUTE)

    partition_table_get_partition_info(size "--partition-name ${partition}" "size")
    partition_table_get_partition_info(offset "--partition-name ${partition}" "offset")

    if("${size}" AND "${offset}")

        set(image_file ${CMAKE_BINARY_DIR}/${partition}.bin)

        # Execute littlefs image generation; this always executes as there is no way to specify for CMake to watch for
        # contents of the base dir changing.
        add_custom_target(littlefs_${partition}_bin ALL
            COMMAND echo "------------- Generating littlefs image ---------------"
            COMMAND ${mklittlefs_exec} -s ${size} -c ${base_dir_full_path} ${image_file}
            DEPENDS ${arg_DEPENDS} build_mklittlefs
            )

        set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" APPEND PROPERTY
            ADDITIONAL_MAKE_CLEAN_FILES
            ${image_file})

        idf_component_get_property(main_args esptool_py FLASH_ARGS)
        idf_component_get_property(sub_args esptool_py FLASH_SUB_ARGS)
        # Last (optional) parameter is the encryption for the target. In our
        # case, spiffs is not encrypt so pass FALSE to the function.
        esptool_py_flash_target(${partition}-flash "${main_args}" "${sub_args}" ALWAYS_PLAINTEXT)
        esptool_py_flash_to_partition(${partition}-flash "${partition}" "${image_file}")

        add_dependencies(${partition}-flash littlefs_${partition}_bin)

        if(arg_FLASH_IN_PROJECT)
            esptool_py_flash_to_partition(flash "${partition}" "${image_file}")
            add_dependencies(flash littlefs_${partition}_bin)
        endif()
    else()
        set(message "littlefs_create_partition_image: Failed to create LITTLEFS image for partition '${partition}'. "
                    "Check project configuration if using the correct partition table file.")
        fail_at_build_time(littlefs_${partition}_bin "${message}")
    endif()

endfunction()