#include "Fundamental.h"

#include "../Simulation.h"
#include "../SimulationException.h"
#include "../ParameterStorage.h"
#include <cmath>

FundamentalAgent::FundamentalAgent(const Simulation* simulation)
    : Agent(simulation), fundamental_value_expectation(0.0), fundamental_value_std(0.0), k1(0.0), k2(0.0), num_fundamental_traders(0) {}

FundamentalAgent::FundamentalAgent(const Simulation* simulation, const std::string& name)
    : Agent(simulation, name), fundamental_value_expectation(0.0), fundamental_value_std(0.0), k1(0.0), k2(0.0), num_fundamental_traders(0) {}


void FundamentalAgent::configure(const pugi::xml_node& node, const std::string& configurationPath) {
    Agent::configure(node, configurationPath);

    pugi::xml_attribute att;
    if (!(att = node.attribute("exchange_1")).empty()) {
        exchange_1 = simulation()->parameters().processString(att.as_string());
    }

    if (!(att = node.attribute("fundamental_value_expectation")).empty()) {
        fundamental_value_expectation = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("fundamental_value_std")).empty()) {
        fundamental_value_std = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("k1")).empty()) {
        k1 = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("k2")).empty()) {
        k2 = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("num_fundamental_traders")).empty()) {
        num_fundamental_traders = std::stoull(simulation()->parameters().processString(att.as_string()));
    }

    // Initialize the normal distribution with the given mean and standard deviation
    normal_dist = std::normal_distribution<double>(fundamental_value_expectation, fundamental_value_std);

    // std::cout << "FundamentalAgent: " << name() << " configured with exchange_1: " << exchange_1
    //           << ", fundamental_value_expectation: " << fundamental_value_expectation
    //           << ", fundamental_value_std: " << fundamental_value_std
    //           << ", k1: " << k1
    //           << ", k2: " << k2
    //           << ", num_fundamental_traders: " << num_fundamental_traders
    //           << std::endl;
}

void FundamentalAgent::receiveMessage(const MessagePtr& msg) {
    const Timestamp currentTimestamp = simulation()->currentTimestamp();

    if (msg->type == "EVENT_SIMULATION_START") {
        simulation()->dispatchMessage(currentTimestamp, 0, name(), name(), "WAKEUP_FOR_FUNDAMENTAL", std::make_shared<EmptyPayload>());
    } else if (msg->type == "WAKEUP_FOR_FUNDAMENTAL") {
        simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "RETRIEVE_L1", std::make_shared<EmptyPayload>());  
    } else if (msg->type == "RESPONSE_RETRIEVE_L1") {
        auto pptr = std::dynamic_pointer_cast<RetrieveL1ResponsePayload>(msg->payload);

        auto price_per_unit = (pptr->bestAskPrice + pptr->bestBidPrice) / 2.0;
        if (price_per_unit == (Decimal) 0) {
            // Keep polling until there's a price
            simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "RETRIEVE_L1", std::make_shared<EmptyPayload>());  
            return;
        }

        // Calculate the fundamental value

        double price_deviation = normal_dist(simulation()->randomGenerator()) - double(price_per_unit);
        double µ = (k1 * abs(price_deviation)) + (k2 * pow(abs(price_deviation), 3)) / num_fundamental_traders;

        if (µ > uniform_dist(simulation()->randomGenerator())) {
            if (price_deviation > 0) {
                // Buy
                auto marketpayload = std::make_shared<PlaceOrderMarketPayload>(OrderDirection::Buy, DEFAULT_ORDER_VOLUME);
                simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "PLACE_ORDER_MARKET", marketpayload);
            } else {
                // Sell
                auto marketpayload = std::make_shared<PlaceOrderMarketPayload>(OrderDirection::Sell, DEFAULT_ORDER_VOLUME);
                simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "PLACE_ORDER_MARKET", marketpayload);
            }
        }
        // Schedule the next wakeup
        simulation()->dispatchMessage(currentTimestamp, 10, name(), exchange_1, "RETRIEVE_L1", std::make_shared<EmptyPayload>());
    }
}