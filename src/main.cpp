#include "orderbook.hpp"
#include "utils.hpp"
#include "ws_client.hpp"
#include <memory>
#include <semaphore>
#include <simdjson.h>


std::counting_semaphore<2> sem(0);

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
    std::vector<Opportunity> opportunities;
    std::thread process_(process, std::ref(orderbooks), std::ref(kConfig), std::ref(opportunities));

    std::vector<std::unique_ptr<wsClient>> connections;
    connectToEndpoints(kConfig, connections, orderbooks);


    
    process_.join();
    // for(auto &x: t){
    //   x.join();
    // }
}