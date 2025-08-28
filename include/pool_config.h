#ifndef POOL_CONFIG_H
#define POOL_CONFIG_H

#include "db_config.h"
#include <vector>
#include <chrono>

/**
 * @brief 连接池配置信息
 * 
 * 这个结构体定义了连接池的所有配置参数
 * 包括数据库连接信息、连接池大小、超时设置等
 */
struct PoolConfig {
    // =========================
    // 数据库连接基本信息
    // =========================
    std::string host;           // 默认主机（单数据库模式）
    std::string user;           // 默认用户名
    std::string password;       // 默认密码
    std::string database;       // 默认数据库名
    unsigned int port;          // 默认端口号

    // =========================
    // 多数据库实例配置（用于负载均衡）
    // =========================
    // std::vector<DBConfig> dbInstances;  // 多个数据库实例配置

    // =========================
    // 连接池大小参数
    // =========================
    unsigned int minConnections;    // 最小连接数（池中始终保持的连接数）
    unsigned int maxConnections;    // 最大连接数（池中最多允许的连接数）
    unsigned int initConnections;   // 初始连接数（启动时创建的连接数）

    // =========================
    // 超时设置（毫秒）
    // =========================
    unsigned int connectionTimeout;  // 等待获取连接的超时时间
    unsigned int maxIdleTime;        // 连接最大空闲时间（超过则关闭）
    unsigned int healthCheckPeriod;  // 健康检查周期

    // =========================
    // 重连设置
    // =========================
    unsigned int reconnectInterval;  // 重连间隔（毫秒）
    unsigned int reconnectAttempts;  // 最大重连尝试次数

    // =========================
    // 其他设置
    // =========================
    bool logQueries;            // 是否记录所有SQL查询
    bool enablePerformanceStats; // 是否启用性能统计

    /**
     * @brief 默认构造函数
     * 设置合理的默认值，适用于大多数场景
     */
    PoolConfig()
        : port(3306)                    // MySQL标准端口
        , minConnections(5)             // 最少保持5个连接
        , maxConnections(20)            // 最多允许20个连接
        , initConnections(5)            // 启动时创建5个连接
        , connectionTimeout(5000)       // 5秒获取连接超时
        , maxIdleTime(600000)          // 10分钟空闲超时
        , healthCheckPeriod(30000)     // 30秒健康检查
        , reconnectInterval(1000)      // 1秒重连间隔
        , reconnectAttempts(3)         // 最多重试3次
        , logQueries(false)            // 默认不记录查询
        , enablePerformanceStats(true) // 默认启用性能统计
    {}

    /**
     * @brief 便捷构造函数（单数据库模式）
     * @param host 主机地址
     * @param user 用户名
     * @param password 密码
     * @param database 数据库名
     * @param port 端口
     */
    PoolConfig(const std::string& host, const std::string& user,
               const std::string& password, const std::string& database,
               unsigned int port = 3306)
        : PoolConfig() // 先调用默认构造函数
    {
        this->host = host;
        this->user = user;
        this->password = password;
        this->database = database;
        this->port = port;
    }


    /**
     * @brief 验证配置是否有效
     * @return 配置是否完整有效
     */
    bool isValid() const {
        // 检查基本参数
        if (minConnections == 0 || maxConnections == 0 || 
            minConnections > maxConnections || initConnections > maxConnections) {
            return false;
        }

        // 检查超时参数
        if (connectionTimeout == 0 || maxIdleTime == 0 || healthCheckPeriod == 0) {
            return false;
        }

        // 检查数据库配置
        // if (!dbInstances.empty()) {
        //     // 多数据库模式：检查每个实例配置
        //     for (const auto& config : dbInstances) {
        //         if (!config.isValid()) {
        //             return false;
        //         }
        //     }
        // } else {
        //     // 单数据库模式：检查默认配置
        //     if (host.empty() || user.empty() || database.empty() || port == 0) {
        //         return false;
        //     }
        // }

        return true;
    }

    /**
     * @brief 添加数据库实例（多数据库模式）
     * @param config 数据库配置
     */
    // void addDatabase(const DBConfig& config) {
    //     if (config.isValid()) {
    //         dbInstances.push_back(config);
    //     }
    // }

    // /**
    //  * @brief 获取数据库实例数量
    //  * @return 数据库实例数量
    //  */
    // size_t getDatabaseCount() const {
    //     return dbInstances.empty() ? 1 : dbInstances.size();
    // }

    /**
     * @brief 获取配置摘要信息（用于日志输出）
     * @return 配置摘要字符串
     */
    std::string getSummary() const {
        std::string summary = "PoolConfig{";
        summary += "connections=[" + std::to_string(minConnections) + 
                   "," + std::to_string(maxConnections) + "]";
        // summary += ", timeout=" + std::to_string(connectionTimeout) + "ms";
        // summary += ", databases=" + std::to_string(getDatabaseCount());
        summary += "}";
        return summary;
    }

    /**
     * @brief 设置连接池大小参数
     * @param min 最小连接数
     * @param max 最大连接数
     * @param init 初始连接数
     */
    void setConnectionLimits(unsigned int min, unsigned int max, unsigned int init) {
        minConnections = min;
        maxConnections = max;
        initConnections = (init == 0) ? min : std::min(init, max);
    }

    /**
     * @brief 设置超时参数
     * @param connTimeout 连接获取超时
     * @param idleTimeout 空闲超时
     * @param checkPeriod 健康检查周期
     */
    void setTimeouts(unsigned int connTimeout, unsigned int idleTimeout, unsigned int checkPeriod) {
        connectionTimeout = connTimeout;
        maxIdleTime = idleTimeout;
        healthCheckPeriod = checkPeriod;
    }
};

#endif // POOL_CONFIG_H