#pragma once
#include "../Agent.h"

#include <memory>
#include <random>
#include <vector>
#include "../ExchangeAgentMessagePayloads.h"

class MomentumAgent : public Agent {
    public:
        MomentumAgent(const Simulation* simulation);
        MomentumAgent(const Simulation* simulation, const std::string& name);

        void configure(const pugi::xml_node& node, const std::string& configurationPath);

        // Inherited via Agent
        void receiveMessage(const MessagePtr& msg) override;
    private:
        std::string exchange_1;

        std::vector<OrderID> outstanding_orders;
        
        double momentum_signal{0.0};
        double previous_price{0.0};
        double cancel_probability;
        double market_to_limit_ratio;
        double demand_saturation;
        double alpha;
        double beta;
        uint64_t num_momentum_traders;

        const uint64_t DEFAULT_ORDER_VOLUME = 100;
        const double DEFAULT_OFFSET_FOR_LIMIT = 0.5; 
        std::uniform_real_distribution<double> uniform_dist{0.0, 1.0};


};