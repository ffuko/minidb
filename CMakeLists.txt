cmake_minimum_required(VERSION 3.24)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_BUILD_TYPE Debug)

# ignroe FetchContent DOWNLOAD_EXTRACT_TIMESTAMP warning
set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)

project(minidb)

add_subdirectory(storage)
add_subdirectory(db)

# register ctest on top of the build directory: https://stackoverflow.com/questions/14446495/cmake-project-structure-with-unit-tests
enable_testing()

add_subdirectory(test)



