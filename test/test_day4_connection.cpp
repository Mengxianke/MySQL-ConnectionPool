#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include "connection_pool.h"
#include "pool_config.h"
#include "logger.h"

/**
 * @brief 第4天连接池核心功能测试
 * 
 * 重点验证：
 * 1. 连接池初始化和配置
 * 2. 连接获取和释放机制
 * 3. 并发访问安全性
 * 4. 连接池状态监控
 * 5. 健康检查功能
 * 6. 连接超时处理
 */

// 测试数据库连接参数
const std::string TEST_HOST = "localhost";
const std::string TEST_USER = "mxk";
const std::string TEST_PASSWORD = "d2v8s2q3";
const std::string TEST_DATABASE = "testdb";
const unsigned int TEST_PORT = 3306;

void printTestHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

bool testPoolInitialization() {
    printTestHeader("测试连接池初始化");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        // 测试重复初始化
        std::cout << "1. 测试基本初始化..." << std::endl;
        
        PoolConfig config(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT);
        config.setConnectionLimits(3, 10, 5);  // min=3, max=10, init=5
        config.setTimeouts(3000, 300000, 10000);  // 连接超时3秒，空闲5分钟，健康检查10秒
        
        pool.init(config);
        std::cout << "连接池初始化成功" << std::endl;
        
        // 检查初始状态
        std::cout << "2. 检查初始状态..." << std::endl;
        std::cout << "是否已初始化: " << (pool.isInitialized() ? "是" : "否") << std::endl;
        std::cout << "总连接数: " << pool.getTotalCount() << std::endl;
        std::cout << "空闲连接数: " << pool.getIdleCount() << std::endl;
        std::cout << "活跃连接数: " << pool.getActiveCount() << std::endl;
        
        // 测试重复初始化
        std::cout << "3. 测试重复初始化..." << std::endl;
        pool.init(config);  // 应该被忽略
        std::cout << "重复初始化被正确处理" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testBasicConnectionOperations() {
    printTestHeader("测试基本连接操作");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 测试获取连接..." << std::endl;
        auto conn1 = pool.getConnection();
        if (conn1) {
            std::cout << "成功获取连接: " << conn1->getConnectionId() << std::endl;
        } else {
            std::cout << "获取连接失败" << std::endl;
            return false;
        }
        
        std::cout << "2. 测试连接功能..." << std::endl;
        try {
            auto result = conn1->executeQuery("SELECT 1 as test_value, NOW() as `current_time`");
            if (result->next()) {
                std::cout << "查询执行成功，值: " << result->getInt("test_value") 
                          << ", 时间: " << result->getString("current_time") << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "查询执行失败: " << e.what() << std::endl;
            return false;
        }
        
        std::cout << "3. 检查连接池状态..." << std::endl;
        std::cout << "空闲连接数: " << pool.getIdleCount() << std::endl;
        std::cout << "活跃连接数: " << pool.getActiveCount() << std::endl;
        std::cout << "总连接数: " << pool.getTotalCount() << std::endl;
        
        std::cout << "4. 测试释放连接..." << std::endl;
        pool.releaseConnection(conn1);
        std::cout << "连接释放成功" << std::endl;
        
        std::cout << "5. 检查释放后状态..." << std::endl;
        std::cout << "空闲连接数: " << pool.getIdleCount() << std::endl;
        std::cout << "活跃连接数: " << pool.getActiveCount() << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testMultipleConnections() {
    printTestHeader("测试多连接获取");

    try {
        auto& pool = ConnectionPool::getInstance();
        std::vector<ConnectionPtr> connections;
        
        std::cout << "1. 获取多个连接..." << std::endl;
        
        // 获取多个连接
        for (int i = 0; i < 5; ++i) {
            auto conn = pool.getConnection();
            if (conn) {
                connections.push_back(conn);
                std::cout << "获取连接 " << (i + 1) << ": " << conn->getConnectionId() << std::endl;
            } else {
                std::cout << "获取连接 " << (i + 1) << " 失败" << std::endl;
                return false;
            }
        }
        
        std::cout << "2. 检查连接池状态..." << std::endl;
        std::cout << "空闲连接数: " << pool.getIdleCount() << std::endl;
        std::cout << "活跃连接数: " << pool.getActiveCount() << std::endl;
        std::cout << "总连接数: " << pool.getTotalCount() << std::endl;
        
        std::cout << "3. 测试所有连接功能..." << std::endl;
        for (size_t i = 0; i < connections.size(); ++i) {
            try {
                auto result = connections[i]->executeQuery("SELECT " + std::to_string(i + 1) + " as conn_num");
                if (result->next()) {
                    std::cout << "连接 " << (i + 1) << " 查询成功，返回: " << result->getInt("conn_num") << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "连接 " << (i + 1) << " 查询失败: " << e.what() << std::endl;
            }
        }
        
        std::cout << "4. 释放所有连接..." << std::endl;
        for (auto& conn : connections) {
            pool.releaseConnection(conn);
        }
        connections.clear();
        
        std::cout << "所有连接释放完成" << std::endl;
        std::cout << "最终空闲连接数: " << pool.getIdleCount() << std::endl;
        std::cout << "最终活跃连接数: " << pool.getActiveCount() << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testConcurrentAccess() {
    printTestHeader("测试并发访问");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 启动并发测试..." << std::endl;
        
        const int numThreads = 10;
        const int operationsPerThread = 5;
        std::vector<std::future<bool>> futures;
        
        auto worker = [&pool, operationsPerThread](int threadId) -> bool {
            try {
                for (int i = 0; i < operationsPerThread; ++i) {
                    // 获取连接
                    auto conn = pool.getConnection(2000); // 2秒超时
                    if (!conn) {
                        std::cout << "线程 " << threadId << " 获取连接失败" << std::endl;
                        return false;
                    }
                    
                    // 执行查询
                    auto result = conn->executeQuery("SELECT " + std::to_string(threadId * 100 + i) + " as value");
                    if (!result->next()) {
                        std::cout << "线程 " << threadId << " 查询失败" << std::endl;
                        return false;
                    }
                    
                    // 模拟处理时间
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    
                    // 释放连接
                    pool.releaseConnection(conn);
                }
                
                std::cout << "线程 " << threadId << " 完成所有操作" << std::endl;
                return true;
                
            } catch (const std::exception& e) {
                std::cout << "线程 " << threadId << " 异常: " << e.what() << std::endl;
                return false;
            }
        };
        
        // 启动所有线程
        for (int i = 0; i < numThreads; ++i) {
            futures.push_back(std::async(std::launch::async, worker, i));
        }
        
        // 等待所有线程完成
        bool allSuccess = true;
        for (auto& future : futures) {
            if (!future.get()) {
                allSuccess = false;
            }
        }
        
        std::cout << "2. 并发测试完成..." << std::endl;
        std::cout << "最终连接池状态:" << std::endl;
        std::cout << "  - 空闲连接数: " << pool.getIdleCount() << std::endl;
        std::cout << "  - 活跃连接数: " << pool.getActiveCount() << std::endl;
        std::cout << "  - 总连接数: " << pool.getTotalCount() << std::endl;
        
        return allSuccess;
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testConnectionTimeout() {
    printTestHeader("测试连接超时");

    try {
        auto& pool = ConnectionPool::getInstance();
        std::vector<ConnectionPtr> connections;
        
        std::cout << "1. 获取所有可用连接..." << std::endl;
        
        // 尝试获取所有连接直到达到最大值
        for (int i = 0; i < 15; ++i) {  // 超过最大连接数
            try {
                auto conn = pool.getConnection(100); // 100ms超时
                if (conn) {
                    connections.push_back(conn);
                    std::cout << "获取连接 " << (i + 1) << std::endl;
                } else {
                    break;
                }
            } catch (const std::exception& e) {
                std::cout << "预期的获取连接异常: " << e.what() << std::endl;
                break;
            }
        }
        
        std::cout << "2. 测试超时获取连接..." << std::endl;
        try {
            auto conn = pool.getConnection(200); // 200ms超时，应该失败
            std::cout << "应该超时但获取到了连接" << std::endl;
            return false;
        } catch (const std::exception& e) {
            std::cout << "正确超时: " << e.what() << std::endl;
        }
        
        std::cout << "3. 释放一个连接后重试..." << std::endl;
        if (!connections.empty()) {
            pool.releaseConnection(connections.back());
            connections.pop_back();
            
            try {
                auto conn = pool.getConnection(1000); // 1秒超时
                if (conn) {
                    std::cout << "释放后成功获取连接: " << conn->getConnectionId() << std::endl;
                    connections.push_back(conn);
                }
            } catch (const std::exception& e) {
                std::cout << "释放后仍无法获取连接: " << e.what() << std::endl;
            }
        }
        
        // 清理所有连接
        for (auto& conn : connections) {
            pool.releaseConnection(conn);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testPoolConfiguration() {
    printTestHeader("测试连接池配置");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 获取当前配置..." << std::endl;
        auto config = pool.getConfig();
        
        std::cout << "配置信息:" << std::endl;
        std::cout << "  - 主机: " << config.host << ":" << config.port << std::endl;
        std::cout << "  - 数据库: " << config.database << std::endl;
        std::cout << "  - 最小连接数: " << config.minConnections << std::endl;
        std::cout << "  - 最大连接数: " << config.maxConnections << std::endl;
        std::cout << "  - 初始连接数: " << config.initConnections << std::endl;
        std::cout << "  - 连接超时: " << config.connectionTimeout << "ms" << std::endl;
        std::cout << "  - 最大空闲时间: " << config.maxIdleTime << "ms" << std::endl;
        std::cout << "  - 健康检查周期: " << config.healthCheckPeriod << "ms" << std::endl;
        
        std::cout << "2. 验证配置有效性..." << std::endl;
        if (config.isValid()) {
            std::cout << "配置验证通过" << std::endl;
        } else {
            std::cout << "配置验证失败" << std::endl;
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testErrorHandling() {
    printTestHeader("测试错误处理");

    try {
        std::cout << "1. 测试无效配置..." << std::endl;
        try {
            PoolConfig invalidConfig;
            invalidConfig.minConnections = 10;
            invalidConfig.maxConnections = 5; // 无效：最小大于最大
            
            // 这应该在初始化时抛出异常，但我们已经初始化了
            // 所以测试配置验证
            if (!invalidConfig.isValid()) {
                std::cout << "正确识别无效配置" << std::endl;
            } else {
                std::cout << "未能识别无效配置" << std::endl;
                return false;
            }
        } catch (const std::exception& e) {
            std::cout << "正确处理无效配置异常: " << e.what() << std::endl;
        }
        
        std::cout << "2. 测试释放空连接..." << std::endl;
        auto& pool = ConnectionPool::getInstance();
        pool.releaseConnection(nullptr); // 应该被安全处理
        std::cout << "空连接释放被安全处理" << std::endl;
        
        std::cout << "3. 测试获取连接状态..." << std::endl;
        auto conn = pool.getConnection();
        if (conn) {
            std::cout << "连接ID: " << conn->getConnectionId() << std::endl;
            std::cout << "创建时间: " << conn->getCreationTime() << std::endl;
            std::cout << "最后活动时间: " << conn->getLastActiveTime() << std::endl;
            
            pool.releaseConnection(conn);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testPerformance() {
    printTestHeader("测试性能基准");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 测试连接获取/释放性能..." << std::endl;
        
        const int iterations = 100;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            auto conn = pool.getConnection();
            if (conn) {
                // 执行简单查询
                auto result = conn->executeQuery("SELECT " + std::to_string(i) + " as iteration");
                result->next();
                
                pool.releaseConnection(conn);
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
               // 使用微秒精度计算，但显示毫秒结果
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        double totalMs = duration_us.count() / 1000.0;
        
        std::cout << iterations << " 次连接操作耗时: " << std::fixed << std::setprecision(1) 
                  << totalMs << "ms" << std::endl;
        std::cout << "平均每次操作: " << std::fixed << std::setprecision(3) 
                  << totalMs / iterations << "ms" << std::endl;
        
        std::cout << "2. 测试并发性能..." << std::endl;
        
        const int concurrentThreads = 5;
        const int operationsPerThread = 20;
        
        start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::future<void>> futures;
        for (int t = 0; t < concurrentThreads; ++t) {
            futures.push_back(std::async(std::launch::async, [&pool, operationsPerThread, t]() {
                for (int i = 0; i < operationsPerThread; ++i) {
                    auto conn = pool.getConnection();
                    if (conn) {
                        auto result = conn->executeQuery("SELECT " + std::to_string(t * 100 + i) + " as value");
                        result->next();
                        pool.releaseConnection(conn);
                    }
                }
            }));
        }
        
        for (auto& future : futures) {
            future.wait();
        }
        
        end = std::chrono::high_resolution_clock::now();
        const int totalOperations = concurrentThreads * operationsPerThread; // 100
               // 使用微秒精度计算，但显示毫秒结果
        duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        totalMs = duration_us.count() / 1000.0;
        
        std::cout  << totalOperations << " 次并发操作耗时: " << std::fixed << std::setprecision(1) 
          << totalMs << "ms (" << concurrentThreads << "个线程)" << std::endl;
        std::cout << "平均每次操作: " << std::fixed << std::setprecision(3) 
          << totalMs / totalOperations << "ms" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

void printSummary(const std::vector<std::pair<std::string, bool>>& results) {
    std::cout << "\n" << std::string(60, '*') << std::endl;
    std::cout << "              第4天测试结果总结" << std::endl;
    std::cout << std::string(60, '*') << std::endl;
    
    size_t passed = 0;
    for (const auto& result : results) {
        std::cout << (result.second ? "成功" : "失败") << " " << result.first << std::endl;
        if (result.second) passed++;
    }
    
    std::cout << "\n通过: " << passed << "/" << results.size() << " 项测试" << std::endl;
    
}

int main() {
    std::cout << "开始第4天连接池核心功能测试..." << std::endl;
    std::cout << "连接参数: " << TEST_USER << "@" << TEST_HOST << ":" << TEST_PORT << "/" << TEST_DATABASE << std::endl;
    
    // 初始化日志系统
    Logger::getInstance().init("", LogLevel::INFO, true);
    
    try {
        // 执行测试并收集结果
        std::vector<std::pair<std::string, bool>> results;
        
        results.emplace_back("连接池初始化测试", testPoolInitialization());
        results.emplace_back("基本连接操作测试", testBasicConnectionOperations());
        results.emplace_back("多连接获取测试", testMultipleConnections());
        results.emplace_back("并发访问测试", testConcurrentAccess());
        results.emplace_back("连接超时测试", testConnectionTimeout());
    
        results.emplace_back("连接池配置测试", testPoolConfiguration());
        results.emplace_back("错误处理测试", testErrorHandling());
        results.emplace_back("性能基准测试", testPerformance());
        
        // 显示测试结果
        printSummary(results);
        
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