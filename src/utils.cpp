#include "utils.hpp"
#include "ws_client.hpp"
#include <exception>
#include <memory>
#include <stdexcept>
#include <format>
#include <string_view>

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

    bool empty_pairs = true;
    for (auto pair: object["pairs"]) {
        int index = getIndex(pair.get_string(), 2);
        if (index == -1) throw std::runtime_error("unknown pair.\narb supported pairs: BTC/USDT, ETH/USDT");
        config.pairs[index] = true;
        empty_pairs = false;
    }
    if (empty_pairs)
        throw std::runtime_error("found empty pairs.\nplease fill config.json");

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

void connectToEndpoints(const config& config, std::vector<std::unique_ptr<wsClient>>& clients) {
    for(size_t i = 0; i < kTotalExchanges; i++) {
        if(config.exchanges[i]) {
            for(size_t j = 0; j < kTotalPairs; j++) {
                if (!config.pairs[j]) continue;

                const auto& exchange = kExchanges[i];
                const auto& pair_sv = kPairs[j];

                auto pos = pair_sv.find('/');
                std::string_view base = pair_sv.substr(0, pos);
                std::string_view quote = pair_sv.substr(pos + 1);

                std::string hostname = "ws.gomarket-cpp.goquant.io/ws/l2-orderbook/";

                switch (i) {
                    case 0:
                        hostname += std::format("{}/{}-{}", exchange, base, quote);
                        break;
                    case 1:
                        hostname += std::format("{}/{}_{}", exchange, base, quote);
                        break;
                    case 2:
                        hostname +=  std::format("{}/{}{}/spot", exchange, base, quote);
                        break;
                }          
                std::cout << "hostname: " << hostname << "\n\n";
                try {
                    clients.emplace_back(std::make_unique<wsClient>(hostname));
                }
                catch (std::exception &e) {
                    std::cerr << "unable to connect to endpoint wss://" << hostname << "\nerror: " 
                        << e.what() << "\n";
                }
            }
        }
    } 




}   