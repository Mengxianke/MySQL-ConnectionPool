#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include <string>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include "connection.h"
#include "pool_config.h"
#include "logger.h"
#include "load_balancer.h"


/**
 * @brief A singleton class for connection
 * 
 * Connection pool
 * store reusable connection
 */
class ConnectionPool {

public:

// singleton
static ConnectionPool& getInstance() {
    static ConnectionPool instance;
    return instance;
}

// disable copy constructor and copy assingments
ConnectionPool(const ConnectionPool&) = delete;
ConnectionPool& operator=(const ConnectionPool&) = delete;

~ConnectionPool();


/**
     * @brief init connction pool
     * @param config pool config
     * @throws std::runtime_error 
     * 
     * 1. store pool config
     * 2. init load balancer
     * 3. create connections
     * 4. start a health-check thread
     */
void init(const PoolConfig& config);

/**
     * @brief showdown connection pool
     * 
     * 1. close all alive connctions
     * 2. clear all resources
     * 3. stop the health-check thread
     */
void shutdown(); 

/**
     * @brief check if connection pool has been initalized
     * @return isIntialized
     */
bool isInitialized() const;


/**
     * @brief get a available connection
     * @param timeout 
     * @return a shared pointer that points to a connection
     * @throws std::runtime_error 
     * 
     * 1. check if has a avaliable connection in FIFO queue
     * 2. if no avaliable connection left, and does not reach to connection limitations, try to create a connection
     * 3. if no condtions met, wait other thread to release connection
     * 4. validate connection
     * 5. add the connection to the m_activeConnections
     */
ConnectionPtr getConnection(unsigned int timeout = 0);

/**
     * @brief release a connection
     * @param connection connection to be released 
     * @return void
     * 
     * 1. remove the connection from the m_activeConnections
     * 2. validate the connection
     * 3. if the connection is still valid, add the connection to the backup queue
     * 4. if the connection is not valid, destory the connection.
     * 5. notify other thread
     */
void releaseConnection(ConnectionPtr connection);

/**
     * @brief get count of idle connection in the queue
     * @return count
     */
size_t getIdleCount() const;

/**
     * @brief get count of the active connections
     * @return count
     */
size_t getActiveCount() const;


/**
     * @brief get the total count of connections
     * @return count
     */
size_t getTotalCount() const;

std::string getStatus() const;

PoolConfig getConfig() const;

std::string getLoadBalancerStatus() const;

void setLoadBalanceStrategy(LoadBalanceStrategy strategy);

LoadBalanceStrategy getLoadBalanceStrategy() const;


void initWithSingleDatabase(const PoolConfig& poolConfig, const std::string& host, const std::string& user, const std::string& password, const std::string& database, unsigned int port = 3306, unsigned int weight = 1);

                               
void initWithMultipleDatabases(const PoolConfig& poolConfig, const std::vector<DBConfig>& databases, LoadBalanceStrategy strategy = LoadBalanceStrategy::WEIGHTED);


std::string getDetailedStatus() const;

// performHealthCheck Manually
void performHealthCheck();


bool adjustConfiguration(const PoolConfig& newConfig);

bool setConnectionLimits(unsigned int minConnections, unsigned int maxConnections);

bool setTimeoutSettings(unsigned int connectionTimeout, unsigned int maxIdleTime, unsigned int healthCheckPeriod);


private:
    // private Constructor
    ConnectionPool();

    PoolConfig m_config;
    // queue for connection storage
    std::queue<ConnectionPtr> m_idleConnections;
    // map that maps active connection's id to shared_ptr pointing to active connection
    std::map<std::string, ConnectionPtr> m_activeConnections;

    // threads synchroniztion
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;

    // connection pool status
    std::atomic<bool> m_isRunning;
    std::atomic<size_t> m_totalConnections;
    
    // background thread for connditions' health check
    std::thread m_healthCheckThread;


    ConnectionPtr createConnection();

    // healthCheckWorker
    // used to perform health check
    // 1. clean up all outdated connections
    // 2. clean up all died connections
    // 3. create connections to meet the minConnection condition.
    void healthCheckWorker();

    void cleanupIdleConnections();
    // ensure Minimum Connections
    void ensureMinimumConnections();
    // validate connection when release connection
    bool validateConnection(ConnectionPtr connection, bool allowReconnect);

    void shrinkPoolToSize(unsigned int targetSize);
    

    // =========================
    // 常量定义
    // =========================
    
    static const unsigned int DEFAULT_HEALTH_CHECK_INTERVAL = 30000; // 30秒
    static const unsigned int DEFAULT_CONNECTION_TIMEOUT = 5000;     // 5秒
    static const unsigned int DEFAULT_MAX_IDLE_TIME = 600000;        // 10分钟

};


#endif // CONNECTION_POOL_H