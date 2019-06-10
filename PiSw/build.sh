cd ./build
if [[ $1 == c ]]; then
    REM echo "Removing cache"
    rmdir CMakeFiles
    rm CMakeCache.txt
fi
cmake ../ -DCMAKE_MAKE_PROGRAM=make -DCMAKE_TOOLCHAIN_FILE=./cmake/toolchain.cmake
make
cd ..
