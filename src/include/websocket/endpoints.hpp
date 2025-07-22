#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include <string>
#include <iostream>

using client = websocketpp::client<websocketpp::config::asio_tls_client>;
using context_ptr = websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context>;

class connect
{
public:
    connect(std::string hostname)
    {
        uri_ = "wss://" + hostname;

        endpoint_.clear_access_channels(websocketpp::log::alevel::all);
        endpoint_.set_error_channels(websocketpp::log::elevel::all);
        endpoint_.init_asio();
        endpoint_.start_perpetual();

        thread_ = websocketpp::lib::make_shared<websocketpp::lib::thread>(&client::run, &endpoint_);

        initialise(uri_);
    }

    int initialise(std::string const &hostname)
    {
        std::string uri = "wss://" + hostname;

        websocketpp::lib::error_code ec;
        client::connection_ptr con = endpoint_.get_connection(uri, ec);
        if (ec)
        {
            std::cout << "> Connect initialization error: " << ec.message() << std::endl;
            return -1;
        }

        // endpoint_.set_open_handler(websocketpp::lib::bind(
        //     &connect::onOpen,
        //     websocketpp::lib::placeholders::_1,
        //     &endpoint_));
        //     con->set_open_handler(websocketpp::lib::bind(
        //         &connect::onOpen,
        //         this,
        //         &endpoint_,
        //         websocketpp::lib::placeholders::_1));
        // con->set_fail_handler(websocketpp::lib::bind(
        //     &connect::onFail,
        //     this,
        //     &endpoint_,
        //     websocketpp::lib::placeholders::_1));
        // con->set_close_handler(websocketpp::lib::bind(
        //     &connect::onClose,
        //     this,
        //     &endpoint_,
        //     websocketpp::lib::placeholders::_1));
        // con->set_message_handler(websocketpp::lib::bind(
        //     &connect::onMessage,
        //     this,
        //     websocketpp::lib::placeholders::_1,
        //     websocketpp::lib::placeholders::_2));
        // con->set_tls_init_handler(websocketpp::lib::bind(
        //     &connect::onTLSInit,
        //     this,
        //     websocketpp::lib::placeholders::_1,
        //     websocketpp::lib::placeholders::_2));

        hdl_ = con->get_handle();
        endpoint_.connect(con);

        return 0;
    }

    ~connect()
    {
        endpoint_.stop_perpetual();
        websocketpp::lib::error_code ec;
        endpoint_.close(hdl_, websocketpp::close::status::going_away, "", ec);
        if (ec)
        {
            std::cout << "> Error closing connection: " << uri_ << "\n";
        }

        thread_->join();
    }

    void onOpen(client *c, websocketpp::connection_hdl hdl)
    {
        status_ = "Open";
        client::connection_ptr con = c->get_con_from_hdl(hdl);
    }

    void onMessage(websocketpp::connection_hdl hdl, client::message_ptr msg)
    {
        std::cout << msg->get_payload() << "\n";
    }

    void onFail(client *c, websocketpp::connection_hdl hdl)
    {
        status_ = "Failed";
        client::connection_ptr con = c->get_con_from_hdl(hdl);
        err_reason_ = con->get_ec().message();
    }

    void onClose(client *c, websocketpp::connection_hdl hdl)
    {
        status_ = "Closed";
    }

    context_ptr onTLSInit(const char *hostname, websocketpp::connection_hdl)
    {
        context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
        try
        {
            ctx->set_options(boost::asio::ssl::context::default_workarounds |
                             boost::asio::ssl::context::no_sslv2 |
                             boost::asio::ssl::context::no_sslv3 |
                             boost::asio::ssl::context::single_dh_use);
        }
        catch (std::exception &e)
        {
            std::cout << "TLS Initialization Error: " << e.what() << '\n';
        }

        return ctx;
    }

private:
    client endpoint_;
    std::string status_;
    std::string err_reason_;
    std::string uri_;
    std::shared_ptr<std::thread> thread_;
    websocketpp::connection_hdl hdl_;
};
