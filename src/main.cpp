#include "simdjson.h"
#include "utils.hpp"
#include "ws_client.hpp"
#include <memory>

int main() {
  simdjson::ondemand::parser kParser;
  config kConfig{};

  {
    const std::string kConfigPath = "../config/config.json";
    loadConfig(kConfigPath, kConfig, kParser);
  }
  // after config is loaded, connect to the websockets

  std::vector<std::unique_ptr<wsClient>> connections;
  connectToEndpoints(kConfig, connections);

}