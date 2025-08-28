# cmake/FindMySQL.cmake
# 查找MySQL路径

# 查找头文件路径
find_path(MYSQL_INCLUDE_DIR
  NAMES mysql.h
  PATHS
    /usr/include/mysql
    /usr/local/include/mysql
    /opt/mysql/include/mysql
)

# 查找库文件
find_library(MYSQL_LIBRARY
  NAMES mysqlclient
  PATHS
    /usr/lib
    /usr/lib/mysql
    /usr/local/lib
    /usr/local/lib/mysql
    /opt/mysql/lib
)

# 处理找到的结果
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL
  DEFAULT_MSG
  MYSQL_LIBRARY MYSQL_INCLUDE_DIR
)

# 设置变量
if(MySQL_FOUND)
  set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})
  set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})
endif()

# 标记为高级变量
mark_as_advanced(MYSQL_INCLUDE_DIR MYSQL_LIBRARY)