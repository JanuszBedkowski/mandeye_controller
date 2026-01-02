#include "ntrip.h"
#include <boost/beast/core/detail/base64.hpp>
#include <iostream>
#include <sstream>

namespace mandeye {

NtripClient::NtripClient(boost::asio::io_context& io,
                         const std::string& host,
                         const std::string& port,
                         const std::string& mountpoint,
                         const std::string& user,
                         const std::string& pass,
                         RtcmCallback cb)
    : resolver_(io),
      socket_(io),
      timer_(io),
      watchdog_timer_(io),
      host_(host),
      port_(port),
      mountpoint_(mountpoint),
      user_(user),
      pass_(pass),
      callback_(cb),
      retry_delay_(std::chrono::seconds(5)),
      watchdog_timeout_(std::chrono::seconds(10)) {}

void NtripClient::start() {
    do_connect();
}

void NtripClient::stop() {
    boost::system::error_code ec;
    socket_.cancel(ec);
    socket_.close(ec);
    timer_.cancel(ec);
    watchdog_timer_.cancel(ec);
}

void NtripClient::schedule_reconnect() {
    std::cout << "[NtripClient] Scheduling reconnect in " << retry_delay_.count() << "s...\n";
    timer_.expires_after(retry_delay_);
    timer_.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            do_connect();
        }
    });
}

void NtripClient::do_connect() {
    std::cout << "[NtripClient] Resolving host " << host_ << ":" << port_ << "\n";
    resolver_.async_resolve(host_, port_,
        [this](const boost::system::error_code& ec, const auto& results) {
            if (ec) {
                std::cerr << "[NtripClient] Resolve error: " << ec.message() << "\n";
                schedule_reconnect();
                return;
            }

            std::cout << "[NtripClient] Connecting...\n";
            boost::asio::async_connect(socket_, results,
                [this](const boost::system::error_code& ec, const auto&) {
                    if (ec) {
                        std::cerr << "[NtripClient] Connect error: " << ec.message() << "\n";
                        schedule_reconnect();
                    } else {
                        send_request();
                    }
                });
        });
}

void NtripClient::send_request() {
    std::string credentials = user_ + ":" + pass_;
    std::string encoded(boost::beast::detail::base64::encoded_size(credentials.size()), '\0');
    boost::beast::detail::base64::encode(encoded.data(), credentials.data(), credentials.size());

    std::ostringstream req;
    req << "GET /" << mountpoint_ << " HTTP/1.1\r\n"
        << "Host: " << host_ << "\r\n"
        << "User-Agent: BoostNtrip/1.0\r\n"
        << "Authorization: Basic " << encoded << "\r\n"
        << "Connection: keep-alive\r\n\r\n";

    std::string request = req.str();
    std::cout << "[NtripClient] Sending request:\n" << request;

    boost::asio::async_write(socket_, boost::asio::buffer(request),
        [this](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                std::cerr << "[NtripClient] Write request error: " << ec.message() << "\n";
                schedule_reconnect();
            } else {
                read_headers();
            }
        });
}

void NtripClient::read_headers() {
    boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
        [this](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                std::cerr << "[NtripClient] Read headers error: " << ec.message() << "\n";
                schedule_reconnect();
            } else {
                header_parsed_ = true;
                read_stream();
            }
        });
}

void NtripClient::read_stream() {
    boost::asio::async_read(socket_, response_, boost::asio::transfer_at_least(1),
        [this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (!ec && callback_) {
                // Reset watchdog timer on successful read
                start_watchdog();

                std::istream is(&response_);
                std::vector<uint8_t> data(bytes_transferred);
                is.read(reinterpret_cast<char*>(data.data()), bytes_transferred);
                callback_(data.data(), data.size());

                read_stream(); // continue reading
            } else {
                std::cerr << "[NtripClient] Stream read error: " << ec.message() << "\n";
                stop_watchdog();
                schedule_reconnect();
            }
        });
}

void NtripClient::send_gga(const std::string& gga_sentence) {
    if (socket_.is_open()) {
        std::string msg = gga_sentence + "\r\n";
        boost::asio::async_write(socket_, boost::asio::buffer(msg),
            [](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    std::cerr << "[NtripClient] GGA send error: " << ec.message() << "\n";
                }
            });
    } else {
        std::cerr << "[NtripClient] Cannot send GGA; socket is not open.\n";
    }
}

void NtripClient::start_watchdog() {
    watchdog_timer_.expires_after(watchdog_timeout_);
    watchdog_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            std::cerr << "[NtripClient] Watchdog timeout: no data received, reconnecting.\n";
            stop();
            schedule_reconnect();
        }
    });
}

void NtripClient::stop_watchdog() {
    boost::system::error_code ec;
    watchdog_timer_.cancel(ec);
}

} // namespace mandeye