#include "orderbook.hpp"
#include "utils.hpp"
#include "ws_client.hpp"
#include <csignal>
#include <memory>
#include <simdjson.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <fstream>
#include <sys/sysinfo.h>
#include <sys/resource.h>
#include <unistd.h>
#include <iomanip> 

/// @brief Semaphores for synchronizing orderbook updates and database writes
std::counting_semaphore<2> sem(0);
std::counting_semaphore<2> sem1(0);

/// @brief Global metrics instance for tracking system performance
Metrics g_metrics;

/// @brief Vector of WebSocket client connections to exchanges
std::vector<std::unique_ptr<wsClient>> connections;

/**
 * @brief Displays detailed system resource usage and performance metrics
 * 
 * Shows:
 * - CPU cores and thread count
 * - Process ID and resource usage
 * - System memory statistics
 * - Process-specific memory usage
 */
void displaySystemDetails() {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        double total_ram = si.totalram * si.mem_unit / (1024.0 * 1024.0);
        double free_ram = si.freeram * si.mem_unit / (1024.0 * 1024.0);
        double used_ram = total_ram - free_ram;

        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);

        long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
        int current_pid = getpid();

        std::cout << "\nSystem Details:\n"
                  << "CPU Cores: " << num_cores << "\n"
                  << "Active Threads: " << std::thread::hardware_concurrency() << "\n"
                  << "Process ID: " << current_pid << "\n"
                  << "\nMemory Usage:\n"
                  << "  Total RAM: " << std::fixed << std::setprecision(2) << total_ram << " MB\n"
                  << "  Used RAM: " << used_ram << " MB\n"
                  << "  Free RAM: " << free_ram << " MB\n"
                  << "\nProcess Resources:\n"
                  << "  User CPU Time: " << usage.ru_utime.tv_sec << "." 
                  << std::setfill('0') << std::setw(6) << usage.ru_utime.tv_usec << " seconds\n"
                  << "  System CPU Time: " << usage.ru_stime.tv_sec << "." 
                  << std::setfill('0') << std::setw(6) << usage.ru_stime.tv_usec << " seconds\n"
                  << "  Max RSS: " << (usage.ru_maxrss / 1024.0) << " MB\n\n";
    } else {
        std::cerr << "Failed to get system information\n";
    }
}

/**
 * @brief Displays available CLI commands and their descriptions
 */
void displayHelp() {
    std::cout << "\nAvailable Commands:\n"
              << "  h, help     - Show this help message\n"
              << "  s, start    - Start displaying opportunities (displays only 10 at a time)\n"
              << "  m, metrics  - Show performance metrics\n"
              << "  y, system   - Show system details and resource usage\n"
              << "  q, quit     - Exit the program\n"
              << "\n";
}

/**
 * @brief Displays the detailed metrics information
 */
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
/**
 * @brief Displays new arbitrage opportunities from the log file
 * 
 * Reads and displays new opportunities that have been logged since the last read.
 * Limits output to 80 lines (10 opportunities) at a time and adds small delays
 * to prevent console flooding.
 * 
 * @param last_read_pos Reference to the last read position in the file
 */
void displayNewOpportunities(std::streampos& last_read_pos) {
    std::ifstream opps_file(kOppStoragePath);
    if (!opps_file) {
        std::cerr << "Failed to open opportunities.txt\n";
        return;
    }
    opps_file.seekg(last_read_pos);
    std::string line;
    int count = 0;
    while (std::getline(opps_file, line)) {
        std::cout << line << std::endl;
        count++;
        
        if (count % 5 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (count > 8*10) break;
    }
    last_read_pos = opps_file.tellg();
    opps_file.close();
}

/**
 * @brief Processes user commands in an interactive loop
 * 
 * Handles user input and executes corresponding commands:
 * - help: Display available commands
 * - start: Show new opportunities
 * - metrics: Display performance metrics
 * - system: Show system resource usage
 * - quit: Exit the program
 */
void commandProcessor() {
    std::string cmd;
    displayHelp();
    std::atomic<bool> is_running = true;
    std::streampos last_read_pos = 0;

    while (is_running) {
        std::cout << "> ";
        std::getline(std::cin, cmd);
        
        if (cmd == "h" || cmd == "help") {
            displayHelp();
        }
        else if (cmd == "s" || cmd == "start") {
            std::cout << "Started displaying opportunities\n\n";
            displayNewOpportunities(last_read_pos); 
        }
        else if (cmd == "m" || cmd == "metrics") {
            displayMetrics();
        }
        else if (cmd == "y" || cmd == "system") {
            displaySystemDetails();
        }
        else if (cmd == "q" || cmd == "quit") {
            is_running = false;
            kill(getpid(), SIGINT);
            break;
        }
        else if (!cmd.empty()) {
            std::cout << "Unknown command. Type 'h' for help.\n";
        }
    }
}

/**
 * @brief Main entry point for the arbitrage detection system
 * 
 * Program flow:
 * 1. Load configuration from JSON
 * 2. Initialize orderbooks and metrics
 * 3. Start processing thread for opportunity detection
 * 4. Start database writer thread for logging
 * 5. Connect to exchanges via WebSocket
 * 6. Start command processor for user interaction
 * 7. Clean up on exit
 * 
 */
int main() {
    simdjson::ondemand::parser kParser;
    config kConfig {};

    try {
        const std::string kConfigPath = "../config/config.json";
        loadConfig(kConfigPath, kConfig, kParser);
        
        std::vector<L2OrderBook> orderbooks(kTotalExchanges);
        std::vector<Opportunity> opportunities;
        L2OrderBook new_ob;

        // Start metrics tracking
        g_metrics.start_time = std::chrono::high_resolution_clock::now();
        
        // Start the main processing thread
        std::thread process_thread(process, std::ref(orderbooks), 
                                 std::ref(kConfig), std::ref(opportunities), std::ref(new_ob));

        std::thread db_thread(dbWriterThread, std::ref(opportunities), std::ref(new_ob));
        
        // Connect to exchanges
        connectToEndpoints(kConfig, connections, orderbooks);
        
        // Start the command processor in a separate thread
        std::thread cmd_thread(commandProcessor);

        if (cmd_thread.joinable()) cmd_thread.join();
        if (process_thread.joinable()) process_thread.join();
        if (db_thread.joinable()) db_thread.join();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}