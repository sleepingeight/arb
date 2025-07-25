#include "orderbook.hpp"
#include "utils.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <semaphore>
#include <vector>

void process(std::vector<L2OrderBook>& orderbooks, config& cfg, std::vector<Opportunity>& out_opps)
{
    int num_orderboks = orderbooks.size();
    double buy_qty[kMaxSize], buy_cost[kMaxSize];
    double sell_qty[kMaxSize], sell_cost[kMaxSize];
    std::vector<L2OrderBookLocal> local_books(num_orderboks);
    while (true) {
        sem.acquire();
        bool should_process = false;
        int count_new = 0;
        size_t new_data_index = -1;
        for (size_t i = 0; i < kTotalExchanges; i++) {
            bool is_new = orderbooks[i].newData.load(std::memory_order_acquire);
            memcpy(&local_books[i], &orderbooks[i], 1616);
            orderbooks[i].newData.store(false, std::memory_order_release);
            count_new = i;
            if(is_new) break;
        }
        // if (!should_process){
        //     std::this_thread::sleep_for(std::chrono::microseconds(50));
        //     continue;
        // }

        int buy_n = 0, sell_n = 0;

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
                    if (net_pct >= cfg.min_profit) {
                        out_opps.push_back({ i, j,
                            bi + 1,
                            si + 1,
                            buy_vwap,
                            sell_vwap,
                            net_pct,
                            common_qty });
                        Opportunity& o = out_opps.back();
                        std::cout << "Buy " << o.order_size << " BTC across " << o.buy_levels
                                  << " asks on exchange " << o.buy_exchange
                                  << " (VWAP: " << o.buy_vwap << ") and sell across "
                                  << o.sell_levels << " bids on exchange " << o.sell_exchange
                                  << " (VWAP: " << o.sell_vwap << "); net profit = "
                                  << o.profit_pct << "%" << std::endl;
                    }
                    if (buy_qty[bi] < sell_qty[si])
                        ++bi;
                    else
                        ++si;
                }
            }
        }
        std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - local_books[count_new].t);
        // std::cout << "" << duration.count() << "" << '\n';    
    }
}