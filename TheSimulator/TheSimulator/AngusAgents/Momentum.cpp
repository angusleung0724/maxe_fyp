#include "Momentum.h"
#include "../Simulation.h"
#include "../SimulationException.h"
#include "../ParameterStorage.h"
#include <limits>
#include <cmath>


MomentumAgent::MomentumAgent(const Simulation* simulation)
    : Agent(simulation), cancel_probability(0.0), market_to_limit_ratio(0.0), num_momentum_traders(0), momentum_signal(0.0), previous_price(0.0) {}

MomentumAgent::MomentumAgent(const Simulation* simulation, const std::string& name)
    : Agent(simulation, name), cancel_probability(0.0), market_to_limit_ratio(0.0), num_momentum_traders(0), momentum_signal(0.0), previous_price(0.0) {}

void MomentumAgent::configure(const pugi::xml_node& node, const std::string& configurationPath) {
    Agent::configure(node, configurationPath);

    pugi::xml_attribute att;
    if (!(att = node.attribute("exchange_1")).empty()) {
        exchange_1 = simulation()->parameters().processString(att.as_string());
    }

    if (!(att = node.attribute("cancel_probability")).empty()) {
        cancel_probability = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("market_to_limit_ratio")).empty()) {
        market_to_limit_ratio = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("num_momentum_traders")).empty()) {
        num_momentum_traders = std::stoull(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("alpha")).empty()) {
        alpha = std::stod(simulation()->parameters().processString(att.as_string()));
    }   

    if (!(att = node.attribute("beta")).empty()) {
        beta = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("demand_saturation")).empty()) {
        demand_saturation = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    std::cout << "MomentumAgent: " << name() << " configured with exchange_1: " << exchange_1
              << ", cancel_probability: " << cancel_probability
              << ", market_to_limit_ratio: " << market_to_limit_ratio
              << ", num_momentum_traders: " << num_momentum_traders
              << ", momentum_signal: " << momentum_signal
              << ", previous_price: " << previous_price
              << std::endl;
}

void MomentumAgent::receiveMessage(const MessagePtr& msg) {
    const Timestamp currentTimestamp = simulation()->currentTimestamp();

    if (msg->type == "EVENT_SIMULATION_START") {
        for (int i = 0; i < 1; ++i) {
            simulation()->dispatchMessage(currentTimestamp, 0, name(), name(), "WAKEUP_FOR_MOMENTUM", std::make_shared<EmptyPayload>());
        }
    } else if (msg->type == "WAKEUP_FOR_MOMENTUM") {
        simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "RETRIEVE_L1", std::make_shared<EmptyPayload>());  
    } else if (msg->type == "RESPONSE_RETRIEVE_L1") {
        auto pptr = std::dynamic_pointer_cast<RetrieveL1ResponsePayload>(msg->payload);

        // Cancel outstanding limit orders with probability cancel_probability
        auto cancel_payload = std::make_shared<CancelOrdersPayload>();
        for (auto& id: outstanding_orders) {
            if (uniform_dist(simulation()->randomGenerator()) < cancel_probability) {
                cancel_payload->cancellations.push_back(CancelOrdersCancellation(id, std::numeric_limits<unsigned int>::max()));
            }
        }

        if (!cancel_payload->cancellations.empty()) {
            simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "CANCEL_ORDERS", cancel_payload);
        }

        auto price_per_unit = double(pptr->bestAskPrice + pptr->bestBidPrice) / 2.0;

        if (price_per_unit == 0) {
            // Keep polling until there's a price
            simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "RETRIEVE_L1", std::make_shared<EmptyPayload>());  
            return;
        }

        momentum_signal = momentum_signal * (1- alpha) + alpha * (price_per_unit - previous_price);
        previous_price = price_per_unit;

        double probability_of_market_order = (beta * std::tanh(demand_saturation * momentum_signal)) / num_momentum_traders;
        double probability_of_limit_order = (probability_of_market_order)*market_to_limit_ratio;

        if (probability_of_market_order > uniform_dist(simulation()->randomGenerator())) {
            // Send market order
            if (momentum_signal > 0) {
                // send buy market order
                auto marketpayload = std::make_shared<PlaceOrderMarketPayload>(OrderDirection::Buy, DEFAULT_ORDER_VOLUME);
                simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "PLACE_ORDER_MARKET", marketpayload);
            } else {
                 // Sell market order
                auto marketpayload = std::make_shared<PlaceOrderMarketPayload>(OrderDirection::Sell, DEFAULT_ORDER_VOLUME);
                simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "PLACE_ORDER_MARKET", marketpayload);
            }
            
        } else if (probability_of_limit_order > uniform_dist(simulation()->randomGenerator())) {
            if (momentum_signal > 0) {
                // Buy limit order
                auto limitpayload = std::make_shared<PlaceOrderLimitPayload>(OrderDirection::Buy, DEFAULT_ORDER_VOLUME, price_per_unit - DEFAULT_OFFSET_FOR_LIMIT);
                simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "PLACE_ORDER_LIMIT", limitpayload);
            } else {
                // Sell limit order
                auto limitpayload = std::make_shared<PlaceOrderLimitPayload>(OrderDirection::Sell, DEFAULT_ORDER_VOLUME, price_per_unit + DEFAULT_OFFSET_FOR_LIMIT);
                simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "PLACE_ORDER_LIMIT", limitpayload);
            }
            
        }

        // Poll for L1 again
        simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "RETRIEVE_L1", std::make_shared<EmptyPayload>());

    } else if (msg->type == "RESPONSE_PLACE_ORDER_LIMIT") {
        auto pptr = std::dynamic_pointer_cast<PlaceOrderLimitResponsePayload>(msg->payload);
        outstanding_orders.push_back(pptr->id);
    } else if (msg->type == "RESPONSE_CANCEL_ORDERS") {
        auto pptr = std::dynamic_pointer_cast<CancelOrdersPayload>(msg->payload);
        for (auto& cancellation : pptr->cancellations) {
            auto it = std::find(outstanding_orders.begin(), outstanding_orders.end(), cancellation.id);
            if (it != outstanding_orders.end()) {
                outstanding_orders.erase(it);
            }
        }
    } else if (msg->type == "RESPONSE_TRADE") {
        auto pptr = std::dynamic_pointer_cast<EventTradePayload>(msg->payload);
        auto it = std::find(outstanding_orders.begin(), outstanding_orders.end(), pptr->trade.aggressingOrderID());
        if (it != outstanding_orders.end()) {
            outstanding_orders.erase(it);
        }
        auto it2 = std::find(outstanding_orders.begin(), outstanding_orders.end(), pptr->trade.restingOrderID());
        if (it2 != outstanding_orders.end()) {
            outstanding_orders.erase(it2);
        }
    }
}