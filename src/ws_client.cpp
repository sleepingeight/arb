#include "ws_client.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

using client = websocketpp::client<websocketpp::config::asio_tls_client>;
using context_ptr =
    websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context>;

wsClient::wsClient(std::string hostname) {
  uri_ = "wss://" + hostname;

  endpoint_.set_access_channels(websocketpp::log::alevel::all);
  endpoint_.clear_access_channels(websocketpp::log::alevel::frame_payload);
  endpoint_.set_error_channels(websocketpp::log::elevel::all);

  endpoint_.init_asio();
  endpoint_.set_tls_init_handler(websocketpp::lib::bind(
      &wsClient::onTLSInit, this, websocketpp::lib::placeholders::_1));
  endpoint_.start_perpetual();

  thread_ = websocketpp::lib::make_shared<websocketpp::lib::thread>(
      &client::run, &endpoint_);

  initialise(hostname);
}

void wsClient::initialise(std::string const &hostname) {
  std::string uri = "wss://" + hostname;

  websocketpp::lib::error_code ec;
  client::connection_ptr con = endpoint_.get_connection(uri, ec);
  if (ec) {
    throw std::runtime_error("unable to initialise connection");
  }
  hdl_ = con->get_handle();

  con->set_open_handler(websocketpp::lib::bind(
      &wsClient::onOpen, this, &endpoint_, websocketpp::lib::placeholders::_1));
  con->set_fail_handler(websocketpp::lib::bind(
      &wsClient::onFail, this, &endpoint_, websocketpp::lib::placeholders::_1));
  con->set_close_handler(
      websocketpp::lib::bind(&wsClient::onClose, this, &endpoint_,
                             websocketpp::lib::placeholders::_1));
  con->set_message_handler(websocketpp::lib::bind(
      &wsClient::onMessage, this, websocketpp::lib::placeholders::_1,
      websocketpp::lib::placeholders::_2));

  endpoint_.connect(con);
}

wsClient::~wsClient() {
  endpoint_.stop_perpetual();
  websocketpp::lib::error_code ec;
  endpoint_.close(hdl_, websocketpp::close::status::going_away, "", ec);

  thread_->join();
}

void wsClient::onOpen(client *c, websocketpp::connection_hdl hdl) {
  status_ = "Open";
  client::connection_ptr con = c->get_con_from_hdl(hdl);
}

void wsClient::onMessage(websocketpp::connection_hdl hdl,
                         client::message_ptr msg) {
  std::cout << msg->get_payload() << "\n";
}

void wsClient::onFail(client *c, websocketpp::connection_hdl hdl) {
  status_ = "Failed";
  client::connection_ptr con = c->get_con_from_hdl(hdl);
  err_reason_ = con->get_ec().message();
}

void wsClient::onClose(client *c, websocketpp::connection_hdl hdl) {
  status_ = "Closed";
}

context_ptr wsClient::onTLSInit(websocketpp::connection_hdl) {
  context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(
      boost::asio::ssl::context::sslv23);
  try {
    ctx->set_options(boost::asio::ssl::context::default_workarounds |
                     boost::asio::ssl::context::no_sslv2 |
                     boost::asio::ssl::context::no_sslv3 |
                     boost::asio::ssl::context::single_dh_use);
  } catch (std::exception &e) {
    std::cout << "TLS Initialization Error: " << e.what() << '\n';
  }

  return ctx;
}
