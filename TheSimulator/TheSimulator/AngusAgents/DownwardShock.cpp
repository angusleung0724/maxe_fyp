#include "DownwardShock.h"
#include "../Simulation.h"
#include "../SimulationException.h"
#include "../ParameterStorage.h"
#include <limits>


DownwardShockAgent::DownwardShockAgent(const Simulation* simulation)
    : Agent(simulation), spike_probability(0.0), volume_per_order(0) {}

DownwardShockAgent::DownwardShockAgent(const Simulation* simulation, const std::string& name)
    : Agent(simulation, name), spike_probability(0.0), volume_per_order(0) {}

void DownwardShockAgent::configure(const pugi::xml_node& node, const std::string& configurationPath) {
    Agent::configure(node, configurationPath);

    pugi::xml_attribute att;
    if (!(att = node.attribute("exchange_1")).empty()) {
        exchange_1 = simulation()->parameters().processString(att.as_string());
    }

    if (!(att = node.attribute("spike_probability")).empty()) {
        spike_probability = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("volume_per_order")).empty()) {
        volume_per_order = std::stoull(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("start_tick")).empty()) {
        start_tick = std::stoull(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("end_tick")).empty()) {
        end_tick = std::stoull(simulation()->parameters().processString(att.as_string()));
    }

    std::cout << "DownwardShockAgent: " << name() << " configured with exchange_1: " << exchange_1
              << ", spike_probability: " << spike_probability
              << ", volume_per_order: " << volume_per_order
              << std::endl;

}

void DownwardShockAgent::receiveMessage(const MessagePtr& msg) {
    const Timestamp currentTimestamp = simulation()->currentTimestamp();

    if (msg->type == "EVENT_SIMULATION_START") {
        simulation()->dispatchMessage(currentTimestamp, start_tick, name(), name(), "WAKEUP_FOR_DOWNWARD_SHOCK", std::make_shared<EmptyPayload>());
    } else if (msg->type == "WAKEUP_FOR_DOWNWARD_SHOCK") {
        auto pptr = std::dynamic_pointer_cast<RetrieveL1ResponsePayload>(msg->payload);

        // Spike the price with probability spike_probability
        if (uniform_dist(simulation()->randomGenerator()) < spike_probability) {
            auto marketpayload = std::make_shared<PlaceOrderMarketPayload>(OrderDirection::Sell, volume_per_order);
            simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "PLACE_ORDER_MARKET", marketpayload);
        }

        if (currentTimestamp < end_tick) {
            simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "WAKEUP_FOR_DOWNWARD_SHOCK", std::make_shared<EmptyPayload>());
        } 
    }
}