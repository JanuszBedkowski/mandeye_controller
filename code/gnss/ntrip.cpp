#include "ntrip.h"
#include <boost/beast/core/detail/base64.hpp>
#include <iostream>

namespace mandeye
{
    NtripClient::NtripClient(boost::asio::io_context& io,
                             const std::string& host,
                             const std::string& port,
                             const std::string& mountpoint,
                             const std::string& user,
                             const std::string& pass,
                             RtcmCallback cb)
        : resolver_(io), socket_(io),
          host_(host), port_(port), mountpoint_(mountpoint),
          user_(user), pass_(pass), callback_(cb) {}

    void NtripClient::start() {
        do_connect();
    }

    void NtripClient::stop() {
        boost::system::error_code ec;
        socket_.close(ec);
    }

    void NtripClient::do_connect() {
        resolver_.async_resolve(host_, port_,
            [this](auto ec, auto results) {
                if (!ec) {
                    boost::asio::async_connect(socket_, results,
                        [this](auto ec, auto) {
                            if (!ec) send_request();
                        });
                }
            });
    }

    void NtripClient::send_request() {
        std::string credentials = user_ + ":" + pass_;
        std::string encoded;
        const auto encodedSize = boost::beast::detail::base64::encoded_size(credentials.size());
        encoded.resize(encodedSize);
        boost::beast::detail::base64::encode(encoded.data(), credentials.data(), credentials.size());
        //encoded.resize(encodedSize);

        std::ostringstream req;
        //mountPointString = "GET %s HTTP/1.1\r\nUser-Agent: %s\r\nAuthorization: Basic %s\r\n" % (self.mountpoint, useragent, self.user)
        req << "GET /" << mountpoint_ << " HTTP/1.1\r\n"
            << "Host: " << host_ << "\r\n"
            << "User-Agent: BoostNtrip/1.0\r\n"
            << "Authorization: Basic " << encoded << "\r\n"
            << "Connection: close\r\n\r\n";
        std::cout << "Sending request:\n" << req.str() << std::endl;
        boost::asio::async_write(socket_, boost::asio::buffer(req.str()),
            [this](auto ec, auto) {
                if (!ec) read_headers();
            });
    }

    void NtripClient::read_headers() {
        boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
            [this](auto ec, auto) {
                if (!ec) {
                    // Header consumed; now read RTCM3 stream
                    header_parsed_ = true;
                    read_stream();
                }
            });
    }

    void NtripClient::read_stream() {
        boost::asio::async_read(socket_, response_,
            boost::asio::transfer_at_least(1),
            [this](auto ec, auto bytes_transferred) {
                if (!ec && callback_) {
                    std::istream is(&response_);
                    std::vector<uint8_t> data(bytes_transferred);
                    is.read(reinterpret_cast<char*>(data.data()), bytes_transferred);
                    callback_(data.data(), data.size());
                    read_stream();
                }
            });
    }

    void NtripClient::send_gga(const std::string& gga_sentence) {
        if (socket_.is_open()) {
            std::string msg = gga_sentence + "\r\n";
            boost::asio::async_write(socket_, boost::asio::buffer(msg),
                [](auto, auto) {});
        }
    }
}