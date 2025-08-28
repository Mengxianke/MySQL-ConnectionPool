#include "load_balancer.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include "logger.h"
#include <stdexcept>


LoadBalancer::LoadBalancer()
    :m_strategy(LoadBalanceStrategy::WEIGHTED)
    ,m_roundRobinIndex(0)
    ,m_rng(std::random_device{}()) {
    
}

void LoadBalancer::init(const std::vector<DBConfig>& configs, LoadBalanceStrategy strategy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // check if the given configs is empty
    if (configs.empty()) {
        throw std::runtime_error("Cannot initialize load balancer with empty database configs");
    }
    // check if each config is valid
    for (size_t i = 0; i < configs.size(); i++) {
        DBConfig config = configs[i]; 
        if (!config.isValid()) {
            throw std::runtime_error("config is not valid");
            LOG_INFO("Database " + std::to_string(i) + ": " + config.user + "@" + 
            config.host + ":" + std::to_string(config.port) + "/" + config.database + 
            " (weight=" + std::to_string(config.weight) + ")");
        }
    }

    m_configs = configs;
    m_strategy = strategy;
    m_roundRobinIndex = 0;
    LOG_INFO("Load balancer initialized with strategy: " + strategyToString(strategy));
}


void LoadBalancer::initSingleDatabase(const std::string& host, const std::string& user,
                                     const std::string& password, const std::string& database,
                                     unsigned int port, unsigned int weight) {
    LOG_INFO("Initializing load balancer with single database: " + user + "@" + 
             host + ":" + std::to_string(port) + "/" + database);

    DBConfig config(host, user, password, database, port, weight);
    std::vector<DBConfig> configs = { config };
    init(configs, LoadBalanceStrategy::WEIGHTED);
}

DBConfig LoadBalancer::getNextDatabase() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_configs.empty()) {
        LOG_ERROR("No database configurations available");
        throw std::runtime_error("No database configurations available");
    }

    LOG_INFO("LoadBalancer::getNextDatabase current strategy is " + strategyToString(m_strategy));
    if (m_strategy == LoadBalanceStrategy::RANDOM) {
        return selectRandom();
    } else if (m_strategy == LoadBalanceStrategy::ROUND_ROBIN) {
        return selectRoundRobin();
    } else if (m_strategy == LoadBalanceStrategy::WEIGHTED) {
        return selectWeighted();
    } else {
        return selectWeighted();
    }
}


void LoadBalancer::setStrategy(LoadBalanceStrategy strategy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_strategy = strategy;
    LOG_INFO("LoadBalancer::setStrategy change strategy with: " + strategyToString(strategy));
    
    if (strategy == LoadBalanceStrategy::ROUND_ROBIN) {
         LOG_INFO("LoadBalancer::setStrategy rest round robin index to 0");
        m_roundRobinIndex = 0;
    }
}

LoadBalanceStrategy LoadBalancer::getStrategy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_strategy;   
}


DBConfig LoadBalancer::selectRandom() {
    LOG_INFO("LoadBalancer::selectRandom Use Random algorithm to selecte a database");
    // generate a distribution
    std::uniform_int_distribution<int> dist(0, m_configs.size() - 1);
    size_t selectedIndex = dist(m_rng);
    LOG_INFO("LoadBalancer::selectRandom select database index: " + std::to_string(selectedIndex));
    return m_configs[selectedIndex];
}


DBConfig LoadBalancer::selectRoundRobin() {
    LOG_INFO("LoadBalancer::selectRoundRobin Use Round Robin algorithm to select a database");
    DBConfig config = m_configs[m_roundRobinIndex];

    // update roundRobin index
    m_roundRobinIndex = (m_roundRobinIndex + 1) % m_configs.size();
    LOG_DEBUG("Round-robin algorithm selected index: " + std::to_string(m_roundRobinIndex) + 
              "/" + std::to_string(m_configs.size()));
    return config;
}


