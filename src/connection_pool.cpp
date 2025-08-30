#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include "connection_pool.h"
#include "pool_config.h"
#include "logger.h"
#include "utils.h"
#include <performance_monitor.h>

ConnectionPool::ConnectionPool() {
    LOG_DEBUG("ConnectionPool instance created");
    m_isRunning = false;
    m_totalConnections = 0;
}


ConnectionPool::~ConnectionPool() {
    LOG_DEBUG("ConnectionPool destructor called");
    shutdown();
}


ConnectionPtr ConnectionPool::createConnection() {
    try {
        DBConfig config = LoadBalancer::getInstance().getNextDatabase();

        // call connection method
        ConnectionPtr conn = std::make_shared<Connection>(
            config.host,
            config.user,
            config.password,
            config.database,
            config.port,
            m_config.reconnectInterval,
            m_config.reconnectAttempts
        );

        auto conn_res = conn->connect();
        if (!conn_res) {
            std::string error = "cannot create a connectionId";
            PerformanceMonitor::getInstance().recordConnectionFailed();
            throw std::runtime_error(error);
        }
        PerformanceMonitor::getInstance().recordConnectionCreated();
        // create the connection successfully
        LOG_DEBUG("create a connection successfully. connectionId: " + conn->getConnectionId());
        return conn;
    } catch(std::exception& e) {
        PerformanceMonitor::getInstance().recordConnectionFailed();
        LOG_ERROR("ConnectionPool::createConnection createConnection has error: " + std::string(e.what()));
        throw;
    }
    
}



void ConnectionPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_isRunning) {
            return;
        }

        m_isRunning = false;

        m_condition.notify_all();
    }
    
    // join all the healthCheckThread
    if (m_healthCheckThread.joinable()) {
        m_healthCheckThread.join();
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    // close all connections in the queue
    while (!m_idleConnections.empty()) {
        ConnectionPtr conn = m_idleConnections.front();
        conn->close();
        m_idleConnections.pop();
    }
    // close all connections in the activePool.
    auto activeConnectionIter = m_activeConnections.begin();
    while (activeConnectionIter != m_activeConnections.end()) {
        ConnectionPtr conn = activeConnectionIter->second;
        conn->close();
        ++activeConnectionIter;
    }
    // make the activeConnections clear
    m_activeConnections.clear();
    
    m_totalConnections = 0;
    
}

bool ConnectionPool::isInitialized() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isRunning;
    // return m_initialized;
}

size_t ConnectionPool::getActiveCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeConnections.size();
}

size_t ConnectionPool::getIdleCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_idleConnections.size();
}

size_t ConnectionPool::getTotalCount() const {
    return m_totalConnections.load();
}

std::string ConnectionPool::getStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string status = "ConnectionPool Status:\n";
    status += "  Running: " + std::string(m_isRunning ? "Yes" : "No") + "\n";
    status += "  Total Connections: " + std::to_string(m_totalConnections.load()) + "\n";
    status += "  Idle Connections: " + std::to_string(m_idleConnections.size()) + "\n";
    status += "  Active Connections: " + std::to_string(m_activeConnections.size()) + "\n";
    status += "  Min Connections: " + std::to_string(m_config.minConnections) + "\n";
    status += "  Max Connections: " + std::to_string(m_config.maxConnections) + "\n";
    status += "  Connection Timeout: " + std::to_string(m_config.connectionTimeout) + "ms\n";
    status += "  Max Idle Time: " + std::to_string(m_config.maxIdleTime) + "ms\n";

    return status;
}

PoolConfig ConnectionPool::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}


