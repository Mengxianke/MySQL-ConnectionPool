# src/CMakeLists.txt

cmake_minimum_required(VERSION 3.10)

# 获取所有源文件
file(GLOB SOURCES "*.cpp")

# 创建库
add_library(dbconnectionpool ${SOURCES})

# 设置包含目录
target_include_directories(dbconnectionpool
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${MYSQL_INCLUDE_DIRS}  # 注意这里修改为MYSQL_INCLUDE_DIRS
)

# 链接依赖库
target_link_libraries(dbconnectionpool
    PUBLIC
        ${MYSQL_LIBRARIES}  # 注意这里修改为MYSQL_LIBRARIES
        Threads::Threads
)

# 设置库属性
set_target_properties(dbconnectionpool PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
)

# 安装目标
install(TARGETS dbconnectionpool
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)