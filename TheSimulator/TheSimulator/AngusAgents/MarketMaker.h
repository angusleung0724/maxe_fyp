#pragma once
#include "../Agent.h"

#include <memory>
#include <random>
#include <vector>
#include "../ExchangeAgentMessagePayloads.h"

class MarketMakerAgent : public Agent {
    public: 
        MarketMakerAgent(const Simulation* simulation);
        MarketMakerAgent(const Simulation* simulation, const std::string& name);

        void configure(const pugi::xml_node& node, const std::string& configurationPath);

        // Inherited via Agent
        void receiveMessage(const MessagePtr& msg) override;
    private:
        std::string exchange_1;

        std::vector<OrderID> outstanding_orders;
        
        double limit_order_probability;
        double cancel_probability;
        uint64_t restart_interval;
        double spread;
        uint64_t max_risk;
        uint64_t num_market_makers;

        bool exceeded_risk_threshold{false};
        uint64_t restart_counter{0};
        int64_t curr_position{0};

        std::uniform_real_distribution<double> uniform_dist{0.0, 1.0};

        const uint64_t DEFAULT_ORDER_VOLUME = 100;

};