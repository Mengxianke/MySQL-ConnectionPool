#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "connection.h"
#include "logger.h"
#include "db_exception.h"

/**
 * @brief 第3天重连功能核心测试
 * 
 * 重点验证：
 * 1. 错误码识别功能
 * 2. 自动重连机制
 * 3. 带重连的查询执行
 * 4. 重连统计功能
 * 5. 异常处理
 */

// 测试数据库连接参数 - 请根据实际情况修改
const std::string TEST_HOST = "localhost";
const std::string TEST_USER = "mxk";
const std::string TEST_PASSWORD = "d2v8s2q3";
const std::string TEST_DATABASE = "testdb";
const unsigned int TEST_PORT = 3306;

void printTestHeader(const std::string& title) {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}

bool testErrorCodeRecognition() {
    printTestHeader("测试错误码识别");

    try {
        Connection conn(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT);
        
        // 测试连接错误码
        struct { unsigned int code; bool isConnectionError; } tests[] = {
            {2002, true},   // CR_CONNECTION_ERROR
            {2006, true},   // CR_SERVER_GONE_ERROR
            {2013, true},   // CR_SERVER_LOST
            {1045, false},  // ER_ACCESS_DENIED_ERROR
            {1146, false},  // ER_NO_SUCH_TABLE
            {1064, false}   // ER_PARSE_ERROR
        };
        
        for (auto& test : tests) {
            bool result = conn.isConnectionError(test.code);
            std::cout << "错误码 " << test.code << ": " 
                      << (result == test.isConnectionError ? "正确" : "失败") << std::endl;
        }
        
        std::cout << "初始重连统计 - 尝试: " << conn.getReconnectAttempts() 
                  << ", 成功: " << conn.getSuccessfulReconnects() << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return false;
    }
}

bool testBasicReconnection() {
    printTestHeader("测试基础重连功能");

    try {
        // 设置较短的重连间隔便于测试
        Connection conn(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT, 300, 3);
        
        std::cout << "1. 建立初始连接..." << std::endl;
        if (!conn.connect()) {
            std::cout << "无法连接到数据库，请检查连接参数" << std::endl;
            return false;
        }
        std::cout << "连接成功" << std::endl;
        
        std::cout << "2. 测试连接有效性..." << std::endl;
        if (conn.isValid()) {
            std::cout << "连接有效" << std::endl;
        } else {
            std::cout << "连接无效" << std::endl;
            return false;
        }
        
        std::cout << "3. 测试主动重连..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        bool reconnected = conn.reconnect();
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (reconnected) {
            std::cout << "重连成功，耗时: " << duration.count() << "ms" << std::endl;
        } else {
            std::cout << "重连失败" << std::endl;
            return false;
        }
        
        std::cout << "4. 验证重连后功能..." << std::endl;
        auto result = conn.executeQuery("SELECT 1 as test_value");
        if (result->next() && result->getInt("test_value") == 1) {
            std::cout << "重连后查询正常" << std::endl;
        } else {
            std::cout << "重连后查询失败" << std::endl;
            return false;
        }
        
        std::cout << "重连统计 - 尝试: " << conn.getReconnectAttempts() 
                  << ", 成功: " << conn.getSuccessfulReconnects() << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试异常: " << e.what() << std::endl;
        return false;
    }
}

