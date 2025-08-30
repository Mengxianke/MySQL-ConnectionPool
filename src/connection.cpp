#include "connection.h"
#include "utils.h"
#include <stdexcept>
#include "query_result.h"
#include "db_exception.h"
#include "thread"
#include <algorithm>
#include <stdexcept>
#include "performance_monitor.h"

Connection::Connection(
    const std::string& host,
    const std::string& user,
    const std::string& password,
    const std::string& database,
    const unsigned int& port,
    unsigned int reconnectInterval,
    unsigned int reconnectAttempts   
    )
: m_mysql(nullptr)
, m_host(host)
, m_user(user)
, m_password(password)
, m_database(database)
, m_port(port)
, m_connectionId(Utils::generateRandomString(16))
, m_creationTime(Utils::currentTimeMillis())
, m_lastActiveTime(m_creationTime) 
, m_reconnectInterval(reconnectInterval)
, m_reconnectAttempts(reconnectAttempts)
, m_totalReconnectAttempts(0)
, m_successfulReconnects(0)
{  
    init();
}


// deConstructor
Connection::~Connection() {
    LOG_INFO("Destroying connection [" + m_connectionId + "]");
    close();
}


// init a connection
void Connection::init() {
    m_mysql = mysql_init(nullptr);
    if (!m_mysql) {
        std::string error = "Failed to initialize MySQL connection object";
        LOG_ERROR(error);
        throw std::runtime_error(error);
    }
    // set connection timeout
    unsigned int connectionTimeout = 5;
    // fetch the connectionTimeout address.
    if (mysql_options(m_mysql, MYSQL_OPT_CONNECT_TIMEOUT, &connectionTimeout) != 0) {
        LOG_WARNING("Failed to set connection timeout");
    }

    // read timeout
    unsigned int readTimout = 30;
    if (mysql_options(m_mysql, MYSQL_OPT_READ_TIMEOUT, &readTimout) != 0) {
        LOG_WARNING("Failed to set read timeout");
    }
    unsigned int writeTimeout = 30;
    if (mysql_options(m_mysql, MYSQL_OPT_WRITE_TIMEOUT, &writeTimeout) != 0) {
        LOG_WARNING("Failed to set write timeout");
    }   
    // 4. 设置字符集为UTF8MB4（支持emoji等4字节字符）
    if (mysql_options(m_mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4") != 0) {
        LOG_WARNING("Failed to set charset to utf8mb4");
    }

    // 5. 启用多语句执行（可选，根据需要）
    // mysql_options(m_mysql, MYSQL_OPTION_MULTI_STATEMENTS_ON, nullptr);

    LOG_DEBUG("MySQL connection object initialized [" + m_connectionId + "]");
}

// connect to mysql_server
bool Connection::connect() {
    // use mutex lock to avoid multiple thread to connect to mysql_server
    std::lock_guard<std::mutex> lock(m_mutex);
    MYSQL * result = mysql_real_connect(
        m_mysql, 
        m_host.c_str(), 
        m_user.c_str(), 
        m_password.c_str(), 
        m_database.c_str(), 
        m_port, 
        nullptr, 
        0);
    if (result == nullptr) {
        std::string error = mysql_error(m_mysql);
        unsigned int errorCode = mysql_errno(m_mysql);
        LOG_ERROR("Failed to connect to MySQL server [" + m_connectionId + "]: " +
                  error + " (Code: " + std::to_string(errorCode) + ")");
        return false;
    }
    updateLastActiveTime();
    LOG_DEBUG("Success to connect to MySQL server [" + m_connectionId + "]");
    return true;
}


// connect with retry
bool Connection::reconnect() {
    // use more flexible lock
    std::unique_lock<std::mutex> lock(m_mutex);

    // relase my_sql first
    if (m_mysql) {
        mysql_close(m_mysql);
        m_mysql = nullptr;
    }

    init();

    if (!m_mysql) {
        LOG_ERROR("Failed to initialize MySQL object during reconnection [" + m_connectionId + "]: ");
        PerformanceMonitor::getInstance().recordReconnection(false);
        return false;
    }    

    // iterate over attempt
    for (unsigned int attempt = 1; attempt <= m_reconnectAttempts; attempt++) {
        m_totalReconnectAttempts++;

        MYSQL * result = mysql_real_connect(
        m_mysql, 
        m_host.c_str(), 
        m_user.c_str(), 
        m_password.c_str(), 
        m_database.c_str(), 
        m_port, 
        nullptr, 
        0);

        if (result != nullptr) {
            m_successfulReconnects++;
            LOG_DEBUG("Success to reconnect to MySQL server [" + m_connectionId + "]"  + " attempt times:" + std::to_string(attempt));
            PerformanceMonitor::getInstance().recordReconnection(true);
            return true;
        }
        // record reconnection error
        // 重连失败，记录错误
        std::string error = mysql_error(m_mysql);
        unsigned int errorCode = mysql_errno(m_mysql);

        LOG_WARNING("Reconnection attempt " + std::to_string(attempt) + " failed [" +
                    m_connectionId + "]: " + error + " (Code: " + std::to_string(errorCode) + ")");
        // compute delay
        auto delay = calculateReconnectDelay(attempt);
        // wait duration
        if (attempt < m_reconnectAttempts) {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::microseconds(delay));
            lock.lock();
        }        
    }
    PerformanceMonitor::getInstance().recordReconnection(false);
    return false;
}


