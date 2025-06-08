#include "MarketMaker.h"
#include "../Simulation.h"
#include "../SimulationException.h"
#include "../ParameterStorage.h"

#include <limits>
#include <cmath>

MarketMakerAgent::MarketMakerAgent(const Simulation* simulation)
    : Agent(simulation), limit_order_probability(0.0), cancel_probability(0.0), restart_interval(0), spread(0.0), max_risk(0) {}

MarketMakerAgent::MarketMakerAgent(const Simulation* simulation, const std::string& name)
    : Agent(simulation, name), limit_order_probability(0.0), cancel_probability(0.0), restart_interval(0), spread(0.0), max_risk(0) {}

void MarketMakerAgent::configure(const pugi::xml_node& node, const std::string& configurationPath) {
    Agent::configure(node, configurationPath);

    pugi::xml_attribute att;
    if (!(att = node.attribute("exchange_1")).empty()) {
        exchange_1 = simulation()->parameters().processString(att.as_string());
    }

    if (!(att = node.attribute("limit_order_probability")).empty()) {
        limit_order_probability = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("cancel_probability")).empty()) {
        cancel_probability = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("restart_interval")).empty()) {
        restart_interval = std::stoull(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("spread")).empty()) {
        spread = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("max_risk")).empty()) {
        max_risk = std::stoull(simulation()->parameters().processString(att.as_string()));
    }
    if (!(att = node.attribute("num_market_makers")).empty()) {
        num_market_makers = std::stoull(simulation()->parameters().processString(att.as_string()));
    }
}

void MarketMakerAgent::receiveMessage(const MessagePtr& msg) {
    const Timestamp currentTimestamp = simulation()->currentTimestamp();

    if (msg->type == "EVENT_SIMULATION_START") {
        simulation()->dispatchMessage(currentTimestamp, 0, name(), name(), "WAKEUP_FOR_MARKET_MAKER", std::make_shared<EmptyPayload>());
    } else if (msg->type == "WAKEUP_FOR_MARKET_MAKER") {
        simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "RETRIEVE_L1", std::make_shared<EmptyPayload>());  
    } else if (msg->type == "RESPONSE_RETRIEVE_L1") {
        auto pptr = std::dynamic_pointer_cast<RetrieveL1ResponsePayload>(msg->payload);

        if ((uint64_t) abs(curr_position) > max_risk) {
            // Cancel all orders if risk threshold is exceeded
            exceeded_risk_threshold = true;
            restart_counter = restart_interval;
        }

        if (exceeded_risk_threshold  && ((uint64_t)abs(curr_position) <= max_risk)) {
            // Restart the agent if risk threshold is no longer exceeded
            exceeded_risk_threshold = false;
        }

        if (exceeded_risk_threshold) {
            // Cancel all orders
            auto cancel_payload = std::make_shared<CancelOrdersPayload>();
            for (auto& id: outstanding_orders) {
                cancel_payload->cancellations.push_back(CancelOrdersCancellation(id, std::numeric_limits<unsigned int>::max()));
            }
            simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "CANCEL_ORDERS", cancel_payload);

            // If position positive, sell excees
            if (curr_position > 0) {
                auto marketpayload = std::make_shared<PlaceOrderMarketPayload>(OrderDirection::Sell, curr_position);
                simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "PLACE_ORDER_MARKET", marketpayload);
            } else if (curr_position < 0) {
                // If position negative, buy excess
                auto marketpayload = std::make_shared<PlaceOrderMarketPayload>(OrderDirection::Buy, -curr_position);
                simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "PLACE_ORDER_MARKET", marketpayload);
            }
        } else if (restart_counter == 0) {
            if (uniform_dist(simulation()->randomGenerator()) < cancel_probability) {
                // Cancel all limit orders
                auto cancel_payload = std::make_shared<CancelOrdersPayload>();
                for (auto& id: outstanding_orders) {
                    cancel_payload->cancellations.push_back(CancelOrdersCancellation(id, std::numeric_limits<unsigned int>::max()));
                }
                simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "CANCEL_ORDERS", cancel_payload);
            }

            if (uniform_dist(simulation()->randomGenerator()) < limit_order_probability) {
                double price = double(pptr->bestBidPrice + pptr->bestAskPrice) / 2;
                // put both buy and sell limit orders
                auto buy_payload = std::make_shared<PlaceOrderLimitPayload>(OrderDirection::Buy, DEFAULT_ORDER_VOLUME, price - (spread / 2));
                auto sell_payload = std::make_shared<PlaceOrderLimitPayload>(OrderDirection::Sell, DEFAULT_ORDER_VOLUME, price + (spread / 2));
                simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "PLACE_ORDER_LIMIT", buy_payload);
                simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "PLACE_ORDER_LIMIT", sell_payload);
            }
        }
        restart_counter--;

        // Poll for L1 again
        simulation()->dispatchMessage(currentTimestamp, 1, name(), exchange_1, "RETRIEVE_L1", std::make_shared<EmptyPayload>());
        
    } else if (msg->type == "RESPONSE_PLACE_ORDER_LIMIT") {
        auto pptr = std::dynamic_pointer_cast<PlaceOrderLimitResponsePayload>(msg->payload);
        outstanding_orders.push_back(pptr->id);
    } else if (msg->type == "RESPONSE_CANCEL_ORDERS") {
        auto pptr = std::dynamic_pointer_cast<CancelOrdersPayload>(msg->payload);
        for (auto& id: pptr->cancellations) {
            auto it = std::find(outstanding_orders.begin(), outstanding_orders.end(), id.id);
            if (it != outstanding_orders.end()) {
                outstanding_orders.erase(it);
            }
        }
    } else if (msg->type == "RESPONSE_TRADE") {
        auto pptr = std::dynamic_pointer_cast<EventTradePayload>(msg->payload);
        auto trade = pptr->trade;

        // Update position based on trade
        if (trade.direction() == OrderDirection::Buy) {
            curr_position += trade.volume();
        } else if (trade.direction() == OrderDirection::Sell) {
            curr_position -= trade.volume();
        }

        // Remove from outstanding orders
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