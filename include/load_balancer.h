#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H


#include <vector>
#include "db_config.h"
#include <mutex>
#include <random>

// define loadBalance Strategy
enum class LoadBalanceStrategy {
    RANDOM,      // RANDOM choose database instance
    ROUND_ROBIN, //ROUND ROBIN algorithm to choose database instance
    WEIGHTED   // Use Weighted to choose database instance
};


class LoadBalancer {

public:

// singleton class
static LoadBalancer& getInstance() {
    // global instance
    static LoadBalancer instance;
    return instance;
}

// disable copy constructor
LoadBalancer(const LoadBalancer&) = delete;
// disable copy assignment
LoadBalancer& operator=(const LoadBalancer&) = delete;


// initalization
void init(
    const std::vector<DBConfig>& configs,
    LoadBalanceStrategy strategy = LoadBalanceStrategy::WEIGHTED);


void initSingleDatabase(const std::string& host, const std::string& user,
                           const std::string& password, const std::string& database,
                           unsigned int port = 3306, unsigned int weight = 1);
                           

// change loadBalance stragety
void setStrategy(LoadBalanceStrategy strategy);

// get using strategy
LoadBalanceStrategy getStrategy() const;

// use load balancer to get db config
DBConfig getNextDatabase();

// addDatabase
void addDatabase(const DBConfig& config);

// remove Database
bool removeDatabase(const std::string& host, unsigned int port);

// update weight when using weighted strategy
bool updateWeight(const std::string& host, unsigned int port, unsigned int weight);

// get DatabaseCount
size_t getDatabaseCount() const;

// get databases configuration
std::vector<DBConfig> getDatabaseConfigs() const;

std::string getStatus() const;

private: 
// multiple db configs
std::vector<DBConfig> m_configs;
LoadBalanceStrategy m_strategy;
size_t m_roundRobinIndex;
mutable std::mutex m_mutex;
std::mt19937 m_rng;

LoadBalancer();

// use random alogrithm to select a database
DBConfig selectRandom();
// use round robin alogrithm to select a database
DBConfig selectRoundRobin();
// use weight to select a database
DBConfig selectWeighted();


};


inline std::string strategyToString(LoadBalanceStrategy strategy) {
    switch (strategy) {
        case LoadBalanceStrategy::RANDOM:      return "Random";
        case LoadBalanceStrategy::ROUND_ROBIN: return "RoundRobin";
        case LoadBalanceStrategy::WEIGHTED:    return "Weighted";
        default:                              return "Unknown";
    }
}


#endif // LOAD_BALANCER_H