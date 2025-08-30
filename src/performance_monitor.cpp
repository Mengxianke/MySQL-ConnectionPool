#include "performance_monitor.h"
#include "logger.h"
#include <iomanip>
#include <fstream>
#include <sstream>

// =========================
// 获取统计信息实现
// =========================

PerformanceStats PerformanceMonitor::getStats() const {
    PerformanceStats stats;

    // 原子读取所有计数器
    // 使用 memory_order_acquire 保证读取的一致性
    stats.totalConnectionsCreated = m_totalConnectionsCreated.load(std::memory_order_acquire);
    stats.totalConnectionsAcquired = m_totalConnectionsAcquired.load(std::memory_order_acquire);
    stats.totalConnectionsReleased = m_totalConnectionsReleased.load(std::memory_order_acquire);
    stats.failedConnectionAttempts = m_failedConnectionAttempts.load(std::memory_order_acquire);

    stats.totalQueriesExecuted = m_totalQueriesExecuted.load(std::memory_order_acquire);
    stats.failedQueries = m_failedQueries.load(std::memory_order_acquire);

    stats.reconnectionAttempts = m_reconnectionAttempts.load(std::memory_order_acquire);
    stats.successfulReconnections = m_successfulReconnections.load(std::memory_order_acquire);

    stats.totalConnectionAcquireTime = m_totalConnectionAcquireTime.load(std::memory_order_acquire);
    stats.totalConnectionUsageTime = m_totalConnectionUsageTime.load(std::memory_order_acquire);
    stats.totalQueryExecutionTime = m_totalQueryExecutionTime.load(std::memory_order_acquire);

    return stats;
}

// =========================
// 重置统计信息实现
// =========================

void PerformanceMonitor::resetStats() {
    LOG_INFO("Resetting performance statistics");

    // 使用 memory_order_release 确保重置操作的可见性
    m_totalConnectionsCreated.store(0, std::memory_order_release);
    m_totalConnectionsAcquired.store(0, std::memory_order_release);
    m_totalConnectionsReleased.store(0, std::memory_order_release);
    m_failedConnectionAttempts.store(0, std::memory_order_release);

    m_totalQueriesExecuted.store(0, std::memory_order_release);
    m_failedQueries.store(0, std::memory_order_release);

    m_reconnectionAttempts.store(0, std::memory_order_release);
    m_successfulReconnections.store(0, std::memory_order_release);

    m_totalConnectionAcquireTime.store(0, std::memory_order_release);
    m_totalConnectionUsageTime.store(0, std::memory_order_release);
    m_totalQueryExecutionTime.store(0, std::memory_order_release);

    LOG_INFO("Performance statistics reset completed");
}

// =========================
// 格式化统计信息实现
// =========================

std::string PerformanceMonitor::getStatsString() const {
    // 获取当前统计信息快照
    PerformanceStats stats = getStats();

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);  // 保留2位小数

    ss << "===== 连接池性能统计报告 =====\n";
    ss << "生成时间: " << getCurrentTimeString() << "\n\n";

    // === 连接相关统计 ===
    ss << "【连接统计】\n";
    ss << "  创建总数: " << stats.totalConnectionsCreated << " 个\n";
    ss << "  获取总数: " << stats.totalConnectionsAcquired << " 次\n";
    ss << "  释放总数: " << stats.totalConnectionsReleased << " 次\n";
    ss << "  失败次数: " << stats.failedConnectionAttempts << " 次\n";
    ss << "  获取成功率: " << stats.connectionAcquireSuccessRate() << "%\n";
    ss << "  平均获取时间: " << stats.avgConnectionAcquireTime() / 1000.0 << " ms\n";
    ss << "  平均使用时间: " << stats.avgConnectionUsageTime() << " ms\n\n";

    // === 查询相关统计 ===
    ss << "【查询统计】\n";
    ss << "  执行总数: " << stats.totalQueriesExecuted << " 次\n";
    ss << "  失败次数: " << stats.failedQueries << " 次\n";
    ss << "  成功率: " << stats.querySuccessRate() << "%\n";
    ss << "  平均执行时间: " << stats.avgQueryExecutionTime() / 1000.0 << " ms\n\n";

    // === 重连相关统计 ===
    ss << "【重连统计】\n";
    ss << "  尝试次数: " << stats.reconnectionAttempts << " 次\n";
    ss << "  成功次数: " << stats.successfulReconnections << " 次\n";
    ss << "  成功率: " << stats.reconnectionSuccessRate() << "%\n\n";

    // === 性能评估 ===
    ss << "【性能评估】\n";
    ss << "  连接获取性能: " << getPerformanceLevel(stats.avgConnectionAcquireTime()) << "\n";
    ss << "  查询执行性能: " << getQueryPerformanceLevel(stats.avgQueryExecutionTime()) << "\n";
    ss << "  系统稳定性: " << getStabilityLevel(stats) << "\n";

    ss << "================================\n";

    return ss.str();
}

