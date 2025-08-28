#ifndef CONNECTION_H
#define CONNECTION_H


#include <string>
#include <memory>
// MYSQL C API
#include <mysql/mysql.h>
#include "query_result.h"
#include "logger.h"

// Connection Class
// used for connection to mysql server, and executeQuery
class Connection {
public:
    // /**
    //  * @brief 
    //  * @param host 主机名或IP地址
    //  * @param user 数据库用户名
    //  * @param password 数据库密码
    //  * @param database 数据库名称
    //  * @param port 端口号，默认3306
    //  */
    Connection(
        const std::string& host,
        const std::string& user,
        const std::string& password,
        const std::string& database,
        const unsigned int& port = 3306,
        unsigned int reconnectInterval = 1000,
        unsigned int reconnectAttempts = 3  
    );

    ~Connection();

    // disable copy constructor
    Connection(const Connection&) = delete;
    // delete copy assignment
    Connection& operator=(const Connection&) = delete;

    /**
     * @brief 连接到数据库
     * @return 是否成功连接
     * 
     * 使用示例：
     * Connection conn("localhost", "user", "pass", "testdb");
     * if (conn.connect()) {
     *     // 连接成功，可以执行操作
     * }
     */
    bool connect();


    bool reconnect();

     /**
     * @brief 关闭数据库连接
     * 通常不需要手动调用，析构函数会自动调用
     */
    void close();

     /**
     * @brief 检查连接是否有效
     * @return 连接是否有效且可用
     */
    bool isValid(bool tryReconnect = false);

    bool isValidQuietly() const;

    // =========================
    // 查询执行方法
    // =========================


    /**
     * @brief 执行SELECT查询
     * @param sql SQL查询语句
     * @return 查询结果的智能指针
     * @throws std::runtime_error 如果查询失败
     * 
     * 使用示例：
     * auto result = conn.executeQuery("SELECT * FROM users WHERE age > 18");
     * while (result->next()) {
     *     std::cout << result->getString("name") << std::endl;
     * }
     */
    QueryResultPtr executeQuery(const std::string& sql);


    /**
     * @brief 执行更新操作（INSERT, UPDATE, DELETE等）
     * @param sql SQL更新语句
     * @return 受影响的行数
     * @throws std::runtime_error 如果执行失败
     * 
     * 使用示例：
     * int affected = conn.executeUpdate("UPDATE users SET status = 1 WHERE id = 123");
     * std::cout << "Updated " << affected << " rows" << std::endl;
     */
    unsigned long long executeUpdate(const std::string& sql);

    // =========================
    // 事务管理方法
    // =========================
     /**
     * @brief 开始事务
     * @return 是否成功开始事务
     * 
     * 使用示例：
     * conn.beginTransaction();
     * try {
     *     conn.executeUpdate("INSERT INTO users ...");
     *     conn.executeUpdate("UPDATE accounts ...");
     *     conn.commit();
     * } catch (...) {
     *     conn.rollback();
     * }
     */
    bool beginTransaction();

    /**
     * @brief 提交事务
     * @return 是否成功提交
     */
    bool commit();

    /**
     * @brief 回滚事务
     * @return 是否成功回滚
     */
    bool rollback();

    // // =========================
    // // 错误处理方法
    // // =========================
    // /**
    //  * @brief 获取上次操作的错误信息
    //  * @return 错误信息字符串
    //  */
    // std::string getLastError() const;

    // /**
    //  * @brief 获取上次操作的错误代码
    //  * @return MySQL错误代码
    //  */
    // unsigned int getLastErrorCode() const;

    // =========================
    // 工具方法
    // =========================

    /**
     * @brief 转义字符串，防止SQL注入
     * @param str 要转义的字符串
     * @return 转义后的字符串
     * 
     * 使用示例：
     * std::string name = conn.escapeString(userInput);
     * std::string sql = "SELECT * FROM users WHERE name = '" + name + "'";
     */
    std::string escapeString(const std::string& str);
    
    /**
     * @brief 获取连接创建时间
     * @return 创建时间（毫秒时间戳）
     */
    int64_t getCreationTime() const;


     /**
     * @brief 获取最后活动时间
     * @return 最后活动时间（毫秒时间戳）
     */
    int64_t getLastActiveTime() const;

    /**
     * @brief 更新最后活动时间
     * 每次使用连接时都会调用此方法
     */
    void updateLastActiveTime();
    
    std::string getConnectionId();



    /**
     * @brief 检查是否是连接断开错误
     * @param errorCode 错误代码
     * @return 是否是连接断开错误
     *
     * 支持的连接错误码：
     * - 2002: CR_CONNECTION_ERROR
     * - 2003: CR_CONN_HOST_ERROR
     * - 2006: CR_SERVER_GONE_ERROR
     * - 2013: CR_SERVER_LOST
     * - 2027: CR_MALFORMED_PACKET
     * - 2055: CR_SERVER_LOST_EXTENDED
     */
    bool isConnectionError(unsigned int errorCode) const;


    /**
     * @brief 获取重连尝试次数
     * @return 总的重连尝试次数
     */
    unsigned int getReconnectAttempts() const;

    /**
     * @brief 获取成功重连次数
     * @return 成功重连的次数
     */
    unsigned int getSuccessfulReconnects() const;

    /**
     * @brief 重置重连统计
     */
    void resetReconnectStats();


private:
    MYSQL* m_mysql;    //msyql connection handler
    std::string m_host;
    std::string m_user;
    std::string m_password;
    std::string m_database;
    unsigned int m_port;
    std::string m_connectionId;
    int64_t m_creationTime;
    int64_t m_lastActiveTime;
    mutable std::mutex m_mutex; 


    // 重连相关参数
    unsigned int m_reconnectInterval; // 重连间隔（毫秒）
    unsigned int m_reconnectAttempts; // 最大重连尝试次数

    // 重连统计
    unsigned int m_totalReconnectAttempts; // 总重连尝试次数
    unsigned int m_successfulReconnects;   // 成功重连次数

    void init();

    // retryQuerySql
    QueryResultPtr executeQueryWithReconnect(const std::string& sql, bool isQuery);

    /**
     * @brief 执行SQL语句的内部方法
     * @param sql SQL语句
     * @param isQuery 是否是查询操作
     * @return 查询结果
     */
    QueryResultPtr executeInternal(const std::string& sql, bool isQuery);

    // Caculate Reconnect Delay
    // param: attempt times
    unsigned int calculateReconnectDelay(unsigned int attempt);

};

using ConnectionPtr = std::shared_ptr<Connection>;

#endif //CONNECTION_H