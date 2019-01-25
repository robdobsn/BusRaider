# PiBusRaider
# Copyright Rob Dobson 2018-2019
# MIT License

cmake_minimum_required (VERSION 3.12)

set(PROJ_NAME BusRaider)
set(PROJECT_ELF ${PROJ_NAME}.elf)

project($PROJ_NAME C CXX ASM)

set(ASM_SOURCE_FILES src/System/startup.s src/System/lowlev.s)
file(GLOB_RECURSE C_SOURCE_FILES "./src/*.c" "./src/System" "./src/Utils" "./src/TargetBus" "./src/CommandInterface" "./src/Fonts")
file(GLOB_RECURSE CXX_SOURCE_FILES "./src/*.cpp" "./src/System" "./src/Utils" "./src/TargetBus" "./src/CommandInterface" "./src/Fonts" "./src/Machines" "./src/FileFormats")

add_executable(${PROJECT_ELF} "${ASM_SOURCE_FILES}" "${C_SOURCE_FILES}" "${CXX_SOURCE_FILES}" )

target_link_libraries(${PROJECT_ELF} ${PROJECT_SOURCE_DIR}/uspi/lib/libuspi.a)

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostartfiles" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra" )

set( CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-sized-deallocation" )

set( CMAKE_EXE_LINKER_FLAGS "-Wl,-T,${PROJECT_SOURCE_DIR}/memmap" )

add_custom_command(TARGET ${PROJECT_ELF} POST_BUILD 
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_ELF}> ${PROJECT_SOURCE_DIR}/bin/kernel.img
  COMMAND curl -F "file=@${PROJECT_SOURCE_DIR}/bin/kernel.img" http://192.168.86.192/uploadpisw
  COMMENT "Copying to output directory")
