#include "LivoxClient.h"
#include "fstream"
#include "livox_lidar_def.h"
#include "livox_lidar_api.h"
#include <iostream>
#include <thread>

namespace mandeye {

    std::string ReplaceAll(std::string str, const std::string &from, const std::string &to) {
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
        }
        return str;
    }

    nlohmann::json LivoxClient::produceStatus() {
        nlohmann::json data;
        data["init_success"] = init_succes;
        data["LivoxLidarInfo"]["dev_type"] = m_LivoxLidarInfo.dev_type;
        data["LivoxLidarInfo"]["lidar_ip"] = m_LivoxLidarInfo.lidar_ip;
        data["LivoxLidarInfo"]["sn"] = m_LivoxLidarInfo.sn;
        data["counters"]["imu"] = m_recivedImuMsgs;
        data["counters"]["lidar"] = m_recivedPointMessages;
        std::lock_guard<std::mutex> lcK(m_bufferMutex);
        data["LivoxLidarInfo"]["timestamp"] = m_timestamp;
        if (m_bufferPtr) {
            data["buffers"]["point"]["counter"] = m_bufferPtr->size();
        } else {
            data["buffers"]["point"]["counter"] = "NULL";
        }
        return data;
    }

    void LivoxClient::startLog() {
        std::lock_guard<std::mutex> lcK(m_bufferMutex);
        m_bufferPtr = std::make_shared<LivoxPointsBuffer>();
    }

    void LivoxClient::stopLog() {
        std::lock_guard<std::mutex> lcK(m_bufferMutex);
        m_bufferPtr = nullptr;
    }

    LivoxPointsBufferPtr LivoxClient::retrieveCollectedLidarData() {
        std::lock_guard<std::mutex> lcK(m_bufferMutex);
        // prepare copy of pointer to buffer
        LivoxPointsBufferPtr returnPointer{ std::make_shared<LivoxPointsBuffer>()};
        std::swap(m_bufferPtr,returnPointer);
        return returnPointer;
    }

    void LivoxClient::startListener(const std::string &interfaceIp) {
        constexpr char configFn[] = "/tmp/config.json";

        std::string fillInConfig(LivoxClient::config);
        fillInConfig = ReplaceAll(fillInConfig, "${HOSTIP}", interfaceIp);

        std::ofstream configFile(configFn);
        configFile << fillInConfig;
        configFile.close();
        init_succes = LivoxLidarSdkInit(configFn);
        if (!init_succes) {
            return;
        }
        SetLivoxLidarPointCloudCallBack(PointCloudCallback, (void *) this);
        SetLivoxLidarImuDataCallback(ImuDataCallback, (void *) this);
        SetLivoxLidarInfoChangeCallback(LidarInfoChangeCallback, (void *) this);
    }

    union ToUint64 {
        uint64_t data;
        uint8_t array[8];
    };


    void LivoxClient::PointCloudCallback(uint32_t handle, const uint8_t dev_type, LivoxLidarEthernetPacket *data,
                                         void *client_data) {
        if (data == nullptr || client_data == nullptr) {
            return;
        }

        LivoxClient *this_ptr = (LivoxClient *) client_data;

        this_ptr->m_recivedPointMessages++;

//  printf("point cloud handle: %u, data_num: %d, data_type: %d, length: %d, frame_counter: %d\n",
//         handle, data->dot_num, data->data_type, data->length, data->frame_cnt);

        if (data->data_type == kLivoxLidarCartesianCoordinateHighData) {
            LivoxLidarCartesianHighRawPoint *p_point_data = (LivoxLidarCartesianHighRawPoint *) data->data;
            std::lock_guard<std::mutex> lcK(this_ptr->m_bufferMutex);
            ToUint64 toUint64;
            std::memcpy(toUint64.array, data->timestamp, sizeof(uint64_t));
            this_ptr->m_timestamp = toUint64.data;
            if (this_ptr->m_bufferPtr == nullptr) {
                return;
            }
            auto &buffer = this_ptr->m_bufferPtr;
            buffer->resize(buffer->size() + data->dot_num);
            for (uint32_t i = 0; i < data->dot_num; i++) {
                LivoxPoint point;
                point.point = p_point_data[i];
                point.timestamp = toUint64.data + i * data->time_interval;
                buffer->push_back(point);
            }
        } else if (data->data_type == kLivoxLidarCartesianCoordinateLowData) {
            LivoxLidarCartesianLowRawPoint *p_point_data = (LivoxLidarCartesianLowRawPoint *) data->data;
        } else if (data->data_type == kLivoxLidarSphericalCoordinateData) {
            LivoxLidarSpherPoint *p_point_data = (LivoxLidarSpherPoint *) data->data;
        }
    }

    void LivoxClient::ImuDataCallback(uint32_t handle, const uint8_t dev_type, LivoxLidarEthernetPacket *data,
                                      void *client_data) {
        if (data == nullptr || client_data == nullptr) {
            return;
        }

        LivoxClient *this_ptr = (LivoxClient *) client_data;

        this_ptr->m_recivedImuMsgs++;
//  printf("Imu data callback handle:%u, data_num:%u, data_type:%u, length:%u, frame_counter:%u.\n",
//         handle, data->dot_num, data->data_type, data->length, data->frame_cnt);
    }


    void LivoxClient::WorkModeCallback(livox_status status, uint32_t handle, LivoxLidarAsyncControlResponse *response,
                                       void *client_data) {
        if (response == nullptr) {
            return;
        }
        printf("WorkModeCallack, status:%u, handle:%u, ret_code:%u, error_key:%u",
               status, handle, response->ret_code, response->error_key);

    }

    void LivoxClient::RebootCallback(livox_status status, uint32_t handle, LivoxLidarRebootResponse *response,
                                     void *client_data) {
        if (response == nullptr) {
            return;
        }
        printf("RebootCallback, status:%u, handle:%u, ret_code:%u",
               status, handle, response->ret_code);
    }

    void LivoxClient::SetIpInfoCallback(livox_status status, uint32_t handle, LivoxLidarAsyncControlResponse *response,
                                        void *client_data) {
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
                                                LivoxLidarDiagInternalInfoResponse *response, void *client_data) {
        if (status != kLivoxLidarStatusSuccess) {
            printf("Query lidar internal info failed.\n");
            QueryLivoxLidarInternalInfo(handle, QueryInternalInfoCallback, nullptr);
            return;
        }

        if (response == nullptr) {
            return;
        }

        uint8_t host_point_ipaddr[4]{0};
        uint16_t host_point_port = 0;
        uint16_t lidar_point_port = 0;

        uint8_t host_imu_ipaddr[4]{0};
        uint16_t host_imu_data_port = 0;
        uint16_t lidar_imu_data_port = 0;

        uint16_t off = 0;
        for (uint8_t i = 0; i < response->param_num; ++i) {
            LivoxLidarKeyValueParam *kv = (LivoxLidarKeyValueParam *) &response->data[off];
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
               host_point_ipaddr[0], host_point_ipaddr[1], host_point_ipaddr[2], host_point_ipaddr[3], host_point_port,
               lidar_point_port);

        printf("Host imu ip addr:%u.%u.%u.%u, host imu port:%u, lidar imu port:%u.\n",
               host_imu_ipaddr[0], host_imu_ipaddr[1], host_imu_ipaddr[2], host_imu_ipaddr[3], host_imu_data_port,
               lidar_imu_data_port);

    }

    void LivoxClient::LidarInfoChangeCallback(const uint32_t handle, const LivoxLidarInfo *info, void *client_data) {
        if (info == nullptr) {
            printf("lidar info change callback failed, the info is nullptr.\n");
            return;
        }
        printf("LidarInfoChangeCallback Lidar handle: %u SN: %s\n", handle, info->sn);
        SetLivoxLidarWorkMode(handle, kLivoxLidarNormal, &LivoxClient::WorkModeCallback, client_data);

        QueryLivoxLidarInternalInfo(handle, &LivoxClient::QueryInternalInfoCallback, client_data);
        LivoxClient *this_ptr = (LivoxClient *) (client_data);
        if (this_ptr) {
            this_ptr->m_LivoxLidarInfo = *info;
        }
    }


} // namespace mandeye