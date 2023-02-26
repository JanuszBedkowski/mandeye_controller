#pragma once
#include "livox_lidar_def.h"
#include <json.hpp>

namespace mandeye {

class LivoxClient {
public:
  nlohmann::json produceStatus();

  void startListener();
  void startLog();
  void stopLog();

private:
  uint64_t m_recivedImuMsgs{0};
  uint64_t m_recivedPointMessages{0};
  LivoxLidarInfo m_LivoxLidarInfo;
  LivoxLidarInfo m_LivoxLidarDeviceType;


  bool init_succes{false};

  static constexpr char config[] = {
      "{\n"
      "\t\"MID360\": {\n"
      "\t\t\"lidar_net_info\" : {\n"
      "\t\t\t\"cmd_data_port\": 56100,\n"
      "\t\t\t\"push_msg_port\": 56200,\n"
      "\t\t\t\"point_data_port\": 56300,\n"
      "\t\t\t\"imu_data_port\": 56400,\n"
      "\t\t\t\"log_data_port\": 56500\n"
      "\t\t},\n"
      "\t\t\"host_net_info\" : {\n"
      "\t\t\t\"cmd_data_ip\" : \"192.168.1.5\",\n"
      "\t\t\t\"cmd_data_port\": 56101,\n"
      "\t\t\t\"push_msg_ip\": \"192.168.1.5\",\n"
      "\t\t\t\"push_msg_port\": 56201,\n"
      "\t\t\t\"point_data_ip\": \"192.168.1.5\",\n"
      "\t\t\t\"point_data_port\": 56301,\n"
      "\t\t\t\"imu_data_ip\" : \"192.168.1.5\",\n"
      "\t\t\t\"imu_data_port\": 56401,\n"
      "\t\t\t\"log_data_ip\" : \"192.168.1.5\",\n"
      "\t\t\t\"log_data_port\": 56501\n"
      "\t\t}\n"
      "\t}\n"
      "}\t"};

  // callbacks
  static void PointCloudCallback(uint32_t handle, const uint8_t dev_type,
                                 LivoxLidarEthernetPacket *data,
                                 void *client_data);

  static void ImuDataCallback(uint32_t handle, const uint8_t dev_type,
                              LivoxLidarEthernetPacket *data,
                              void *client_data);

  static void WorkModeCallback(livox_status status, uint32_t handle,
                               LivoxLidarAsyncControlResponse *response,
                               void *client_data);

  static void SetIpInfoCallback(livox_status status, uint32_t handle,
                                LivoxLidarAsyncControlResponse *response,
                                void *client_data);

  static void RebootCallback(livox_status status, uint32_t handle,
                             LivoxLidarRebootResponse *response,
                             void *client_data);

  static void
  QueryInternalInfoCallback(livox_status status, uint32_t handle,
                            LivoxLidarDiagInternalInfoResponse *packet,
                            void *client_data);

  static void LidarInfoChangeCallback(const uint32_t handle,
                                      const LivoxLidarInfo *info,
                                      void *client_data);
};
} // namespace mandeye