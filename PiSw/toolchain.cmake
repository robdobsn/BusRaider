set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR BCM2835)

set(CMAKE_C_COMPILER "arm-none-eabi-gcc.exe")
set(CMAKE_CXX_COMPILER "arm-none-eabi-gcc.exe")
set(CMAKE_ASM_COMPILER "arm-none-eabi-as.exe")
# set(CMAKE_ASM_COMPILER "arm-none-eabi-as.exe")
# set(CMAKE_LINKER "arm-none-eabi-ld.exe")
set(CMAKE_OBJCOPY "arm-none-eabi-objcopy.exe")

set(CMAKE_EXE_LINKER_FLAGS "--specs=nosys.specs" CACHE INTERNAL "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