// =========================
// 辅助方法实现
// =========================

std::string PerformanceMonitor::getCurrentTimeString() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string PerformanceMonitor::getPerformanceLevel(double avgTimeUs) const {
    if (avgTimeUs < 1000) {        // < 1ms
        return "优秀 (< 1ms)";
    } else if (avgTimeUs < 10000) { // < 10ms
        return "良好 (< 10ms)";
    } else if (avgTimeUs < 50000) { // < 50ms
        return "一般 (< 50ms)";
    } else {
        return "较差 (> 50ms)";
    }
}

std::string PerformanceMonitor::getQueryPerformanceLevel(double avgTimeUs) const {
    if (avgTimeUs < 10000) {        // < 10ms
        return "优秀 (< 10ms)";
    } else if (avgTimeUs < 100000) { // < 100ms
        return "良好 (< 100ms)";
    } else if (avgTimeUs < 500000) { // < 500ms
        return "一般 (< 500ms)";
    } else {
        return "较差 (> 500ms)";
    }
}

std::string PerformanceMonitor::getStabilityLevel(const PerformanceStats& stats) const {
    double connectionSuccessRate = stats.connectionAcquireSuccessRate();
    double querySuccessRate = stats.querySuccessRate();
    
    if (connectionSuccessRate > 99.5 && querySuccessRate > 99.5) {
        return "优秀 (成功率 > 99.5%)";
    } else if (connectionSuccessRate > 98.0 && querySuccessRate > 98.0) {
        return "良好 (成功率 > 98%)";
    } else if (connectionSuccessRate > 95.0 && querySuccessRate > 95.0) {
        return "一般 (成功率 > 95%)";
    } else {
        return "较差 (成功率过低)";
    }
}


bool PerformanceMonitor::exportToCSV(const std::string& filePath) {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open CSV file for writing: " + filePath);
            return false;
        }

        LOG_INFO("Exporting performance statistics to CSV: " + filePath);

        // 获取当前统计信息快照
        PerformanceStats stats = getStats();

        // 写入CSV头部
        file << "统计项目,数值,单位,说明\n";

        // === 基础统计数据 ===
        file << "总创建连接数," << stats.totalConnectionsCreated << ",个,累计创建的数据库连接数\n";
        file << "总获取连接数," << stats.totalConnectionsAcquired << ",次,累计获取连接的请求数\n";
        file << "总释放连接数," << stats.totalConnectionsReleased << ",次,累计释放连接的次数\n";
        file << "连接失败次数," << stats.failedConnectionAttempts << ",次,获取连接失败的次数\n";

        file << "总查询执行数," << stats.totalQueriesExecuted << ",次,累计执行的SQL查询数\n";
        file << "查询失败次数," << stats.failedQueries << ",次,执行失败的查询数\n";

        file << "重连尝试次数," << stats.reconnectionAttempts << ",次,网络断开后的重连尝试\n";
        file << "重连成功次数," << stats.successfulReconnections << ",次,重连成功的次数\n";

        // === 时间统计（转换为毫秒，更容易理解） ===
        file << "总连接获取时间," << stats.totalConnectionAcquireTime / 1000.0 << ",毫秒,获取连接的累计耗时\n";
        file << "总连接使用时间," << stats.totalConnectionUsageTime / 1000.0 << ",毫秒,连接被占用的累计时间\n";
        file << "总查询执行时间," << stats.totalQueryExecutionTime / 1000.0 << ",毫秒,SQL执行的累计耗时\n";

        // === 计算指标（这些是最有价值的数据） ===
        file << "平均连接获取时间," << stats.avgConnectionAcquireTime() / 1000.0 << ",毫秒,平均获取一个连接的时间\n";
        file << "平均连接使用时间," << stats.avgConnectionUsageTime() << ",毫秒,平均占用连接的时间\n";
        file << "平均查询执行时间," << stats.avgQueryExecutionTime() / 1000.0 << ",毫秒,平均执行一个查询的时间\n";

        file << "连接获取成功率," << stats.connectionAcquireSuccessRate() << ",%,成功获取连接的比例\n";
        file << "查询执行成功率," << stats.querySuccessRate() << ",%,查询执行成功的比例\n";
        file << "重连成功率," << stats.reconnectionSuccessRate() << ",%,重连尝试成功的比例\n";

        // === 添加导出时间戳 ===
        file << "导出时间," << getCurrentTimeString() << ",时间戳,统计数据的导出时间\n";

        file.close();
        LOG_INFO("CSV export completed successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to export CSV: " + std::string(e.what()));
        return false;
    }
}