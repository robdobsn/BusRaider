set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR BCM2835)

set(CMAKE_C_COMPILER "arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "arm-none-eabi-gcc")
set(CMAKE_ASM_COMPILER "arm-none-eabi-as")
# set(CMAKE_LINKER "arm-none-eabi-ld")
set(CMAKE_OBJCOPY "arm-none-eabi-objcopy")
set(CMAKE_OBJDUMP "arm-none-eabi-objdump")

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostartfiles" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=hard" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=vfp" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv6zk" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mtune=arm1176jzf-s" )

set(CMAKE_EXE_LINKER_FLAGS "--specs=nosys.specs" CACHE INTERNAL "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
