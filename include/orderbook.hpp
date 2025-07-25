#pragma once

#include <chrono>
#include "utils.hpp"

const int kMaxSize = 50;

struct alignas(64) L2OrderBook {
    double askQuantity[kMaxSize];
    double askPrice[kMaxSize];
    double bidQuantity[kMaxSize];
    double bidPrice[kMaxSize];
    std::chrono::high_resolution_clock::time_point t;
    int askSize;
    int bidSize;
    bool newData;
};

struct alignas(64) Opportunity {
    int buy_exchange;
    int sell_exchange;
    int buy_levels;
    int sell_levels;
    double buy_vwap;
    double sell_vwap;
    double profit_pct;
    double order_size;
};

void process(std::vector<L2OrderBook>&, config&, std::vector<Opportunity>&);
