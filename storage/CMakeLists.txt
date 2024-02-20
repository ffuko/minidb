file(GLOB_RECURSE SOURCES *.cpp)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

add_library(storage_lib STATIC ${SOURCES})
target_include_directories(storage_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
