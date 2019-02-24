# PiBusRaider
# Copyright Rob Dobson 2018-2019
# MIT License

cmake_minimum_required (VERSION 3.12)

set(PROJ_NAME BusRaider)
set(PROJECT_ELF ${PROJ_NAME}.elf)
set(PROJECT_HEX ${PROJ_NAME}.hex)

project($PROJ_NAME C CXX ASM)

set(ASM_SOURCE_FILES src/System/startup.s src/System/lowlev.s)
file(GLOB_RECURSE C_SOURCE_FILES "./src/*.c")
file(GLOB_RECURSE CXX_SOURCE_FILES "./src/*.cpp")

add_executable(${PROJECT_ELF} "${ASM_SOURCE_FILES}" "${C_SOURCE_FILES}" "${CXX_SOURCE_FILES}" )

target_link_libraries(${PROJECT_ELF} ${PROJECT_SOURCE_DIR}/uspi/lib/libuspi.a)

set( CMAKE_ASM_FLAGS "--defsym RASPPI=1" )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostartfiles" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DRASPPI=1" )

set( CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-sized-deallocation" )

set( CMAKE_EXE_LINKER_FLAGS "-Wl,-T,${PROJECT_SOURCE_DIR}/memmap" )

# add_custom_command(TARGET ${PROJECT_ELF} POST_BUILD 
#   COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_ELF}> ${PROJECT_SOURCE_DIR}/bin/kernel.img
#  COMMAND ${CMAKE_OBJDUMP} -d $<TARGET_FILE:${PROJECT_ELF}> | $(PREFIX)c++filt > ${PROJECT_SOURCE_DIR}/${PROJ_NAME}.lst
# #  COMMAND curl -F "file=@${PROJECT_SOURCE_DIR}/bin/kernel.img" http://192.168.86.192/uploadpisw
#   # COMMAND "C:/Program Files (x86)/teraterm/ttermpro.exe" /R ${PROJECT_SOURCE_DIR}/bin/kernel.img /speed=115200 /c=39
#   COMMENT "Copying to output directory and uploading ...")

add_custom_command(TARGET ${PROJECT_ELF} POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_ELF}> ${PROJECT_SOURCE_DIR}/bin/kernel.img
  COMMAND ${CMAKE_OBJDUMP} -d $<TARGET_FILE:${PROJECT_ELF}> | $(PREFIX)c++filt > ${PROJECT_SOURCE_DIR}/${PROJ_NAME}.lst
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_ELF}> ${PROJECT_HEX}
  # COMMAND "C:/Program Files (x86)/teraterm/ttermpro.exe" /R ${PROJECT_HEX} /speed=115200 /c=39
  COMMAND python ${PROJECT_SOURCE_DIR}/test/SendAndTest.py test1 ihex 10 COM39 115200 ${PROJECT_SOURCE_DIR}
  COMMENT "Sending IHEX ...")
