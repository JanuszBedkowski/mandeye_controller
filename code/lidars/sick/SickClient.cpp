#include "lidars/sick/SickClient.h"
#include "lidars/sick/sick_config.inc.h"
#include <iostream>
#include <fstream>
#include <sick_scan_xd/sick_scan_api.h>
namespace mandeye
{

    std::mutex SickClient::m_instancesMutex;
    SickClient* SickClient::m_instance {nullptr};

    nlohmann::json SickClient::produceStatus()
    {
        nlohmann::json data;
        data["SickLidar"]["status"]["init_success"] = true; // Simulate successful initialization
        data["SickLidar"]["status"]["is_done"] = isDone.load(); // Current status of the data thread
        data["counters"]["lidar"] = m_recivedPointMessages.load(); // Number of received point messages
        data["counters"]["imu"] = m_recivedImuMessages.load();
        data["SickLidar"]["status"]["buffer_lidar_size"] = m_bufferLidarPtr ? m_bufferLidarPtr->size() : 0;
        data["SickLidar"]["status"]["buffer_imu_size"] = m_bufferIMUPtr ? m_bufferIMUPtr->size() : 0;
        data["SickLidar"]["status"]["api_status"] = m_apiStatus;
        data["SickLidar"]["status"]["api_status_message"] = m_apiStatusMessage;
        return data;
    }

    SickClient* SickClient::GetInstanceFromHandle(SickScanApiHandle apiHandle) {
        // Find the instance in the static vector of instances
        std::lock_guard<std::mutex> lock(m_instancesMutex);
        if (m_instance && m_instance->m_apiHandle == apiHandle) {
            return m_instance; // Return the instance if it matches the apiHandle
        }
        return nullptr; // Not found
    }

    bool SickClient::startListener(const std::string& interfaceIp)
    {
        assert(m_instance == nullptr && "SickClient instance already exists, only one instance is allowed");
        //save the config to a temporary file
        std::string configFilePath = "/tmp/sick.launch";
        std::ofstream configFile(configFilePath);
        assert(configFile.is_open() && "Failed to open config file for SickLidar");
        configFile << Launchfile;
        configFile.close();

        std::vector<char*> cli;
        cli.push_back("test_app");
        cli.push_back(const_cast<char*>(configFilePath.c_str()));
        std::string receiverIpArg = "udp_receiver_ip:=" + interfaceIp;
        cli.push_back(const_cast<char*>(receiverIpArg.c_str()));

        m_apiHandle = SickScanApiCreate(cli.size(), cli.data());
        assert(m_apiHandle != nullptr && "Failed to create SickScanApiHandle");
        SickScanApiInitByCli(m_apiHandle, cli.size(), cli.data());

        std::cout << "SickLidar: startListener called with interfaceIp: " << interfaceIp << std::endl;
        // register instance in the static vector
        {
            std::lock_guard<std::mutex> lock1(m_instancesMutex);
            m_instance = this;
        }
        SickScanApiRegisterCartesianPointCloudMsg(m_apiHandle, &SickClient::customizedPointCloudMsgCb);
        SickScanApiRegisterImuMsg(m_apiHandle, &SickClient::imuCallback );

        std::lock_guard<std::mutex> lock(m_bufferMutex);
        // Simulate starting the listener
        m_watchThread = std::thread(&SickClient::DataThreadFunction, this);
        // wait 10 seconds for the API to initialize
        std::this_thread::sleep_for(std::chrono::seconds(10));
        int status = -1;
        char statusMesssage[256];
        m_apiStatus = SickScanApiGetStatus(m_apiHandle, &status, statusMesssage, 256);
        if (m_apiStatus != SICK_SCAN_API_SUCCESS) {
            std::cerr << "SickClient: Failed to start listener, API status: " << statusMesssage << std::endl;
            return false; // Failed to start listener
        }
        // check if data is comming in
        if (m_recivedPointMessages > 0 || m_recivedImuMessages > 0)
        {

            std::cout << "SickClient: Listener started successfully" << std::endl;
            return true;
        }
        std::cerr << "SickClient: No data received after starting listener, API status: " << statusMesssage << std::endl;
        return false;

    }

    void SickClient::stopListener()
    {
        std::lock_guard<std::mutex> lock(m_instancesMutex);
        m_instance = nullptr; // Clear the static instance pointer
        isDone.store(true);
    }


