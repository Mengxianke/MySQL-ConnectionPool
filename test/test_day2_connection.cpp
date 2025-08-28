#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "connection.h"
#include "db_config.h"
#include "pool_config.h"
#include "logger.h"

/**
 * @brief 第2天数据库连接功能测试
 * 
 * 注意：运行此测试前需要：
 * 1. 安装并启动MySQL服务器
 * 2. 创建测试数据库和表
 * 3. 修改连接参数
 */

// 测试用的数据库连接参数（请根据实际情况修改）
const std::string TEST_HOST = "localhost";
const std::string TEST_USER = "mxk";          // 修改为你的MySQL用户名
const std::string TEST_PASSWORD = "d2v8s2q3";      // 修改为你的MySQL密码
const std::string TEST_DATABASE = "testdb";        // 修改为你的测试数据库
const unsigned int TEST_PORT = 3306;

// 创建测试数据库和表的SQL语句
const std::string CREATE_DB_SQL = "CREATE DATABASE IF NOT EXISTS " + TEST_DATABASE;
const std::string USE_DB_SQL = "USE " + TEST_DATABASE;
const std::string CREATE_TABLE_SQL = R"(
CREATE TABLE IF NOT EXISTS test_users (
id INT AUTO_INCREMENT PRIMARY KEY,
name VARCHAR(50) NOT NULL,
age INT NOT NULL,
email VARCHAR(100),
created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
)
)";

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}

void testConfigStructures() {
    printSeparator("测试配置结构");

    // 测试DBConfig
    std::cout << "1. 测试DBConfig结构..." << std::endl;

    DBConfig config1;  // 默认构造
    std::cout << "默认构造成功，端口: " << config1.port << std::endl;

    DBConfig config2("localhost", "testuser", "xiaokang", "testdb", 3306, 5);
    std::cout << "参数构造成功: " << config2.getConnectionString() << std::endl;

    if (config2.isValid()) {
        std::cout << "配置验证通过" << std::endl;
    }

    // 测试PoolConfig
    std::cout << "\n2. 测试PoolConfig结构..." << std::endl;

    PoolConfig poolConfig;
    std::cout << "默认构造成功: " << poolConfig.getSummary() << std::endl;

    PoolConfig poolConfig2("localhost", "testuser", "xiaokang", "testdb");
    poolConfig2.setConnectionLimits(5, 20, 10);
    poolConfig2.setTimeouts(3000, 300000, 30000);
    std::cout << "参数设置成功: " << poolConfig2.getSummary() << std::endl;

    if (poolConfig2.isValid()) {
        std::cout << "池配置验证通过" << std::endl;
    }

    // 测试多数据库配置
    std::cout << "\n3. 测试多数据库配置..." << std::endl;
    PoolConfig multiConfig;
    multiConfig.addDatabase(DBConfig("db1.example.com", "user", "pass", "db1", 3306, 3));
    multiConfig.addDatabase(DBConfig("db2.example.com", "user", "pass", "db2", 3306, 2));
    std::cout << "添加了 " << multiConfig.getDatabaseCount() << " 个数据库实例" << std::endl;
}

bool setupTestEnvironment() {
    printSeparator("设置测试环境");

    std::cout << "正在尝试连接到MySQL服务器..." << std::endl;
    std::cout << "连接参数: " << TEST_USER << "@" << TEST_HOST << ":" << TEST_PORT << std::endl;
    std::cout << "\n注意：如果连接失败，请检查：" << std::endl;
    std::cout << "1. MySQL服务是否启动" << std::endl;
    std::cout << "2. 用户名密码是否正确" << std::endl;
    std::cout << "3. 用户是否有足够权限" << std::endl;
    std::cout << "4. 防火墙设置是否正确" << std::endl;

    try {
        // 先连接到MySQL服务器（不指定数据库）
        Connection setupConn(TEST_HOST, TEST_USER, TEST_PASSWORD, "", TEST_PORT);
        if (!setupConn.connect()) {
            std::cout << "无法连接到MySQL服务器" << std::endl;
            std::cout << "错误: " << setupConn.getLastError() << std::endl;
            return false;
        }
        
        std::cout << "成功连接到MySQL服务器" << std::endl;
        
        // 创建测试数据库
        std::cout << "正在创建测试数据库..." << std::endl;
        setupConn.executeUpdate(CREATE_DB_SQL);
        setupConn.executeUpdate(USE_DB_SQL);
        setupConn.executeUpdate(CREATE_TABLE_SQL);
        
        std::cout << "测试环境设置完成" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "设置测试环境失败: " << e.what() << std::endl;
        return false;
    }
}