void Connection::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_mysql) {
        mysql_close(m_mysql);
        m_mysql = nullptr;
        LOG_INFO("MySQL connection closed [" + m_connectionId + "]");
    }
    
}


std::string Connection::getConnectionId() {
    return m_connectionId;
}


bool Connection::isValidQuietly() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mysql) {
        LOG_INFO("MySQL isNotValid  [" + m_connectionId + "]" + "since no m_mysql");
        return false;
    }
    
    auto ping_result = mysql_ping(m_mysql);
    if (ping_result != 0) { 
        LOG_INFO("MySQL isNotValid  [" + m_connectionId + "]" + "since ping result is not zero");
        return false;
    }   
    return true;
}

// isValid
bool Connection::isValid(bool tryReconnect) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_mysql) {
            LOG_INFO("MySQL isNotValid  [" + m_connectionId + "]" + "since no m_mysql");
            return false;
        }

        auto ping_result = mysql_ping(m_mysql);
        if (ping_result != 0) {
            LOG_INFO("MySQL isNotValid  [" + m_connectionId + "]" + "since ping result is not zero");
            // cannot ping mysql server
            // get errorCode
            unsigned int errorCode = mysql_errno(m_mysql);
            std::string errorMsg = mysql_error(m_mysql);
            if (!tryReconnect || !isConnectionError(errorCode)) {
                return false;
            }
        }
        // Valid
        LOG_INFO("MySQL isValid  [" + m_connectionId + "]");
        updateLastActiveTime();
        return true;
    }
    // release the lock
    // cannot ping mysql server and is Connection Error. try to reConnect to mysql server
    LOG_INFO("cannot ping mysql server and try to reconnect to the server [" + m_connectionId + "]");
    return reconnect();
}

// updateLastActiveTime
void Connection::updateLastActiveTime() {
    m_lastActiveTime = Utils::currentTimeMillis();
};


QueryResultPtr Connection::executeQuery(const std::string& sql) {
    return executeQueryWithReconnect(sql, true);
}

unsigned long long Connection::executeUpdate(const std::string& sql) {
    auto queryResult_ptr = executeQueryWithReconnect(sql, false);
    return queryResult_ptr? queryResult_ptr->getAffectedRows() : 0;
}


