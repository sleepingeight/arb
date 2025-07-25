#pragma once

#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <simdjson.h>
#include "orderbook.hpp"

using client = websocketpp::client<websocketpp::config::asio_tls_client>;
using context_ptr = websocketpp::lib::shared_ptr<boost::asio::ssl::context>;

class wsClient {
public:
  wsClient() = delete;
  wsClient(std::string, bool, L2OrderBook&);
  ~wsClient();

private:
  void initialise(std::string const &hostname);

  context_ptr onTLSInit(websocketpp::connection_hdl);
  void onOpen(client *c, websocketpp::connection_hdl hdl);
  void onMessage(websocketpp::connection_hdl hdl, client::message_ptr msg);
  void onFail(client *c, websocketpp::connection_hdl hdl);
  void onClose(client *c, websocketpp::connection_hdl hdl);

  client endpoint_;
  std::string status_;
  std::string err_reason_;
  std::string uri_;
  bool double_in_string_;
  std::shared_ptr<std::thread> thread_;
  websocketpp::connection_hdl hdl_;

  simdjson::ondemand::parser parser_;

  L2OrderBook& snapshot_;
};

void connectToEndpoints(const config&, std::vector<std::unique_ptr<wsClient>>&, std::vector<L2OrderBook>&);
