#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <string>
#include <atomic>
#include <mutex>
#include <chrono>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
//#include "utils.h"

/**
 * @brief 性能统计信息结构体
 * 
 * 设计原则：
 * 1. 只存储最关键的数字，不存储复杂对象
 * 2. 使用基础数据类型，方便原子操作
 * 3. 提供计算方法，而不是存储计算结果
 */
struct PerformanceStats {
    // === 连接相关统计 ===
    uint64_t totalConnectionsCreated = 0;      // 总共创建的连接数
    uint64_t totalConnectionsAcquired = 0;     // 总共获取的连接数
    uint64_t totalConnectionsReleased = 0;     // 总共释放的连接数
    uint64_t failedConnectionAttempts = 0;     // 连接失败次数

    // === 查询相关统计 ===
    uint64_t totalQueriesExecuted = 0;         // 总查询执行次数
    uint64_t failedQueries = 0;                // 查询失败次数

    // === 重连相关统计 ===
    uint64_t reconnectionAttempts = 0;         // 重连尝试次数
    uint64_t successfulReconnections = 0;      // 成功重连次数

    // === 时间统计（微秒为单位，更精确） ===
    uint64_t totalConnectionAcquireTime = 0;   // 总连接获取时间
    uint64_t totalConnectionUsageTime = 0;     // 总连接使用时间
    uint64_t totalQueryExecutionTime = 0;      // 总查询执行时间

    // === 计算方法：把复杂计算封装成方法 ===
    
    /**
     * @brief 计算平均连接获取时间（微秒）
     */
    double avgConnectionAcquireTime() const {
        return totalConnectionsAcquired > 0 ?
            static_cast<double>(totalConnectionAcquireTime) / totalConnectionsAcquired : 0.0;
    }

    /**
     * @brief 计算平均连接使用时间（微秒）
     */
    double avgConnectionUsageTime() const {
        return totalConnectionsReleased > 0 ?
            static_cast<double>(totalConnectionUsageTime) / totalConnectionsReleased : 0.0;
    }

    /**
     * @brief 计算平均查询执行时间（微秒）
     */
    double avgQueryExecutionTime() const {
        return totalQueriesExecuted > 0 ?
            static_cast<double>(totalQueryExecutionTime) / totalQueriesExecuted : 0.0;
    }

    /**
     * @brief 计算重连成功率（百分比）
     */
    double reconnectionSuccessRate() const {
        return reconnectionAttempts > 0 ?
            static_cast<double>(successfulReconnections) / reconnectionAttempts * 100.0 : 0.0;
    }

    /**
     * @brief 计算查询成功率（百分比）
     */
    double querySuccessRate() const {
        return totalQueriesExecuted > 0 ?
            static_cast<double>(totalQueriesExecuted - failedQueries) / totalQueriesExecuted * 100.0 : 0.0;
    }

    /**
     * @brief 计算连接获取成功率（百分比）
     */
    double connectionAcquireSuccessRate() const {
        uint64_t totalAttempts = totalConnectionsAcquired + failedConnectionAttempts;
        return totalAttempts > 0 ?
            static_cast<double>(totalConnectionsAcquired) / totalAttempts * 100.0 : 0.0;
    }
};

/**
 * @brief 连接池性能监控类
 * 
 * 设计特点：
 * 1. 单例模式 - 全局唯一，方便访问
 * 2. 线程安全 - 使用原子操作，避免锁竞争
 * 3. 高性能 - 记录操作极快，不影响主业务
 * 4. 易使用 - 接口简单，一行代码搞定
 */
class PerformanceMonitor {
public:
    /**
     * @brief 获取性能监控单例实例
     */
    static PerformanceMonitor& getInstance() {
        static PerformanceMonitor instance;
        return instance;
    }

    // === 数据记录接口（高频调用，必须极快） ===
    