void testBasicConnection() {
    printSeparator("测试基础连接功能");
    
    try {
        std::cout << "1. 测试连接创建..." << std::endl;
        Connection conn(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT);
        std::cout << "连接对象创建成功，ID: " << conn.getConnectionId() << std::endl;
        std::cout << "创建时间: " << conn.getCreationTime() << std::endl;
        
        std::cout << "\n2. 测试连接建立..." << std::endl;
        if (conn.connect()) {
            std::cout << "数据库连接建立成功" << std::endl;
        } else {
            std::cout << "数据库连接失败: " << conn.getLastError() << std::endl;
            return;
        }
        
        std::cout << "\n3. 测试连接有效性..." << std::endl;
        if (conn.isValid()) {
            std::cout << "连接有效性检查通过" << std::endl;
        } else {
            std::cout << "连接无效" << std::endl;
        }
        
        std::cout << "\n4. 测试连接信息..." << std::endl;
        std::cout << "最后活动时间: " << conn.getLastActiveTime() << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "基础连接测试失败: " << e.what() << std::endl;
    }
}

void testQueryOperations() {
    printSeparator("测试查询操作");
    
    try {
        Connection conn(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT);
        if (!conn.connect()) {
            std::cout << "连接失败，跳过查询测试" << std::endl;
            return;
        }
        
        // 清空测试表
        std::cout << "1. 清空测试表..." << std::endl;
        unsigned long long deleted = conn.executeUpdate("DELETE FROM test_users");
        std::cout << "删除了 " << deleted << " 行记录" << std::endl;
        
        // 插入测试数据
        std::cout << "\n2. 插入测试数据..." << std::endl;
        
        std::string insertSql = "INSERT INTO test_users (name, age, email) VALUES "
                               "('张三', 25, 'zhangsan@example.com'), "
                               "('李四', 30, 'lisi@example.com'), "
                               "('王五', 28, 'wangwu@example.com')";
        
        unsigned long long inserted = conn.executeUpdate(insertSql);
        std::cout << "插入了 " << inserted << " 行记录" << std::endl;
        
        // 查询数据
        std::cout << "\n3. 查询测试数据..." << std::endl;
        auto result = conn.executeQuery("SELECT id, name, age, email FROM test_users ORDER BY age");
        
        std::cout << "查询成功，结果信息：" << std::endl;
        std::cout << "  - 字段数量: " << result->getFieldCount() << std::endl;
        std::cout << "  - 行数: " << result->getRowCount() << std::endl;
        
        // 获取字段名
        auto fieldNames = result->getFieldNames();
        std::cout << "  - 字段名: ";
        for (const auto& name : fieldNames) {
            std::cout << name << " ";
        }
        std::cout << std::endl;
        
        // 遍历结果
        std::cout << "\n4. 遍历查询结果..." << std::endl;
        std::cout << "ID\t姓名\t年龄\t邮箱" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        int rowCount = 0;
        while (result->next()) {
            int id = result->getInt("id");
            std::string name = result->getString("name");
            int age = result->getInt("age");
            std::string email = result->getString("email");
            
            std::cout << id << "\t" << name << "\t" << age << "\t" << email << std::endl;
            rowCount++;
        }
        
        std::cout << "成功遍历 " << rowCount << " 行数据" << std::endl;
        
        // 测试不同数据类型
        std::cout << "\n5. 测试数据类型转换..." << std::endl;
        result->reset();  // 重置到开始
        if (result->next()) {
            int id = result->getInt(0);           // 按索引获取
            std::string name = result->getString(1);
            long long ageLong = result->getLong("age");  // 按字段名获取
            
            std::cout << "类型转换测试: ID=" << id << ", Name=" << name << ", Age(long)=" << ageLong << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "查询操作测试失败: " << e.what() << std::endl;
    }
}

void testTransactionOperations() {
    printSeparator("测试事务操作");
    
    try {
        Connection conn(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT);
        if (!conn.connect()) {
            std::cout << "连接失败，跳过事务测试" << std::endl;
            return;
        }
        
        // 测试成功事务
        std::cout << "1. 测试事务提交..." << std::endl;
        
        if (conn.beginTransaction()) {
            std::cout << "事务开始成功" << std::endl;
            
            // 插入一条记录
            unsigned long long affected = conn.executeUpdate(
                "INSERT INTO test_users (name, age, email) VALUES ('事务测试', 20, 'transaction@test.com')"
            );
            std::cout << "插入记录: " << affected << " 行" << std::endl;
            
            if (conn.commit()) {
                std::cout << "事务提交成功" << std::endl;
            } else {
                std::cout << "事务提交失败" << std::endl;
            }
        }
        
        // 验证数据是否真的插入了
        auto result = conn.executeQuery("SELECT COUNT(*) as count FROM test_users WHERE name = '事务测试'");
        if (result->next()) {
            int count = result->getInt("count");
            std::cout << "验证提交结果: 找到 " << count << " 条记录" << std::endl;
        }
        
        // 测试事务回滚
        std::cout << "\n2. 测试事务回滚..." << std::endl;
        
        if (conn.beginTransaction()) {
            std::cout << "事务开始成功" << std::endl;
            
            // 插入一条记录
            conn.executeUpdate(
                "INSERT INTO test_users (name, age, email) VALUES ('回滚测试', 21, 'rollback@test.com')"
            );
            std::cout << "插入记录（将被回滚）" << std::endl;
            
            if (conn.rollback()) {
                std::cout << "事务回滚成功" << std::endl;
            } else {
                std::cout << "事务回滚失败" << std::endl;
            }
        }
        
        // 验证数据是否真的被回滚了
        result = conn.executeQuery("SELECT COUNT(*) as count FROM test_users WHERE name = '回滚测试'");
        if (result->next()) {
            int count = result->getInt("count");
            std::cout << "验证回滚结果: 找到 " << count << " 条记录（应该是0）" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "事务操作测试失败: " << e.what() << std::endl;
    }
}

void testErrorHandling() {
    printSeparator("测试错误处理");
    
    try {
        Connection conn(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT);
        if (!conn.connect()) {
            std::cout << "连接失败，跳过错误处理测试" << std::endl;
            return;
        }
        
        // 测试SQL语法错误
        std::cout << "1. 测试SQL语法错误处理..." << std::endl;
        try {
            conn.executeQuery("SELECT * FROM non_existent_table");
            std::cout << "应该抛出异常" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "正确捕获异常: " << e.what() << std::endl;
        }
        
        // 测试字符串转义
        std::cout << "\n2. 测试字符串转义..." << std::endl;
        std::string dangerousInput = "Robert'); DROP TABLE test_users; --";
        std::string escaped = conn.escapeString(dangerousInput);
        std::cout << "原始字符串: " << dangerousInput << std::endl;
        std::cout << "转义后字符串: " << escaped << std::endl;
        
        // 测试错误码获取
        std::cout << "\n3. 测试错误信息获取..." << std::endl;
        try {
            conn.executeQuery("INVALID SQL STATEMENT");
        } catch (...) {
            std::cout << "错误码: " << conn.getLastErrorCode() << std::endl;
            std::cout << "错误信息: " << conn.getLastError() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "错误处理测试失败: " << e.what() << std::endl;
    }
}

void testPerformance() {
    printSeparator("测试基础性能");
    
    try {
        Connection conn(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT);
        if (!conn.connect()) {
            std::cout << "连接失败，跳过性能测试" << std::endl;
            return;
        }
        
        // 清空表
        conn.executeUpdate("DELETE FROM test_users");
        
        // 测试批量插入性能
        std::cout << "1. 测试批量插入性能..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 100; ++i) {
            std::string sql = "INSERT INTO test_users (name, age, email) VALUES "
                             "('用户" + std::to_string(i) + "', " + std::to_string(20 + i % 30) + 
                             ", 'user" + std::to_string(i) + "@test.com')";
            conn.executeUpdate(sql);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "插入100条记录耗时: " << duration.count() << " 毫秒" << std::endl;
        
        // 测试查询性能
        std::cout << "\n2. 测试查询性能..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 50; ++i) {
            auto result = conn.executeQuery("SELECT * FROM test_users LIMIT 10");
            int count = 0;
            while (result->next()) {
                count++;
            }
        }
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "50次查询操作耗时: " << duration.count() << " 毫秒" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "性能测试失败: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "开始第2天数据库连接功能测试..." << std::endl;
    
    // 初始化日志系统
    Logger::getInstance().init("", LogLevel::INFO, true);
    
    try {
        // 运行各项测试
        testConfigStructures();
        
        // 设置测试环境
        if (!setupTestEnvironment()) {
            std::cout << "\n  无法设置测试环境，跳过数据库相关测试" << std::endl;
            std::cout << "请检查MySQL连接参数并重新运行测试" << std::endl;
            return 1;
        }
        
        testBasicConnection();
        testQueryOperations();
        testTransactionOperations();
        testErrorHandling();
        testPerformance();
        
        std::cout << "\n 恭喜！第2天所有测试都通过了！" << std::endl;
        std::cout << "你已经成功实现了：" << std::endl;
        std::cout << "灵活的配置管理系统" << std::endl;
        std::cout << "安全的查询结果封装" << std::endl;
        std::cout << "完整的数据库连接类" << std::endl;
        std::cout << "事务管理功能" << std::endl;
        std::cout << "完善的错误处理机制" << std::endl;
        std::cout << "\n明天我们将实现自定义重连逻辑，提升系统可靠性！" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "测试失败: " << e.what() << std::endl;
        return 1;
    }
}