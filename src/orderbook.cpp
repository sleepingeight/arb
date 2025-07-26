#include "orderbook.hpp"
#include "utils.hpp"
#include <chrono>
#include <cstddef>
#include <cstring>
#include <semaphore>
#include <vector>
#include "sqlite3.h"
#include <fstream>
#include <iomanip>
#include <array>

// Forward declaration of global variables from main.cpp
extern struct Metrics g_metrics;

/**
 * Implementation notes:
 * - Uses semaphore synchronization for thread safety
 * - Maintains local copy of orderbooks to prevent data races
 * - Calculates VWAP using cumulative quantities and costs
 * - Optimizes memory access with aligned data structures
 * - Processes opportunities in O(n) time per orderbook update
 */
void process(std::vector<L2OrderBook>& orderbooks, config& cfg, std::vector<Opportunity>& out_opps, L2OrderBook& new_ob)
{
    int num_orderboks = orderbooks.size();
    double buy_qty[kMaxSize], buy_cost[kMaxSize];
    double sell_qty[kMaxSize], sell_cost[kMaxSize];
    std::vector<L2OrderBook> local_books(num_orderboks);
    
    while (true) {
        sem.acquire();
        g_metrics.updates_processed++;
        out_opps.clear();
        int count_new = 0;
        for (size_t i = 0; i < kTotalExchanges; i++) {
            if(orderbooks[i].newData) {
                memcpy(&local_books[i], &orderbooks[i], sizeof(L2OrderBook));
                orderbooks[i].newData = false;
                count_new = i;
                break;
            }
        }

        int buy_n = 0, sell_n = 0;
        out_opps.clear();

        for (int i = 0; i < kTotalExchanges; ++i) {
            if (!cfg.exchanges[i])
                continue;
            const auto& lbuy = local_books[i];
            if (lbuy.askSize == 0)
                continue;

            // Build buy cumulatives up to actual askSize
            {
                double total_q = 0.0, total_c = 0.0;
                buy_n = 0;
                for (int lvl = 0; lvl < lbuy.askSize && total_q < cfg.max_order_size; ++lvl) {
                    double avail = std::min(lbuy.askQuantity[lvl], cfg.max_order_size - total_q);
                    total_q += avail;
                    total_c += avail * lbuy.askPrice[lvl];
                    buy_qty[buy_n] = total_q;
                    buy_cost[buy_n] = total_c;
                    ++buy_n;
                }
                if (buy_n == 0)
                    continue;
            }

            for (int j = 0; j < kTotalExchanges; ++j) {
                if (!cfg.exchanges[j])
                    continue;
                const auto& lsell = local_books[j];
                if (lsell.bidSize == 0)
                    continue;

                // Build sell cumulatives up to actual bidSize
                {
                    double total_q = 0.0, total_c = 0.0;
                    sell_n = 0;
                    for (int lvl = 0; lvl < lsell.bidSize && total_q < cfg.max_order_size; ++lvl) {
                        double avail = std::min(lsell.bidQuantity[lvl], cfg.max_order_size - total_q);
                        total_q += avail;
                        total_c += avail * lsell.bidPrice[lvl];
                        sell_qty[sell_n] = total_q;
                        sell_cost[sell_n] = total_c;
                        ++sell_n;
                    }
                    if (sell_n == 0)
                        continue;
                }
                
                int bi = 0, si = 0;
                while (bi < buy_n && si < sell_n) {
                    double common_qty = (buy_qty[bi] < sell_qty[si] ? buy_qty[bi] : sell_qty[si]);
                    double buy_vwap = buy_cost[bi] / buy_qty[bi];
                    double sell_vwap = sell_cost[si] / sell_qty[si];
                    double gross_pct = (sell_vwap - buy_vwap) / buy_vwap * 100.0;
                    double net_pct = gross_pct - (cfg.fees[i] + cfg.fees[j]);
                    double net_profit = net_pct * common_qty * buy_vwap / 100.0;
                    if (net_profit >= cfg.min_profit) {
                        auto now = std::chrono::high_resolution_clock::now();
                        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                            now - local_books[count_new].t).count();
                            
                        out_opps.push_back({
                            i, j,
                            bi + 1,
                            si + 1,
                            buy_vwap,
                            sell_vwap,
                            net_pct,
                            common_qty,
                            static_cast<double>(latency),
                            now
                        });

                        g_metrics.opportunities_found++;
                        g_metrics.updateLatency(static_cast<uint64_t>(latency));
                    }
                    
                    if (buy_qty[bi] < sell_qty[si])
                        ++bi;
                    else
                        ++si;
                }
            }
        }
        memcpy(&new_ob, &local_books[count_new], sizeof(L2OrderBook));
        sem1.release();
    }
}

