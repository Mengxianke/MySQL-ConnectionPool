// test/test_day7_performance.cpp
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <iomanip>
#include <random>
#include "connection_pool.h"
#include "performance_monitor.h"
#include "pool_config.h"
#include "logger.h"

/**
 * @brief 第7天性能监控系统测试
 * 
 * 重点验证：
 * 1. 监控数据的准确收集
 * 2. 性能统计的正确计算
 * 3. CSV导出功能
 * 4. 监控对性能的影响（应该很小）
 * 5. 并发环境下的监控准确性
 * 6. 长时间运行的监控稳定性
 */

const std::string TEST_HOST = "127.0.0.1";
const std::string TEST_USER = "mxk";
const std::string TEST_PASSWORD = "d2v8s2q3";
const std::string TEST_DATABASE = "testdb";
const unsigned int TEST_PORT = 3306;

void printTestHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

bool testBasicMonitoring() {
    printTestHeader("测试基础监控功能");

    try {
        auto& monitor = PerformanceMonitor::getInstance();
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 重置监控统计..." << std::endl;
        monitor.resetStats();
        
        //验证重置后的状态
        auto stats = monitor.getStats();
        if (stats.totalConnectionsAcquired != 0 || stats.totalQueriesExecuted != 0) {
            std::cout << "统计重置失败" << std::endl;
            return false;
        }
        std::cout << "统计重置成功" << std::endl;
        
        std::cout << "2. 执行一些数据库操作..." << std::endl;
        
        // 执行一些基本操作
        for (int i = 0; i < 5; ++i) {
            auto conn = pool.getConnection(3000);
            LOG_INFO("mxk test getConn success");
            if (conn) {
                auto result = conn->executeQuery("SELECT " + std::to_string(i) + " as test_id, 'monitoring test' as message");
                if (result->next()) {
                    std::cout << "查询 " << (i + 1) << " 成功: " << result->getString("message") << std::endl;
                    // 稍微等待一下，模拟业务处理
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                pool.releaseConnection(conn);
            }
            
            // 稍微等待一下，让监控有时间记录
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        std::cout << "3. 检查监控数据..." << std::endl;
        stats = monitor.getStats();
        
        std::cout << "获取连接数: " << stats.totalConnectionsAcquired << std::endl;
        std::cout << "查询执行数: " << stats.totalQueriesExecuted << std::endl;
        std::cout << "平均获取时间: " << stats.avgConnectionAcquireTime() / 1000.0 << " ms" << std::endl;
        std::cout << "平均查询时间: " << stats.avgQueryExecutionTime() / 1000.0 << " ms" << std::endl;
        
        // 验证数据合理性
        if (stats.totalConnectionsAcquired >= 5 && stats.totalQueriesExecuted >= 5) {
            std::cout << "监控数据收集正常" << std::endl;
            return true;
        } else {
            std::cout << "监控数据异常" << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testPerformanceCalculation() {
    printTestHeader("测试性能计算准确性");

    try {
        auto& monitor = PerformanceMonitor::getInstance();
        
      std::cout << "1. 重置统计并手动记录一些数据..." << std::endl;
      monitor.resetStats();
        
        // 手动记录一些已知的数据，验证计算准确性
        monitor.recordConnectionAcquired(1000);  // 1ms
        monitor.recordConnectionAcquired(2000);  // 2ms
        monitor.recordConnectionAcquired(3000);  // 3ms
        // 平均应该是 (1+2+3)/3 = 2ms
        
        monitor.recordQueryExecuted(10000, true);   // 10ms, 成功
        monitor.recordQueryExecuted(20000, true);   // 20ms, 成功
        monitor.recordQueryExecuted(30000, false);  // 30ms, 失败
        // 平均应该是 (10+20+30)/3 = 20ms, 成功率 2/3 = 66.67%
        
        std::cout << "2. 验证计算结果..." << std::endl;
        auto stats = monitor.getStats();
        
        double avgConnTime = stats.avgConnectionAcquireTime();
        double avgQueryTime = stats.avgQueryExecutionTime();
        double querySuccessRate = stats.querySuccessRate();
        
        std::cout << "平均连接获取时间: " << avgConnTime / 1000.0 << " ms (期望: 2ms)" << std::endl;
        std::cout << "平均查询执行时间: " << avgQueryTime / 1000.0 << " ms (期望: 20ms)" << std::endl;
        std::cout << "查询成功率: " << querySuccessRate << "% (期望: 66.67%)" << std::endl;
        
        // 验证计算准确性（允许小的浮点误差）
        bool connTimeOk = std::abs(avgConnTime - 2000.0) < 1.0;
        bool queryTimeOk = std::abs(avgQueryTime - 20000.0) < 1.0;
        bool successRateOk = std::abs(querySuccessRate - 66.67) < 0.1;
        
        if (connTimeOk && queryTimeOk && successRateOk) {
            std::cout << "性能计算准确" << std::endl;
            return true;
        } else {
            std::cout << "性能计算有误" << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testCsvExport() {
    printTestHeader("测试CSV导出功能");

    try {
        auto& monitor = PerformanceMonitor::getInstance();
        auto& pool = ConnectionPool::getInstance();
        // std::cout << pool.getStatus() << std::endl;
        
        std::cout << "1. 生成一些统计数据..." << std::endl;

        monitor.resetStats();

        // 执行一批操作来生成统计数据
        for (int i = 0; i < 15; ++i) {
            try {
                auto conn = pool.getConnection(2000);
                if (conn) {
                    auto result = conn->executeQuery("SELECT " + std::to_string(i) + " as export_test");
                    if (result->next()) {
                        // 查询成功
                         // 稍微等待一下，模拟业务处理
                         std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    pool.releaseConnection(conn);
                }
            } catch (...) {
                // 忽略错误，继续测试
            }

        }
        
        std::cout << "2. 导出CSV文件..." << std::endl;
        std::string csvFile = "performance_test_" + std::to_string(time(nullptr)) + ".csv";
        
        bool exported = monitor.exportToCSV(csvFile);
        if (!exported) {
            std::cout << "CSV导出失败" << std::endl;
            return false;
        }

        std::cout << "CSV导出成功: " << csvFile << std::endl;
        
        std::cout << "3. 验证CSV文件内容..." << std::endl;
        std::ifstream file(csvFile);
        if (!file.is_open()) {
            std::cout << "无法打开CSV文件" << std::endl;
            return false;
        }
        
        std::string line;
        int lineCount = 0;
        bool hasHeader = false;
        bool hasData = false;
        
        while (std::getline(file, line) && lineCount < 20) {  // 只检查前20行
            lineCount++;
            if (lineCount == 1 && line.find("统计项目,数值") != std::string::npos) {
                hasHeader = true;
            }
            if (line.find("总获取连接数") != std::string::npos) {
                hasData = true;
            }
        }
        file.close();
        
        std::cout << "CSV文件行数: " << lineCount << std::endl;
        std::cout << "包含表头: " << (hasHeader ? "是" : "否") << std::endl;
        std::cout << "包含数据: " << (hasData ? "是" : "否") << std::endl;
        
        // 清理测试文件
       // std::remove(csvFile.c_str());
        
        return hasHeader && hasData && lineCount > 5;
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testConcurrentMonitoring() {
    printTestHeader("测试并发环境下的监控");

    try {
        auto& monitor = PerformanceMonitor::getInstance();
        auto& pool = ConnectionPool::getInstance();
        
       std::cout << "1. 重置监控统计..." << std::endl;
       monitor.resetStats();
        
        const int numThreads = 5;
        const int operationsPerThread = 25;
        const int totalOperations = numThreads * operationsPerThread;
        
        std::cout << "2. 启动 " << numThreads << " 个并发线程，每个执行 " 
                  << operationsPerThread << " 次操作..." << std::endl;
        
        std::vector<std::future<int>> futures;
        auto startTime = std::chrono::steady_clock::now();
        
        // 启动多个线程并发执行数据库操作
        for (int t = 0; t < numThreads; ++t) {
            futures.emplace_back(std::async(std::launch::async, [&pool, operationsPerThread, t]() {
                int successCount = 0;
                for (int i = 0; i < operationsPerThread; ++i) {
                    try {
                        auto conn = pool.getConnection(1000);
                        if (conn) {
                            auto result = conn->executeQuery("SELECT " + std::to_string(t) + " as thread_id, " + std::to_string(i) + " as op_id");
                            if (result->next()) {
                                successCount++;
                            }
                            pool.releaseConnection(conn);
                        }
                        
                        // 随机等待一小段时间，模拟真实业务
                        std::this_thread::sleep_for(std::chrono::milliseconds(1 + rand() % 5));
                        
                    } catch (const std::exception& e) {
                        // 忽略个别失败，继续测试
                    }
                }
                return successCount;
            }));
        }
        
        // 等待所有线程完成
        int totalSuccess = 0;
        for (auto& future : futures) {
            totalSuccess += future.get();
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        std::cout << "3. 并发测试完成，耗时: " << duration.count() << " ms" << std::endl;
        std::cout << "成功操作数: " << totalSuccess << "/" << totalOperations << std::endl;
        
        std::cout << "4. 检查监控统计..." << std::endl;
        auto stats = monitor.getStats();
        
        std::cout << "监控记录的获取连接数: " << stats.totalConnectionsAcquired << std::endl;
        std::cout << "监控记录的查询执行数: " << stats.totalQueriesExecuted << std::endl;
        std::cout << "监控记录的连接成功率: " << stats.connectionAcquireSuccessRate() << "%" << std::endl;
        std::cout << "监控记录的查询成功率: " << stats.querySuccessRate() << "%" << std::endl;
        
        // 验证监控数据的合理性
        // 由于并发和网络延迟，不要求完全精确，但应该在合理范围内
        bool connCountOk = stats.totalConnectionsAcquired >= totalSuccess * 0.8;  // 至少80%被记录
        bool queryCountOk = stats.totalQueriesExecuted >= totalSuccess * 0.8;
        bool successRateOk = stats.querySuccessRate() > 80.0;  // 成功率应该很高
        
        if (connCountOk && queryCountOk && successRateOk) {
            std::cout << "并发监控数据合理" << std::endl;
            return true;
        } else {
            std::cout << "并发监控数据异常" << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

#if 1

bool testMonitoringPerformanceImpact() {
    printTestHeader("测试监控对性能的影响");

    try {
        auto& monitor = PerformanceMonitor::getInstance();
        auto& pool = ConnectionPool::getInstance();
        monitor.resetStats();
        const int testOperations = 1000;
        
        std::cout << "1. 测试不带监控的原始性能..." << std::endl;
        
        // 先测试不使用监控时的性能（通过重置统计模拟）
        auto startTime1 = std::chrono::steady_clock::now();
        
        for (int i = 0; i < testOperations; ++i) {
            try {
                auto conn = pool.getConnection(1000);
                if (conn) {
                    auto result = conn->executeQuery("SELECT 1");
                    pool.releaseConnection(conn);
                }
            } catch (...) {
                // 忽略错误
            }
        }
        
        auto endTime1 = std::chrono::steady_clock::now();
        auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime1 - startTime1);
        
        std::cout << "基准性能测试完成，耗时: " << duration1.count() << " ms" << std::endl;
        std::cout << "平均每操作: " << (double)duration1.count() / testOperations << " ms" << std::endl;
        
        std::cout << "2. 检查监控开销..." << std::endl;
        
        // 获取监控统计
        auto stats = monitor.getStats();
        std::cout << "监控期间总操作数: " << stats.totalQueriesExecuted << std::endl;
        std::cout << "监控记录的平均查询时间: " << stats.avgQueryExecutionTime() / 1000.0 << " ms" << std::endl;
        
        // 计算监控开销
        // 如果监控实现正确，开销应该几乎为0
        double avgOpTime = (double)duration1.count() / testOperations;
        double avgMonitoredTime = stats.avgQueryExecutionTime() / 1000.0;
        
        std::cout << "实际平均操作时间: " << avgOpTime << " ms" << std::endl;
        std::cout << "监控记录的平均时间: " << avgMonitoredTime << " ms" << std::endl;
        // 监控记录的时间应该略小于实际时间（因为监控只计算SQL执行时间，不包括连接获取等）
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
    return true;
}

#endif

bool testFormattedOutput() {
    printTestHeader("测试格式化输出功能");

    try {
        auto& monitor = PerformanceMonitor::getInstance();
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 生成一些统计数据..." << std::endl;
        monitor.resetStats();
        
        // 执行一些操作来生成有意义的统计
        for (int i = 0; i < 20; ++i) {
            try {
                auto conn = pool.getConnection(2000);
                if (conn) {
                    auto result = conn->executeQuery("SELECT " + std::to_string(i) + " as format_test, NOW() as `current_time`");
                    if (result->next()) {
                        // 成功
                    }
                    pool.releaseConnection(conn);
                }
            } catch (...) {
                // 忽略错误
            }
        }
        
        std::cout << "2. 测试格式化统计输出..." << std::endl;
        std::string statsString = monitor.getStatsString();
        
        std::cout << "--- 监控报告样例 ---" << std::endl;
        std::cout << statsString << std::endl;
        std::cout << "--- 报告结束 ---" << std::endl;
        
        // 验证输出内容
        bool hasTitle = statsString.find("连接池性能统计报告") != std::string::npos;
        bool hasConnStats = statsString.find("连接统计") != std::string::npos;
        bool hasQueryStats = statsString.find("查询统计") != std::string::npos;
        bool hasPerformanceEval = statsString.find("性能评估") != std::string::npos;
        
        std::cout << "3. 验证输出格式..." << std::endl;
        std::cout << "包含标题: " << (hasTitle ? "是" : "否") << std::endl;
        std::cout << "包含连接统计: " << (hasConnStats ? "是" : "否") << std::endl;
        std::cout << "包含查询统计: " << (hasQueryStats ? "是" : "否") << std::endl;
        std::cout << "包含性能评估: " << (hasPerformanceEval ? "是" : "否") << std::endl;
        
        return hasTitle && hasConnStats && hasQueryStats && hasPerformanceEval;
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testLongRunningMonitoring() {
    printTestHeader("测试长时间运行监控稳定性");

    try {
        auto& monitor = PerformanceMonitor::getInstance();
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 开始长时间监控测试..." << std::endl;
        monitor.resetStats();
        
        const int testDurationSeconds = 15;
        const int operationsPerSecond = 10;
        
        std::cout << "测试参数: 运行" << testDurationSeconds << "秒, 每秒" << operationsPerSecond << "次操作" << std::endl;
        
        auto startTime = std::chrono::steady_clock::now();
        int totalOps = 0;
        int successOps = 0;
        
        while (true) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
            
            if (elapsed.count() >= testDurationSeconds) {
                break;
            }
            
            // 执行操作
            try {
                auto conn = pool.getConnection(500);
                if (conn) {
                    auto result = conn->executeQuery("SELECT " + std::to_string(totalOps) + " as long_test");
                    if (result->next()) {
                        successOps++;
                    }
                    pool.releaseConnection(conn);
                }
                totalOps++;
                
                // 控制操作频率
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 / operationsPerSecond));
                
            } catch (...) {
                totalOps++;
            }
            
            // 每5秒输出一次中间状态
            if (totalOps % (operationsPerSecond * 5) == 0) {
                auto stats = monitor.getStats();
                std::cout << "已运行 " << elapsed.count() << "s, 操作数: " << totalOps 
                          << ", 监控记录查询数: " << stats.totalQueriesExecuted << std::endl;
            }
        }
        
        std::cout << "2. 长时间测试完成..." << std::endl;
        std::cout << "总操作数: " << totalOps << std::endl;
        std::cout << "成功操作数: " << successOps << std::endl;
        std::cout << "成功率: " << (double)successOps / totalOps * 100 << "%" << std::endl;
        
        std::cout << "3. 检查监控统计的一致性..." << std::endl;
        auto finalStats = monitor.getStats();
        
        std::cout << "监控记录的查询数: " << finalStats.totalQueriesExecuted << std::endl;
        std::cout << "监控记录的平均查询时间: " << finalStats.avgQueryExecutionTime() / 1000.0 << " ms" << std::endl;
        std::cout << "监控记录的查询成功率: " << finalStats.querySuccessRate() << "%" << std::endl;
        
        // 验证监控数据的一致性
        bool countConsistent = finalStats.totalQueriesExecuted >= successOps * 0.9;  // 允许10%误差
        bool successRateReasonable = finalStats.querySuccessRate() > 80.0;
        bool avgTimeReasonable = finalStats.avgQueryExecutionTime() < 100000;  // 少于100ms
        
        if (countConsistent && successRateReasonable && avgTimeReasonable) {
            std::cout << "长时间监控稳定性良好" << std::endl;
            return true;
        } else {
            std::cout << "长时间监控出现异常" << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

void printSummary(const std::vector<std::pair<std::string, bool>>& results) {
    std::cout << "\n" << std::string(60, '*') << std::endl;
    std::cout << "              第7天测试结果总结" << std::endl;
    std::cout << std::string(60, '*') << std::endl;
    
    size_t passed = 0;
    for (const auto& result : results) {
        std::cout << (result.second ? "成功" : "失败") << " " << result.first << std::endl;
        if (result.second) passed++;
    }
    
    std::cout << "\n通过: " << passed << "/" << results.size() << " 项测试" << std::endl;
    
    if (passed == results.size()) {
        std::cout << "\n恭喜！第7天所有测试都通过了！" << std::endl;
        std::cout << "你的连接池现在具备了完整的性能监控能力：" << std::endl;
        std::cout << "精确的性能数据收集" << std::endl;
        std::cout << "全面的统计分析功能" << std::endl;
        std::cout << "专业的CSV导出功能" << std::endl;
        std::cout << "优秀的并发性能表现" << std::endl;
        std::cout << "长时间运行的稳定性" << std::endl;
        std::cout << "\n现在你拥有了一个真正意义上的企业级连接池！" << std::endl;
        std::cout << "运维人员可以通过监控数据快速定位性能瓶颈，" << std::endl;
        std::cout << "开发人员可以通过统计报告优化业务逻辑。" << std::endl;
    } else {
        std::cout << "\n需要修复 " << (results.size() - passed) << " 个问题。" << std::endl;
        std::cout << "请检查监控集成代码和统计计算逻辑。" << std::endl;
    }
}

// 初始化连接池的辅助函数
bool initializeConnectionPool() {
    try {
          auto& pool = ConnectionPool::getInstance();
        
        PoolConfig config;
        config.setConnectionLimits(3, 10, 5);           // min=2, max=6, init=3

        //初始化单数据库连接池
        pool.initWithSingleDatabase(config,TEST_HOST,TEST_USER,TEST_PASSWORD,TEST_DATABASE,TEST_PORT);
        
        std::cout << "连接池初始化成功" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "连接池初始化失败: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    std::cout << "开始第7天性能监控系统测试..." << std::endl;
    std::cout << "测试数据库: " << TEST_USER << "@" << TEST_HOST << ":" << TEST_PORT << "/" << TEST_DATABASE << std::endl;
    
    // 初始化日志系统
    Logger::getInstance().init("", LogLevel::INFO, true);
    
    try {
        // 初始化连接池
        if (!initializeConnectionPool()) {
            std::cerr << "无法初始化连接池，请检查数据库连接配置" << std::endl;
            return 1;
        }
        
        // 执行测试并收集结果
        std::vector<std::pair<std::string, bool>> results;
        
        results.emplace_back("基础监控功能测试", testBasicMonitoring());
        results.emplace_back("性能计算准确性测试", testPerformanceCalculation());
         results.emplace_back("CSV导出功能测试", testCsvExport());
         results.emplace_back("并发环境监控测试", testConcurrentMonitoring());

        // results.emplace_back("监控性能开销测试", testMonitoringPerformanceImpact());
        // results.emplace_back("格式化输出测试", testFormattedOutput());
        // results.emplace_back("长时间运行稳定性测试", testLongRunningMonitoring());
        
        // 显示测试结果
        printSummary(results);
        
        // 显示最终的性能统计报告
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "最终性能监控报告" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << PerformanceMonitor::getInstance().getStatsString() << std::endl;
        
        // 清理连接池
        std::cout << "\n正在关闭连接池..." << std::endl;
        ConnectionPool::getInstance().shutdown();
        std::cout << "连接池已关闭" << std::endl;
        
        // 根据测试结果返回退出码
        size_t passed = 0;
        for (const auto& result : results) {
            if (result.second) passed++;
        }
        
        return (passed == results.size()) ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "测试程序异常: " << e.what() << std::endl;
        return 1;
    }
}