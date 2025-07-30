# arb

A high-performance C++ system for real-time cryptocurrency arbitrage across multiple exchanges. The system monitors orderbook data from multiple exchanges and identifies profitable trading opportunities based on price differentials, and detects potential arbitrage oppurtunities in sub-milliseconds latency.

## Features

- Real-time orderbook monitoring via WebSocket connections
- Support for multiple exchanges (OKX, Deribit, Bybit)
- Multiple trading pairs (BTC/USDT, ETH/USDT, SOL/USDT)
- Configurable trading parameters
- SQLite database integration for orderbook analytics
- Low-latency processing with optimized data structures

## Performance Metrics

- P50 Latency: 603 microseconds
- P95 Latency: 770 microseconds

Achieved consistent performance of <1ms to detect every oppurtunity from an update.
Note that this performance is achievied without collection of metrics, if metrics are collected, an increas of 100ms latency is expected, due to the usage of atomic variables in metrics to avoid races.


## Arbitrage Strategy

The system implements a multi-level arbitrage strategy based on Volume-Weighted Average Price (VWAP) calculations.

### Understanding VWAP

VWAP represents the average price of an asset weighted by volume across price levels. Unlike simple price comparison between exchanges, VWAP provides a more accurate picture of the actual execution price for larger orders that may need to "eat" into multiple levels of the order book.

For example, if you want to buy 1 BTC and the order book looks like:
```
Ask Prices | Quantities
50,000     | 0.3 BTC
50,100     | 0.4 BTC
50,200     | 0.5 BTC
```
The VWAP for buying 1 BTC would be:
```
(0.3 * 50,000 + 0.4 * 50,100 + 0.3 * 50,200) / 1.0 = 50,100
```
This is higher than the best ask price of 50,000 because the order size requires going deeper into the order book.

### Multi-Level Arbitrage Implementation

1. **Order Book Processing**
   - System maintains full L2 order book data (up to 50 levels) for each exchange
   - Each price level contains both price and available quantity
   - Order books are updated in real-time via WebSocket feeds

2. **VWAP Calculation Algorithm**
   ```cpp
   // Pseudo-code of our implementation
   for each price level:
       available = min(level.quantity, remaining_size)
       cumulative_quantity += available
       cumulative_cost += available * level.price
       
       if cumulative_quantity >= target_size:
           break
           
   vwap = cumulative_cost / cumulative_quantity
   ```

3. **Cross-Exchange Opportunity Detection**
   - For each exchange pair (A, B):
     * Calculate buy VWAP on exchange A using ask levels
     * Calculate sell VWAP on exchange B using bid levels
     * Consider trading fees from both exchanges
     * If (sell_vwap - buy_vwap)/buy_vwap > min_profit:
       * Record opportunity with exact levels needed

4. **Dynamic Level Selection**
   - System tracks how many levels are needed for each side
   - Buy side might use different number of levels than sell side
   - Each opportunity records:
     * Number of ask levels needed on buy side
     * Number of bid levels needed on sell side
     * Total quantity achievable
     * Expected profit percentage

This implementation allows us to:
- Find opportunities that simple top-of-book comparison would miss
- Calculate exact profit potential including fees
- Determine precise order sizes that maximize profit
- Account for varying liquidity depths across exchanges

## Architecture

### Components

1. **WebSocket Client (`ws_client`)**
   - Handles real-time connections to exchange WebSocket APIs
   - Processes incoming orderbook updates
   - Thread-safe implementation with TLS support

2. **Orderbook Manager (`orderbook`)**
   - Maintains Level 2 (L2) orderbook data
   - Cache-aligned data structures for optimal performance
   - Implements VWAP calculations and opportunity detection

3. **Configuration System (`utils`)**
   - JSON-based configuration management
   - Dynamic exchange and trading pair selection
   - Configurable profit thresholds and order sizes

### Data Flow

1. WebSocket connections receive real-time orderbook updates
2. Updates are processed and stored in L2OrderBook structures
3. Processing thread analyzes orderbooks for arbitrage opportunities
4. Profitable opportunities are identified based on configured thresholds
5. Key metrics are stored in SQLite database for analysis

## Dependencies

- WebSocket++ for WebSocket connections
- simdjson for fast JSON parsing
- SQLite3 for database operations
- Boost.Asio for networking
- CMake build system

## Building

> Due to websocketpp not supporting c++20 and also does not work with newer boost and cmake versions, it is recommended to use cmake version < 4 and boost version - 1.83. This will be fixed soon.

```bash
sudo apt install libboost-dev libssl-dev
mkdir build
cd build
cmake -G Ninja ..
ninja
```

You can also use 
```bash
cmake -G Ninja -DFAKE=ON ..
ninja
```
to create fake arbitrage opportunities only to witness what `arb` can do.

## Configuration

Create a `config.json` file in the `config` directory with the following structure:

```json
{
    "exchanges": ["okx", "deribit", "bybit"],
    "pairs": ["BTC/USDT", "ETH/USDT", "SOL/USDT"],
    "min_profit": 0.1,
    "max_order_size": 1.0,
    "latency_ms": 50,
    "fees": {
        "okx": 0.1,
        "deribit": 0.1,
        "bybit": 0.1
    }
}
```

## Usage

1. Configure the system through `config.json`
2. Run the executable (when in build folder):
   ```bash
   ./arb
   ```
3. You can ask the CLI for displaying opportunities, or the file `storage/opportunities.txt` has the information of all oppurtunities. 
4. The file: `storage/orderbook_summary.db` has the persistent information of the updates.

### Available Commands

The system provides an interactive command-line interface with the following commands:

- `h` or `help`
  - Shows the help message listing all available commands
  - Useful for quick reference during operation

- `s` or `start`
  - Starts displaying arbitrage opportunities
  - Shows up to 10 opportunities at a time
  - Automatically updates with new opportunities
  - Includes profit percentage, exchange pairs, and trading volumes

- `m` or `metrics`
  - Displays performance metrics of the system
  - Shows:
    * Total runtime in seconds
    * Number of updates processed
    * Number of opportunities found
    * Latency statistics (minimum, average, maximum) in microseconds

- `y` or `system`
  - Shows detailed system resource usage
  - Displays:
    * CPU information (cores and active threads)
    * Process ID
    * Memory usage (total, used, and free RAM in MB)
    * Process resources (user/system CPU time, maximum memory usage)

- `q` or `quit`
  - Gracefully exits the program
  - Ensures proper cleanup of WebSocket connections
  - Saves any pending data to the database

## Performance Optimization

- Cache-aligned data structures (64-byte alignment)
- Synchronization using semaphores instead of busy waiting to reduce cpu overhead
- Optimized the biggest bottleneck - JSON parsing with simdjson, which uses SIMD internally
- Efficient memory layout for orderbook data

## Database Schema

The system maintains an SQLite database with the following schema:

```sql
CREATE TABLE OrderBook (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER,
    topAsk REAL,
    topAskQty REAL,
    topBid REAL,
    topBidQty REAL,
    midPrice REAL,
    spread REAL,
    imbalance REAL
);
```