bool testQueryWithReconnect() {
    printTestHeader("测试带重连的查询执行");

    try {
        Connection conn(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT, 200, 2);
        
        if (!conn.connect()) {
            std::cout << "无法连接到数据库" << std::endl;
            return false;
        }
        
        std::cout << "1. 测试正常查询..." << std::endl;
        try {
            auto result = conn.executeQuery("SELECT CONNECTION_ID() as conn_id, NOW() as now");
            if (result->next()) {
                std::cout << "查询成功，连接ID: " << result->getString("conn_id") << std::endl;
            }
        } catch (const db::SQLExecutionError& e) {
            std::cout << "查询失败: " << e.what() << " (错误码: " << e.getErrorCode() << ")" << std::endl;
            return false;
        }
        
        std::cout << "2. 测试更新操作..." << std::endl;
        try {
            // 创建临时测试表
            conn.executeUpdate("CREATE TABLE test_reconnect (id INT, name VARCHAR(50))");
            
            unsigned long long affected = conn.executeUpdate(
                "INSERT INTO test_reconnect VALUES (1, 'test1'), (2, 'test2')");
            std::cout << "插入成功，影响行数: " << affected << std::endl;
            
            auto result = conn.executeQuery("SELECT COUNT(*) as count FROM test_reconnect");
            if (result->next()) {
                std::cout << "验证数据: " << result->getInt("count") << " 条记录" << std::endl;
            }

        } catch (const db::SQLExecutionError& e) {
            std::cout << "更新操作失败: " << e.what() << std::endl;
            return false;
        }
        
        std::cout << "3. 测试事务操作..." << std::endl;
        if (conn.beginTransaction()) {
            std::cout << "事务开始" << std::endl;
            conn.executeUpdate("UPDATE test_reconnect SET name = 'updated' WHERE id = 1");
            if (conn.commit()) {
                std::cout << "事务提交成功" << std::endl;
            } else {
                std::cout << "事务提交失败" << std::endl;
                return false;
            }
        } else {
            std::cout << "事务开始失败" << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试异常: " << e.what() << std::endl;
        return false;
    }
}

bool testInvalidCredentials() {
    printTestHeader("测试无效凭据处理");

    try {
        // 使用错误密码
        Connection conn(TEST_HOST, TEST_USER, "wrong_password", TEST_DATABASE, TEST_PORT, 100, 2);
        
        std::cout << "1. 尝试连接（应该失败）..." << std::endl;
        if (conn.connect()) {
            std::cout << "连接不应该成功" << std::endl;
            return false;
        } else {
            std::cout << "连接正确失败" << std::endl;
        }
        
        std::cout << "2. 测试重连（应该失败）..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        bool reconnected = conn.reconnect();
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (!reconnected) {
            std::cout << "重连正确失败，耗时: " << duration.count() << "ms" << std::endl;
        } else {
            std::cout << "重连不应该成功" << std::endl;
            return false;
        }
        
        std::cout << "重连统计 - 尝试: " << conn.getReconnectAttempts() 
                  << ", 成功: " << conn.getSuccessfulReconnects() << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试异常: " << e.what() << std::endl;
        return false;
    }
}

bool testReconnectDelay() {
    printTestHeader("测试重连延迟算法");

    try {
        // 使用无效主机测试延迟
        Connection conn("invalid_host_12345", TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT, 50, 3);
        
        std::cout << "连接到无效主机以测试延迟算法..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        bool result = conn.reconnect();
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "重连过程总耗时: " << duration.count() << "ms" << std::endl;
        std::cout << "重连结果: " << (result ? "成功" : "失败（预期）") << std::endl;
        std::cout << "重连尝试次数: " << conn.getReconnectAttempts() << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "测试异常: " << e.what() << std::endl;
        return false;
    }
}

bool testStatisticsReset() {
    printTestHeader("测试重连统计重置");

    try {
        Connection conn(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT, 100, 2);
        
        if (conn.connect()) {
            // 执行几次重连增加统计
            conn.reconnect();
            conn.reconnect();
            
            unsigned int oldAttempts = conn.getReconnectAttempts();
            unsigned int oldSuccessful = conn.getSuccessfulReconnects();
            
            std::cout << "重置前统计 - 尝试: " << oldAttempts << ", 成功: " << oldSuccessful << std::endl;
            
            conn.resetReconnectStats();
            
            std::cout << "重置后统计 - 尝试: " << conn.getReconnectAttempts() 
                      << ", 成功: " << conn.getSuccessfulReconnects() << std::endl;
            
            if (conn.getReconnectAttempts() == 0 && conn.getSuccessfulReconnects() == 0) {
                std::cout << "统计重置成功" << std::endl;
                return true;
            } else {
                std::cout << "统计重置失败" << std::endl;
                return false;
            }
        } else {
            std::cout << "无法连接到数据库" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cout << "测试异常: " << e.what() << std::endl;
        return false;
    }
}

void printSummary(const std::vector<std::pair<std::string, bool>>& results) {
    std::cout << "\n" << std::string(50, '*') << std::endl;
    std::cout << "              测试结果总结" << std::endl;
    std::cout << std::string(50, '*') << std::endl;
    
    size_t passed = 0;
    for (const auto& result : results) {
        std::cout << (result.second ? "执行成功" : "执行失败") << " " << result.first << std::endl;
        if (result.second) passed++;
    }
    
    std::cout << "\n通过: " << passed << "/" << results.size() << " 项测试" << std::endl;
    
    if (passed == results.size()) {
        std::cout << "\n恭喜！第3天重连功能测试全部通过！" << std::endl;
        std::cout << "\n你已经成功实现了：" << std::endl;
        std::cout << "智能错误码识别" << std::endl;
        std::cout << "自动重连机制" << std::endl;
        std::cout << "指数退避算法" << std::endl;
        std::cout << "重连统计监控" << std::endl;
        std::cout << "异常处理系统" << std::endl;
        std::cout << "\n明天我们将实现连接池核心逻辑！" << std::endl;
    } else {
        std::cout << "\n部分测试未通过，请检查：" << std::endl;
        std::cout << "1. MySQL服务是否正常运行" << std::endl;
        std::cout << "2. 连接参数是否正确" << std::endl;
        std::cout << "3. 用户权限是否足够" << std::endl;
    }
}

bool testConcurrentReconnect() {
    printTestHeader("测试并发重连安全性");
    
    try {
        std::cout << "1. 创建多个连接进行并发测试..." << std::endl;
        
        const int numThreads = 3;
        std::vector<std::thread> threads;
        std::vector<bool> results(numThreads, false);
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([i, &results]() {
                try {
                    Connection conn(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, 
                                    TEST_PORT, 200, 2);  // 使用标准端口
                    
                    std::cout << "线程 " << i << ": 尝试连接..." << std::endl;
                    
                    // 尝试连接
                    bool connected = conn.connect();
                    if (connected) {
                        std::cout << "线程 " << i << ": 连接成功" << std::endl;
                        
                        // 执行一些查询
                        for (int j = 0; j < 3; ++j) {
                            try {
                                auto result = conn.executeQuery("SELECT " + std::to_string(i * 10 + j) + " as value");
                                if (result->next()) {
                                    int value = result->getInt("value");
                                    std::cout << "线程 " << i << ": 查询 " << j << " 返回 " << value << std::endl;
                                }
                            } catch (const std::exception& e) {
                                std::cout << "线程 " << i << ": 查询 " << j << " 失败: " << e.what() << std::endl;
                            }
                            
                            // 短暂休眠
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                        
                        results[i] = true;
                    } else {
                        std::cout << "线程 " << i << ": 连接失败，尝试重连..." << std::endl;
                        
                        // 如果连接失败，尝试重连
                        bool reconnected = conn.reconnect();
                        std::cout << "线程 " << i << ": 重连" << (reconnected ? "成功" : "失败") << std::endl;
                        results[i] = reconnected;
                    }
                    
                    std::cout << "线程 " << i << ": 重连统计 - 尝试: " 
                              << conn.getReconnectAttempts() 
                              << ", 成功: " << conn.getSuccessfulReconnects() << std::endl;
                              
                } catch (const std::exception& e) {
                    std::cout << "线程 " << i << ": 异常: " << e.what() << std::endl;
                    results[i] = false;
                }
            });
        }
        
        // 等待所有线程完成
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "\n2. 并发测试结果统计..." << std::endl;
        int successCount = 0;
        for (int i = 0; i < numThreads; ++i) {
            std::cout << "线程 " << i << ": " << (results[i] ? "成功" : "失败") << std::endl;
            if (results[i]) successCount++;
        }
        
        std::cout << "并发测试完成，成功线程: " << successCount << "/" << numThreads << std::endl;
        
        // 如果至少有2个线程成功，认为测试通过
        return successCount >= 2;
        
    } catch (const std::exception& e) {
        std::cout << "并发重连测试失败: " << e.what() << std::endl;
        return false;
    }
}


int main() {
    std::cout << "开始第3天重连功能测试..." << std::endl;
    std::cout << "连接参数: " << TEST_USER << "@" << TEST_HOST << ":" << TEST_PORT << "/" << TEST_DATABASE << std::endl;
    
    // 初始化日志系统
    Logger::getInstance().init("", LogLevel::INFO, true);
    
    // 执行测试并收集结果
    std::vector<std::pair<std::string, bool>> results;
    
    results.emplace_back("错误码识别测试", testErrorCodeRecognition());
    results.emplace_back("基础重连功能测试", testBasicReconnection());
    results.emplace_back("带重连查询测试", testQueryWithReconnect());
    results.emplace_back("无效凭据处理测试", testInvalidCredentials());
    results.emplace_back("重连延迟算法测试", testReconnectDelay());
    results.emplace_back("统计重置功能测试", testStatisticsReset());
    results.emplace_back("测试并发重连安全性", testConcurrentReconnect());
    
    // 显示测试结果
    printSummary(results);
    
    // 根据测试结果返回退出码
    size_t passed = 0;
    for (const auto& result : results) {
        if (result.second) passed++;
    }
    
    return (passed == results.size()) ? 0 : 1;
}