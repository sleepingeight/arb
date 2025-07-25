#include "utils.hpp"
#include <exception>
#include <stdexcept>
#include <string_view>

/**
 * Implementation notes:
 * - Uses simdjson for zero-copy JSON parsing
 * - Validates all required configuration fields
 * - Ensures 1-1 mapping between exchanges and fees
 * - Performs type checking on numeric values
 * - Exits with failure on invalid configuration
 */
void loadConfig(const std::string& file_path, config& config, simdjson::ondemand::parser& parser) {
  auto json = simdjson::padded_string::load("../config/config.json");
  simdjson::ondemand::document doc = parser.iterate(json); 
  simdjson::ondemand::object object = doc.get_object();

  try {
    bool empty_exchanges = true;
    for (auto exchange: object["exchanges"]) {
        int index = getIndex(exchange.get_string(), 1);
        if (index == -1) throw std::runtime_error("unknown exchange.\narb supported exchanges: okx, derbit, bybit");
        config.exchanges[index] = true;
        empty_exchanges = false;
    }
    if (empty_exchanges)
        throw std::runtime_error("found empty exchanges.\nplease fill config.json");

    int num_pairs = 0;
    for (auto pair: object["pairs"]) {
        int index = getIndex(pair.get_string(), 2);
        if (index == -1) throw std::runtime_error("unknown pair.\narb supported pairs: BTC/USDT, ETH/USDT, SOL/USDT");
        config.pairs[index] = true;
        num_pairs++;
    }
    if (num_pairs == 0)
        throw std::runtime_error("found empty pairs.\nplease fill config.json");
    if (num_pairs > 1)
        throw std::runtime_error("sorry, currently only one pair is supported.\n");

    config.min_profit = object["min_profit"].get_double();
    config.max_order_size = object["max_order_size"].get_double();
    config.latency_ms = object["latency_ms"].get_double();
    simdjson::ondemand::object fees = object["fees"];
    for (auto fee: fees) {
        int index = getIndex(fee.escaped_key(), 1);
        if (index == -1) throw std::runtime_error("unknown exchange in fees.\narb supported exchanges: okx, derbit, bybit");
        if (config.exchanges[index] == 0) throw std::runtime_error("exchanges and fees mismatch\n.arb supports only 1-1 mapping between fees and exhanges");
        config.fees[index] = fee.value().get_double();
    }
  } 
  catch (std::exception &e) {
    std::cerr << "bad config.json: " << e.what() << "\n"; 
    std::exit(EXIT_FAILURE);
  }
}

/**
 * Implementation notes:
 * - Uses string_view for efficient string comparison
 * - Linear search optimized for small arrays
 * - Type parameter determines search target:
 *   1: Exchange names
 *   2: Trading pairs
 */
int getIndex(std::string_view name, int type) {
    if(type == 1) {
        for(int i = 0; i < kTotalExchanges; i++) {
            if (kExchanges[i] == name) return i;
        }
    }
    else if(type == 2) {
        for(int i = 0; i < kTotalPairs; i++) {
            if (kPairs[i] == name) return i;
        }
    }
    return -1;
}