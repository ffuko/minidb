include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(index_test ${CMAKE_CURRENT_SOURCE_DIR}/storage/index_test.cpp)
add_executable(disk_manager_test 
    ${CMAKE_CURRENT_SOURCE_DIR}/storage/disk/disk_manager_test.cpp
)
add_executable(page_test
    ${CMAKE_CURRENT_SOURCE_DIR}/storage/disk/page_test.cpp
)
add_executable(buffer_pool_test
    ${CMAKE_CURRENT_SOURCE_DIR}/storage/buffer/buffer_pool_test.cpp
)

target_link_libraries(index_test PUBLIC storage_lib GTest::gtest_main)
target_link_libraries(disk_manager_test PUBLIC storage_lib GTest::gtest_main)
target_link_libraries(page_test PUBLIC storage_lib GTest::gtest_main)
target_link_libraries(buffer_pool_test PUBLIC storage_lib GTest::gtest_main)

include(GoogleTest)
# gtest_discover_tests(index_test)
gtest_discover_tests(disk_manager_test)
gtest_discover_tests(page_test)
gtest_discover_tests(buffer_pool_test)



