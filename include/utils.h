#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <sstream>
#include <random>
#include <chrono>

/**
 * @brief 通用工具类和功能
 * 提供项目中常用的工具函数，包括字符串处理、时间获取、随机数生成等
 */
namespace Utils {

    /**
     * @brief 将字符串按指定分隔符分割为子字符串向量
     * @param str 要分割的字符串
     * @param delimiter 分隔符字符
     * @return 分割后的子字符串向量
     * 
     * 示例：
     * split("hello,world,test", ',') 返回 {"hello", "world", "test"}
     */
    inline std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);

        // 使用getline按分隔符读取
        while (std::getline(tokenStream, token, delimiter)) {
            if (!token.empty()) {  // 过滤空字符串
                tokens.push_back(token);
            }
        }
        return tokens;
    }

    /**
     * @brief 生成指定长度的随机字符串
     * @param length 字符串长度
     * @return 随机生成的字符串（包含数字和字母）
     * 
     * 用途：生成连接ID、会话标识等
     */
    inline std::string generateRandomString(size_t length) {
        // 字符集：数字+大小写字母
        static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

        std::string result;
        result.reserve(length);  // 预分配内存，提高性能

        // 使用C++11线程安全的随机数生成器
        static thread_local std::mt19937 rng{std::random_device{}()};
        static thread_local std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);

        for (size_t i = 0; i < length; ++i) {
            result += charset[dist(rng)];
        }

        return result;
    }

    /**
     * @brief 获取当前时间戳（毫秒）
     * @return 自1970年1月1日以来的毫秒数
     * 
     * 用途：记录连接创建时间、计算连接使用时长等
     */
    inline int64_t currentTimeMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /**
     * @brief 获取当前时间戳（微秒）
     * @return 自1970年1月1日以来的微秒数
     * 
     * 用途：精确的性能测量
     */
    inline int64_t currentTimeMicros() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    /**
     * @brief 将任意类型数据转换为字符串
     * @tparam T 数据类型
     * @param value 要转换的值
     * @return 转换后的字符串
     * 
     * 示例：toString(123) 返回 "123"
     */
    template<typename T>
    std::string toString(const T& value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }
    
   /**
 * @brief MySQL专用的SQL字符串转义函数
 * @param str 要转义的字符串
 * @return 转义后的字符串
 * 
 * 基于MySQL官方文档的转义规则：
 * https://dev.mysql.com/doc/refman/8.0/en/string-literals.html
 * 
 * 注意：这是简化版本，生产环境建议使用MySQL的mysql_real_escape_string
 */
inline std::string escapeMySQLString(const std::string& str) {
    std::string escaped;
    escaped.reserve(str.length() * 2);  // 预留足够空间，避免频繁重新分配
    
    for (char c : str) {
        switch (c) {
            case '\0':  escaped += "\\0";  break;   // NULL字符
            case '\n':  escaped += "\\n";  break;   // 换行符
            case '\r':  escaped += "\\r";  break;   // 回车符
            case '\\':  escaped += "\\\\"; break;   // 反斜杠
            case '\'':  escaped += "\\'";  break;   // 单引号（最重要的）
            case '"':   escaped += "\\\""; break;   // 双引号
            case '\x1a': escaped += "\\Z"; break;   // Ctrl+Z (Windows EOF)
            case '\t':  escaped += "\\t";  break;   // 制表符
            case '\b':  escaped += "\\b";  break;   // 退格符
            default:    escaped += c;     break;   // 其他字符直接添加
        }
    }
    return escaped;
}

/**
 * @brief 构建安全的SQL查询字符串
 * @param value 要插入的值
 * @return 带引号的转义后字符串
 */
inline std::string quoteMySQLString(const std::string& value) {
    return "'" + escapeMySQLString(value) + "'";
}

    /**
     * @brief 格式化字节大小为人类可读的字符串
     * @param bytes 字节数
     * @return 格式化后的字符串（如 "1.5 KB", "2.3 MB"）
     */
    inline std::string formatBytes(uint64_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024.0 && unit < 4) {
            size /= 1024.0;
            ++unit;
        }

        std::ostringstream oss;
        oss.precision(1);
        oss << std::fixed << size << " " << units[unit];
        return oss.str();
    }

    /**
     * @brief 去除字符串首尾的空白字符
     * @param str 要处理的字符串
     * @return 去除空白字符后的字符串
     */
    inline std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r\f\v");
        if (first == std::string::npos) {
            return "";
        }
        
        size_t last = str.find_last_not_of(" \t\n\r\f\v");
        return str.substr(first, (last - first + 1));
    }

} // namespace Utils

#endif // UTILS_H