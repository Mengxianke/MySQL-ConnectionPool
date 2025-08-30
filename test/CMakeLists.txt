# test/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 定义一个函数来添加测试
function(add_pool_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(${test_name} PRIVATE dbconnectionpool)
endfunction()

# 添加测试
add_pool_test(test_connection_pool test_basic1.cpp)
add_pool_test(test_day5_connection test_day5_connection.cpp)
add_pool_test(test_day6_connection test_day6_connection.cpp)
add_pool_test(test_day7_connection test_day7_connection.cpp)