#pragma once

#include <semaphore>
#include <string_view>
#include <array>
#include <string>
#include <simdjson.h>

/// @brief Total number of supported exchanges
const int kTotalExchanges = 3;

/// @brief Total number of supported trading pairs
const int kTotalPairs = 3;

/// @brief Array of supported exchange names
/// @note Order must match the exchange indices used throughout the code
constexpr std::array<std::string_view, kTotalExchanges> kExchanges = {"okx", "deribit", "bybit"};

/// @brief Array of supported trading pairs
/// @note Format is BASE/QUOTE (e.g., BTC/USDT)
constexpr std::array<std::string_view, kTotalPairs> kPairs = {"BTC/USDT", "ETH/USDT", "SOL/USDT"};

/// @brief Base websocket hostnames for each exchange
/// @note Order must match kExchanges array
constexpr std::array<std::string_view, kTotalExchanges> kHostNames = {
    "ws.gomarket-cpp.goquant.io/ws/l2-orderbook/okx/",
    "ws.gomarket-cpp.goquant.io/ws/l2-orderbook/deribit/",
    "ws.gomarket-cpp.goquant.io/ws/l2-orderbook/bybit/"
};

/// @brief Flag indicating whether each exchange uses string representation for doubles
/// @note Required for proper JSON parsing of numeric values
constexpr std::array<bool, kTotalExchanges> kUseDoubleInString = {true, false, true};

/// @brief Path to store detected arbitrage opportunities
/// @note File is opened in append mode
const std::string kOppStoragePath = "../storage/opportunities.txt";

/// @brief Path to SQLite database for orderbook summaries
/// @note Database is created if it doesn't exist
const std::string kDbStoragePath = "../storage/orderbook_summary.db";

/**
 * @brief Configuration structure for the trading system
 * 
 * Contains all configurable parameters for the arbitrage system.
 * Optimized struct layout for proper memory alignment.
 */
struct config {
    double fees[3];           ///< Trading fees for each exchange (in percentage)
    double min_profit;        ///< Minimum profit threshold for trades (in USD)
    double max_order_size;    ///< Maximum allowed order size (in base currency)
    double latency_ms;        ///< Expected latency in milliseconds
    bool exchanges[kTotalExchanges];  ///< Active exchanges flags
    bool pairs[kTotalPairs];         ///< Active trading pairs flags
};

/**
 * @brief Loads configuration from a JSON file
 * 
 * Validates and loads trading configuration including:
 * - Enabled exchanges and their fees
 * - Trading pairs (currently limited to one pair)
 * - Profit thresholds and order size limits
 * - Expected latency parameters
 * 
 * @param file_path Path to the configuration JSON file
 * @param config Reference to the config structure to populate
 * @param parser Reference to the JSON parser
 * @throws std::runtime_error if configuration is invalid or missing required fields
 */
void loadConfig(const std::string& file_path, config& config, simdjson::ondemand::parser& parser);

/**
 * @brief Gets the index of an exchange or trading pair name
 * 
 * Used for mapping between string identifiers and array indices:
 * - Exchange names are mapped to indices in kExchanges
 * - Trading pairs are mapped to indices in kPairs
 * 
 * @param name Name of the exchange or trading pair to look up
 * @param type Type of lookup: 1 for exchange, 2 for trading pair
 * @return Index of the item if found, -1 if not found
 */
int getIndex(std::string_view name, int type);

/// @brief Semaphore for synchronizing orderbook updates
/// @note Binary semaphore used as a signal between WebSocket and processing threads
extern std::counting_semaphore<2> sem;

/// @brief Semaphore for synchronizing database writes
/// @note Binary semaphore used as a signal between processing and database threads
extern std::counting_semaphore<2> sem1;

