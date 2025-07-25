#include "ws_client.hpp"

#include <cstdlib>
#include <simdjson.h>

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>
#include <format>

#include "orderbook.hpp"

using client = websocketpp::client<websocketpp::config::asio_tls_client>;
using context_ptr
    = websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context>;

wsClient::wsClient(std::string hostname, bool double_in_string,
    L2OrderBook& orderbook)
    : snapshot_(orderbook), double_in_string_(double_in_string)
{
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

void wsClient::initialise(std::string const& hostname)
{
    std::string uri = "wss://" + hostname;

    websocketpp::lib::error_code ec;
    client::connection_ptr con = endpoint_.get_connection(uri, ec);
    if (ec) {
        throw std::runtime_error("unable to initialise connection");
    }
    hdl_ = con->get_handle();

    con->set_open_handler(
        websocketpp::lib::bind(&wsClient::onOpen, this, &endpoint_,
            websocketpp::lib::placeholders::_1));
    con->set_fail_handler(
        websocketpp::lib::bind(&wsClient::onFail, this, &endpoint_,
            websocketpp::lib::placeholders::_1));
    con->set_close_handler(
        websocketpp::lib::bind(&wsClient::onClose, this, &endpoint_,
            websocketpp::lib::placeholders::_1));
    con->set_message_handler(websocketpp::lib::bind(
        &wsClient::onMessage, this, websocketpp::lib::placeholders::_1,
        websocketpp::lib::placeholders::_2));

    endpoint_.connect(con);

    snapshot_.newData.store(false, std::memory_order_release);
}

wsClient::~wsClient()
{
    endpoint_.stop_perpetual();
    websocketpp::lib::error_code ec;
    endpoint_.close(hdl_, websocketpp::close::status::going_away, "", ec);

    thread_->join();
}

void wsClient::onOpen(client* c, websocketpp::connection_hdl hdl)
{
    status_ = "Open";
    client::connection_ptr con = c->get_con_from_hdl(hdl);
}

void wsClient::onMessage(websocketpp::connection_hdl hdl, client::message_ptr msg)
{
    snapshot_.t = std::chrono::high_resolution_clock::now();
    simdjson::ondemand::document doc = parser_.iterate(msg->get_raw_payload());
    simdjson::ondemand::object object = doc.get_object();
    std::cout << msg->get_raw_payload() << "\n";

    simdjson::ondemand::value asks;
    auto err = doc["asks"].get(asks);

    int i = 0;
    if (err == simdjson::SUCCESS) {
        std::cout << "got asks\n";
        for (auto ask : asks) {
            int j = 0;
            for (auto val : ask) {
                if (j == 0) {
                    if (double_in_string_) {
                        snapshot_.askQuantity[i] = val.get_double_in_string();
                    } else {
                        snapshot_.askQuantity[i] = val.get_double();
                    }
                    j++;
                } else {
                    if (double_in_string_) {
                        snapshot_.askPrice[i] = val.get_double_in_string();
                    } else {
                        snapshot_.askPrice[i] = val.get_double();
                    }
                }
            }
                i++;
        }
    }
    snapshot_.askSize = i;
    std::cout << "\n\nno. of asks " << i << "\n";

    i = 0;
    simdjson::ondemand::array bids;
    err = doc["bids"].get(bids);
    if (err == simdjson::SUCCESS) {
        for (auto bid : bids) {
            int j = 0;
            for (auto val : bid) {
                if (j == 0) {
                    if (double_in_string_) {
                        snapshot_.bidPrice[i] = val.get_double_in_string();
                    } else {
                        snapshot_.bidPrice[i] = val.get_double();
                    }
                    j++;
                } else {
                    if (double_in_string_) {
                        snapshot_.bidQuantity[i] = val.get_double_in_string();
                    } else {
                        snapshot_.bidQuantity[i] = val.get_double();
                    }
                }
            }
            i++;
        }
        snapshot_.bidSize = i;
        std::cout << "no. of bids " << i << "\n";
    }
    snapshot_.newData.store(true, std::memory_order_release);
}

void wsClient::onFail(client* c, websocketpp::connection_hdl hdl)
{
    status_ = "Failed";
    client::connection_ptr con = c->get_con_from_hdl(hdl);
    err_reason_ = con->get_ec().message();
}

void wsClient::onClose(client* c, websocketpp::connection_hdl hdl)
{
    status_ = "Closed";
}

context_ptr
wsClient::onTLSInit(websocketpp::connection_hdl)
{
    context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(
        boost::asio::ssl::context::sslv23);
    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::no_sslv3
            | boost::asio::ssl::context::single_dh_use);
    } catch (std::exception& e) {
        std::cout << "TLS Initialization Error: " << e.what() << '\n';
    }

    return ctx;
}

void connectToEndpoints(const config& config, std::vector<std::unique_ptr<wsClient>>& clients, std::vector<L2OrderBook>& orderbooks) {
    for(size_t i = 0; i < kTotalExchanges; i++) {
        if(config.exchanges[i]) {
            for(size_t j = 0; j < kTotalPairs; j++) {
                if (!config.pairs[j]) continue;

                const auto& exchange = kExchanges[i];
                const auto& pair_sv = kPairs[j];

                auto pos = pair_sv.find('/');
                std::string_view base = pair_sv.substr(0, pos);
                std::string_view quote = pair_sv.substr(pos + 1);

                std::string hostname = "ws.gomarket-cpp.goquant.io/ws/l2-orderbook/";

                switch (i) {
                    case 0:
                        hostname += std::format("{}/{}-{}", exchange, base, quote);
                        break;
                    case 1:
                        hostname += std::format("{}/{}_{}", exchange, base, quote);
                        break;
                    case 2:
                        hostname +=  std::format("{}/{}{}/spot", exchange, base, quote);
                        break;
                }          
                std::cout << "hostname: " << hostname << "\n\n";
                try {
                    clients.emplace_back(std::make_unique<wsClient>(hostname, kUseDoubleInString[i], orderbooks[i]));
                }
                catch (std::exception &e) {
                    std::cerr << "unable to connect to endpoint wss://" << hostname << "\nerror: " 
                        << e.what() << "\n";
                }
            }
        }
    } 
}   