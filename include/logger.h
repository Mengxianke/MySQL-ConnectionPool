#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <iostream>

/**
 * @brief 日志级别枚举
 * 级别越高，输出的日志越少
 */
enum class LogLevel {
DEBUG = 0,   // 调试信息：详细的程序执行信息
INFO = 1,    // 一般信息：程序正常运行信息
WARNING = 2, // 警告信息：潜在问题，但不影响运行
ERROR = 3,   // 错误信息：出现错误，但程序可以继续
FATAL = 4    // 致命错误：严重错误，程序可能崩溃
};

/**
 * @brief 线程安全的日志类，使用单例模式
 * 
 * 特点：
 * 1. 单例模式：全局唯一实例，避免资源冲突
 * 2. 线程安全：多线程环境下安全使用
 * 3. 灵活输出：支持文件和控制台输出
 * 4. 格式化：自动添加时间戳和日志级别
 */
class Logger {
public:
/**
     * @brief 获取日志单例实例
     * @return Logger单例的引用
     */
static Logger& getInstance() {
    static Logger instance;  // C++11保证线程安全的单例
    return instance;
}

/**
     * @brief 初始化日志系统
     * @param logFile 日志文件路径，如果为空则只输出到控制台
     * @param level 日志级别，低于此级别的日志不会输出
     * @param toConsole 是否同时输出到控制台
     */
void init(const std::string& logFile = "", LogLevel level = LogLevel::INFO, bool toConsole = true) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_level = level;
    m_toConsole = toConsole;

    // 如果指定了日志文件，尝试打开
    if (!logFile.empty()) {
        m_fileStream.open(logFile, std::ios::app);  // 追加模式
        if (!m_fileStream.is_open()) {
            std::cerr << "Failed to open log file: " << logFile << std::endl;
        }
    }

    m_initialized = true;

    // 记录初始化信息
    std::cout << "Logger initialized, level=" + levelToString(level) + 
             ", file=" + (logFile.empty() ? "none" : logFile) << std::endl;
}

/**
     * @brief 记录调试日志
     * @param message 日志消息
     */
void debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

/**
     * @brief 记录信息日志
     * @param message 日志消息
     */
void info(const std::string& message) {
    log(LogLevel::INFO, message);
}

/**
     * @brief 记录警告日志
     * @param message 日志消息
     */
void warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

/**
     * @brief 记录错误日志
     * @param message 日志消息
     */
void error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

/**
     * @brief 记录致命错误日志
     * @param message 日志消息
     */
void fatal(const std::string& message) {
    log(LogLevel::FATAL, message);
}

/**
     * @brief 设置日志级别
     * @param level 新的日志级别
     */
void setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level = level;
}

/**
     * @brief 获取当前日志级别
     * @return 当前日志级别
     */
LogLevel getLevel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_level;
}

private:
// 私有构造函数（单例模式）
Logger() : m_level(LogLevel::DEBUG), m_initialized(false), m_toConsole(true) {}

// 禁用拷贝构造和赋值操作（单例模式）
Logger(const Logger&) = delete;
Logger& operator=(const Logger&) = delete;

/**
     * @brief 记录日志的内部方法
     * @param level 日志级别
     * @param message 日志消息
     */
    void log(LogLevel level, const std::string& message) {
        // 如果未初始化，使用默认设置初始化
        if (!m_initialized) {
            init();
        }

        // 如果日志级别低于设置的级别，则忽略
        if (level < m_level) {
            return;
        }

        // 格式化日志消息
        std::string formattedMessage = formatMessage(level, message);

        // 加锁保证线程安全
        std::lock_guard<std::mutex> lock(m_mutex);

        // 输出到文件
        if (m_fileStream.is_open()) {
            m_fileStream << formattedMessage << std::endl;
            m_fileStream.flush();  // 立即刷新到磁盘
        }

        // 输出到控制台
        if (m_toConsole) {
            if (level == LogLevel::ERROR || level == LogLevel::FATAL) {
                std::cerr << formattedMessage << std::endl;  // 错误输出到stderr
            } else {
                std::cout << formattedMessage << std::endl;  // 普通输出到stdout
            }
        }
    }

    /**
     * @brief 格式化日志消息
     * @param level 日志级别
     * @param message 原始消息
     * @return 格式化后的消息
     */
    std::string formatMessage(LogLevel level, const std::string& message) {
        std::stringstream ss;

        // 添加时间戳
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();

        // 添加日志级别
        ss << " [" << levelToString(level) << "] ";

        // 添加消息
        ss << message;

        return ss.str();
    }

    /**
     * @brief 将日志级别转换为字符串
     * @param level 日志级别
     * @return 对应的字符串
     */
    std::string levelToString(LogLevel level) {
        switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
        default:                return "UNKNOWN";
        }
    }

private:
    mutable std::mutex m_mutex;       // 互斥锁，确保线程安全，mutable允许在const方法中使用
    std::ofstream m_fileStream;       // 日志文件流
    LogLevel m_level;                 // 当前日志级别
    bool m_initialized;               // 是否已初始化
    bool m_toConsole;                 // 是否输出到控制台
};

// 方便使用的宏定义
#define LOG_DEBUG(message)   Logger::getInstance().debug(message)
#define LOG_INFO(message)    Logger::getInstance().info(message)
#define LOG_WARNING(message) Logger::getInstance().warning(message)
#define LOG_ERROR(message)   Logger::getInstance().error(message)
#define LOG_FATAL(message)   Logger::getInstance().fatal(message)

#endif // LOGGER_H
