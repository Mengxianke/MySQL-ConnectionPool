#ifndef QUERY_RESULT_H
#define QUERY_RESULT_H

#include <string>
#include <vector>
#include <memory>
#include <mysql/mysql.h>
#include <stdexcept>
#include "logger.h"

/**
 * @brief MySQL查询结果封装类
 * 
 * 这个类封装了MySQL的查询结果，提供了安全、便捷的数据访问接口
 * 
 * 设计原则：
 * 1. RAII：构造时获取资源，析构时自动释放
 * 2. 类型安全：提供强类型的数据访问方法
 * 3. 异常安全：所有可能出错的操作都有适当的错误处理
 * 4. 易用性：提供直观的API，支持索引和字段名两种访问方式
 */
class QueryResult {
public:
    /**
     * @brief 构造函数
     * @param result MySQL结果集指针（可以为nullptr，表示无结果集的操作）
     * @param affectedRows 受影响的行数（用于INSERT/UPDATE/DELETE等操作）
     * 
     * 注意：构造函数会接管result的所有权，析构时自动释放
     */
    explicit QueryResult(MYSQL_RES* result, unsigned long long affectedRows = 0);

    /**
     * @brief 析构函数，自动释放MySQL结果集
     * 这是RAII原则的体现：资源获取即初始化，对象销毁即资源释放
     */
    ~QueryResult();

    // 禁用拷贝构造和拷贝赋值（避免重复释放资源）
    QueryResult(const QueryResult&) = delete;
    QueryResult& operator=(const QueryResult&) = delete;

    // 允许移动构造和移动赋值（C++11特性）
    QueryResult(QueryResult&& other) noexcept;
    QueryResult& operator=(QueryResult&& other) noexcept;

    // =========================
    // 结果集导航方法
    // =========================

    /**
     * @brief 移至下一行
     * @return 是否成功移动到下一行（false表示已到末尾）
     * 
     * 使用示例：
     * while (result.next()) {
     *     std::string name = result.getString("name");
     *     // 处理每一行数据
     * }
     */
    bool next();

    /**
     * @brief 重置到第一行（如果支持）
     * @return 是否成功重置
     * 注意：某些MySQL配置可能不支持重置
     */
    bool reset();

    // =========================
    // 元数据获取方法
    // =========================

    /**
     * @brief 获取字段数量
     * @return 结果集中的字段数量
     */
    unsigned int getFieldCount() const;

    /**
     * @brief 获取行数
     * @return 结果集中的总行数
     * 注意：只有使用mysql_store_result()才能获取准确行数
     */
    unsigned long long getRowCount() const;

    /**
     * @brief 获取受影响的行数
     * @return 受INSERT/UPDATE/DELETE等操作影响的行数
     */
    unsigned long long getAffectedRows() const;

    /**
     * @brief 获取所有字段名
     * @return 字段名向量
     */
    std::vector<std::string> getFieldNames() const;

    // =========================
    // 数据访问方法（按索引）
    // =========================

    /**
     * @brief 获取指定索引的字段值（字符串）
     * @param index 字段索引（从0开始）
     * @return 字段值（NULL值返回空字符串）
     */
    std::string getString(unsigned int index) const;

    /**
     * @brief 获取指定索引的字段值（整数）
     * @param index 字段索引
     * @return 字段值（NULL值返回0）
     * @throws std::invalid_argument 如果无法转换为整数
     */
    int getInt(unsigned int index) const;

    /**
     * @brief 获取指定索引的字段值（长整数）
     * @param index 字段索引
     * @return 字段值（NULL值返回0）
     * @throws std::invalid_argument 如果无法转换为长整数
     */
    long long getLong(unsigned int index) const;

    /**
     * @brief 获取指定索引的字段值（浮点数）
     * @param index 字段索引
     * @return 字段值（NULL值返回0.0）
     * @throws std::invalid_argument 如果无法转换为浮点数
     */
    double getDouble(unsigned int index) const;

    /**
     * @brief 检查指定索引的字段是否为NULL
     * @param index 字段索引
     * @return 是否为NULL
     */
    bool isNull(unsigned int index) const;

    // =========================
    // 数据访问方法（按字段名）
    // =========================

    /**
     * @brief 获取指定名称的字段值（字符串）
     * @param fieldName 字段名称
     * @return 字段值
     * @throws std::out_of_range 如果字段名不存在
     */
    std::string getString(const std::string& fieldName) const;

    /**
     * @brief 获取指定名称的字段值（整数）
     * @param fieldName 字段名称
     * @return 字段值
     */
    int getInt(const std::string& fieldName) const;

    /**
     * @brief 获取指定名称的字段值（长整数）
     * @param fieldName 字段名称
     * @return 字段值
     */
    long long getLong(const std::string& fieldName) const;

    /**
     * @brief 获取指定名称的字段值（浮点数）
     * @param fieldName 字段名称
     * @return 字段值
     */
    double getDouble(const std::string& fieldName) const;

    /**
     * @brief 检查指定名称的字段是否为NULL
     * @param fieldName 字段名称
     * @return 是否为NULL
     */
    bool isNull(const std::string& fieldName) const;

    // =========================
    // 便利方法
    // =========================

    /**
     * @brief 检查结果集是否为空
     * @return 是否为空结果集
     */
    bool isEmpty() const;

    /**
     * @brief 检查是否有结果集
     * @return 是否存在结果集（区别于空结果集）
     */
    bool hasResultSet() const;

private:
    MYSQL_RES* m_result;                    // MySQL结果集指针
    MYSQL_ROW m_currentRow;                 // 当前行数据, 这是一行数据的类型安全表示, 它目前被实现为一个计数字节字符串的数组
    unsigned long* m_lengths;               // 当前行各字段的长度
    unsigned int m_fieldCount;              // 字段数量
    unsigned long long m_rowCount;          // 行数
    unsigned long long m_affectedRows;      // 受影响的行数
    std::vector<std::string> m_fieldNames;  // 字段名列表

    /**
     * @brief 初始化结果集信息
     * 从MySQL结果集中提取元数据信息
     */
    void initializeMetadata();

    /**
     * @brief 根据字段名获取字段索引
     * @param fieldName 字段名称
     * @return 字段索引
     * @throws std::out_of_range 如果字段名不存在
     */
    unsigned int getFieldIndex(const std::string& fieldName) const;

    /**
     * @brief 检查索引是否有效
     * @param index 索引值
     * @throws std::out_of_range 如果索引超出范围
     */
    void checkIndex(unsigned int index) const;

    /**
     * @brief 检查当前行是否有效
     * @throws std::runtime_error 如果当前没有有效行
     */
    void checkRow() const;
    int safeConvert(const char* value, int defaultValue) const;
    long long safeConvert(const char* value, long long defaultValue) const;
    double safeConvert(const char* value, double defaultValue) const;
#if 0
    /**
     * @brief 安全的字符串转换
     * @param value 要转换的字符串
     * @param defaultValue 默认值
     * @return 转换结果
     */
    template<typename T>
    T safeConvert(const char* value, T defaultValue) const;
#endif
};

// 智能指针类型定义，推荐使用方式
using QueryResultPtr = std::shared_ptr<QueryResult>;

#endif // QUERY_RESULT_H