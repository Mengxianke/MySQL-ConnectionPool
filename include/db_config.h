#ifndef DB_CONFIG_H
#define DB_CONFIG_H

#include <string>
#include <vector>

/**
 * @brief 单个数据库实例的配置信息
 * 
 * 这个结构体包含了连接单个MySQL数据库所需的所有信息
 * 采用结构体而不是类的原因：
 * 1. 数据简单，不需要复杂的方法
 * 2. 方便初始化和赋值
 * 3. 可以直接用于函数参数传递
 */
struct DBConfig {
    std::string host;       // 数据库主机地址（如：localhost, 192.168.1.100）
    std::string user;       // 数据库用户名
    std::string password;   // 数据库密码
    std::string database;   // 数据库名称
    unsigned int port;      // 端口号，MySQL默认3306
    unsigned int weight;    // 权重，用于负载均衡（数值越大，被选中概率越大）

    /**
     * @brief 默认构造函数
     * 设置MySQL的标准默认值
     */
    DBConfig() : port(3306), weight(1) {}

    /**
     * @brief 便捷构造函数
     * @param host 主机地址
     * @param user 用户名  
     * @param password 密码
     * @param database 数据库名
     * @param port 端口，默认3306
     * @param weight 权重，默认1
     */
    DBConfig(const std::string& host, const std::string& user,
             const std::string& password, const std::string& database,
             unsigned int port = 3306, unsigned int weight = 1)
        : host(host), user(user), password(password), database(database),
          port(port), weight(weight) {}

    /**
     * @brief 验证配置是否有效
     * @return 配置是否完整有效
     */
    bool isValid() const {
        return !host.empty() && !user.empty() && !database.empty() && port > 0;
    }

    /**
     * @brief 获取连接字符串描述（不包含密码，用于日志）
     * @return 连接描述字符串
     */
    std::string getConnectionString() const {
        return user + "@" + host + ":" + std::to_string(port) + "/" + database;
    }

    /**
     * @brief 比较操作符，用于容器排序和查找
     * @param other 另一个配置
     * @return 是否相等
     */
    bool operator==(const DBConfig& other) const {
        return host == other.host && 
               port == other.port && 
               user == other.user && 
               database == other.database;
    }

    /**
     * @brief 不等比较操作符
     * @param other 另一个配置
     * @return 是否不相等
     */
    bool operator!=(const DBConfig& other) const {
        return !(*this == other);
    }
};

/**
 * @brief 数据库配置列表类型定义
 * 使用类型别名提高代码可读性
 */
using DBConfigList = std::vector<DBConfig>;

#endif // DB_CONFIG_H