#include "simdjson.h"
#include "config_loader.hpp"

int main() {
  simdjson::ondemand::parser kParser;
  config kConfig;
  const std::string kConfigPath = "../config/config.json";
  loadConfig(kConfigPath, kConfig, kParser);
}