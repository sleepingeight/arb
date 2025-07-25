#pragma once

#include <chrono>
#include "utils.hpp"

/// @brief Maximum size of the orderbook (number of price levels)
const int kMaxSize = 50;

/**
 * @brief Level 2 Orderbook structure
 * 
 * Maintains a snapshot of the order book with multiple price levels.
 * Aligned to 64-byte boundary for optimal cache line usage.
 */
struct alignas(64) L2OrderBook {
    double askQuantity[kMaxSize];  ///< Quantities available at ask prices
    double askPrice[kMaxSize];     ///< Ask prices sorted in ascending order
    double bidQuantity[kMaxSize];  ///< Quantities available at bid prices
    double bidPrice[kMaxSize];     ///< Bid prices sorted in descending order
    std::chrono::high_resolution_clock::time_point t;  ///< Timestamp of last update
    int askSize;                   ///< Number of valid ask price levels
    int bidSize;                   ///< Number of valid bid price levels
    bool newData;                  ///< Flag indicating new data is available
};

/**
 * @brief Structure representing an arbitrage opportunity
 * 
 * Contains details about a potential arbitrage opportunity between two exchanges.
 * Aligned to 64-byte boundary for optimal cache line usage.
 */
struct alignas(64) Opportunity {
    int buy_exchange;    ///< Index of the exchange to buy from
    int sell_exchange;   ///< Index of the exchange to sell on
    int buy_levels;      ///< Number of price levels needed for buy
    int sell_levels;     ///< Number of price levels needed for sell
    double buy_vwap;     ///< Volume-weighted average price for buy
    double sell_vwap;    ///< Volume-weighted average price for sell
    double profit_pct;   ///< Expected profit percentage
    double order_size;   ///< Size of the order in base currency
};

/**
 * @brief Process orderbooks to find arbitrage opportunities
 * @param orderbooks Vector of orderbooks from different exchanges
 * @param cfg Trading configuration parameters
 * @param out_opps Vector to store found opportunities
 * @note Thread-safe through semaphore synchronization
 */
void process(std::vector<L2OrderBook>& orderbooks, config& cfg, std::vector<Opportunity>& out_opps);
