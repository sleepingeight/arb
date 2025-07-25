#include "orderbook.hpp"
#include "utils.hpp"
#include "ws_client.hpp"
#include <memory>
#include <simdjson.h>

using namespace simdjson;

int main()
{
    simdjson::ondemand::parser kParser;
    config kConfig {};

    {
      const std::string kConfigPath = "../config/config.json";
      loadConfig(kConfigPath, kConfig, kParser);
    }


    std::vector<L2OrderBook> orderbooks(kTotalExchanges);

    // after config is loaded, connect to the websockets

    std::vector<std::unique_ptr<wsClient>> connections;
    connectToEndpoints(kConfig, connections, orderbooks);

    std::vector<Opportunity> opportunities;

    std::thread process_(process, std::ref(orderbooks), std::ref(kConfig), std::ref(opportunities));
    process_.join();
}