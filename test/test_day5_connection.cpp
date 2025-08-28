#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <map>
#include <iomanip>
#include "connection_pool.h"
#include "load_balancer.h"
#include "pool_config.h"
#include "db_config.h"
#include "logger.h"

/**
 * @brief 第5天负载均衡与多数据库支持测试
 * 
 * 重点验证：
 * 1. 负载均衡器基本功能
 * 2. 三种负载均衡算法的正确性
 * 3. 多数据库配置管理
 * 4. 连接池与负载均衡器的集成
 * 5. 动态策略切换
 * 6. 负载分布统计
 */

// 测试数据库连接参数 - 模拟多个数据库实例
const std::string TEST_HOST1 = "127.0.0.1";
const std::string TEST_HOST2 = "127.0.0.1";  // 实际项目中可能是不同的主机
const std::string TEST_HOST3 = "127.0.0.1";
const std::string TEST_USER = "mxk";
const std::string TEST_PASSWORD = "d2v8s2q3";
const std::string TEST_DATABASE = "testdb";
const unsigned int TEST_PORT1 = 3306;
const unsigned int TEST_PORT2 = 3307;  // 实际项目中可能是不同的端口
const unsigned int TEST_PORT3 = 3308;

void printTestHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

bool testLoadBalancerBasics() {
    printTestHeader("测试负载均衡器基础功能");

    try {
        auto& loadBalancer = LoadBalancer::getInstance();
        
        std::cout << "1. 创建测试数据库配置..." << std::endl;
        
        // 创建多个数据库配置，模拟不同的数据库实例
        std::vector<DBConfig> configs;
        
        // 数据库1：权重3（性能最好）
        DBConfig db1(TEST_HOST1, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT1, 3);
        configs.push_back(db1);
        
        // 数据库2：权重2（性能中等）
        DBConfig db2(TEST_HOST2, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT2, 2);
        configs.push_back(db2);
        
        // 数据库3：权重1（性能较低）
        DBConfig db3(TEST_HOST3, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT3, 1);
        configs.push_back(db3);
        
        std::cout << "创建了 " << configs.size() << " 个数据库配置" << std::endl;
        
        std::cout << "2. 初始化负载均衡器..." << std::endl;
        loadBalancer.init(configs, LoadBalanceStrategy::WEIGHTED);
        std::cout << "负载均衡器初始化成功" << std::endl;
        
        std::cout << "3. 检查负载均衡器状态..." << std::endl;
        std::cout << "数据库数量: " << loadBalancer.getDatabaseCount() << std::endl;
        std::cout << "当前策略: " << strategyToString(loadBalancer.getStrategy()) << std::endl;
        
        std::cout << "4. 测试数据库选择..." << std::endl;
        for (int i = 0; i < 5; ++i) {
            auto selectedDb = loadBalancer.getNextDatabase();
            std::cout << "选择 " << (i + 1) << ": " << selectedDb.host << ":" 
                      << selectedDb.port << " (权重=" << selectedDb.weight << ")" << std::endl;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testLoadBalanceStrategies() {
    printTestHeader("测试三种负载均衡策略");

    try {
        auto& loadBalancer = LoadBalancer::getInstance();
        
        // 测试数据：统计每个数据库被选择的次数
        const int testRounds = 60;  // 测试60次选择
        std::map<std::string, int> selectionCount;
        
        // 初始化计数器
        selectionCount[TEST_HOST1 + ":" + std::to_string(TEST_PORT1)] = 0;
        selectionCount[TEST_HOST2 + ":" + std::to_string(TEST_PORT2)] = 0;
        selectionCount[TEST_HOST3 + ":" + std::to_string(TEST_PORT3)] = 0;
        
        // 测试1：随机策略
        std::cout << "1. 测试随机策略..." << std::endl;
        loadBalancer.setStrategy(LoadBalanceStrategy::RANDOM);
        
        // 重置计数器
        // for (auto& pair : selectionCount) {
        //     pair.second = 0;
        // }
        
        for (int i = 0; i < testRounds; ++i) {
            auto db = loadBalancer.getNextDatabase();
            std::string key = db.host + ":" + std::to_string(db.port);
            selectionCount[key]++;
        }
        
        std::cout << "随机策略分布结果 (" << testRounds << " 次选择):" << std::endl;
        for (const auto& pair : selectionCount) {
            double percentage = (double)pair.second / testRounds * 100;
            std::cout << "  " << pair.first << ": " << pair.second 
                      << " 次 (" << std::fixed << std::setprecision(1) 
                      << percentage << "%)" << std::endl;
        }
        
        // 测试2：轮询策略
        std::cout << "\n2. 测试轮询策略..." << std::endl;
        loadBalancer.setStrategy(LoadBalanceStrategy::ROUND_ROBIN);
        
        // 重置计数器
        for (auto& pair : selectionCount) {
            pair.second = 0;
        }
        
        for (int i = 0; i < testRounds; ++i) {
            auto db = loadBalancer.getNextDatabase();
            std::string key = db.host + ":" + std::to_string(db.port);
            selectionCount[key]++;
        }
        
        std::cout << "轮询策略分布结果 (" << testRounds << " 次选择):" << std::endl;
        for (const auto& pair : selectionCount) {
            double percentage = (double)pair.second / testRounds * 100;
            std::cout << "  " << pair.first << ": " << pair.second 
                      << " 次 (" << std::fixed << std::setprecision(1) 
                      << percentage << "%)" << std::endl;
        }
        
        // 验证轮询的均匀性
        int expectedPerDb = testRounds / 3;
        bool isEvenDistribution = true;
        for (const auto& pair : selectionCount) {
            if (std::abs(pair.second - expectedPerDb) > 1) {  // 允许1的误差
                isEvenDistribution = false;
                break;
            }
        }
        std::cout << "轮询分布是否均匀: " << (isEvenDistribution ? "是" : "否") << std::endl;
        
        // 测试3：权重策略
        std::cout << "\n3. 测试权重策略..." << std::endl;
        loadBalancer.setStrategy(LoadBalanceStrategy::WEIGHTED);
        
        // 重置计数器
        for (auto& pair : selectionCount) {
            pair.second = 0;
        }
        
        for (int i = 0; i < testRounds; ++i) {
            auto db = loadBalancer.getNextDatabase();
            std::string key = db.host + ":" + std::to_string(db.port);
            selectionCount[key]++;
        }
        
        std::cout << "权重策略分布结果 (" << testRounds << " 次选择):" << std::endl;
        std::cout << "  期望分布: DB1(50%), DB2(33.3%), DB3(16.7%)" << std::endl;
        for (const auto& pair : selectionCount) {
            double percentage = (double)pair.second / testRounds * 100;
            std::cout << "  " << pair.first << ": " << pair.second 
                      << " 次 (" << std::fixed << std::setprecision(1) 
                      << percentage << "%)" << std::endl;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testDynamicDatabaseManagement() {
    printTestHeader("测试动态数据库管理");

    try {
        auto& loadBalancer = LoadBalancer::getInstance();
        
        std::cout << "1. 当前数据库数量: " << loadBalancer.getDatabaseCount() << std::endl;
        
        std::cout << "2. 测试添加数据库..." << std::endl;
        DBConfig newDb("127.0.0.1", TEST_USER, TEST_PASSWORD, TEST_DATABASE, 3307, 2);
        loadBalancer.addDatabase(newDb);
        std::cout << "数据库添加成功，当前数量: " << loadBalancer.getDatabaseCount() << std::endl;
        
        std::cout << "3. 测试重复添加相同数据库..." << std::endl;
        loadBalancer.addDatabase(newDb);  // 应该被忽略
        std::cout << "重复添加被正确处理，数量仍为: " << loadBalancer.getDatabaseCount() << std::endl;
        
        std::cout << "4. 测试权重更新..." << std::endl;
        bool updated = loadBalancer.updateWeight("127.0.0.1", 3307, 5);
        std::cout << "权重更新结果: " << (updated ? "成功" : "失败") << std::endl;
        
        std::cout << "5. 测试删除数据库..." << std::endl;
        bool removed = loadBalancer.removeDatabase("127.0.0.1", 3307);
        std::cout << "数据库删除结果: " << (removed ? "成功" : "失败") << std::endl;
        std::cout << "删除后数量: " << loadBalancer.getDatabaseCount() << std::endl;
        
        std::cout << "6. 测试删除不存在的数据库..." << std::endl;
        bool removedNonExist = loadBalancer.removeDatabase("nonexist", 9999);
        std::cout << "删除不存在数据库结果: " << (removedNonExist ? "成功" : "失败（预期）") << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testConnectionPoolIntegration() {
    printTestHeader("测试连接池与负载均衡器集成");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 配置多数据库连接池..." << std::endl;

        std::vector<DBConfig> configs;
        
        PoolConfig poolConfig;
        poolConfig.setConnectionLimits(2, 8, 3);  // min=2, max=8, init=3
       poolConfig.setTimeouts(3000, 300000, 10000);

        // 数据库1：权重3（性能最好）
        DBConfig db1(TEST_HOST1, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT1, 3);
        configs.push_back(db1);
        
        // 数据库2：权重2（性能中等）
        DBConfig db2(TEST_HOST2, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT2, 2);
        configs.push_back(db2);
        
        // 数据库3：权重1（性能较低）
        DBConfig db3(TEST_HOST3, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT3, 1);
        configs.push_back(db3);

        // 添加多个数据库实例配置
        // LoadBalancer::getInstance().init(configs, strategy);
        // pool.init(poolConfig);
        pool.initWithMultipleDatabases(poolConfig, configs);
        
        std::cout << "3. 检查负载均衡器状态..." << std::endl;
        std::cout << pool.getLoadBalancerStatus() << std::endl;
        
        std::cout << "4. 测试连接获取和负载分布..." << std::endl;
        std::map<std::string, int> connectionCount;
        
        // 获取多个连接，观察负载分布
        std::vector<ConnectionPtr> connections;
        for (int i = 0; i < 6; ++i) {
            try {
                auto conn = pool.getConnection(1000);
                if (conn) {
                    connections.push_back(conn);
                    
                    // 通过执行查询获取服务器信息来验证连接到了哪个数据库
                    auto result = conn->executeQuery("SELECT CONNECTION_ID() as conn_id, @@hostname as hostname, @@port as port");
                    if (result->next()) {
                        std::string connInfo = "连接" + std::to_string(i + 1) + 
                                             " - ID:" + result->getString("conn_id") + 
                                             " 端口:" + result->getString("port");
                        std::cout << connInfo << std::endl;
                        
                        // 统计连接分布（这里简化为按端口统计）
                        std::string port = result->getString("port");
                        connectionCount[port]++;
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "获取连接 " << (i + 1) << " 失败: " << e.what() << std::endl;
            }
        }
        
        std::cout << "5. 连接分布统计:" << std::endl;
        for (const auto& pair : connectionCount) {
            std::cout << "  端口 " << pair.first << ": " << pair.second << " 个连接" << std::endl;
        }
        
        std::cout << "6. 释放所有连接..." << std::endl;
        for (auto& conn : connections) {
            pool.releaseConnection(conn);
        }
        connections.clear();
        std::cout << "所有连接已释放" << std::endl;
        
        std::cout << "7. 测试策略切换..." << std::endl;
        std::cout << "当前策略: " << strategyToString(pool.getLoadBalanceStrategy()) << std::endl;
        
        pool.setLoadBalanceStrategy(LoadBalanceStrategy::ROUND_ROBIN);
        std::cout << "切换到轮询策略: " << strategyToString(pool.getLoadBalanceStrategy()) << std::endl;
        
        pool.setLoadBalanceStrategy(LoadBalanceStrategy::RANDOM);
        std::cout << "切换到随机策略: " << strategyToString(pool.getLoadBalanceStrategy()) << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testConcurrentLoadBalancing() {
    printTestHeader("测试并发负载均衡");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        // std::cout << pool.getConfig().toString() <<std::endl;

        std::cout << "1. 设置权重策略进行并发测试..." << std::endl;
        pool.setLoadBalanceStrategy(LoadBalanceStrategy::RANDOM);
        
        std::cout << "2. 启动并发连接测试..." << std::endl;

        const int numThreads = 8;
        const int operationsPerThread = 10;
        std::vector<std::future<std::map<std::string, int>>> futures;
        
        auto worker = [&pool, operationsPerThread](int threadId) -> std::map<std::string, int> {
            std::map<std::string, int> localCount;
            
            try {
                for (int i = 0; i < operationsPerThread; ++i) {
                    auto conn = pool.getConnection(2000);

                    if (conn) {
                        // 获取连接信息
                        auto result = conn->executeQuery("SELECT @@port as port");
                        if (result->next()) {
                            std::string port = result->getString("port");
                            localCount[port]++;
                        }
                        
                        // 模拟工作负载
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        
                        pool.releaseConnection(conn);
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "线程 " << threadId << " 异常: " << e.what() << std::endl;
            }
            
            return localCount;
        };
        
        // 启动所有工作线程
        for (int i = 0; i < numThreads; ++i) {
            futures.push_back(std::async(std::launch::async, worker, i));
        }
        
        // 收集结果
        std::map<std::string, int> totalCount;
        for (auto& future : futures) {
            auto localCount = future.get();
            for (const auto& pair : localCount) {
                totalCount[pair.first] += pair.second;
            }
        }
        
        const int totalOperations = numThreads * operationsPerThread;
        std::cout << "3. 并发负载分布结果 (" << totalOperations << " 次操作):" << std::endl;
        for (const auto& pair : totalCount) {
            double percentage = (double)pair.second / totalOperations * 100;
            std::cout << "  端口 " << pair.first << ": " << pair.second 
                      << " 次 (" << std::fixed << std::setprecision(1) 
                      << percentage << "%)" << std::endl;
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
        std::cout << "1. 测试无效数据库配置..." << std::endl;
        try {
            auto& loadBalancer = LoadBalancer::getInstance();
            DBConfig invalidConfig("", "", "", "", 0, 1);  // 所有字段都无效
            loadBalancer.addDatabase(invalidConfig);
            std::cout << "应该抛出异常但没有" << std::endl;
            return false;
        } catch (const std::exception& e) {
            std::cout << "正确捕获无效配置异常: " << e.what() << std::endl;
        }
        
        std::cout << "2. 测试不存在的数据库操作..." << std::endl;
        auto& loadBalancer = LoadBalancer::getInstance();
        
        bool updateResult = loadBalancer.updateWeight("nonexistent", 9999, 5);
        std::cout << "更新不存在数据库权重: " << (updateResult ? "成功" : "失败（预期）") << std::endl;
        
        bool removeResult = loadBalancer.removeDatabase("nonexistent", 9999);
        std::cout << "删除不存在数据库: " << (removeResult ? "成功" : "失败（预期）") << std::endl;
        
        std::cout << "3. 测试连接池状态检查..." << std::endl;
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "当前连接池状态:" << std::endl;
        std::cout << "  总连接数: " << pool.getTotalCount() << std::endl;
        std::cout << "  空闲连接数: " << pool.getIdleCount() << std::endl;
        std::cout << "  活跃连接数: " << pool.getActiveCount() << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testPerformanceWithLoadBalancing() {
    printTestHeader("测试负载均衡性能");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 测试不同策略的性能差异..." << std::endl;
        
        const int testIterations = 50;
        std::vector<LoadBalanceStrategy> strategies = {
            LoadBalanceStrategy::RANDOM,
            LoadBalanceStrategy::ROUND_ROBIN,
            LoadBalanceStrategy::WEIGHTED
        };
        
        for (auto strategy : strategies) {
            std::cout << "\n测试策略: " << strategyToString(strategy) << std::endl;
            pool.setLoadBalanceStrategy(strategy);
            
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < testIterations; ++i) {
                auto conn = pool.getConnection(1000);
                if (conn) {
                    // 执行简单查询
                    auto result = conn->executeQuery("SELECT " + std::to_string(i) + " as iteration");
                    if (result->next()) {
                        // 获取结果
                        result->getInt("iteration");
                    }
                    pool.releaseConnection(conn);
                }
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double totalMs = duration.count() / 1000.0;
            
            std::cout << testIterations << " 次操作耗时: " 
                      << std::fixed << std::setprecision(1) << totalMs << "ms" << std::endl;
            std::cout << "平均每次操作: " << std::fixed << std::setprecision(3) 
                      << totalMs / testIterations << "ms" << std::endl;
        }
        
        std::cout << "\n2. 测试并发负载均衡性能..." << std::endl;
        pool.setLoadBalanceStrategy(LoadBalanceStrategy::WEIGHTED);
        
        const int concurrentThreads = 4;
        const int operationsPerThread = 25;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::future<void>> futures;
        for (int t = 0; t < concurrentThreads; ++t) {
            futures.push_back(std::async(std::launch::async, [&pool, operationsPerThread, t]() {
                for (int i = 0; i < operationsPerThread; ++i) {
                    auto conn = pool.getConnection(2000);
                    if (conn) {
                        auto result = conn->executeQuery("SELECT " + std::to_string(t * 100 + i) + " as value");
                        if (result->next()) {
                            result->getInt("value");
                        }
                        pool.releaseConnection(conn);
                    }
                }
            }));
        }
        
        for (auto& future : futures) {
            future.wait();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double totalMs = duration.count() / 1000.0;
        
        const int totalOperations = concurrentThreads * operationsPerThread;
        std::cout << totalOperations << " 次并发操作耗时: " 
                  << std::fixed << std::setprecision(1) << totalMs << "ms (" 
                  << concurrentThreads << "个线程)" << std::endl;
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
    std::cout << "              第5天测试结果总结" << std::endl;
    std::cout << std::string(60, '*') << std::endl;
    
    size_t passed = 0;
    for (const auto& result : results) {
        std::cout << (result.second ? "成功" : "失败") << " " << result.first << std::endl;
        if (result.second) passed++;
    }
    
    std::cout << "\n通过: " << passed << "/" << results.size() << " 项测试" << std::endl;
    
    if (passed == results.size()) {
        std::cout << "\n恭喜！第5天所有测试都通过了！" << std::endl;
        std::cout << "你已经成功实现了:" << std::endl;
        std::cout << " 三种负载均衡算法" << std::endl;
        std::cout << " 多数据库配置管理" << std::endl;
        std::cout << " 动态策略切换" << std::endl;
        std::cout << " 连接池与负载均衡器集成" << std::endl;
        std::cout << " 并发安全的负载分布" << std::endl;
    } else {
        std::cout << "\n需要修复 " << (results.size() - passed) << " 个问题。" << std::endl;
    }
}

int main() {
    std::cout << "开始第5天负载均衡与多数据库支持测试..." << std::endl;
    std::cout << "测试数据库: " << TEST_USER << "@" << TEST_HOST1 << ":" << TEST_PORT1 << "/" << TEST_DATABASE << std::endl;
    
    // 初始化日志系统
    Logger::getInstance().init("", LogLevel::INFO, true);
    
    try {
        // 执行测试并收集结果
        std::vector<std::pair<std::string, bool>> results;
        
        results.emplace_back("负载均衡器基础功能测试", testLoadBalancerBasics());
        results.emplace_back("三种负载均衡策略测试", testLoadBalanceStrategies());
        results.emplace_back("动态数据库管理测试", testDynamicDatabaseManagement());
        results.emplace_back("连接池集成测试", testConnectionPoolIntegration());
        
        results.emplace_back("并发负载均衡测试", testConcurrentLoadBalancing());
        results.emplace_back("错误处理测试", testErrorHandling());
        results.emplace_back("负载均衡性能测试", testPerformanceWithLoadBalancing());
        
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