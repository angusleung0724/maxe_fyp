#pragma once
#include "../Agent.h"

#include <memory>
#include <random>
#include <vector>
#include "../ExchangeAgentMessagePayloads.h"


class DownwardShockAgent : public Agent {
    public: 
        DownwardShockAgent(const Simulation* simulation);
        DownwardShockAgent(const Simulation* simulation, const std::string& name);

        void configure(const pugi::xml_node& node, const std::string& configurationPath);

        // Inherited via Agent
        void receiveMessage(const MessagePtr& msg) override;
    
    private:
        std::string exchange_1;

        double spike_probability;
        uint64_t volume_per_order;
        uint64_t start_tick;
        uint64_t end_tick;

        std::uniform_real_distribution<double> uniform_dist{0.0, 1.0};


};