#pragma once
#include "../Agent.h"

#include <memory>
#include <random>
#include "../ExchangeAgentMessagePayloads.h"


class ExchangePopulator : public Agent {
public:
    ExchangePopulator(const Simulation* simulation);
    ExchangePopulator(const Simulation* simulation, const std::string& name);

    void configure(const pugi::xml_node& node, const std::string& configurationPath);

    // Inherited via Agent
    void receiveMessage(const MessagePtr& msg) override;

private:
    std::string exchange;
    double initial_price;
    uint64_t quantity_per_level;
    uint64_t num_levels_both_sides;
    double level_spacing;
};