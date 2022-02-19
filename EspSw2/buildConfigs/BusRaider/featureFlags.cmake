# System naming
add_compile_definitions(HW_SYSTEM_NAME=BusRaider)
add_compile_definitions(HW_DEFAULT_FRIENDLY_NAME=BusRaider)
add_compile_definitions(HW_DEFAULT_HOSTNAME=BusRaider)
add_compile_definitions(HW_DEFAULT_ADVNAME=BusRaider)

# Hardware revision detection
# add_compile_definitions(HW_REVISION_ESP32_LIMIT_ESPREV=3)
# add_compile_definitions(HW_REVISION_ESP32_LIMIT_HWREV=2)
# add_compile_definitions(HW_REVISION_LADDER_PIN=32)
# add_compile_definitions(HW_REVISION_LOW_THRESH_1=1700)
# add_compile_definitions(HW_REVISION_HIGH_THRESH_1=2050)
# add_compile_definitions(HW_REVISION_HWREV_THRESH_1=4)

# Main features
# add_compile_definitions(FEATURE_POWER_UP_LED_ASAP)
add_compile_definitions(FEATURE_WIFI_FUNCTIONALITY)
add_compile_definitions(FEATURE_WEB_SERVER_OR_WEB_SOCKETS)
add_compile_definitions(FEATURE_WEB_SOCKETS)
add_compile_definitions(FEATURE_WEB_SERVER_STATIC_FILES)
# add_compile_definitions(FEATURE_EMBED_MICROPYTHON)
# add_compile_definitions(FEATURE_INCLUDE_ROBOT_CONTROLLER)
# add_compile_definitions(FEATURE_BUS_HIATUS_DURING_POWER_ON)

# LittleFS configuration
add_compile_definitions(CONFIG_LITTLEFS_MAX_PARTITIONS=3)
add_compile_definitions(CONFIG_LITTLEFS_PAGE_SIZE=256)
add_compile_definitions(CONFIG_LITTLEFS_READ_SIZE=128)
add_compile_definitions(CONFIG_LITTLEFS_WRITE_SIZE=128)
add_compile_definitions(CONFIG_LITTLEFS_LOOKAHEAD_SIZE=128)
add_compile_definitions(CONFIG_LITTLEFS_CACHE_SIZE=512)
add_compile_definitions(CONFIG_LITTLEFS_BLOCK_CYCLES=512)

# File system
set(FS_TYPE "littlefs")
set(FS_IMAGE_PATH "${BUILD_CONFIG_DIR}/FSImage")

# Micropython
set(MICROPYTHON_VERSION "")

# Web UI
set(UI_SOURCE_PATH "${BUILD_CONFIG_DIR}/WebUI")
# set(WEB_UI_GEN_FLAGS --nogzip)

# Firmware image name
set(FW_IMAGE_NAME "BusRaider")