    void SickClient::imuCallback( SickScanApiHandle apiHandle,  const SickScanImuMsg* msg)
    {

        const uint64_t ts = uint64_t(msg->header.timestamp_sec) * 1e9 + msg->header.timestamp_nsec;
        // get instance from apiHandle
        SickClient* instance = GetInstanceFromHandle(apiHandle);
        if (instance) {
            instance->m_recivedImuMessages.fetch_add(1);
            // Here you can also add the IMU data to the buffer if needed
            std::lock_guard<std::mutex> lock(instance->m_bufferMutex);
            if (instance->m_bufferIMUPtr) {
                LidarIMU imuData;
                imuData.timestamp = ts;
                imuData.gyro_x = msg->angular_velocity.x;
                imuData.gyro_y = msg->angular_velocity.y;
                imuData.gyro_z = msg->angular_velocity.z;
                imuData.acc_x = msg->linear_acceleration.x / 9.8;
                imuData.acc_y = msg->linear_acceleration.y / 9.8;
                imuData.acc_z = msg->linear_acceleration.z / 9.8;
                imuData.laser_id = 0;
                auto now = std::chrono::system_clock::now();
                auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

                imuData.epoch_time = millis.count();
                instance->m_bufferIMUPtr->emplace_back(imuData);
            }
        } else {
            std::cerr << "SickClient: Instance not found for apiHandle" << std::endl;
        }
    }

    inline float bytesToFloat(const uint8_t* bytes)
    {
        float value;
        std::memcpy(&value, bytes, sizeof(float));
        return value;
    }
    inline uint32_t bytesToUInt32(const uint8_t* bytes)
    {
        uint32_t value;
        std::memcpy(&value, bytes, sizeof(uint32_t));
        return value;
    }

    void SickClient::customizedPointCloudMsgCb(SickScanApiHandle apiHandle, const SickScanPointCloudMsg* msg)
    {

        const uint64_t ts = uint64_t(msg->header.timestamp_sec) * 1e9 + msg->header.timestamp_nsec;
        // get instance from apiHandle
        SickClient* instance = GetInstanceFromHandle(apiHandle);
        if (!instance) {
            std::cerr << "SickClient: Instance not found for apiHandle" << std::endl;
            return;
        }
//#define DEV
#ifdef DEV
        for (int i =0; i < msg->fields.size; i++)
        {
            const std::string_view fieldName{ msg->fields.buffer[i].name} ;
            const auto type =  msg->fields.buffer[i].datatype;
            std::cout << "    -  Field: " << fieldName << " " << int(type) << " offset: " << msg->fields.buffer[i].offset << std::endl;
        }
#endif
        // log for debugging
        instance->m_recivedPointMessages ++;
        if (instance->m_bufferLidarPtr)
        {
            int offsetX = -1;
            int offsetY = -1;
            int offsetZ = -1;
            int offsetI = -1;
            int offsetLidarSec = -1;
            int offsetLidarNsec = -1;
            constexpr auto TypeX = SickScanNativeDataType::SICK_SCAN_POINTFIELD_DATATYPE_FLOAT32;
            constexpr auto TypeY = SickScanNativeDataType::SICK_SCAN_POINTFIELD_DATATYPE_FLOAT32;
            constexpr auto TypeZ = SickScanNativeDataType::SICK_SCAN_POINTFIELD_DATATYPE_FLOAT32;
            constexpr auto TypeI = SickScanNativeDataType::SICK_SCAN_POINTFIELD_DATATYPE_FLOAT32;
            constexpr auto TypeLidarSec = SickScanNativeDataType::SICK_SCAN_POINTFIELD_DATATYPE_UINT32;
            constexpr auto TypeLidarNsec = SickScanNativeDataType::SICK_SCAN_POINTFIELD_DATATYPE_UINT32;

            for (int i =0; i < msg->fields.size; i++)
            {
                const std::string_view fieldName{ msg->fields.buffer[i].name} ;
                const auto type =  msg->fields.buffer[i].datatype;

                if (fieldName == "x" && type == TypeX)
                {
                    offsetX = msg->fields.buffer[i].offset;
                }
                else if (fieldName == "y"  && type == TypeY)
                {
                    offsetY = msg->fields.buffer[i].offset;
                }
                else if (fieldName == "z"  && type == TypeZ)
                {
                    offsetZ = msg->fields.buffer[i].offset;
                }
                else if (fieldName == "i" && type == TypeI)
                {
                    offsetI = msg->fields.buffer[i].offset;
                }
                else if (fieldName == "lidar_sec" && type == TypeLidarSec)
                {
                    offsetLidarSec = msg->fields.buffer[i].offset;
                }
                else if (fieldName == "lidar_nsec" && type == TypeLidarNsec)
                {
                    offsetLidarNsec = msg->fields.buffer[i].offset;
                }
            }
            assert(offsetX >= 0);
            assert(offsetY >= 0);
            assert(offsetZ >= 0);
            assert(offsetI >= 0);
            assert(offsetLidarNsec >=0);
            assert(offsetLidarSec >=0);

            for (int rowId  = 0; rowId <  msg->height; rowId++)
            {
                const uint32_t rowOffset =  rowId * msg->row_step;
                for (int colId = 0; colId < msg->width; colId++ )
                {
                    const uint32_t colOffset =  colId * msg->point_step;
                    const uint32_t data_offset = rowOffset + colOffset;
                    assert(data_offset < msg->data.size);
                    const uint8_t* pointPtr = msg->data.buffer + data_offset ;

                    const float x = offsetX>=0 ? bytesToFloat(pointPtr+offsetX) : -1;
                    const float y = offsetY>=0 ? bytesToFloat(pointPtr+offsetY) : -1;
                    const float z = offsetZ>=0 ? bytesToFloat(pointPtr+offsetZ) : -1;
                    const float i  = offsetI>=0 ? bytesToFloat(pointPtr+offsetI) : -1;
                    const uint32_t lidar_sec = offsetLidarSec >= 0 ? bytesToUInt32(pointPtr + offsetLidarSec) : 0;
                    const uint32_t lidar_nsec = offsetLidarNsec >= 0 ? bytesToUInt32(pointPtr + offsetLidarNsec) : 0;

                    LidarPoint point;
                    point.x = x;
                    point.y = y;
                    point.z = z;
                    point.intensity = i;
                    point.timestamp = uint64_t(lidar_sec) * 1e9 + lidar_nsec; // Convert to nanoseconds
                    point.laser_id = 0;

                    std::lock_guard<std::mutex> lock(instance->m_bufferMutex);
                    instance->m_bufferLidarPtr->emplace_back(point);
                }
            }
        }

    }

