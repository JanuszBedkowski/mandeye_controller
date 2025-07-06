#pragma once
#include <boost/asio.hpp>
#include <functional>
#include <string>
namespace mandeye
{
  class NtripClient {
  public:
    using RtcmCallback = std::function<void(const uint8_t*, std::size_t)>;

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

    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf response_;
    std::string host_, port_, mountpoint_, user_, pass_;
    RtcmCallback callback_;
    bool header_parsed_ = false;
  };
}