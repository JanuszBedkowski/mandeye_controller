#include "optris.h"
#include <exception>
#include <iostream>
#include <thread>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

// optris
#include "IRDevice.h"
#include "IRImager.h"
#include "IRLogger.h"
#include "ImageBuilder.h"

namespace mandeye
{

void write_png(const char* filename, int width, int height, const unsigned char* data)
{
	int channels = 3; // Assuming RGBA format
	int result = stbi_write_png(filename, width, height, channels, data, width * channels);
	if(!result)
	{
		std::cerr << "Error writing PNG file: " << filename << std::endl;
	}
}

class OptisClientCallbacks{
public:
	static void onThermalFrame(unsigned short* thermal, unsigned int w, unsigned int h, evo::IRFrameMetadata meta, void* arg)
	{
		OptisClient * this_ptr = (OptisClient*)arg;
		assert(this_ptr);
		this_ptr->m_cameraImages ++;
		if (!this_ptr->isLogging())
		{
			return ;
		}

		unsigned char builderData[w * h];
		evo::ImageBuilder _iBuilder;

		_iBuilder.setPaletteScalingMethod(evo::eMinMax);

		_iBuilder.setData(w, h, thermal);
		unsigned char bufferThermal[w * h * 3];
		unsigned char imageData[w * h * 4];
		_iBuilder.convertTemperatureToPaletteImage(bufferThermal);
		_iBuilder.convertTemperatureToPaletteImage(imageData, true);


		const double timeDiff = this_ptr->GetTimeStamp() - this_ptr->m_cameraTimingSecLast;
		if (timeDiff < this_ptr->m_cameraTimingSec)
		{
			return;
		}
		this_ptr->m_cameraTimingSecLast = this_ptr->GetTimeStamp();

		char filename[1024];
		snprintf(filename, 1024, "%s/thermal_%f.png", this_ptr->m_logDirectory.c_str(), this_ptr->GetTimeStamp());
		write_png(filename, w, h, imageData);

		std::cout << "Serialized file to " << filename << std::endl;

//		std::cout << meta.counterHW << std::endl;
//		std::cout << meta.tempBox << " " << meta.tempChip << " " << meta.tempFlag << std::endl;
	}

};

nlohmann::json OptisClient::produceStatus()
{
	nlohmann::json data;
	data["name"] = "Optris";
	data["images"] = m_cameraImages;

	return data;
}

void OptisClient::cameraLoop()
{
	evo::IRLogger::setVerbosity(evo::IRLOG_ERROR, evo::IRLOG_OFF);
	evo::IRDeviceParams params;
	evo::IRDeviceParamsReader::readXML(m_cameraCalibrationFile.c_str(), params);
	m_dev = nullptr;
	m_dev = evo::IRDevice::IRCreateDevice(params);
	if(m_dev)
	{
		evo::IRImager imager;
		if(imager.init(&params, m_dev->getFrequency(), m_dev->getWidth(), m_dev->getHeight(), m_dev->controlledViaHID()))
		{
			if(imager.getWidth() != 0 && imager.getHeight() != 0)
			{
				std::cout << "Thermal channel: " << imager.getWidth() << "x" << imager.getHeight() << "@" << imager.getMaxFramerate() << "Hz"
						  << std::endl;

				// Start UVC streaming
				if(m_dev->startStreaming() == 0)
				{
					// Enter loop in order to pass raw data to Optris image processing library.
					// Processed data are supported by the frame callback function.

					unsigned char bufferRaw[m_dev->getRawBufferSize()];
					evo::RawdataHeader header;
					imager.initRawdataHeader(header);

					imager.setThermalFrameCallback(&OptisClientCallbacks::onThermalFrame);

					while(m_running)
					{
						double timestamp = 0;
						if(m_dev->getFrame(bufferRaw, &timestamp) == evo::IRIMAGER_SUCCESS)
						{
							imager.process(bufferRaw, (void*)this);
						}
					}
				}
				else
				{
					std::cout << "Error occurred in starting stream ... aborting. You may need to reconnect the camera." << std::endl;
				}
			}
		}
	}
}

bool OptisClient::startListener(const std::string& cameraCalibration)
{

	m_cameraCalibrationFile = cameraCalibration;
	m_running = true;
	cameraThread = std::thread(&OptisClient::cameraLoop, this);
	return true;
}

void OptisClient::stopListener()
{
	m_running = false;
	cameraThread.join();
}
void OptisClient::startLog(const std::string& directory)
{
	m_isLogging = true;
	m_logDirectory = directory;
}

void OptisClient::stopLog()
{
	m_isLogging = false;
}


} // namespace mandeye