// exectueQuery.
// sql: row sql
// isQuery: used for query
QueryResultPtr Connection::executeInternal(const std::string& sql, bool isQuery) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_mysql) {
        std::string error = "Connection not established [" + m_connectionId + "]";
        LOG_ERROR(error);
        throw db::SQLExecutionError(error, CR_SERVER_GONE_ERROR);
    }

    // execute sql
    updateLastActiveTime();
    // run debug log
    LOG_DEBUG("Executing " + std::string(isQuery ? "query" : "update") + 
              " [" + m_connectionId + "]: " + sql);
    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        // erorr when execut query
        unsigned int errorCode = mysql_errno(m_mysql);
        std::string errorMsg = mysql_error(m_mysql);
        LOG_ERROR("Failed to execute " + std::string(isQuery ? "query" : "update") + 
                  " [" + m_connectionId + "]: " + errorMsg + ", SQL: " + sql);
        throw db::SQLExecutionError(errorMsg + " (Code: " + std::to_string(errorCode) + ")",
                                errorCode);
    }
    // use different api to get result
    if (isQuery) {
        MYSQL_RES * queryResult = mysql_store_result(m_mysql);
        // if not valid, have field count but queryReuslt is NULL , throw runtime error
        if (queryResult == nullptr && mysql_field_count(m_mysql) > 0) {
            unsigned int errorCode = mysql_errno(m_mysql);
            std::string errorMsg = mysql_error(m_mysql);
            throw db::SQLExecutionError("Failed to store result: " + errorMsg +
                                        " (Code: " + std::to_string(errorCode) + ")",
                                    errorCode);
        }
        return std::make_shared<QueryResult>(queryResult);
    } else {
        // not query operation
        unsigned long long affectedRows = mysql_affected_rows(m_mysql);
        return std::make_shared<QueryResult>(nullptr, affectedRows);
    }
}


QueryResultPtr Connection::executeQueryWithReconnect(const std::string& sql, bool isQuery) {
    auto startTime = std::chrono::steady_clock::now();
    // first retry mysql connection
    unsigned int errorCode = 0;
    std::string errorMessage;
    // retry query sql
    for (unsigned int attempt = 0; attempt <= m_reconnectAttempts; attempt++) {
        if (attempt > 0) {
            bool reconnect_res = reconnect();
            if (!reconnect_res) {
                // 
                errorMessage = "Failed to reconnect";
                errorCode = CR_SERVER_GONE_ERROR;
                LOG_WARNING("Reconnection failed [" + m_connectionId + "]");
                continue;
            }
        }

        try {
            auto queryResult = executeInternal(sql, isQuery);
            auto endTime = std::chrono::steady_clock::now();
            auto takenTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            PerformanceMonitor::getInstance().recordQueryExecuted(takenTime.count(), true);
            return queryResult;
        } catch(const db::SQLExecutionError& e) {
            // catch database errorMesg, and code
            errorCode = e.getErrorCode();
            errorMessage = e.what();
            
            if (!isConnectionError(errorCode)) {
                LOG_ERROR("exectuteQueryWithReconnection meet other errors, errorCode: " + std::to_string(errorCode));
                auto endTime = std::chrono::steady_clock::now();
                auto takenTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
                PerformanceMonitor::getInstance().recordQueryExecuted(takenTime.count(), false);
                throw std::runtime_error("exectuteQueryWithReconnection meet other errors");
            }

             // 是连接错误，记录日志，继续重试
            LOG_WARNING("Connection lost during " + std::string(isQuery ? "query" : "update") +
                        " execution [" + m_connectionId + "]: " + errorMessage);
        }
    }
    // 所有重试都失败了
    std::string error = "Failed to execute " + std::string(isQuery ? "query" : "update") +
                        " after " + std::to_string(m_reconnectAttempts + 1) + " attempts [" +
                        m_connectionId + "]: " + errorMessage + " SQL: " + sql;
    LOG_ERROR(error);
    auto endTime = std::chrono::steady_clock::now();
    auto takenTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    PerformanceMonitor::getInstance().recordQueryExecuted(takenTime.count(), false);
    throw std::runtime_error(error);
}


// =========================
// 事务管理方法（增强版）
// =========================
bool Connection::beginTransaction()
{
    LOG_DEBUG("Beginning transaction [" + m_connectionId + "]");

    try
    {
        // 明确处理返回值，虽然对事务开始来说返回值不重要
        auto result = executeInternal("START TRANSACTION", false);
        (void)result; // 明确标记不使用返回值，避免编译器警告

        LOG_DEBUG("Transaction started successfully [" + m_connectionId + "]");
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to begin transaction [" + m_connectionId + "]: " + e.what());
        return false;
    }
}

