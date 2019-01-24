cd ./build
if "%~1" == "c" (
    REM echo "Removing cache"
    rmdir /s /q CMakeFiles
    del CMakeCache.txt
)
cmake ../ -G"MinGW Makefiles" -DCMAKE_MAKE_PROGRAM=make -DCMAKE_TOOLCHAIN_FILE=./cmake/toolchain.cmake
make
cd ..
