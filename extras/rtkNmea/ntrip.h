#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <string>
#include <vector>
#include <chrono>

namespace mandeye {

  using RtcmCallback = std::function<void(const uint8_t* data, std::size_t size)>;

  class NtripClient {
  public:
    NtripClient(boost::asio::io_context& io,
                const std::string& host,
                const std::string& port,
                const std::string& mountpoint,
                const std::string& user,
                const std::string& pass,
                RtcmCallback cb);

    void start();
    void stop();
    void send_gga(const std::string& gga_sentence);

  private:
    void do_connect();
    void send_request();
    void read_headers();
    void read_stream();
    void schedule_reconnect();
    void start_watchdog();
    void stop_watchdog();

    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::steady_timer timer_;
    boost::asio::steady_timer watchdog_timer_;

    boost::asio::streambuf response_;

    std::string host_, port_, mountpoint_, user_, pass_;
    RtcmCallback callback_;

    std::chrono::seconds retry_delay_;
    std::chrono::seconds watchdog_timeout_;

    bool header_parsed_ = false;
  };

} // namespace mandeye
