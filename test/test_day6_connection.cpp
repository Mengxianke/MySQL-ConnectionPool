#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <iomanip>
#include "connection_pool.h"
#include "load_balancer.h"
#include "pool_config.h"
#include "logger.h"

/**
 * @brief 第6天健康检查与连接池优化测试
 * 
 * 重点验证：
 * 1. 健康检查线程的正常工作
 * 2. 空闲连接的自动清理
 * 3. 最小连接数的自动维护
 * 4. 动态配置调整功能
 * 5. 连接池的自我修复能力
 * 6. 长时间运行的稳定性
 */

// 测试数据库连接参数
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

bool testIdleConnectionCleanup() {
    printTestHeader("测试空闲连接清理");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        PoolConfig config;
        config.setConnectionLimits(2, 6, 3);           // min=2, max=6, init=3

        // 设置很短的空闲超时时间，方便测试
        config.setTimeouts(3000, 3000, 1000);          // 空闲3秒就超时，健康检查1秒一次

        //初始化单数据库连接池
        pool.initWithSingleDatabase(config,TEST_HOST,TEST_USER,TEST_PASSWORD,TEST_DATABASE,TEST_PORT);
        
        std::cout << "设置空闲超时: 3秒，健康检查: 1秒" << std::endl;
        
        std::cout << "2. 获取所有连接并立即释放..." << std::endl;
        std::vector<ConnectionPtr> connections;
        
        // 获取一些连接
        for (int i = 0; i < 4; ++i) {
            auto conn = pool.getConnection(2000);
            if (conn) {
                connections.push_back(conn);
                std::cout << "获取连接 " << (i + 1) << ": " << conn->getConnectionId() << std::endl;
            }
        }
        
        // 立即释放所有连接，让它们变成空闲状态
        for (auto& conn : connections) {
            pool.releaseConnection(conn);
        }
        connections.clear();
        
        std::cout << "所有连接已释放，当前空闲连接数: " << pool.getIdleCount() << std::endl;
        
        std::cout << "3. 等待空闲连接超时..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        std::cout << "4. 检查清理结果..." << std::endl;
        std::cout << "清理后总连接数: " << pool.getTotalCount() << std::endl;
        std::cout << "清理后空闲连接数: " << pool.getIdleCount() << std::endl;
        std::cout << "清理后活跃连接数: " << pool.getActiveCount() << std::endl;
        
        // 验证最小连接数是否被维持
        if (pool.getTotalCount() >= 2) {  // 最小连接数是2
            std::cout << "最小连接数得到维持" << std::endl;
        } else {
            std::cout << "最小连接数未维持" << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testMinimumConnectionMaintenance() {
    printTestHeader("测试最小连接数维护");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 人为减少连接数..." << std::endl;
        
        // 通过设置很小的最大连接数来强制缩减
        std::cout << "当前连接数: " << pool.getTotalCount() << std::endl;
        
        // 设置更小的连接池，模拟连接丢失
        bool adjusted = pool.setConnectionLimits(3, 5);  // 提高最小连接数到3
        std::cout << "调整连接限制结果: " << (adjusted ? "成功" : "失败") << std::endl;
        
        std::cout << "2. 等待健康检查补充连接..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        std::cout << "3. 检查连接补充结果..." << std::endl;
        size_t currentConnections = pool.getTotalCount();
        std::cout << "当前总连接数: " << currentConnections << std::endl;
        
        if (currentConnections >= 3) {
            std::cout << "最小连接数维护成功" << std::endl;
        } else {
            std::cout << "最小连接数维护失败" << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testDynamicConfiguration() {
    printTestHeader("测试动态配置调整");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 当前配置状态..." << std::endl;
        std::cout << "总连接数: " << pool.getTotalCount() << std::endl;
        std::cout << "空闲连接数: " << pool.getIdleCount() << std::endl;
        
        std::cout << "2. 测试扩大连接池..." << std::endl;
        bool result1 = pool.setConnectionLimits(4, 10);
        std::cout << "扩大连接池结果: " << (result1 ? "成功" : "失败") << std::endl;
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cout << "扩大后连接数: " << pool.getTotalCount() << std::endl;
        
        std::cout << "3. 测试缩小连接池..." << std::endl;
        bool result2 = pool.setConnectionLimits(2, 4);
        std::cout << "缩小连接池结果: " << (result2 ? "成功" : "失败") << std::endl;
        
        std::cout << "缩小后连接数: " << pool.getTotalCount() << std::endl;
        
        std::cout << "4. 测试调整超时设置..." << std::endl;
        bool result3 = pool.setTimeoutSettings(5000, 10000, 3000);
        std::cout << "调整超时设置结果: " << (result3 ? "成功" : "失败") << std::endl;
        
        std::cout << "5. 测试无效配置..." << std::endl;
        bool result4 = pool.setConnectionLimits(10, 5);  // 最小 > 最大，应该失败
        std::cout << "无效配置拒绝结果: " << (result4 ? "失败（不应该成功）" : "成功（正确拒绝）") << std::endl;
        
        return result1 && result2 && result3 && !result4;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testConnectionValidation() {
    printTestHeader("测试连接验证和自动修复");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 获取连接测试基本验证..." << std::endl;
        
        auto conn = pool.getConnection();
        if (!conn) {
            std::cout << "无法获取连接" << std::endl;
            return false;
        }
        
        std::cout << "获取连接: " << conn->getConnectionId() << std::endl;
        
        std::cout << "2. 测试连接功能..." << std::endl;
        try {
            auto result = conn->executeQuery("SELECT 'Health Check Test' as message, NOW() as `current_time`");
            if (result->next()) {
                std::cout << "连接验证成功: " << result->getString("message") 
                          << ", 时间: " << result->getString("current_time") << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "连接验证失败: " << e.what() << std::endl;
            return false;
        }
        
        pool.releaseConnection(conn);
        
        std::cout << "3. 触发健康检查验证所有连接..." << std::endl;
        pool.performHealthCheck();
        std::cout << "健康检查完成" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testLongRunningStability() {
    printTestHeader("测试长时间运行稳定性");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 启动长时间稳定性测试..." << std::endl;
        
        // 设置快速的健康检查周期
        pool.setTimeoutSettings(3000, 8000, 1000);  // 健康检查每1秒一次
        
        const int testDuration = 10;  // 测试10秒
        const int operationsPerSecond = 5;
        
        std::cout << "测试参数: 持续" << testDuration << "秒, 每秒" << operationsPerSecond << "次操作" << std::endl;
        
        auto startTime = std::chrono::steady_clock::now();
        int totalOperations = 0;
        int successOperations = 0;
        
        while (true) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
            
            if (elapsed.count() >= testDuration) {
                break;
            }
            
            // 执行操作
            try {
                auto conn = pool.getConnection(1000);
                if (conn) {
                    auto result = conn->executeQuery("SELECT " + std::to_string(totalOperations) + " as op_id");
                    if (result->next()) {
                        successOperations++;
                    }
                    pool.releaseConnection(conn);
                }
                totalOperations++;
                
                // 控制操作频率
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 / operationsPerSecond));
                
            } catch (const std::exception& e) {
                std::cout << "操作异常: " << e.what() << std::endl;
                totalOperations++;
            }
        }
        
        std::cout << "2. 稳定性测试结果..." << std::endl;
        std::cout << "总操作数: " << totalOperations << std::endl;
        std::cout << "成功操作数: " << successOperations << std::endl;
        std::cout << "成功率: " << std::fixed << std::setprecision(1) 
                  << (double)successOperations / totalOperations * 100 << "%" << std::endl;
        
        std::cout << "3. 最终连接池状态..." << std::endl;
        std::cout << "总连接数: " << pool.getTotalCount() << std::endl;
        std::cout << "空闲连接数: " << pool.getIdleCount() << std::endl;
        std::cout << "活跃连接数: " << pool.getActiveCount() << std::endl;
        
        // 成功率应该很高
        double successRate = (double)successOperations / totalOperations * 100;
        return successRate > 95.0;  // 要求95%以上的成功率
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testErrorRecovery() {
    printTestHeader("测试错误恢复能力");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 模拟错误场景..." << std::endl;
        
        // 获取多个连接但不释放，模拟连接泄漏
        std::vector<ConnectionPtr> leakedConnections;
        for (int i = 0; i < 3; ++i) {
            auto conn = pool.getConnection();
            if (conn) {
                leakedConnections.push_back(conn);
                std::cout << "模拟泄漏连接 " << (i + 1) << ": " << conn->getConnectionId() << std::endl;
            }
        }
        
        std::cout << "2. 检查连接池在压力下的表现..." << std::endl;
        std::cout << "当前总连接数: " << pool.getTotalCount() << std::endl;
        std::cout << "当前空闲连接数: " << pool.getIdleCount() << std::endl;
        std::cout << "当前活跃连接数: " << pool.getActiveCount() << std::endl;
        
        std::cout << "3. 等待健康检查处理..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(15));
        
        std::cout << "4. 释放'泄漏'的连接..." << std::endl;
        for (auto& conn : leakedConnections) {
            pool.releaseConnection(conn);
        }
        leakedConnections.clear();
        
        std::cout << "5. 验证恢复后的状态..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        std::cout << "恢复后总连接数: " << pool.getTotalCount() << std::endl;
        std::cout << "恢复后空闲连接数: " << pool.getIdleCount() << std::endl;
        std::cout << "恢复后活跃连接数: " << pool.getActiveCount() << std::endl;
        
        // 验证连接池是否能正常工作
        auto testConn = pool.getConnection();
        if (testConn) {
            auto result = testConn->executeQuery("SELECT 'Recovery Test' as status");
            if (result->next()) {
                std::cout << "连接池恢复验证: " << result->getString("status") << std::endl;
                pool.releaseConnection(testConn);
                return true;
            }
        }
        
        return false;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

void printSummary(const std::vector<std::pair<std::string, bool>>& results) {
    std::cout << "\n" << std::string(60, '*') << std::endl;
    std::cout << "              第6天测试结果总结" << std::endl;
    std::cout << std::string(60, '*') << std::endl;
    
    size_t passed = 0;
    for (const auto& result : results) {
        std::cout << (result.second ? "成功" : "失败") << " " << result.first << std::endl;
        if (result.second) passed++;
    }
    
    std::cout << "\n通过: " << passed << "/" << results.size() << " 项测试" << std::endl;
    
    if (passed == results.size()) {
        std::cout << "\n恭喜！第6天所有测试都通过了！" << std::endl;
        std::cout << "你的连接池现在具备了：" << std::endl;
        std::cout << " 自动健康检查和维护" << std::endl;
        std::cout << " 智能连接清理机制" << std::endl;
        std::cout << " 动态配置调整能力" << std::endl;
        std::cout << " 自我修复和错误恢复" << std::endl;
        std::cout << " 长时间稳定运行" << std::endl;
        std::cout << "\n你的连接池已经达到生产环境可用的水平！" << std::endl;
    } else {
        std::cout << "\n需要修复 " << (results.size() - passed) << " 个问题。" << std::endl;
    }
}


#if 1
// 在测试文件中添加以下函数

// =========================
// SQL执行相关测试函数
// =========================

bool createTestTable() {
    std::cout << "  创建测试表..." << std::endl;
    
    auto& pool = ConnectionPool::getInstance();

    auto conn = pool.getConnection(3000);

    if (!conn) {
        std::cout << "无法获取连接创建测试表" << std::endl;
        return false;
    }
    
    try {
        // 删除可能存在的测试表
        conn->executeUpdate("DROP TABLE IF EXISTS test_health_check");
        
        // 创建新的测试表
        std::string createSQL = R"(
            CREATE TABLE test_health_check (
                id INT AUTO_INCREMENT PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                value INT NOT NULL,
                created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                INDEX idx_name (name)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        )";
        
        conn->executeUpdate(createSQL);
        pool.releaseConnection(conn);
        
        std::cout << "测试表创建成功" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "创建测试表失败: " << e.what() << std::endl;
        pool.releaseConnection(conn);
        return false;
    }
}

bool testInsertOperations() {
    std::cout << "  测试INSERT操作..." << std::endl;
    
    auto& pool = ConnectionPool::getInstance();
    int successCount = 0;
    const int testCount = 10;
    
    for (int i = 1; i <= testCount; ++i) {
        auto conn = pool.getConnection(2000);
        if (!conn) {
            std::cout << "获取连接失败 (INSERT " << i << ")" << std::endl;
            continue;
        }
        
        try {
            std::string sql = "INSERT INTO test_health_check (name, value) VALUES ('test_user_" + 
                             std::to_string(i) + "', " + std::to_string(i * 10) + ")";
            
            unsigned long long affected = conn->executeUpdate(sql);
            if (affected > 0) {
                successCount++;
                std::cout << "INSERT " << i << " 成功, 影响行数: " << affected << std::endl;
            } else {
                std::cout << "INSERT " << i << " 无影响行数" << std::endl;
            }
            
            pool.releaseConnection(conn);
            
        } catch (const std::exception& e) {
            std::cout << "INSERT " << i << " 失败: " << e.what() << std::endl;
            pool.releaseConnection(conn);
        }
    }
    
    std::cout << "INSERT操作完成: " << successCount << "/" << testCount << " 成功" << std::endl;
    return successCount >= testCount * 0.8; // 80%成功率算通过
}

bool testSelectOperations() {
    std::cout << "  测试SELECT操作..." << std::endl;
    
    auto& pool = ConnectionPool::getInstance();
    auto conn = pool.getConnection(3000);
    if (!conn) {
        std::cout << "无法获取连接进行SELECT测试" << std::endl;
        return false;
    }
    
    try {
        // 测试1: 查询总数
        auto result1 = conn->executeQuery("SELECT COUNT(*) as total_count FROM test_health_check");
        if (result1->next()) {
            int totalCount = result1->getInt("total_count");
            std::cout << "总记录数查询成功: " << totalCount << " 条记录" << std::endl;
        }
        
        // 测试2: 查询具体数据
        auto result2 = conn->executeQuery("SELECT id, name, value FROM test_health_check ORDER BY id LIMIT 5");
        int recordCount = 0;
        std::cout << "查询前5条记录:" << std::endl;
        
        while (result2->next()) {
            recordCount++;
            std::cout << "    [" << result2->getInt("id") << "] " 
                      << result2->getString("name") << " = " 
                      << result2->getInt("value") << std::endl;
        }
        
        // 测试3: 条件查询
        auto result3 = conn->executeQuery("SELECT COUNT(*) as count FROM test_health_check WHERE value > 50");
        if (result3->next()) {
            int conditionCount = result3->getInt("count");
            std::cout << "条件查询成功: value > 50 的记录有 " << conditionCount << " 条" << std::endl;
        }
        
        pool.releaseConnection(conn);
        std::cout << "SELECT操作测试完成" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "SELECT操作失败: " << e.what() << std::endl;
        pool.releaseConnection(conn);
        return false;
    }
}

bool testSqlExecutionBasics() {
    printTestHeader("测试SQL执行基础功能");

    auto& pool = ConnectionPool::getInstance();
    PoolConfig config;
    config.setConnectionLimits(2, 6, 3);           // min=2, max=6, init=3
    //初始化单数据库连接池
    //pool.init(config);
    pool.initWithSingleDatabase(config,TEST_HOST,TEST_USER,TEST_PASSWORD,TEST_DATABASE,TEST_PORT);
    try {
        std::cout << "1. 准备测试环境..." << std::endl;
        if (!createTestTable()) {
            return false;
        }
        
        std::cout << "2. 测试INSERT操作..." << std::endl;
        if (!testInsertOperations()) {
            return false;
        }
        
        std::cout << "3. 测试SELECT操作..." << std::endl;
        if (!testSelectOperations()) {
            return false;
        }
        
        std::cout << "4. 测试完成,清理测试表..." << std::endl;
        auto& pool = ConnectionPool::getInstance();
        auto conn = pool.getConnection();
        if (conn) {
            try {
                //conn->executeUpdate("DROP TABLE IF EXISTS test_health_check");
                pool.releaseConnection(conn);
                std::cout << "测试表清理完成" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "清理测试表失败: " << e.what() << std::endl;
                pool.releaseConnection(conn);
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

// =========================
// 并发SQL执行测试
// =========================

bool concurrentInsertWorker(int workerId, int operationsPerWorker, std::atomic<int>& totalSuccess) {
    auto& pool = ConnectionPool::getInstance();
    int localSuccess = 0;
    
    for (int i = 0; i < operationsPerWorker; ++i) {
        try {
            auto conn = pool.getConnection(1000);
            if (!conn) {
                std::cout << "Worker " << workerId << " 获取连接失败" << std::endl;
                continue;
            }
            
            std::string sql = "INSERT INTO test_concurrent (worker_id, operation_id, data_value) VALUES (" +
                             std::to_string(workerId) + ", " + 
                             std::to_string(i) + ", " + 
                             std::to_string(workerId * 1000 + i) + ")";
            
            unsigned long long affected = conn->executeUpdate(sql);
            if (affected > 0) {
                localSuccess++;
            }
            
            pool.releaseConnection(conn);
            
            // 模拟一些处理时间
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
        } catch (const std::exception& e) {
            std::cout << "Worker " << workerId << " 操作异常: " << e.what() << std::endl;
        }
    }
    
    totalSuccess += localSuccess;
    std::cout << "Worker " << workerId << " 完成: " << localSuccess << "/" << operationsPerWorker << " 成功" << std::endl;
    return localSuccess >= operationsPerWorker * 0.8; // 80%成功率
}

bool concurrentSelectWorker(int workerId, int operationsPerWorker, std::atomic<int>& totalSuccess) {
    auto& pool = ConnectionPool::getInstance();
    int localSuccess = 0;
    
    for (int i = 0; i < operationsPerWorker; ++i) {
        try {
            auto conn = pool.getConnection(1000);
            if (!conn) {
                continue;
            }
            
            // 随机查询不同的数据
            int randomWorker = (workerId + i) % 5; // 假设有5个worker
            std::string sql = "SELECT COUNT(*) as count FROM test_concurrent WHERE worker_id = " + 
                             std::to_string(randomWorker);
            
            auto result = conn->executeQuery(sql);
            if (result->next()) {
                localSuccess++;
            }
            
            pool.releaseConnection(conn);
            
            // 模拟查询间隔
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            
        } catch (const std::exception& e) {
            // 静默处理SELECT错误，避免日志过多
        }
    }
    
    totalSuccess += localSuccess;
    return localSuccess >= operationsPerWorker * 0.8;
}

bool testConcurrentSqlExecution() {
    printTestHeader("测试并发SQL执行");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 创建并发测试表..." << std::endl;
        auto conn = pool.getConnection();
        if (!conn) {
            std::cout << "无法获取连接" << std::endl;
            return false;
        }
        
        try {
            conn->executeUpdate("DROP TABLE IF EXISTS test_concurrent");
            conn->executeUpdate(R"(
                CREATE TABLE test_concurrent (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    worker_id INT NOT NULL,
                    operation_id INT NOT NULL,
                    data_value INT NOT NULL,
                    created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    INDEX idx_worker (worker_id)
                ) ENGINE=InnoDB
            )");
            pool.releaseConnection(conn);
            std::cout << "并发测试表创建成功" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "创建并发测试表失败: " << e.what() << std::endl;
            pool.releaseConnection(conn);
            return false;
        }
        
        std::cout << "2. 启动并发INSERT测试..." << std::endl;
        const int insertWorkers = 5;
        const int insertOpsPerWorker = 10;
        std::vector<std::future<bool>> insertFutures;
        std::atomic<int> insertSuccess{0};
        
        auto insertStart = std::chrono::steady_clock::now();
        
        for (int i = 0; i < insertWorkers; ++i) {
            insertFutures.push_back(
                std::async(std::launch::async, concurrentInsertWorker, i, insertOpsPerWorker, std::ref(insertSuccess))
            );
        }
        
        bool allInsertSuccess = true;
        for (auto& future : insertFutures) {
            if (!future.get()) {
                allInsertSuccess = false;
            }
        }
        
        auto insertEnd = std::chrono::steady_clock::now();
        auto insertDuration = std::chrono::duration_cast<std::chrono::milliseconds>(insertEnd - insertStart);
        
        std::cout << "并发INSERT完成: " << insertSuccess.load() << "/" 
                  << (insertWorkers * insertOpsPerWorker) << " 成功, 耗时: " 
                  << insertDuration.count() << "ms" << std::endl;
        
        std::cout << "3. 启动并发SELECT测试..." << std::endl;
        const int selectWorkers = 8;
        const int selectOpsPerWorker = 15;
        std::vector<std::future<bool>> selectFutures;
        std::atomic<int> selectSuccess{0};
        
        auto selectStart = std::chrono::steady_clock::now();
        
        for (int i = 0; i < selectWorkers; ++i) {
            selectFutures.push_back(
                std::async(std::launch::async, concurrentSelectWorker, i, selectOpsPerWorker, std::ref(selectSuccess))
            );
        }
        
        bool allSelectSuccess = true;
        for (auto& future : selectFutures) {
            if (!future.get()) {
                allSelectSuccess = false;
            }
        }
        
        auto selectEnd = std::chrono::steady_clock::now();
        auto selectDuration = std::chrono::duration_cast<std::chrono::milliseconds>(selectEnd - selectStart);
        
        std::cout << "并发SELECT完成: " << selectSuccess.load() << "/" 
                  << (selectWorkers * selectOpsPerWorker) << " 成功, 耗时: " 
                  << selectDuration.count() << "ms" << std::endl;
        
        std::cout << "4. 验证数据完整性..." << std::endl;
        conn = pool.getConnection();
        if (conn) {
            auto result = conn->executeQuery("SELECT COUNT(*) as total FROM test_concurrent");
            if (result->next()) {
                int totalRecords = result->getInt("total");
                std::cout << "数据库中总记录数: " << totalRecords << std::endl;
            }
            
            // 清理测试表
            //conn->executeUpdate("DROP TABLE IF EXISTS test_concurrent");
            pool.releaseConnection(conn);
        }
        
        return allInsertSuccess && allSelectSuccess;
        
    } catch (const std::exception& e) {
        std::cout << "并发测试失败: " << e.what() << std::endl;
        return false;
    }
}

// =========================
// 混合操作压力测试
// =========================

bool testMixedSqlOperations() {
    printTestHeader("测试混合SQL操作压力");

    try {
        auto& pool = ConnectionPool::getInstance();
        
        std::cout << "1. 创建压力测试表..." << std::endl;
        auto conn = pool.getConnection();
        if (!conn) {
            return false;
        }
        
        try {
            conn->executeUpdate("DROP TABLE IF EXISTS test_mixed");
            conn->executeUpdate(R"(
                CREATE TABLE test_mixed (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    test_type VARCHAR(20) NOT NULL,
                    test_data VARCHAR(100) NOT NULL,
                    test_number INT NOT NULL,
                    created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                ) ENGINE=InnoDB
            )");
            pool.releaseConnection(conn);
        } catch (const std::exception& e) {
            std::cout << "创建压力测试表失败: " << e.what() << std::endl;
            pool.releaseConnection(conn);
            return false;
        }
        
        std::cout << "2. 执行混合操作..." << std::endl;
        const int totalOperations = 50;
        int insertCount = 0, selectCount = 0, updateCount = 0;
        int successCount = 0;
        
        auto startTime = std::chrono::steady_clock::now();
        
        for (int i = 0; i < totalOperations; ++i) {
            auto conn = pool.getConnection(1000);
            if (!conn) {
                continue;
            }
            
            try {
                int opType = i % 3; // 0=INSERT, 1=SELECT, 2=UPDATE
                
                if (opType == 0) {
                    // INSERT操作
                    std::string sql = "INSERT INTO test_mixed (test_type, test_data, test_number) VALUES " +
                                     std::string("('INSERT', 'data_") + std::to_string(i) + "', " + std::to_string(i) + ")";
                    conn->executeUpdate(sql);
                    insertCount++;
                    
                } else if (opType == 1) {
                    // SELECT操作
                    auto result = conn->executeQuery("SELECT COUNT(*) as count FROM test_mixed WHERE test_number > " + std::to_string(i/2));
                    if (result->next()) {
                        selectCount++;
                    }
                    
                } else {
                    // UPDATE操作（只有在有数据时才执行）
                    if (i > 5) {
                        std::string sql = "UPDATE test_mixed SET test_data = 'updated_" + std::to_string(i) + 
                                         "' WHERE id = " + std::to_string((i % 10) + 1);
                        conn->executeUpdate(sql);
                        updateCount++;
                    }
                }
                
                successCount++;
                pool.releaseConnection(conn);
                
            } catch (const std::exception& e) {
                pool.releaseConnection(conn);
                std::cout << "操作 " << i << " 失败: " << e.what() << std::endl;
            }
            
            // 模拟真实应用的操作间隔
            if (i % 10 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        std::cout << "3. 混合操作测试结果:" << std::endl;
        std::cout << "INSERT操作: " << insertCount << " 次" << std::endl;
        std::cout << "SELECT操作: " << selectCount << " 次" << std::endl;
        std::cout << "UPDATE操作: " << updateCount << " 次" << std::endl;
        std::cout << "总成功操作: " << successCount << "/" << totalOperations << std::endl;
        std::cout << "总耗时: " << duration.count() << "ms" << std::endl;
        std::cout << "平均每操作: " << std::fixed << std::setprecision(1) 
                  << (double)duration.count() / totalOperations << "ms" << std::endl;
        
        // 清理
        conn = pool.getConnection();
        if (conn) {
            conn->executeUpdate("DROP TABLE IF EXISTS test_mixed");
            pool.releaseConnection(conn);
        }
        
        return successCount >= totalOperations * 0.9; // 90%成功率
        
    } catch (const std::exception& e) {
        std::cout << "混合操作测试失败: " << e.what() << std::endl;
        return false;
    }
}
#endif

int main() {
    std::cout << "开始第6天健康检查与连接池优化测试..." << std::endl;
    std::cout << "测试数据库: " << TEST_USER << "@" << TEST_HOST << ":" << TEST_PORT << "/" << TEST_DATABASE << std::endl;
    
    // 初始化日志系统
    Logger::getInstance().init("", LogLevel::INFO, true);
    
    try {
        // 执行测试并收集结果
        std::vector<std::pair<std::string, bool>> results;
        
       results.emplace_back("空闲连接清理测试", testIdleConnectionCleanup());
       results.emplace_back("最小连接数维护测试", testMinimumConnectionMaintenance());
       results.emplace_back("动态配置调整测试", testDynamicConfiguration());
       results.emplace_back("连接验证和修复测试", testConnectionValidation());
       results.emplace_back("长时间运行稳定性测试", testLongRunningStability());
       results.emplace_back("错误恢复能力测试", testErrorRecovery());
        
          // 新增的SQL执行测试
         results.emplace_back("SQL执行基础功能测试", testSqlExecutionBasics());
         results.emplace_back("并发SQL执行测试", testConcurrentSqlExecution());
         results.emplace_back("混合SQL操作压力测试", testMixedSqlOperations());
    
        
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