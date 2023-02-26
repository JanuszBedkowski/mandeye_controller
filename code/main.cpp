#include "web_server.h"
#include <chrono>
#include <json.hpp>
#include <ostream>
#include <thread>

#include <LivoxClient.h>

namespace mandeye {

std::shared_ptr<LivoxClient> livoxCLientPtr;

using json = nlohmann::json;

std::string produceReport() {
  json j;
  j["name"] = "Mandye";
  if (livoxCLientPtr){
    j["livox"] = livoxCLientPtr->produceStatus();
  }
  else
  {
    j["livox"] = {};
  }
  std::ostringstream s;
  s << std::setw(4) << j;
  return s.str();
}

} // namespace mandeye

int main(int argc, char **argv) {

  health_server::setStatusHandler(mandeye::produceReport);
  std::thread http_thread1(health_server::server_worker);
  std::thread th([&](){
    mandeye::livoxCLientPtr = std::make_shared<mandeye::LivoxClient>();
    mandeye::livoxCLientPtr->startListener();
  });
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return 0;
}