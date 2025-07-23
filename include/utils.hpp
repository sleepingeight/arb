#pragma once

#include <string_view>
#include <array>
#include <string>
#include "simdjson.h"

const int kTotalExchanges = 3;
const int kTotalPairs = 2;
constexpr std::array<std::string_view, kTotalExchanges> kExchanges = {"okx", "binance", "bybit"};
constexpr std::array<std::string_view, kTotalPairs> kPairs = {"BTC/USDT", "ETH/USDT"};


/*
reordering for proper packing of struct without any space wastage
*/
struct config {
    double fees[3];
    double min_profit;
    double max_order_size;
    double latency_ms;
    int exchanges[3];
    int pairs[2];
};

int loadConfig(const std::string&, config&, simdjson::ondemand::parser&);
int getIndex(std::string_view, int);