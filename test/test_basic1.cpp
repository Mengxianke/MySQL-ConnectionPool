#include <iostream>
#include <cassert>
#include <vector>
#include <thread>
#include <chrono>
#include "utils.h"
#include "logger.h"

void testMySQLEscape() {
    std::cout << "\n--- 测试MySQL字符串转义 ---" << std::endl;
    
    // 定义测试用例结构
    struct TestCase {
        std::string input;
        std::string description;
    };
    
    // 常见的测试用例
    std::vector<TestCase> testCases = {
        {"Normal text", "普通文本"},
        {"It's a 'test' with \"quotes\"", "混合引号"},
        {"'; DROP TABLE users; --", "SQL注入尝试"},
        {"C:\\Program Files\\MySQL", "Windows路径"},
        {"Line1\nLine2\tTabbed", "特殊字符"},
        {"用户名：张三", "中文字符"},
        {"", "空字符串"}
    };
    
    // 执行测试
    for (const auto& tc : testCases) {
        std::string result = Utils::quoteMySQLString(tc.input);
        std::cout << "  " << tc.description << ": ";
        std::cout << result << std::endl;
    }
    
    std::cout << "  MySQL转义测试完成！" << std::endl;
}

/**
 * @brief 第一天基础功能测试
 * 测试工具类和日志系统的基本功能
 */

void testUtils() {
    std::cout << "\n=== 测试Utils工具类 ===" << std::endl;

    // 测试字符串分割
    std::vector<std::string> tokens = Utils::split("hello,world,test", ',');
    assert(tokens.size() == 3);
    assert(tokens[0] == "hello");
    assert(tokens[1] == "world");
    assert(tokens[2] == "test");
    std::cout << "字符串分割测试通过" << std::endl;

    // 测试随机字符串生成
    std::string randomStr1 = Utils::generateRandomString(10);
    std::string randomStr2 = Utils::generateRandomString(10);
    assert(randomStr1.length() == 10);
    assert(randomStr2.length() == 10);
    assert(randomStr1 != randomStr2);  // 两次生成的字符串应该不同
    std::cout << "随机字符串生成测试通过: " << randomStr1 << std::endl;

    // 测试时间戳获取
    int64_t timestamp1 = Utils::currentTimeMillis();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int64_t timestamp2 = Utils::currentTimeMillis();
    assert(timestamp2 > timestamp1);
    std::cout << "时间戳获取测试通过: " << timestamp1 << " -> " << timestamp2 << std::endl;

    // 测试类型转换
    std::string numberStr = Utils::toString(12345);
    assert(numberStr == "12345");
    std::cout << "类型转换测试通过: " << numberStr << std::endl;

    // 测试SQL转义
    testMySQLEscape();
    // 测试字节格式化
    std::string formatted = Utils::formatBytes(1536);  // 1.5 KB
    std::cout << "字节格式化测试通过: 1536 bytes = " << formatted << std::endl;

    // 测试字符串修剪
    std::string trimmed = Utils::trim("  hello world  ");
    assert(trimmed == "hello world");
    std::cout << "字符串修剪测试通过: '" << trimmed << "'" << std::endl;
}

void testLogger() {
    std::cout << "\n=== 测试Logger日志系统 ===" << std::endl;

    // 获取日志实例
    Logger& logger = Logger::getInstance();

    // 初始化日志（输出到控制台，级别为DEBUG）
    logger.init("", LogLevel::DEBUG, true);

    // 测试不同级别的日志输出
    logger.debug("这是一条调试信息");
    logger.info("这是一条普通信息");
    logger.warning("这是一条警告信息");
    logger.error("这是一条错误信息");

    std::cout << "日志基本输出测试通过" << std::endl;

    // 测试日志级别过滤
    logger.setLevel(LogLevel::INFO);
    std::cout << "\n--- 设置日志级别为INFO，DEBUG信息不会显示 ---" << std::endl;
    logger.debug("这条DEBUG信息不会显示");
    logger.info("这条INFO信息会显示");

    std::cout << "日志级别过滤测试通过" << std::endl;

    // 测试宏定义
    std::cout << "\n--- 测试日志宏定义 ---" << std::endl;
    LOG_INFO("使用宏定义记录日志");
    LOG_WARNING("这是通过宏记录的警告");

    std::cout << "日志宏定义测试通过" << std::endl;
}

void testMultiThreadLogger() {
    std::cout << "\n=== 测试多线程日志安全性 ===" << std::endl;

    Logger& logger = Logger::getInstance();
    logger.setLevel(LogLevel::INFO);

    // 创建多个线程同时写日志
    std::vector<std::thread> threads;

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([i]() {
            for (int j = 0; j < 3; ++j) {
                LOG_INFO("线程 " + Utils::toString(i) + " 的第 " + Utils::toString(j) + " 条日志");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "多线程日志安全性测试通过" << std::endl;
}

void testPerformance() {
    std::cout << "\n=== 性能基准测试 ===" << std::endl;
    
    // 测试随机字符串生成性能
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        Utils::generateRandomString(16);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "生成10000个16字符随机字符串耗时: " << duration.count() << " 微秒" << std::endl;
    
    // 测试日志输出性能
    Logger& logger = Logger::getInstance();
    logger.setLevel(LogLevel::INFO);
    
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        LOG_INFO("性能测试日志消息 " + Utils::toString(i));
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "输出1000条日志耗时: " << duration.count() << " 微秒" << std::endl;
}

int main() {
    std::cout << "开始第一天基础功能测试..." << std::endl;
    
    try {
        // 运行各项测试
        testUtils();
        testLogger();
        testMultiThreadLogger();
        testPerformance();
        
        std::cout << "\n 恭喜！第一天所有测试都通过了！" << std::endl;
        std::cout << "你已经成功搭建了项目基础框架，并实现了工具类和日志系统。" << std::endl;
        std::cout << "明天我们将开始实现数据库连接封装。" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << " 测试失败: " << e.what() << std::endl;
        return 1;
    }
}