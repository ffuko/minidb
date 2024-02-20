cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_BUILD_TYPE Debug)

project(minisql)

add_subdirectory(storage)

# register ctest on top of the build directory: https://stackoverflow.com/questions/14446495/cmake-project-structure-with-unit-tests
enable_testing()

add_subdirectory(test)



