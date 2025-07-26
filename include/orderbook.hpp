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
 * 
 * Memory layout is optimized for sequential access:
 * - Ask and bid arrays are grouped together
 * - Price and quantity arrays are adjacent for each side
 * - Control variables are placed at the end
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
 * 
 * The structure captures:
 * - Exchange identifiers for both sides
 * - Price levels used in calculation
 * - VWAP prices and profit metrics
 * - Order size and timing information
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
    double detection_latency_us;  ///< Detection latency in microseconds
    std::chrono::high_resolution_clock::time_point detection_time;  ///< When opportunity was detected
};

/**
 * @brief Process orderbooks to find arbitrage opportunities
 * 
 * This function continuously monitors orderbooks from multiple exchanges and
 * identifies profitable arbitrage opportunities using VWAP calculations.
 * 
 * @param orderbooks Vector of orderbooks from different exchanges
 * @param cfg Trading configuration parameters
 * @param out_opps Vector to store found opportunities
 * @param new_ob Reference to store the latest processed orderbook
 * @note Thread-safe through semaphore synchronization
 */
void process(std::vector<L2OrderBook>& orderbooks, config& cfg, std::vector<Opportunity>& out_opps, L2OrderBook& new_ob);

/**
 * @brief Performance metrics tracking structure
 * 
 * Thread-safe structure for tracking various performance metrics:
 * - Update and opportunity counts
 * - Latency statistics (min, max, total)
 * - Runtime tracking
 * 
 * All counters are atomic to ensure accurate concurrent updates.
 */
struct alignas(64) Metrics {
    std::atomic<uint64_t> updates_processed{0};    ///< Total number of orderbook updates processed
    std::atomic<uint64_t> opportunities_found{0};  ///< Total number of opportunities detected
    std::atomic<uint64_t> total_latency_us{0};     ///< Cumulative latency for statistics
    std::atomic<uint64_t> max_latency_us{0};       ///< Maximum observed latency
    std::atomic<uint64_t> min_latency_us{std::numeric_limits<uint64_t>::max()};  ///< Minimum observed latency
    std::chrono::high_resolution_clock::time_point start_time;  ///< Program start time

    /**
     * @brief Updates latency statistics atomically
     * 
     * Updates running statistics including:
     * - Total latency for average calculation
     * - Maximum latency using compare-and-swap
     * - Minimum latency using compare-and-swap
     * 
     * @param latency New latency value in microseconds
     */
    void updateLatency(uint64_t latency) {
        total_latency_us += latency;
        
        uint64_t current_max = max_latency_us.load(std::memory_order_relaxed);
        while (latency > current_max && 
               !max_latency_us.compare_exchange_weak(current_max, latency)) {}
        
        uint64_t current_min = min_latency_us.load(std::memory_order_relaxed);
        while (latency < current_min && 
               !min_latency_us.compare_exchange_weak(current_min, latency)) {}
    }
};

/**
 * @brief Database writer thread function
 * 
 * Continuously writes orderbook summaries and opportunities to persistent storage:
 * - Orderbook metrics to SQLite database
 * - Opportunity details to text file
 * 
 * @param opportunities Vector of opportunities to write
 * @param ob Latest orderbook state to summarize
 * @return -1 on error, never returns on success
 */
int dbWriterThread(std::vector<Opportunity>& opportunities, L2OrderBook& ob);