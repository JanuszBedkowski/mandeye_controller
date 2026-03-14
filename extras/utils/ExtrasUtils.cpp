#include "ExtrasUtils.h"
#include <zmq.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

namespace mandeye::extras {

std::string getEnvString(const std::string& env, const std::string& def)
{
    const char* env_p = std::getenv(env.c_str());
    if (env_p == nullptr)
        return def;
    return std::string{env_p};
}

void startZeroMQListener(const StatusCallback& callback)
{
    try {
        zmq::context_t context(1);
        zmq::socket_t socket(context, zmq::socket_type::sub);
        socket.connect(keys::ZMQ_ENDPOINT);
        socket.set(zmq::sockopt::subscribe, "");
        socket.set(zmq::sockopt::conflate, 1);

        std::cout << "Connected to " << keys::ZMQ_ENDPOINT << std::endl;
        std::cout << "Waiting for messages..." << std::endl;

        while (true) {
            zmq::message_t message;
            auto result = socket.recv(message, zmq::recv_flags::none);
            if (result) {
                const std::string msg_str(static_cast<char*>(message.data()), message.size());
                const nlohmann::json j = nlohmann::json::parse(msg_str);
                if (j.is_object()) {
                    callback(j);
                }
            }
        }
    }
    catch (const zmq::error_t& e) {
        std::cerr << "ZeroMQ Error: " << e.what() << std::endl;
        std::abort();
    }
}

} // namespace mandeye::extras