    /**
     * @brief 记录连接创建
     * 
     * 使用场景：在 createConnection() 成功后调用
     */
    void recordConnectionCreated() {
        m_totalConnectionsCreated.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief 记录连接获取
     * @param timeTaken 获取连接所花费的时间（微秒）
     * 
     * 使用场景：在 getConnection() 返回前调用
     */
    void recordConnectionAcquired(int64_t timeTaken) {
        m_totalConnectionsAcquired.fetch_add(1, std::memory_order_relaxed);
        m_totalConnectionAcquireTime.fetch_add(timeTaken, std::memory_order_relaxed);
    }

    /**
     * @brief 记录连接释放
     * @param usageTime 连接使用时间（微秒）
     * 
     * 使用场景：在 releaseConnection() 中调用
     */
    void recordConnectionReleased(int64_t usageTime) {
        m_totalConnectionsReleased.fetch_add(1, std::memory_order_relaxed);
        m_totalConnectionUsageTime.fetch_add(usageTime, std::memory_order_relaxed);
    }

    /**
     * @brief 记录连接失败
     * 
     * 使用场景：在 getConnection() 失败时调用
     */
    void recordConnectionFailed() {
        m_failedConnectionAttempts.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief 记录查询执行
     * @param queryTime 查询执行时间（微秒）
     * @param success 查询是否成功
     * 
     * 使用场景：在 executeQuery() 或 executeUpdate() 中调用
     */
    void recordQueryExecuted(int64_t queryTime, bool success) {
        m_totalQueriesExecuted.fetch_add(1, std::memory_order_relaxed);
        m_totalQueryExecutionTime.fetch_add(queryTime, std::memory_order_relaxed);
        if (!success) {
            m_failedQueries.fetch_add(1, std::memory_order_relaxed);
        }
    }

    /**
     * @brief 记录重连尝试
     * @param success 重连是否成功
     * 
     * 使用场景：在 reconnect() 方法中调用
     */
    void recordReconnection(bool success) {
        m_reconnectionAttempts.fetch_add(1, std::memory_order_relaxed);
        if (success) {
            m_successfulReconnections.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // === 数据查询接口（低频调用，可以稍慢） ===
    
    /**
     * @brief 获取性能统计信息快照
     * @return 当前的性能统计信息
     * 
     * 注意：返回的是快照，不保证绝对一致性
     * 但对于监控来说，这种精度已经足够了
     */
    PerformanceStats getStats() const;

    /**
     * @brief 重置所有统计信息
     * 
     * 使用场景：比如每天凌晨重置，获取当日统计
     */
    void resetStats();

    /**
     * @brief 导出统计信息到CSV文件
     * @param filePath CSV文件路径
     * @return 是否成功导出
     */
    bool exportToCSV(const std::string& filePath);

    /**
     * @brief 获取格式化的统计信息字符串
     * @return 可读的统计信息
     */
    std::string getStatsString() const;

// 辅助方法实现
    std::string getCurrentTimeString() const;
    std::string getPerformanceLevel(double avgTimeUs) const;

    std::string getQueryPerformanceLevel(double avgTimeUs) const;


    std::string getStabilityLevel(const PerformanceStats& stats) const;


private:
    // 私有构造函数（单例模式）
    PerformanceMonitor() = default;
    
    // 禁用拷贝
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;

    // === 原子变量存储（无锁高性能） ===
    
    // 连接统计
    std::atomic<uint64_t> m_totalConnectionsCreated{0};
    std::atomic<uint64_t> m_totalConnectionsAcquired{0};
    std::atomic<uint64_t> m_totalConnectionsReleased{0};
    std::atomic<uint64_t> m_failedConnectionAttempts{0};

    // 查询统计
    std::atomic<uint64_t> m_totalQueriesExecuted{0};
    std::atomic<uint64_t> m_failedQueries{0};

    // 重连统计
    std::atomic<uint64_t> m_reconnectionAttempts{0};
    std::atomic<uint64_t> m_successfulReconnections{0};

    // 时间统计（微秒）
    std::atomic<uint64_t> m_totalConnectionAcquireTime{0};
    std::atomic<uint64_t> m_totalConnectionUsageTime{0};
    std::atomic<uint64_t> m_totalQueryExecutionTime{0};
};

#endif // PERFORMANCE_MONITOR_H