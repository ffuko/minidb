file(GLOB_RECURSE SOURCES *.cpp)

include(FetchContent)
FetchContent_Declare(
    expected
    GIT_REPOSITORY https://github.com/TartanLlama/expected
    GIT_TAG 3f0ca7b19253129700a073abfa6d8638d9f7c80c
)
FetchContent_MakeAvailable(expected)

add_library(storage_lib STATIC ${SOURCES})
target_include_directories(
    storage_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/common
)

target_link_libraries(storage_lib PUBLIC expected)