ConnectionPtr ConnectionPool::getConnection(unsigned int timeout) {

    if (!m_isRunning) {
        PerformanceMonitor::getInstance().recordConnectionFailed();
        throw std::runtime_error("Connection pool is not in running status, cannot get a Connection from the pool");
    }


    auto startTime = std::chrono::steady_clock::now();

    // use default value
    if (timeout == 0) {
        timeout = m_config.connectionTimeout;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    // compuete the timeout time point
    auto tiemoutPoint = std::chrono::steady_clock::now() + std::chrono::microseconds(timeout);
    // run loop
    while (true) {
        // fetch the idel connection from the FIFO queue
        if (m_idleConnections.size() > 0) {
            LOG_DEBUG("ConnectionPool::getConnection has ideal Connections");
            ConnectionPtr idelConnection = m_idleConnections.front();
            // check if it is valid connection
            if (this->validateConnection(idelConnection, false)) {
                m_idleConnections.pop();
                m_activeConnections[idelConnection->getConnectionId()] =  idelConnection;
                idelConnection->updateLastActiveTime();
                auto endTime = std::chrono::steady_clock::now();
                auto takenTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
                PerformanceMonitor::getInstance().recordConnectionAcquired(takenTime.count());
                return idelConnection;
            } else {
                m_totalConnections--;
                LOG_INFO("ConnectionPool::getConnection fetch ideal connection from the pool, but it is not valid, connectionId: " + idelConnection->getConnectionId());
                // continue looking for connections from the FIFO queue
                continue;
            }
        }

        
        if (m_totalConnections < m_config.maxConnections) {
        // try to create a connection, and return the connection
        try {
            // concurrently create Connection
            // unlock first
            lock.unlock();
            ConnectionPtr conn = createConnection();
            lock.lock();
            
            if (conn) {
                if (this->validateConnection(conn, false)) {
                    m_totalConnections++;
                    m_activeConnections[conn->getConnectionId()] = conn;
                    conn->updateLastActiveTime();
                    LOG_DEBUG("ConnectionPool::getConnection no avaliable connection and create a connection and success");
                    return conn;
                }
            }
        } catch(const std::exception& e) {
                LOG_ERROR("ConnectionPool::getConnection no avaliable connection and try to create a connection, but failed");
                if (!lock.owns_lock()) {
                    lock.lock();
                }
            }
        }
        LOG_INFO("no avaliable connections from the pool, and cannot create connections, try to wait other thread to release connection...");
        auto wait_result = m_condition.wait_until(lock, tiemoutPoint);
        if (wait_result == std::cv_status::timeout) {
            throw std::runtime_error("Timeout waiting for available connection after " + 
                                   std::to_string(timeout) + "ms");
        }
    }
}


void ConnectionPool::releaseConnection(ConnectionPtr connection) {
    
    if (connection == nullptr) {
        LOG_WARNING("Attempted to release null connection");
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    // remove the connecetion from the activeConnections
    auto connId = connection->getConnectionId();
    LOG_INFO("release  a connection conId: " + connId);
    m_activeConnections.erase(connId);
    auto usageTime = Utils::currentTimeMillis() - connection->getLastActiveTime();
    // check total Connections
    if (m_totalConnections > m_config.maxConnections) {
        connection->close();
        m_totalConnections--;
        m_condition.notify_all();
        PerformanceMonitor::getInstance().recordConnectionReleased(usageTime);
        return;
    }

    if (this->validateConnection(connection, false)) {
        // put the connection to the FIFO Queue
        m_idleConnections.push(connection);
    } else {
        connection->close();
        m_totalConnections--;
        // check if the pool needs to create a another new connection
        if (m_totalConnections < m_config.minConnections) {
            try {
                ConnectionPtr conn = createConnection();
                if (conn) {
                    if (this->validateConnection(conn, false)) {
                        m_totalConnections++;
                        // put it into FIFO queue
                        m_idleConnections.push(conn);
                        LOG_DEBUG("ConnectionPool::releaseConnection Replacement connection created: " + conn->getConnectionId());
                    }
                }
            } catch(std::exception& e) {
                LOG_ERROR("ConnectionPool::releaseConnection failed to create a replacement connection error msg: " + std::string(e.what()));
            }
        }
    }
    PerformanceMonitor::getInstance().recordConnectionReleased(usageTime);
    // notify other threads
    m_condition.notify_all();   
}


void ConnectionPool::init(const PoolConfig& config) {
    // grab the mutex lock
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isRunning) {
        LOG_WARNING("ConnectionPool::init init connectionPool, but it is in running status");
        return;
    }
    // if (m_initialized) {
    //     return;
    // }
    if (!config.isValid()) {
        throw std::runtime_error("ConnectionPool::init init connectionPool, but config is not valid");
        return;
    }
    m_config = config;
    try {
        // init load balancer
        LoadBalanceStrategy strategy = LoadBalanceStrategy::WEIGHTED;
        // disable copy constructor
        LoadBalancer& balancer = LoadBalancer::getInstance();
        // create connections
        size_t targetConnections = std::min(config.initConnections, config.maxConnections);
        size_t createdConnections = 0;
        for (size_t i = 1; i <= targetConnections; i++) {
            try {
                ConnectionPtr conn = createConnection();
                if (conn) {
                    // check if the conn is valid
                    if (!this->validateConnection(conn, false)) {
                        LOG_DEBUG("ConnectionPool::init creatd Connection but is valid: connectionId: " + conn->getConnectionId());
                        conn->close();
                    } else {
                        // put the connection into queue
                        m_idleConnections.push(conn);
                        m_totalConnections++;
                        createdConnections++;
                    }
                }  
            } catch(const std::exception& e) {
                LOG_ERROR("created Connection failed retry time " + std::to_string(i) + "error msg: " + e.what());
            }
        }

        LOG_DEBUG("the number of created connections is: " + std::to_string(createdConnections));

        // throw error when no connections has been built
        if (targetConnections > 0 && createdConnections == 0) {
            LOG_ERROR("no connections has been created");
            std::string error = "no connections has been created";
            throw std::runtime_error(error);
        }

        // if created connections count less than minConnections, log warning.
        if (createdConnections < m_config.minConnections) {
            LOG_WARNING("the number of created connections is: " + std::to_string(createdConnections) + " the number is less than minConnectons: " + std::to_string(m_config.minConnections));
        }
        // m_initialized = true;
        m_isRunning = true;
        // start a health-check thread
        m_healthCheckThread = std::thread([this]() -> void {
            return this->healthCheckWorker();
        });
        // no need to detach the thread
        // healthCheckThread.detach();
        
    } catch (const std::exception& e) {
        m_isRunning = false;
        // clear all created connections
        while (!m_idleConnections.empty()) {
            ConnectionPtr conn = m_idleConnections.front();
            m_idleConnections.pop();
            if (conn) {
                conn->close();
            }
        }
        m_totalConnections = 0;
        LOG_ERROR("ConnectionPool::init init connections has error, abort the process, err msg: " +  std::string(e.what()));
        throw;
    }
}


void ConnectionPool::initWithSingleDatabase(
    const PoolConfig& poolConfig,
    const std::string& host, 
    const std::string& user,
    const std::string& password, 
    const std::string& database,
    unsigned int port, 
    unsigned int weight) {
    LOG_INFO("initWithSingleDatabase called");
    LoadBalancer::getInstance().initSingleDatabase(host, user, password, database, port, weight);
    LOG_DEBUG("initSingleDatabase before called");
    init(poolConfig);
}


void ConnectionPool::initWithMultipleDatabases(
    const PoolConfig& poolConfig,
    const std::vector<DBConfig>& databases,
    LoadBalanceStrategy strategy) {
    
    LoadBalancer::getInstance().init(databases, strategy);

    init(poolConfig);
}


std::string ConnectionPool::getLoadBalancerStatus() const {
    try {
        return LoadBalancer::getInstance().getStatus();
    } catch (const std::exception& e) {
        return "Failed to get load balancer status: " + std::string(e.what());
    }
}


void ConnectionPool::setLoadBalanceStrategy(LoadBalanceStrategy strategy) {
    try {
        LoadBalancer::getInstance().setStrategy(strategy);
        LOG_INFO("Load balance strategy changed to: " + strategyToString(strategy));
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to change load balance strategy: " + std::string(e.what()));
        throw;
    }
}


LoadBalanceStrategy ConnectionPool::getLoadBalanceStrategy() const {
    try {
        return LoadBalancer::getInstance().getStrategy();
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to get load balance strategy: " + std::string(e.what()));
        return LoadBalanceStrategy::WEIGHTED;  // 返回默认值
    }
}


void ConnectionPool::healthCheckWorker() {
    
    while(m_isRunning) {
        try {
            // sleep
            std::this_thread::sleep_for(std::chrono::microseconds(m_config.healthCheckPeriod));
            if (!m_isRunning) {
                return;
            }
            LOG_INFO("ConnectionPool::healthCheckWorker perform health check");
            cleanupIdleConnections();
            ensureMinimumConnections();
            LOG_INFO("ConnectionPool::healthCheckWorker health check completed");

        } catch(std::exception& e) {
            LOG_ERROR("Error in health check worker: " + std::string(e.what()));
        }
    }
}

bool ConnectionPool::validateConnection(ConnectionPtr connection, bool allowReconnect) {

    if (!connection) {
        LOG_INFO("ConnectionPool::validateConnection conn is nullptr");
        return false;
    }

    if (connection->isValid(allowReconnect)) {
        LOG_INFO("ConnectionPool::validateConnection conn: " + connection->getConnectionId() + " is valid");
        return true;
    }
    LOG_INFO("ConnectionPool::validateConnection conn: " + connection->getConnectionId() + " is not valid");
    return false;
}


void ConnectionPool::ensureMinimumConnections() {
    std::lock_guard<std::mutex> lock(m_mutex);
    LOG_INFO("ConnectionPool::ensureMinimumConnections called. current totalConnections: " + std::to_string(m_totalConnections) + " minConnections: " + std::to_string(m_config.minConnections));
    int successCount = 0;
    if (m_totalConnections < m_config.minConnections) { 
        size_t neededCreationCount = m_config.minConnections - m_totalConnections;
        LOG_INFO("ConnectionPool::ensureMinimumConnections need to create more connections: " + std::to_string(neededCreationCount));
        for (size_t i = 1; i <= neededCreationCount; i++) {
            ConnectionPtr conn = createConnection();
            // check if it is a valid conn
            if (validateConnection(conn, false)) {
                m_idleConnections.push(conn);
                m_totalConnections++;
                successCount++;
                LOG_INFO("ConnectionPool::ensureMinimumConnections created connection: " + conn->getConnectionId() + " success.");
            } else {
                conn->close();
                LOG_INFO("ConnectionPool::ensureMinimumConnections created connection: " + conn->getConnectionId() + " is not valid.");
            }
        }
    }
    LOG_INFO("ConnectionPool::ensureMinimumConnections successfully created: " + std::to_string(successCount) + " connections");
    return;
}

void ConnectionPool::cleanupIdleConnections() {
    std::lock_guard<std::mutex> lock(m_mutex);
    LOG_INFO("ConnectionPool::cleanupIdleConnections called");

    LOG_INFO("ConnectionPool::cleanupIdleConnections before the cleanup stage, current pool has: " + std::to_string(m_idleConnections.size()) + "connections" );
    std::queue<ConnectionPtr> keepConnections;
    
    int64_t now = Utils::currentTimeMillis();

    while(!m_idleConnections.empty()) {
        ConnectionPtr conn = m_idleConnections.front();
        // popup the conn
        m_idleConnections.pop();
        bool isValid = conn->isValidQuietly();
        // the reason the pool discard the conn
        std::string reason;
        // check if it is a died connection
        if (isValid) {
            // check if it is outdated
            int64_t lastActiveTime = conn->getLastActiveTime();
            //auto nowTime = std::chrono::steady_clock::now();
            int64_t idelTime = now - lastActiveTime;

            if (idelTime <= static_cast<int64_t>(m_config.maxIdleTime)) {
                keepConnections.push(conn);
                LOG_INFO("ConnectionPool::cleanupIdleConnections conn is kept, connId: " + conn->getConnectionId()); 
                continue;
            } else {
                // is outdated, but current connection pool does not have enough connections. push back the connection to the pool
                if (m_totalConnections < m_config.minConnections) {
                    keepConnections.push(conn);
                    LOG_INFO("ConnectionPool::cleanupIdleConnections conn is kept, connId: " + conn->getConnectionId()); 
                    continue;
                }
            } 
        }
        // discard the conn
        conn->close();
        m_totalConnections--;
        LOG_INFO("ConnectionPool::cleanupIdleConnections conn is cleaned up, connId: " + conn->getConnectionId());        
    }

    m_idleConnections = std::move(keepConnections);
    LOG_INFO("ConnectionPool::cleanupIdleConnections after the cleanup stage, current pool has: " + std::to_string(m_idleConnections.size()) + "connections" );
    return;
}


void ConnectionPool::performHealthCheck() {
    if (!m_isRunning) {
        LOG_WARNING("Cannot perform health check: connection pool is not running");
        return;
    }

    LOG_INFO("Manual health check triggered");
    
    try {
        cleanupIdleConnections();
        ensureMinimumConnections();
        LOG_INFO("Manual health check completed successfully");
    } catch (const std::exception& e) {
        LOG_ERROR("Manual health check failed: " + std::string(e.what()));
        throw;
    }
}



void ConnectionPool::shrinkPoolToSize(unsigned int targetSize) {
    
    LOG_INFO("ConnectionPool::shrinkPoolToSize targetSize: " + std::to_string(targetSize)); 
    
    if (!m_isRunning) {
        return;
    }

    if (m_totalConnections <= targetSize) {
        return;
    }

    size_t needToRemoveCount = m_totalConnections - targetSize;
    LOG_INFO("ConnectionPool::shrinkPoolToSize needToRemoveCount: " + std::to_string(needToRemoveCount)); 

    size_t removedCount = 0;
    // remove the connection from the idelPool
    while(m_idleConnections.size() > 0 && m_totalConnections > targetSize) {
        ConnectionPtr conn = m_idleConnections.front();
        conn->close();
        m_totalConnections--;
        m_idleConnections.pop();
        removedCount++;
    }
    LOG_INFO("ConnectionPool::shrinkPoolToSize removed connections count: " + std::to_string(removedCount));
    return;
}

bool ConnectionPool::adjustConfiguration(const PoolConfig& newConfig) {
    std::lock_guard<std::mutex> lock(m_mutex);
    LOG_INFO("ConnectionPool::adjustConfiguration called");
    PoolConfig& oldConfig = m_config;
    try {
        m_config = newConfig;
        if (m_totalConnections > newConfig.maxConnections) {
            shrinkPoolToSize(newConfig.maxConnections);
        }
        // Only perform shrink operation, healthChcek thread is responsbile for creating more connections to meet the mini threshold
        LOG_INFO("ConnectionPool::adjustConfiguration adjust successfully");
        return true;
    } catch(std::exception& e) {
        m_config = oldConfig;
        LOG_ERROR("ConnectionPool::adjustConfiguration has error: roll back" + std::string(e.what()));
        return false;
    }
}

bool ConnectionPool::setConnectionLimits(unsigned int minConnections, unsigned int maxConnections) {
    // param validation
    if (minConnections > maxConnections) {
        LOG_ERROR("ConnectionPool::setConnectionLimits param validation failed: minConnections: " + std::to_string(minConnections) + " maxConnections: " + std::to_string(maxConnections));
        return false;
    }

    PoolConfig config = m_config;
    config.minConnections = minConnections;
    config.maxConnections = maxConnections;
    LOG_INFO("ConnectionPool::setConnectionLimits minConnections: " + std::to_string(minConnections) + " maxConnections: " + std::to_string(maxConnections));
    return adjustConfiguration(config);
}

bool ConnectionPool::setTimeoutSettings(unsigned int connectionTimeout, unsigned int maxIdleTime, unsigned int healthCheckPeriod) {
    if (connectionTimeout == 0 || maxIdleTime == 0 || healthCheckPeriod == 0) {
        LOG_ERROR("Invalid timeout settings: values cannot be zero");
        return false;
    }
    PoolConfig config = m_config;
    config.connectionTimeout = connectionTimeout;
    config.maxIdleTime = maxIdleTime;
    config.healthCheckPeriod = healthCheckPeriod;
    return adjustConfiguration(config);
}



std::string ConnectionPool::getDetailedStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::stringstream ss;
    ss << "=== Connection Pool Detailed Status ===\n";
    ss << "Pool State:\n";
    ss << "  Running: " << (m_isRunning ? "Yes" : "No") << "\n";
    ss << "  Total Connections: " << m_totalConnections.load() << "\n";
    ss << "  Idle Connections: " << m_idleConnections.size() << "\n";
    ss << "  Active Connections: " << m_activeConnections.size() << "\n";
    
    ss << "Configuration:\n";
    ss << "  Min Connections: " << m_config.minConnections << "\n";
    ss << "  Max Connections: " << m_config.maxConnections << "\n";
    ss << "  Connection Timeout: " << m_config.connectionTimeout << "ms\n";
    ss << "  Max Idle Time: " << m_config.maxIdleTime << "ms\n";
    ss << "  Health Check Period: " << m_config.healthCheckPeriod << "ms\n";
    
    ss << "Health Status:\n";
    ss << "  Pool Utilization: " << std::fixed << std::setprecision(1)
       << (double)m_activeConnections.size() / m_config.maxConnections * 100 << "%\n";
    
    if (!m_activeConnections.empty()) {
        ss << "Active Connections:\n";
        for (const auto& pair : m_activeConnections) {
            ss << "  [" << pair.first << "] - Active since: " 
               << pair.second->getLastActiveTime() << "\n";
        }
    }
    
    ss << "=======================================";
    return ss.str();
}