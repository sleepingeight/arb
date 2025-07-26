#include "orderbook.hpp"
#include "utils.hpp"
#include "ws_client.hpp"
#include <csignal>
#include <memory>
#include <simdjson.h>
#include <atomic>
#include <iomanip>
#include <thread>
#include <chrono>

// Global control flags
std::atomic<bool> g_running = true;
std::atomic<bool> g_display_opportunities = false;
std::atomic<int> g_min_profit_filter = 0;  // in basis points
std::atomic<bool> g_can_print = true;

// Synchronisation
std::counting_semaphore<2> sem(0);
std::counting_semaphore<2> sem1(0);

// Performance metrics
Metrics g_metrics;

// Websocket clients
std::vector<std::unique_ptr<wsClient>> connections;

void displayHelp() {
    std::cout << "\nAvailable Commands:\n"
              << "  h, help     - Show this help message\n"
              << "  s, start    - Start displaying opportunities\n"
              << "  p, stop     - Pause opportunity display\n"
              << "  m, metrics  - Show performance metrics\n"
              << "  f N         - Filter opportunities with profit >= N basis points\n"
              << "  q, quit     - Exit the program\n"
              << "\n";
}

void displayMetrics() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - g_metrics.start_time);
    uint64_t updates = g_metrics.updates_processed.load(std::memory_order_relaxed);
    uint64_t opps = g_metrics.opportunities_found.load(std::memory_order_relaxed);
    
    std::cout << "\nPerformance Metrics:\n"
              << "Runtime: " << duration.count() << " seconds\n"
              << "Updates Processed: " << updates << "\n"
              << "Opportunities Found: " << opps << "\n";

    if (opps > 0) {
        uint64_t avg_latency = g_metrics.total_latency_us.load(std::memory_order_relaxed) / opps;
        uint64_t min_latency = g_metrics.min_latency_us.load(std::memory_order_relaxed);
        uint64_t max_latency = g_metrics.max_latency_us.load(std::memory_order_relaxed);
        
        std::cout << "Latency (μs):\n"
                  << "  Min: " << min_latency << "\n"
                  << "  Avg: " << avg_latency << "\n"
                  << "  Max: " << max_latency << "\n";
    }
    std::cout << "\n";
}

void displayOpportunity(const Opportunity& opp) {
    static const std::array<std::string, 3> exchanges = {"OKX", "Deribit", "Bybit"};
    
    std::cout << "\nArbitrage Opportunity:\n"
              << "Buy on " << exchanges[opp.buy_exchange] 
              << " at " << std::fixed << std::setprecision(2) << opp.buy_vwap
              << " using " << opp.buy_levels << " levels\n"
              << "Sell on " << exchanges[opp.sell_exchange]
              << " at " << opp.sell_vwap
              << " using " << opp.sell_levels << " levels\n"
              << "Profit: " << std::setprecision(3) << opp.profit_pct << "%\n"
              << "Order Size: " << std::setprecision(6) << opp.order_size << " BTC\n"
              << "Market Impact: " << (opp.buy_levels + opp.sell_levels) << " levels deep\n"
              << "Detection Latency: " << std::fixed << std::setprecision(2) 
              << opp.detection_latency_us << " μs\n"
              << std::string(50, '-') << "\n";
}

void commandProcessor() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::string cmd;
    displayHelp();
    
    while (g_running) {
        std::cout << "> ";
        std::getline(std::cin, cmd);
        
        if (cmd == "h" || cmd == "help") {
            displayHelp();
        }
        else if (cmd == "s" || cmd == "start") {
            g_display_opportunities = true;
            std::cout << "Started displaying opportunities\n";
        }
        else if (cmd == "p" || cmd == "stop") {
            g_display_opportunities = false;
            std::cout << "Paused opportunity display\n";
        }
        else if (cmd == "m" || cmd == "metrics") {
            displayMetrics();
        }
        else if (cmd[0] == 'f' && cmd.length() > 1) {
            try {
                g_min_profit_filter = std::stoi(cmd.substr(2));
                std::cout << "Filtering opportunities with profit >= " 
                         << g_min_profit_filter << " basis points\n";
            } catch (...) {
                std::cout << "Invalid filter value. Usage: f <basis_points>\n";
            }
        }
        else if (cmd == "q" || cmd == "quit") {
            g_running = false;
            kill(getpid(), SIGINT);
            break;
        }
        else if (!cmd.empty()) {
            std::cout << "Unknown command. Type 'h' for help.\n";
        }
    }
}

int main() {
    simdjson::ondemand::parser kParser;
    config kConfig {};

    try {
        const std::string kConfigPath = "../config/config.json";
        loadConfig(kConfigPath, kConfig, kParser);
        
        std::vector<L2OrderBook> orderbooks(kTotalExchanges);
        std::vector<Opportunity> opportunities;
        
        // Start metrics tracking
        g_metrics.start_time = std::chrono::high_resolution_clock::now();
        
        // Start the main processing thread
        std::thread process_thread(process, std::ref(orderbooks), 
                                 std::ref(kConfig), std::ref(opportunities));
        
        // Connect to exchanges
        connectToEndpoints(kConfig, connections, orderbooks);
        
        // Start the command processor in a separate thread
        std::thread cmd_thread(commandProcessor);

        // Main loop - handle opportunity display and metrics
        while (g_running) {
            if (g_display_opportunities && g_can_print.load(std::memory_order_acquire) && !opportunities.empty()) {
                int cnt = 0;
                // need to add versioning here, else same might get repeated
                for (const auto& opp : opportunities) {
                    displayOpportunity(opp);
                    cnt++;
                    // if(cnt > 5) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                opportunities.clear();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Cleanup
        if (cmd_thread.joinable()) cmd_thread.join();
        if (process_thread.joinable()) process_thread.join();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}