#pragma once

#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <simdjson.h>
#include "orderbook.hpp"
#include "utils.hpp"

using client = websocketpp::client<websocketpp::config::asio_tls_client>;
using context_ptr = websocketpp::lib::shared_ptr<boost::asio::ssl::context>;

/**
 * @brief WebSocket client for real-time exchange data
 * 
 * Manages WebSocket connections to cryptocurrency exchanges, handling connection
 * lifecycle, message processing, and orderbook updates. Thread-safe implementation
 * with TLS support.
 */
class wsClient {
public:
    wsClient() = delete;  ///< Default constructor disabled

    /**
     * @brief Constructs a WebSocket client
     * @param hostname The WebSocket server hostname
     * @param double_in_string Whether numbers are received as strings
     * @param orderbook Reference to the orderbook to update
     * @throws std::runtime_error if connection fails
     */
    wsClient(std::string hostname, bool double_in_string, L2OrderBook& orderbook);
    
    /**
     * @brief Destructor - ensures proper cleanup of WebSocket connection
     */
    ~wsClient();

private:
    /**
     * @brief Initializes the WebSocket connection
     * @param hostname The WebSocket server hostname
     * @throws std::runtime_error if initialization fails
     */
    void initialise(std::string const& hostname);

    /**
     * @brief TLS initialization callback
     * @param hdl Connection handle
     * @return Configured SSL context
     */
    context_ptr onTLSInit(websocketpp::connection_hdl hdl);

    /**
     * @brief Connection open callback
     * @param c Client pointer
     * @param hdl Connection handle
     */
    void onOpen(client* c, websocketpp::connection_hdl hdl);

    /**
     * @brief Message received callback
     * @param hdl Connection handle
     * @param msg Received message
     */
    void onMessage(websocketpp::connection_hdl hdl, client::message_ptr msg);

    /**
     * @brief Connection failure callback
     * @param c Client pointer
     * @param hdl Connection handle
     */
    void onFail(client* c, websocketpp::connection_hdl hdl);

    /**
     * @brief Connection close callback
     * @param c Client pointer
     * @param hdl Connection handle
     */
    void onClose(client* c, websocketpp::connection_hdl hdl);

    client endpoint_;                     ///< WebSocket client endpoint
    std::string status_;                 ///< Current connection status
    std::string err_reason_;             ///< Error message if connection failed
    std::string uri_;                    ///< WebSocket URI
    bool double_in_string_;              ///< Whether numbers are received as strings
    std::shared_ptr<std::thread> thread_; ///< WebSocket client thread
    websocketpp::connection_hdl hdl_;    ///< Connection handle
    simdjson::ondemand::parser parser_;  ///< JSON parser
    L2OrderBook& snapshot_;              ///< Reference to orderbook to update
};

/**
 * @brief Connects to all configured exchange endpoints
 * @param config Trading configuration
 * @param clients Vector to store created WebSocket clients
 * @param orderbooks Vector of orderbooks for each exchange
 * @throws std::runtime_error if connection to any endpoint fails
 */
void connectToEndpoints(const config& config, 
                       std::vector<std::unique_ptr<wsClient>>& clients,
                       std::vector<L2OrderBook>& orderbooks);