    void SickClient::startLog()
    {
        std::cout << "SickClient: startLog called" << std::endl;
        // Initialize buffers
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_bufferLidarPtr = std::make_shared<LidarPointsBuffer>();
        m_bufferIMUPtr = std::make_shared<LidarIMUBuffer>();
        // Simulate starting log
        std::cout << "SickClient: Logging started" << std::endl;
    }

    void SickClient::stopLog()
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        // Stop the data thread
        m_bufferLidarPtr.reset();
        m_bufferIMUPtr.reset();
        std::cout << "SickLidar: stopLog called" << std::endl;
    }

    std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> SickClient::retrieveData()
    {
        std::cout << "SickLidar: retrieveData called" << std::endl;
        // Return empty buffers for now

        std::lock_guard<std::mutex> lock(m_bufferMutex);

        LidarPointsBufferPtr returnPointerLidar{ std::make_shared<LidarPointsBuffer>() };
        LidarIMUBufferPtr returnPointerImu{ std::make_shared<LidarIMUBuffer>() };
        std::swap(m_bufferIMUPtr, returnPointerImu);
        std::swap(m_bufferLidarPtr, returnPointerLidar);
        return std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr>(returnPointerLidar, returnPointerImu);
    }

    void SickClient::DataThreadFunction()
    {
        std::cout << "SickLidar: DataThreadFunction started" << std::endl;
        while (!isDone)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            int status =0;
            char message[256];

            SickScanApiGetStatus(m_apiHandle, &status, message, 256); // This will keep the API alive and process incoming messages
            m_apiStatusMessage = std::string(message);
            m_apiStatus = status;
            std::cout << "SickLidar: DataThreadFunction status: " << status << " message: " << m_apiStatusMessage << std::endl;

        }
        std::cout << "SickLidar: DataThreadFunction about to ended" << std::endl;
        // Cleanup Sick SDK API handle
        SickScanApiDeregisterCartesianPointCloudMsg(m_apiHandle, &customizedPointCloudMsgCb);
        SickScanApiDeregisterImuMsg(m_apiHandle, &imuCallback );
        SickScanApiClose(m_apiHandle);
        SickScanApiRelease(m_apiHandle);
        std::cout << "SickLidar: DataThreadFunction ended" << std::endl;
    }
} // namespace mandeye
