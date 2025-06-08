#include "ExchangePopulator.h"
#include "../Simulation.h"

#include "../SimulationException.h"
#include "../ParameterStorage.h"


ExchangePopulator::ExchangePopulator(const Simulation* simulation)
    : Agent(simulation), quantity_per_level(0), num_levels_both_sides(0), level_spacing(0.0) {}

ExchangePopulator::ExchangePopulator(const Simulation* simulation, const std::string& name)
    : Agent(simulation, name), quantity_per_level(0), num_levels_both_sides(0), level_spacing(0.0) {}

void ExchangePopulator::configure(const pugi::xml_node& node, const std::string& configurationPath) {

    Agent::configure(node, configurationPath);

    pugi::xml_attribute att;
    if (!(att = node.attribute("exchange")).empty()) {
        exchange = simulation()->parameters().processString(att.as_string());
    }

    if (!(att = node.attribute("initial_price")).empty()) {
        initial_price = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("quantity_per_level")).empty()) {
        quantity_per_level = std::stoull(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("num_levels_both_sides")).empty()) {
        num_levels_both_sides = std::stoull(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("level_spacing")).empty()) {
        level_spacing = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    // std::cout << "ExchangePopulator: " << name() << " configured with exchange: " << exchange
    //           << ", initial_price: " << initial_price
    //           << ", quantity_per_level: " << quantity_per_level
    //           << ", num_levels_both_sides: " << num_levels_both_sides
    //           << ", level_spacing: " << level_spacing
    //           << std::endl;

}

void ExchangePopulator::receiveMessage(const MessagePtr& msg) {
    const Timestamp currentTimestamp = simulation()->currentTimestamp();

    if (msg->type == "EVENT_SIMULATION_START") {
        simulation()->dispatchMessage(currentTimestamp, 0, name(), name(), "WAKEUP_FOR_POPULATOR", std::make_shared<EmptyPayload>());
    } else if (msg->type == "WAKEUP_FOR_POPULATOR") {
        // Populate the order book with limit orders, centered around initial price
        for (uint64_t i = 0; i < num_levels_both_sides; ++i) {
            auto pptr1 = std::make_shared<PlaceOrderLimitPayload>(OrderDirection::Buy, quantity_per_level, initial_price - (i * level_spacing));
            simulation()->dispatchMessage(currentTimestamp, 0, name(), exchange, "PLACE_ORDER_LIMIT", pptr1);
            auto pptr2 = std::make_shared<PlaceOrderLimitPayload>(OrderDirection::Sell, quantity_per_level, initial_price + (i * level_spacing));
            simulation()->dispatchMessage(currentTimestamp, 0, name(), exchange, "PLACE_ORDER_LIMIT", pptr2);
        }
        std::cout << "Populated" << std::endl;
    } else if (msg->type == "RESPONSE_PLACE_ORDER_LIMIT") {
        // std::cout << "Received response for limit order placement" << std::endl;
        // auto pptr = std::dynamic_pointer_cast<PlaceOrderLimitResponsePayload>(msg->payload);
        // if (pptr) {
        //     std::cout << "Order placed with ID: " << pptr->id << std::endl;
        // }
    }
}