bool Connection::commit()
{
    LOG_DEBUG("Committing transaction [" + m_connectionId + "]");

    try
    {
        auto result = executeInternal("COMMIT", false);
        (void)result; // 明确标记不使用返回值

        LOG_DEBUG("Transaction committed successfully [" + m_connectionId + "]");
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to commit transaction [" + m_connectionId + "]: " + e.what());
        return false;
    }
}

bool Connection::rollback()
{
    LOG_DEBUG("Rolling back transaction [" + m_connectionId + "]");

    try
    {
        auto result = executeInternal("ROLLBACK", false);
        (void)result; // 明确标记不使用返回值

        LOG_DEBUG("Transaction rolled back successfully [" + m_connectionId + "]");
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to rollback transaction [" + m_connectionId + "]: " + e.what());
        return false;
    }
}


// get Last Error for mysql_server
// std::string Connection::getLastError() const {
//     if (!m_mysql) {
//         return "MySQL connection not initialized";
//     }
//     return mysql_error(m_mysql);
// };

// unsigned int Connection::getLastErrorCode() const {
//     if (!m_mysql) {
//         return 0;
//     }
//     return mysql_errno(m_mysql);
// }



std::string Connection::escapeString(const std::string& str) {
    if (!m_mysql) {
        LOG_ERROR("Connection not established, cannot escape string [" + m_connectionId + "]");
        throw std::runtime_error("Connection not established");
    }

    // 分配足够的缓冲区（最坏情况下每个字符都需要转义）
    std::vector<char> escaped(str.length() * 2 + 1);

    // 使用MySQL的转义函数
    unsigned long escapedLength = mysql_real_escape_string(
        m_mysql, 
        escaped.data(), 
        str.c_str(), 
        str.length()
    );

    return std::string(escaped.data(), escapedLength);
}


int64_t Connection::getCreationTime() const {
    return m_creationTime;
}

int64_t Connection::getLastActiveTime() const {
    return m_lastActiveTime;
}


bool Connection::isConnectionError(unsigned int errorCode) const {
    if (errorCode == 2002) {
        LOG_DEBUG("Connection meet error: errorCode: 2002");
        return true;
    } else if (errorCode == 2003) {
        LOG_DEBUG("Connection meet error: errorCode: 2003");
        return true;
    } else if (errorCode == 2006) {
        LOG_DEBUG("Connection meet error: errorCode: 2006");
        return true;
    } else if (errorCode == 2013) {
        LOG_DEBUG("Connection meet error: errorCode: 2013");
        return true;
    } else if (errorCode == 2027) {
        LOG_DEBUG("Connection meet error: errorCode: 2027");
        return true;
    } else if (errorCode == 2055) {
        LOG_DEBUG("Connection meet error: errorCode: 2055");
        return true;
    } else {
        LOG_DEBUG("is not Connection Error, errorCode: ");
        return false;
    }
}

// Compute reconnect delay
unsigned int Connection::calculateReconnectDelay(unsigned int attempt) {
    unsigned int baseDelay = m_reconnectInterval;

    // 计算指数退避延迟
    unsigned int exponentialDelay = baseDelay * (1 << (attempt - 1));
    unsigned int maxDelay = 30000;
    unsigned int delay = std::min(exponentialDelay, maxDelay);

    // 更安全的随机抖动计算
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(0.8, 1.2); // 80% 到 120%

    double jitteredDelay = delay * dist(rng);
    delay = static_cast<unsigned int>(std::max(1.0, jitteredDelay)); // 确保至少1ms

    LOG_DEBUG("Calculated reconnect delay: " + std::to_string(delay) +
              "ms for attempt " + std::to_string(attempt) +
              " [" + m_connectionId + "]");

    return delay;
}   



unsigned int Connection::getReconnectAttempts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_reconnectAttempts;
}


unsigned int Connection::getSuccessfulReconnects() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_successfulReconnects;
}


void Connection::resetReconnectStats()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_totalReconnectAttempts = 0;
    m_successfulReconnects = 0;
    LOG_INFO("Reconnection statistics reset [" + m_connectionId + "]");
}