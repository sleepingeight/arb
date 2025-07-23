#include "utils.hpp"
#include <exception>
#include <stdexcept>
#include <string_view>

int loadConfig(const std::string& file_path, config& config, simdjson::ondemand::parser& parser) {
  auto json = simdjson::padded_string::load("../config/config.json");
  simdjson::ondemand::document doc = parser.iterate(json); 
  simdjson::ondemand::object object = doc.get_object();

  try {
    for (auto exchange: object["exchanges"]) {
        int index = getIndex(exchange.get_string(), 1);
        if (index == -1) throw std::runtime_error("unknown exchange.\narb supported exchanges: okx, binance, bybit");
        config.exchanges[index] = 1;
    }
    for (auto pair: object["pairs"]) {
        int index = getIndex(pair.get_string(), 2);
        if (index == -1) throw std::runtime_error("unknown pair.\narb supported pairs: BTC/USDT, ETH/USDT");
        config.pairs[index] = 1;
    }
    config.min_profit = object["min_profit"].get_double();
    config.max_order_size = object["max_order_size"].get_double();
    config.latency_ms = object["latency_ms"].get_double();
    simdjson::ondemand::object fees = object["fees"];
    for (auto fee: fees) {
        int index = getIndex(fee.escaped_key(), 1);
        if (index == -1) throw std::runtime_error("unknown exchange in fees.\narb supported exchanges: okx, binance, bybit");
        if (config.exchanges[index] == 0) throw std::runtime_error("exchanges and fees mismatch\n.arb supports only 1-1 mapping between fees and exhanges");
        config.fees[index] = fee.value().get_double();
    }
  } 
  catch (std::exception &e) {
    std::cerr << "bad config.json: " << e.what() << "\n"; 
    return -1;
  }

  return 0;
}

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