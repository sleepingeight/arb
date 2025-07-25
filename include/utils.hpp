#pragma once

#include <string_view>
#include <array>
#include <string>
#include <simdjson.h>

const int kTotalExchanges = 3;
const int kTotalPairs = 3;
constexpr std::array<std::string_view, kTotalExchanges> kExchanges = {"okx", "deribit", "bybit"};
constexpr std::array<std::string_view, kTotalPairs> kPairs = {"BTC/USDT", "ETH/USDT", "SOL/USDT"};
constexpr std::array<std::string_view, kTotalExchanges> kHostNames = {
    "ws.gomarket-cpp.goquant.io/ws/l2-orderbook/okx/",
    "ws.gomarket-cpp.goquant.io/ws/l2-orderbook/deribit/",
    "ws.gomarket-cpp.goquant.io/ws/l2-orderbook/bybit/"
};
constexpr std::array<bool, kTotalExchanges> kUseDoubleInString = {true, false, true};

/*
reordering for proper packing of struct without any space wastage
*/
struct config {
    double fees[3];
    double min_profit;
    double max_order_size;
    double latency_ms;
    bool exchanges[kTotalExchanges];
    bool pairs[kTotalPairs];
};

void loadConfig(const std::string&, config&, simdjson::ondemand::parser&);
int getIndex(std::string_view, int);

extern std::counting_semaphore<2> sem;

