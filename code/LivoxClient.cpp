#include "LivoxClient.h"
#include "fstream"
#include "livox_lidar_def.h"
#include "livox_lidar_api.h"

namespace mandeye {

nlohmann::json LivoxClient::produceStatus(){
  nlohmann::json data;
  data["init_success"] = init_succes;
  data["LivoxLidarInfo"]["dev_type"] = m_LivoxLidarInfo.dev_type;
  data["LivoxLidarInfo"]["lidar_ip"] = m_LivoxLidarInfo.lidar_ip;
  data["LivoxLidarInfo"]["sn"] = m_LivoxLidarInfo.sn;
  data["counters"]["imu"] = m_recivedImuMsgs;
  data["counters"]["lidar"] = m_recivedPointMessages;
  return data;
}

void LivoxClient::startListener(){
  constexpr char configFn[] = "/tmp/config.json";
  std::ofstream configFile (configFn);
  configFile << LivoxClient::config;
  configFile.close();
  init_succes = LivoxLidarSdkInit(configFn);
  if (!init_succes){
    return;
  }
  SetLivoxLidarPointCloudCallBack(PointCloudCallback, (void*) this);
  SetLivoxLidarImuDataCallback(ImuDataCallback, (void*) this);
  SetLivoxLidarInfoChangeCallback(LidarInfoChangeCallback, (void*) this);

}



void LivoxClient::PointCloudCallback(uint32_t handle, const uint8_t dev_type, LivoxLidarEthernetPacket* data, void* client_data) {
  if (data == nullptr || client_data == nullptr) {
    return;
  }

  LivoxClient *this_ptr = (LivoxClient*) client_data;

  this_ptr->m_recivedPointMessages++;

//  printf("point cloud handle: %u, data_num: %d, data_type: %d, length: %d, frame_counter: %d\n",
//         handle, data->dot_num, data->data_type, data->length, data->frame_cnt);

  if (data->data_type == kLivoxLidarCartesianCoordinateHighData) {
    LivoxLidarCartesianHighRawPoint *p_point_data = (LivoxLidarCartesianHighRawPoint *)data->data;
    for (uint32_t i = 0; i < data->dot_num; i++) {
      //p_point_data[i].x;
      //p_point_data[i].y;
      //p_point_data[i].z;
    }
  }
  else if (data->data_type == kLivoxLidarCartesianCoordinateLowData) {
    LivoxLidarCartesianLowRawPoint *p_point_data = (LivoxLidarCartesianLowRawPoint *)data->data;
  } else if (data->data_type == kLivoxLidarSphericalCoordinateData) {
    LivoxLidarSpherPoint* p_point_data = (LivoxLidarSpherPoint *)data->data;
  }
}

void LivoxClient::ImuDataCallback(uint32_t handle, const uint8_t dev_type,  LivoxLidarEthernetPacket* data, void* client_data) {
  if (data == nullptr || client_data == nullptr) {
    return;
  }

  LivoxClient *this_ptr = (LivoxClient*) client_data;

  this_ptr->m_recivedImuMsgs++;
//  printf("Imu data callback handle:%u, data_num:%u, data_type:%u, length:%u, frame_counter:%u.\n",
//         handle, data->dot_num, data->data_type, data->length, data->frame_cnt);
}


void LivoxClient::WorkModeCallback(livox_status status, uint32_t handle,LivoxLidarAsyncControlResponse *response, void *client_data) {
  if (response == nullptr) {
    return;
  }
  printf("WorkModeCallack, status:%u, handle:%u, ret_code:%u, error_key:%u",
         status, handle, response->ret_code, response->error_key);

}

void LivoxClient::RebootCallback(livox_status status, uint32_t handle, LivoxLidarRebootResponse* response, void* client_data) {
  if (response == nullptr) {
    return;
  }
  printf("RebootCallback, status:%u, handle:%u, ret_code:%u",
         status, handle, response->ret_code);
}

void LivoxClient::SetIpInfoCallback(livox_status status, uint32_t handle, LivoxLidarAsyncControlResponse *response, void *client_data) {
  if (response == nullptr) {
    return;
  }
  printf("LivoxLidarIpInfoCallback, status:%u, handle:%u, ret_code:%u, error_key:%u",
         status, handle, response->ret_code, response->error_key);

  if (response->ret_code == 0 && response->error_key == 0) {
    LivoxLidarRequestReboot(handle, RebootCallback, nullptr);
  }
}

void LivoxClient::QueryInternalInfoCallback(livox_status status, uint32_t handle,
                               LivoxLidarDiagInternalInfoResponse* response, void* client_data) {
  if (status != kLivoxLidarStatusSuccess) {
    printf("Query lidar internal info failed.\n");
    QueryLivoxLidarInternalInfo(handle, QueryInternalInfoCallback, nullptr);
    return;
  }

  if (response == nullptr) {
    return;
  }

  uint8_t host_point_ipaddr[4] {0};
  uint16_t host_point_port = 0;
  uint16_t lidar_point_port = 0;

  uint8_t host_imu_ipaddr[4] {0};
  uint16_t host_imu_data_port = 0;
  uint16_t lidar_imu_data_port = 0;

  uint16_t off = 0;
  for (uint8_t i = 0; i < response->param_num; ++i) {
    LivoxLidarKeyValueParam* kv = (LivoxLidarKeyValueParam*)&response->data[off];
    if (kv->key == kKeyLidarPointDataHostIPCfg) {
      memcpy(host_point_ipaddr, &(kv->value[0]), sizeof(uint8_t) * 4);
      memcpy(&(host_point_port), &(kv->value[4]), sizeof(uint16_t));
      memcpy(&(lidar_point_port), &(kv->value[6]), sizeof(uint16_t));
    } else if (kv->key == kKeyLidarImuHostIPCfg) {
      memcpy(host_imu_ipaddr, &(kv->value[0]), sizeof(uint8_t) * 4);
      memcpy(&(host_imu_data_port), &(kv->value[4]), sizeof(uint16_t));
      memcpy(&(lidar_imu_data_port), &(kv->value[6]), sizeof(uint16_t));
    }
    off += sizeof(uint16_t) * 2;
    off += kv->length;
  }

  printf("Host point cloud ip addr:%u.%u.%u.%u, host point cloud port:%u, lidar point cloud port:%u.\n",
         host_point_ipaddr[0], host_point_ipaddr[1], host_point_ipaddr[2], host_point_ipaddr[3], host_point_port, lidar_point_port);

  printf("Host imu ip addr:%u.%u.%u.%u, host imu port:%u, lidar imu port:%u.\n",
         host_imu_ipaddr[0], host_imu_ipaddr[1], host_imu_ipaddr[2], host_imu_ipaddr[3], host_imu_data_port, lidar_imu_data_port);

}

void LivoxClient::LidarInfoChangeCallback(const uint32_t handle, const LivoxLidarInfo* info, void* client_data) {
  if (info == nullptr) {
    printf("lidar info change callback failed, the info is nullptr.\n");
    return;
  }
  printf("LidarInfoChangeCallback Lidar handle: %u SN: %s\n", handle, info->sn);
  SetLivoxLidarWorkMode(handle, kLivoxLidarNormal, &LivoxClient::WorkModeCallback, client_data);

  QueryLivoxLidarInternalInfo(handle, &LivoxClient::QueryInternalInfoCallback, client_data);
  LivoxClient * this_ptr = (LivoxClient*)(client_data);
  if (this_ptr)
  {
    this_ptr->m_LivoxLidarInfo = *info;
  }
}



} // namespace mandeye