DBConfig LoadBalancer::selectWeighted() {
    LOG_INFO("LoadBalancer::selectWeighted Use weight algorithm to select a database");
    // compute the total weights
    int totalWeights = 0;
    for (auto& config : m_configs) {
        int weight = config.weight;
        totalWeights += weight;
    }
    // generate a distribution. range from [0, totalWeigths)
    std::uniform_int_distribution<int> dist(0, totalWeights - 1);
    // generate a random weight
    int randomWeight = dist(m_rng);

    int currentWeight = 0;
    size_t index = 0;
    for (auto& config : m_configs) {
        ++index;
        int weight = config.weight;
        currentWeight += weight;
        // check if the randomWeight fall in the range
        if (randomWeight < currentWeight) {
            LOG_INFO("LoadBalancer::selectWeighted Use weight algorithm to select a database index: " + std::to_string(index));
            return config;
        }
    }
    LOG_WARNING("Weighted selection algorithm fallback to first database");
    return m_configs[0];
}



void LoadBalancer::addDatabase(const DBConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // check if the config is already exist
    for (auto& existingConfig : m_configs) {
        if (existingConfig.host == config.host && existingConfig.port == config.port) {
            LOG_WARNING("LoadBalancer::addDatabase already has the database config, host" + config.host + " port is:" + std::to_string(config.port));
            return;
        }
    }
    // check if the config is valid
    if (!config.isValid()) {
        throw std::runtime_error("invalid config");
    }
    m_configs.push_back(config);
    LOG_INFO("Database added: " + config.user + "@" + config.host + ":" + 
             std::to_string(config.port) + "/" + config.database + 
             " (weight=" + std::to_string(config.weight) + ")");
    LOG_INFO("Total databases: " + std::to_string(m_configs.size()));
}


bool LoadBalancer::removeDatabase(const std::string& host, unsigned int port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    LOG_INFO("LoadBalancer::removeDatabase remove before configs count " + std::to_string(m_configs.size()));
    auto it = std::remove_if(m_configs.begin(), m_configs.end(), [host, port](DBConfig existingConfig) -> bool {
        return existingConfig.host  == host && existingConfig.port == port;
    });
    if (it != m_configs.end()) {
        m_configs.erase(it, m_configs.end());
        LOG_INFO("LoadBalancer::removeDatabase remove after configs count " + std::to_string(m_configs.size()));
        // update round robin index
        if (m_configs.empty()) {
            m_roundRobinIndex = 0;
        } else {
            m_roundRobinIndex %= m_configs.size();
        }
        return true;
    }
    LOG_INFO("LoadBalancer::removeDatabase no target database");
    return false;
    
}



bool LoadBalancer::updateWeight(const std::string& host, unsigned int port, unsigned int weight) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_configs.begin(), m_configs.end(), [host, port](DBConfig existingConfig) -> bool {
        return existingConfig.host  == host && existingConfig.port == port;
    });

    if (it != m_configs.end()) {
        auto oldWeight = it->weight;
        it->weight = weight;
        LOG_INFO("Database weight updated: " + it->user + "@" + host + ":" + 
                 std::to_string(port) + "/" + it->database + 
                 " weight changed from " + std::to_string(oldWeight) + 
                 " to " + std::to_string(weight));
        return true;
    }
    LOG_WARNING("Database not found for weight update: " + host + ":" + std::to_string(port));
    return false;
}


size_t LoadBalancer::getDatabaseCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_configs.size();
}


std::vector<DBConfig> LoadBalancer::getDatabaseConfigs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_configs;
}


std::string LoadBalancer::getStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::stringstream ss;
    ss << "LoadBalancer Status:\n";
    ss << "  Strategy: " << strategyToString(m_strategy) << "\n";
    ss << "  Database Count: " << m_configs.size() << "\n";
    ss << "  Round Robin Index: " << m_roundRobinIndex << "\n";
    
    if (!m_configs.empty()) {
        ss << "  Database Configurations:\n";
        for (size_t i = 0; i < m_configs.size(); ++i) {
            const auto& config = m_configs[i];
            ss << "    [" << i << "] " << config.user << "@" << config.host 
               << ":" << config.port << "/" << config.database 
               << " (weight=" << config.weight << ")\n";
        }
        
        // 如果是权重策略，显示总权重
        if (m_strategy == LoadBalanceStrategy::WEIGHTED) {
            int totalWeight = std::accumulate(m_configs.begin(), m_configs.end(), 0,
                [](int sum, const DBConfig& config) {
                    return sum + config.weight;
                });
            ss << "  Total Weight: " << totalWeight << "\n";
        }
    }

    return ss.str();
}