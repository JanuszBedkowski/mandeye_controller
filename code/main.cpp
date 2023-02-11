#include "web_server.h"
#include <json.hpp>
#include <ostream>
#include <chrono>
#include <thread>

namespace mandeye {
    using json = nlohmann::json;

    std::string produceReport() {
        json j;
        j["name"] = "Mandye";
        std::ostringstream s;
        s << std::setw(4) << j;
        return s.str();
    }

} // namespace mandeye


int main(int *argc, char **argv) {
    health_server::setStatusHandler(mandeye::produceReport);
    std::thread http_thread1(health_server::server_worker);

    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}