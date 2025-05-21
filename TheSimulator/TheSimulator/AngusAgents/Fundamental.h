#pragma once
#include "../Agent.h"

#include <memory>
#include <random>
#include <vector>
#include "../ExchangeAgentMessagePayloads.h"

class FundamentalAgent : public Agent {
public:
    FundamentalAgent(const Simulation* simulation);
    FundamentalAgent(const Simulation* simulation, const std::string& name);

    void configure(const pugi::xml_node& node, const std::string& configurationPath);

    // Inherited via Agent
    void receiveMessage(const MessagePtr& msg) override;

private:
    std::string exchange_1;

    std::vector<OrderID> outstanding_orders;
    
    double fundamental_value_expectation;
    double fundamental_value_std;
    double k1;
    double k2;
    uint64_t num_fundamental_traders;

  std::uniform_real_distribution<double> uniform_dist{0.0, 1.0};
  std::normal_distribution<double> normal_dist;

  const uint64_t DEFAULT_ORDER_VOLUME = 100;

};