/**
 * Implementation notes:
 * - Uses SQLite transactions for atomic writes
 * - Calculates derived metrics (spread, imbalance)
 * - Handles database errors gracefully
 * - Maintains continuous operation through semaphore synchronization
 */
int dbWriterThread(std::vector<Opportunity>&oppurtunities, L2OrderBook& ob) {
    sqlite3* db;
    if (sqlite3_open(kDbStoragePath.c_str(), &db)) {
        std::cerr << "DB open failed\n";
        return -1;
    }

    // Open opportunities file for append
    std::ofstream opps_file(kOppStoragePath, std::ios::app);
    if (!opps_file) {
        std::cerr << "Failed to open opportunities.txt\n";
        sqlite3_close(db);
        return -1;
    }

    const char* createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS OrderBook (
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
    )";

    char* errMsg = nullptr;
    if (sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Table creation failed: " << errMsg << "\n";
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return -1;
    }

    std::vector<Opportunity> local_opps;

    while (true) {
        sem1.acquire();

        local_opps = oppurtunities;

        for (const auto& opp : local_opps) {
            opps_file << "\nArbitrage Opportunity:\n"
                     << "Buy on " << kExchanges[opp.buy_exchange] 
                     << " at " << std::fixed << std::setprecision(2) << opp.buy_vwap
                     << " using " << opp.buy_levels << " levels\n"
                     << "Sell on " << kExchanges[opp.sell_exchange]
                     << " at " << opp.sell_vwap
                     << " using " << opp.sell_levels << " levels\n"
                     << "Profit: " << std::setprecision(3) << opp.profit_pct << "%\n"
                     << "Order Size: " << std::setprecision(6) << opp.order_size << " BTC\n"
                     << "Market Impact: " << (opp.buy_levels + opp.sell_levels) << " levels deep\n"
                     << "Detection Latency: " << std::fixed << std::setprecision(2) 
                     << opp.detection_latency_us << " μs\n"
                     << std::string(50, '-') << "\n";
        }
        opps_file.flush();

        const char* sql = R"(
            INSERT INTO OrderBook (
                timestamp, topAsk, topAskQty, topBid, topBidQty, midPrice, spread, imbalance
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
            return -1;
        }
    
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    
        double topAsk = ob.askPrice[0];
        double topAskQty = ob.askQuantity[0];
        double topBid = ob.bidPrice[0];
        double topBidQty = ob.bidQuantity[0];
    
        double mid = (topAsk + topBid) / 2.0;
        double spread = topAsk - topBid;
        double imbalance = (topBidQty - topAskQty) / (topBidQty + topAskQty + 1e-9);
    
        int idx = 1;
        auto ts = std::chrono::duration_cast<std::chrono::microseconds>(ob.t.time_since_epoch()).count();
    
        sqlite3_bind_int64(stmt, idx++, ts);
        sqlite3_bind_double(stmt, idx++, topAsk);
        sqlite3_bind_double(stmt, idx++, topAskQty);
        sqlite3_bind_double(stmt, idx++, topBid);
        sqlite3_bind_double(stmt, idx++, topBidQty);
        sqlite3_bind_double(stmt, idx++, mid);
        sqlite3_bind_double(stmt, idx++, spread);
        sqlite3_bind_double(stmt, idx++, imbalance);
    
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Insert failed: " << sqlite3_errmsg(db) << "\n";
        }
    
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "END TRANSACTION;", nullptr, nullptr, nullptr);
    }

    opps_file.close();
    sqlite3_close(db);
}