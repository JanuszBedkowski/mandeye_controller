#include "LibCameraWrapper.h"

#include <thread>
using namespace libcamera;
using namespace std::chrono_literals;
#include <algorithm>
#include <set>
#include <sys/mman.h>

namespace mandeye
{
constexpr std::string_view controlToString(uint32_t id)
{
	using namespace libcamera::controls;
	switch(id)
	{
	case AE_ENABLE:
		return "AE_ENABLE";
	case AE_STATE:
		return "AE_STATE";
	case AE_METERING_MODE:
		return "AE_METERING_MODE";
	case AE_CONSTRAINT_MODE:
		return "AE_CONSTRAINT_MODE";
	case AE_EXPOSURE_MODE:
		return "AE_EXPOSURE_MODE";
	case EXPOSURE_VALUE:
		return "EXPOSURE_VALUE";
	case EXPOSURE_TIME:
		return "EXPOSURE_TIME";
	case EXPOSURE_TIME_MODE:
		return "EXPOSURE_TIME_MODE";
	case ANALOGUE_GAIN:
		return "ANALOGUE_GAIN";
	case ANALOGUE_GAIN_MODE:
		return "ANALOGUE_GAIN_MODE";
	case AE_FLICKER_MODE:
		return "AE_FLICKER_MODE";
	case AE_FLICKER_PERIOD:
		return "AE_FLICKER_PERIOD";
	case AE_FLICKER_DETECTED:
		return "AE_FLICKER_DETECTED";
	case BRIGHTNESS:
		return "BRIGHTNESS";
	case CONTRAST:
		return "CONTRAST";
	case LUX:
		return "LUX";
	case AWB_ENABLE:
		return "AWB_ENABLE";
	case AWB_MODE:
		return "AWB_MODE";
	case AWB_LOCKED:
		return "AWB_LOCKED";
	case COLOUR_GAINS:
		return "COLOUR_GAINS";
	case COLOUR_TEMPERATURE:
		return "COLOUR_TEMPERATURE";
	case SATURATION:
		return "SATURATION";
	case SENSOR_BLACK_LEVELS:
		return "SENSOR_BLACK_LEVELS";
	case SHARPNESS:
		return "SHARPNESS";
	case FOCUS_FO_M:
		return "FOCUS_FO_M";
	case COLOUR_CORRECTION_MATRIX:
		return "COLOUR_CORRECTION_MATRIX";
	case SCALER_CROP:
		return "SCALER_CROP";
	case DIGITAL_GAIN:
		return "DIGITAL_GAIN";
	case FRAME_DURATION:
		return "FRAME_DURATION";
	case FRAME_DURATION_LIMITS:
		return "FRAME_DURATION_LIMITS";
	case SENSOR_TEMPERATURE:
		return "SENSOR_TEMPERATURE";
	case SENSOR_TIMESTAMP:
		return "SENSOR_TIMESTAMP";
	case AF_MODE:
		return "AF_MODE";
	case AF_RANGE:
		return "AF_RANGE";
	case AF_SPEED:
		return "AF_SPEED";
	case AF_METERING:
		return "AF_METERING";
	case AF_WINDOWS:
		return "AF_WINDOWS";
	case AF_TRIGGER:
		return "AF_TRIGGER";
	case AF_PAUSE:
		return "AF_PAUSE";
	case LENS_POSITION:
		return "LENS_POSITION";
	case AF_STATE:
		return "AF_STATE";
	case AF_PAUSE_STATE:
		return "AF_PAUSE_STATE";
	case HDR_MODE:
		return "HDR_MODE";
	case HDR_CHANNEL:
		return "HDR_CHANNEL";
	case GAMMA:
		return "GAMMA";
	case DEBUG_METADATA_ENABLE:
		return "DEBUG_METADATA_ENABLE";
	case FRAME_WALL_CLOCK:
		return "FRAME_WALL_CLOCK";
	default:
		return "";
	}
}

template <typename T>
bool checkIfInRange(const libcamera::ControlInfo& control, const T& value)
{
	T min = control.min().get<T>();
	T max = control.max().get<T>();
	if(value < min || value > max)
	{
		std::cerr << "Value " << value << " out of range [" << min << ", " << max << "]" << std::endl;
		return false;
	}
	return true;
}
template <typename T>
bool LibCameraWrapper::setControlNumeric(const std::string& name, T valueInput)
{
	for(auto const& control : m_controlsInfo)
	{
		if(control.first->name() == name)
		{
			const int id = control.first->id();
			const int type = control.first->type();

			if(control.first->isArray())
			{
				std::cerr << "Control " << name << " is an array, skipping" << std::endl;
				return false;
			}

			if(type == libcamera::ControlTypeNone)
			{
				std::cerr << "Control " << name << " type " << type << " not supported" << std::endl;
				return false;
			}

			if(type == libcamera::ControlTypeBool)
			{
				auto value = static_cast<bool>(valueInput);
				m_controlList.set(id, value);
			}
			else if(type == libcamera::ControlTypeByte)
			{
				const auto value = static_cast<uint8_t>(valueInput);
				if(!checkIfInRange<uint8_t>(control.second, value))
					return false;
				m_controlList.set(id, value);
			}
			else if(type == libcamera::ControlTypeUnsigned16)
			{
				const auto value = static_cast<uint16_t>(valueInput);
				if(!checkIfInRange<uint16_t>(control.second, value))
					return false;
				m_controlList.set(id, value);
			}
			else if(type == libcamera::ControlTypeInteger32)
			{
				const auto value = static_cast<int32_t>(valueInput);
				if(!checkIfInRange<int32_t>(control.second, value))
					return false;
				m_controlList.set(id, value);
			}
			else if(type == libcamera::ControlTypeInteger64)
			{
				const auto value = static_cast<int64_t>(valueInput);
				if(!checkIfInRange<int64_t>(control.second, value))
					return false;
				m_controlList.set(id, value);
			}
			else if(type == libcamera::ControlTypeFloat)
			{
				const auto value = static_cast<float>(valueInput);
				if(!checkIfInRange<float>(control.second, value))
					return false;
				m_controlList.set(id, value);
			}
			else if(type == libcamera::ControlTypeString)
			{
				std::string value = std::to_string(valueInput);
				m_controlList.set(id, ControlValue(value));
			}
			else if(type == libcamera::ControlTypeRectangle)
			{
				std::cerr << "Control " << name << " is a Rectangle; pass a [x,y,w,h] array" << std::endl;
				return false;
			}
			else
			{
				std::cerr << "Control " << name << " has unhandled type " << type << std::endl;
				return false;
			}
			return true;
		}
	}
	std::cerr << "Control " << name << " not found" << std::endl;
	return false;
}

// implementations
template bool LibCameraWrapper::setControlNumeric<bool>(const std::string& name, bool value);
template bool LibCameraWrapper::setControlNumeric<int64_t>(const std::string& name, int64_t value);
template bool LibCameraWrapper::setControlNumeric<float>(const std::string& name, float value);

// Array / rectangle controls: libcamera exposes these as Span<>/Rectangle, which
// setControlNumeric() cannot handle. Accepts a JSON array from the config "picamera" block.
bool LibCameraWrapper::setControlArray(const std::string& name, const nlohmann::json& arr)
{
	if(!arr.is_array())
	{
		return false;
	}
	try
	{
		if(name == "ColourGains" && arr.size() == 2)
		{
			m_controlList.set(libcamera::controls::ColourGains, {arr.at(0).get<float>(), arr.at(1).get<float>()});
			return true;
		}
		if(name == "FrameDurationLimits" && arr.size() == 2)
		{
			m_controlList.set(libcamera::controls::FrameDurationLimits,
							  {arr.at(0).get<int64_t>(), arr.at(1).get<int64_t>()});
			return true;
		}
		if(name == "ScalerCrop" && arr.size() == 4)
		{
			m_controlList.set(libcamera::controls::ScalerCrop,
							  libcamera::Rectangle(arr.at(0).get<int>(), arr.at(1).get<int>(),
												   arr.at(2).get<unsigned int>(), arr.at(3).get<unsigned int>()));
			return true;
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << "setControlArray(" << name << ") failed: " << e.what() << std::endl;
		return false;
	}
	std::cerr << "Array control " << name << " not supported (expected ColourGains[2], FrameDurationLimits[2], ScalerCrop[4])"
			  << std::endl;
	return false;
}

std::vector<libcamera::Span<uint8_t>> LibCameraWrapper::Mmap(libcamera::FrameBuffer* buffer)
{
	auto item = m_mapped_buffers.find(buffer);
	if(item == m_mapped_buffers.end())
		return {};
	return item->second;
}

std::vector<std::unique_ptr<Request>> LibCameraWrapper::CreateRequests()
{
	std::vector<std::unique_ptr<Request>> requests;
	const std::vector<std::unique_ptr<libcamera::FrameBuffer>>& buffers = m_allocator->buffers(m_stream);
	for(unsigned int i = 0; i < buffers.size(); ++i)
	{
		std::unique_ptr<libcamera::Request> request = m_camera->createRequest();
		if(!request)
		{
			std::cerr << "Can't create request" << std::endl;
			return {};
		}

		const std::unique_ptr<libcamera::FrameBuffer>& buffer = buffers[i];
		int ret = request->addBuffer(m_stream, buffer.get());
		if(ret < 0)
		{
			std::cerr << "Can't set buffer for request" << std::endl;
			return {};
		}

		requests.push_back(std::move(request));
	}
	return requests;
}

void LibCameraWrapper::AdjustSystemClock()
{
	timespec ts_real, ts_mono;
	clock_gettime(CLOCK_REALTIME, &ts_real);
	clock_gettime(CLOCK_MONOTONIC, &ts_mono);
	// Convert to nanoseconds
	uint64_t realtime_ns = uint64_t(ts_real.tv_sec) * 1'000'000'000ULL + ts_real.tv_nsec;
	uint64_t monotonic_ns = uint64_t(ts_mono.tv_sec) * 1'000'000'000ULL + ts_mono.tv_nsec;
	m_monoOffset = (realtime_ns > monotonic_ns) ? realtime_ns - monotonic_ns : 0;
}

std::vector<std::string> LibCameraWrapper::enumerateCameraIds()
{
	std::vector<std::string> ids;
	CameraManager cm;
	if(int ret = cm.start(); ret != 0)
	{
		std::cerr << "enumerateCameraIds: CameraManager::start() failed (" << ret << ")" << std::endl;
		return ids;
	}
	for(auto const& camera : cm.cameras())
		ids.push_back(camera->id());
	cm.stop();
	return ids;
}

bool LibCameraWrapper::start(int camNo, nlohmann::json config, StreamRole role)
{
	// Start from a clean slate: the control list is long-lived, so anything not set by
	// *this* config (a pinned ExposureTime, a leftover AeEnable, ScalerCrop, ...) would
	// otherwise leak in from a previous /setConfig and could not be cleared without a
	// process restart. Each start() must reflect exactly the config it was handed.
	m_controlList.clear();

	AdjustSystemClock();
	m_cm = std::make_unique<CameraManager>();
	m_cm->start();
	for(auto const& camera : m_cm->cameras())
		std::cout << camera->id() << std::endl;
	auto cameras = m_cm->cameras();
	if(cameras.empty())
	{
		std::cout << "No cameras were identified on the system." << std::endl;
		m_cm->stop();
		return false;
	}

	for(auto const& camera : cameras)
	{
		std::cout << camera->id() << std::endl;
	}
	if(camNo < 0 || static_cast<size_t>(camNo) >= cameras.size())
	{
		std::cout << "Camera number " << camNo << " is out of range." << std::endl;
		m_cm->stop();
		return false;
	}

	std::string cameraId = cameras[camNo]->id();
	std::cout << "Using camera " << cameraId << std::endl;
	m_camera = m_cm->get(cameraId);
	std::cout << "Camera is " << m_camera->id() << std::endl;
	m_camera->acquire();

	// Capture the native sensor readout modes once, from a Raw-role configuration.
	m_sensorModes.clear();
	if(auto rawCfg = m_camera->generateConfiguration({StreamRole::Raw}); rawCfg && !rawCfg->empty())
	{
		const auto& rawStream = rawCfg->at(0);
		for(const auto& s : rawStream.formats().sizes(rawStream.pixelFormat))
		{
			m_sensorModes.push_back(s);
		}
		std::cout << "Sensor modes: ";
		for(const auto& s : m_sensorModes)
			std::cout << s.width << "x" << s.height << " ";
		std::cout << std::endl;
	}

	m_config = m_camera->generateConfiguration({role});

	StreamConfiguration& streamConfig = m_config->at(0);

	streamConfig.pixelFormat = libcamera::formats::RGB888;

	// Resolution: use the config json ("width"/"height") when given, otherwise fall back to
	// the largest native sensor mode - the Viewfinder role we generate from defaults to a
	// small preview size, which we never want here. validate() below snaps the chosen size
	// to something the pipeline can actually deliver.
	libcamera::Size requestedSize = streamConfig.size;
	const bool haveWH = config.contains("width") && config.contains("height") &&
						config["width"].get<int>() > 0 && config["height"].get<int>() > 0;
	if(haveWH)
	{
		requestedSize = libcamera::Size(config["width"].get<unsigned int>(), config["height"].get<unsigned int>());
	}
	else if(!m_sensorModes.empty())
	{
		requestedSize = *std::max_element(m_sensorModes.begin(), m_sensorModes.end(),
										  [](const libcamera::Size& a, const libcamera::Size& b) {
											  return static_cast<uint64_t>(a.width) * a.height <
													 static_cast<uint64_t>(b.width) * b.height;
										  });
		std::cout << "No width/height in config - defaulting to largest sensor mode " << requestedSize.toString()
				  << std::endl;
	}
	streamConfig.size = requestedSize;

	// Note: an explicit SensorConfiguration (binning factor) does nothing on rpi/pisp -
	// the pipeline picks the sensor mode from the requested width/height alone. So the
	// readout mode is controlled purely by the resolution: e.g. on the IMX519, 2328x1748
	// is the 2x2-binned full-array mode and 4656x3496 is the full-resolution mode.

	const libcamera::CameraConfiguration::Status vst = m_config->validate();
	const char* vstStr = (vst == libcamera::CameraConfiguration::Valid)      ? "Valid"
						 : (vst == libcamera::CameraConfiguration::Adjusted) ? "Adjusted"
																			: "Invalid";
	std::cout << "Resolution: requested " << requestedSize.toString() << " -> validated " << streamConfig.size.toString()
			  << " (" << vstStr << ", " << streamConfig.pixelFormat.toString() << ", stride " << streamConfig.stride << ")"
			  << std::endl;
	std::cout << "Validated configuration is: " << streamConfig.toString() << std::endl;
	m_camera->configure(m_config.get());

	m_allocator = std::make_unique<libcamera::FrameBufferAllocator>(m_camera);

	for(libcamera::StreamConfiguration& cfg : *m_config)
	{
		int ret = m_allocator->allocate(cfg.stream());
		if(ret < 0)
		{
			std::cerr << "Can't allocate buffers" << std::endl;
		}

		for(const std::unique_ptr<libcamera::FrameBuffer>& buffer : m_allocator->buffers(cfg.stream()))
		{
			// "Single plane" buffers appear as multi-plane here, but we can spot them because then
			// planes all share the same fd. We accumulate them so as to mmap the buffer only once.
			size_t buffer_size = 0;
			for(unsigned i = 0; i < buffer->planes().size(); i++)
			{
				const libcamera::FrameBuffer::Plane& plane = buffer->planes()[i];
				buffer_size += plane.length;
				if(i == buffer->planes().size() - 1 || plane.fd.get() != buffer->planes()[i + 1].fd.get())
				{
					void* memory = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, plane.fd.get(), 0);
					m_mapped_buffers[buffer.get()].push_back(libcamera::Span<uint8_t>(static_cast<uint8_t*>(memory), buffer_size));
					buffer_size = 0;
				}
			}
		}
	}

	m_stream = streamConfig.stream();
	requests = CreateRequests();

	m_camera->requestCompleted.connect(this, &LibCameraWrapper::requestComplete);

	m_controlsInfo = m_camera->controls();
	auto configDump = getCameraConfig();

	// apply config from json

	if(config.contains("picamera"))
	{
		const auto picamera = config["picamera"];
		for(const auto& [key, value] : picamera.items())
		{
			if(key.find("_") == 0 || value.is_null())
			{
				continue;
			}
			try
			{
				if(value.is_null())
				{
					std::cout << "Skipping " << key << " - unsupported set to null" << std::endl;
				}
				if(value.is_boolean())
				{
					setControlNumeric(key, value.get<bool>());
				}
				else if(value.is_number_float())
				{
					setControlNumeric(key, value.get<float>());
				}
				else if(value.is_number_integer())
				{
					setControlNumeric(key, value.get<int32_t>());
				}
				else if(value.is_array())
				{
					setControlArray(key, value);
				}
				else
				{
					std::cout << "Skipping " << key << " - unsupported type" << std::endl;
				}
			}
			catch(const std::exception& e)
			{
				std::cerr << "Failed to set " << key << ": " << e.what() << std::endl;
			}
		}
	}

	// NoiseReductionMode: if the loaded config did not set it, apply Fast as a real default
	// (previously getCameraConfig() faked this in the dump, which never took effect).
	if(!m_controlList.contains(controls::draft::NOISE_REDUCTION_MODE))
	{
		setControlNumeric("NoiseReductionMode", static_cast<int64_t>(controls::draft::NoiseReductionModeFast));
	}

	if(config.contains("rateMs"))
	{
		m_rateMs = config["rateMs"].get<uint32_t>();
		std::cout << "Setting rate to " << m_rateMs << std::endl;
	}
	m_running.store(true);
	m_camera->start(&m_controlList);
	return true;
}

void LibCameraWrapper::stop()
{
	// Stop delivering frames first: requestComplete() bails on !m_running, so once this is
	// set no callback touches m_mapped_buffers / m_config / the camera again. The sleep lets
	// an in-progress callback (a few ms) finish before we tear anything down.
	m_running.store(false);
	std::cout << "Stopping camera" << std::endl;
	std::this_thread::sleep_for(100ms);

	if(m_camera)
	{
		m_camera->stop();

		// unmap the buffers mapped in start() before the allocator frees the underlying fds
		for(auto& kv : m_mapped_buffers)
		{
			for(libcamera::Span<uint8_t>& s : kv.second)
			{
				if(s.data())
					munmap(s.data(), s.size());
			}
		}
		m_mapped_buffers.clear();

		if(m_allocator && m_stream)
			m_allocator->free(m_stream); // free BEFORE clearing m_stream
		m_stream = nullptr;

		requests.clear();
		m_camera->release();
		m_camera.reset();
		m_allocator.reset();
	}
	if(m_cm)
	{
		m_cm->stop();
		m_cm.reset();
	}
}

void LibCameraWrapper::requestComplete(Request* request)
{
	// m_monoOffset is sampled once in start(); re-sampling here made the delivered
	// wall-clock timestamp jitter with NTP slew.
	if(request->status() == Request::RequestCancelled)
		return;
	// stop() has begun tearing down - do not touch buffers / config / requeue.
	if(!m_running.load())
		return;

	const auto now = std::chrono::steady_clock::now();
	const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_FrameStart);
	if(duration.count() > m_rateMs)
	{
		m_FrameStart = std::chrono::steady_clock::now();
		nlohmann::json metadataDump;
		for(auto f : request->metadata())
		{
			const auto id = f.first;
			std::string name;
			if(name = controlToString(id); name.empty())
			{
				name = std::to_string(id);
			}
			// libcamera reports these in microseconds; expose them in milliseconds
			if(id == libcamera::controls::EXPOSURE_TIME || id == libcamera::controls::FRAME_DURATION)
			{
				if(f.second.type() == libcamera::ControlTypeInteger32)
				{
					metadataDump[name] = f.second.get<int32_t>() / 1000.0;
					continue;
				}
				if(f.second.type() == libcamera::ControlTypeInteger64)
				{
					metadataDump[name] = f.second.get<int64_t>() / 1000.0;
					continue;
				}
			}
			metadataDump[name] = f.second.toString();
		}

		if(m_config->at(0).pixelFormat == libcamera::formats::MJPEG)
		{
			const libcamera::Request::BufferMap& buffers = request->buffers();
			for(auto bufferPair : buffers)
			{
				libcamera::FrameBuffer* buffer = bufferPair.second;
				libcamera::StreamConfiguration& streamConfig = m_config->at(0);
				m_requestTimestamp = buffer->metadata().timestamp + m_monoOffset;
				auto mem = Mmap(buffer);
				std::vector<uchar> memv(mem[0].begin(), mem[0].end());
				cv::Mat img = cv::imdecode(memv, cv::IMREAD_COLOR);

				if(m_callback)
				{
					m_callback(img, m_requestTimestamp, metadataDump);
				};
			}
		}
		else if(m_config->at(0).pixelFormat == libcamera::formats::RGB888)
		{
			const libcamera::Request::BufferMap& buffers = request->buffers();
			for(auto bufferPair : buffers)
			{
				libcamera::FrameBuffer* buffer = bufferPair.second;
				m_requestTimestamp = buffer->metadata().timestamp + m_monoOffset;
				libcamera::StreamConfiguration& streamConfig = m_config->at(0);
				unsigned int vw = streamConfig.size.width;
				unsigned int vh = streamConfig.size.height;
				unsigned int vstr = streamConfig.stride;
				auto mem = Mmap(buffer);
				cv::Mat img(vh, vw, CV_8UC3);
				uint ls = vw * 3;
				uint8_t* ptr = mem[0].data();
				for(unsigned int i = 0; i < vh; i++, ptr += vstr)
				{
					memcpy(img.ptr(i), ptr, ls);
				}
				if(m_callback)
				{
					m_callback(img, m_requestTimestamp, metadataDump);
				}
			}
		}
		else
		{
			std::cout << "Unsupported pixel format" << std::endl;
		}
	}
	request->reuse(libcamera::Request::ReuseBuffers);

	if(!m_oneFrame)
	{
		m_camera->queueRequest(request);
	}
}

void LibCameraWrapper::capture(bool oneFrame)
{
	m_FrameStart = std::chrono::steady_clock::now();
	m_oneFrame = oneFrame;
	for(std::unique_ptr<Request>& request : requests)
	{
		m_camera->queueRequest(request.get());
	}
}

nlohmann::json reportValue(const libcamera::ControlValue& value, const libcamera::ControlType type)
{
	if(value.isNone())
		return nlohmann::json();
	switch(type)
	{
	case libcamera::ControlTypeBool:
		return nlohmann::json(value.get<bool>());
	case libcamera::ControlTypeByte:
		return nlohmann::json(value.get<uint8_t>());
	case libcamera::ControlTypeUnsigned16:
		return nlohmann::json(value.get<uint16_t>());
	case libcamera::ControlTypeUnsigned32:
		return nlohmann::json(value.get<uint32_t>());
	case libcamera::ControlTypeInteger32:
		return nlohmann::json(value.get<int32_t>());
	case libcamera::ControlTypeInteger64:
		return nlohmann::json(value.get<int64_t>());
	case libcamera::ControlTypeFloat:
		return nlohmann::json(value.get<float>());
	case libcamera::ControlTypeString:
		return nlohmann::json(value.get<std::string>());
	case libcamera::ControlTypeRectangle:
	{
		const libcamera::Rectangle r = value.get<libcamera::Rectangle>();
		return nlohmann::json::array({r.x, r.y, r.width, r.height});
	}
	default:
		return nlohmann::json();
	}
}

// Report an array (isArray) ControlValue as a JSON array of its element type.
nlohmann::json reportArrayValue(const libcamera::ControlValue& value, const libcamera::ControlType type)
{
	nlohmann::json arr = nlohmann::json::array();
	if(value.isNone() || !value.isArray())
		return arr;
	switch(type)
	{
	case libcamera::ControlTypeByte:
		for(uint8_t x : value.get<libcamera::Span<const uint8_t>>())
			arr.push_back(x);
		break;
	case libcamera::ControlTypeInteger32:
		for(int32_t x : value.get<libcamera::Span<const int32_t>>())
			arr.push_back(x);
		break;
	case libcamera::ControlTypeInteger64:
		for(int64_t x : value.get<libcamera::Span<const int64_t>>())
			arr.push_back(x);
		break;
	case libcamera::ControlTypeFloat:
		for(float x : value.get<libcamera::Span<const float>>())
			arr.push_back(x);
		break;
	default:
		break;
	}
	return arr;
}

nlohmann::json LibCameraWrapper::getCameraConfig()
{
	nlohmann::json config;
	if(!m_camera)
		return config;
	config["id"] = m_camera->id();
	config["controls_info"] = {};
	config["rateMs"] = m_rateMs;
	config["picamera"]["_Note1"] = "Here User can adjust setting of their camera!";

	// --- resolution: current size + the discrete list of selectable modes ---
	if(m_config && !m_config->empty())
	{
		const auto& sc = m_config->at(0);
		config["width"] = sc.size.width;
		config["height"] = sc.size.height;

		std::set<std::pair<unsigned int, unsigned int>> sizes;
		for(const auto& s : sc.formats().sizes(sc.pixelFormat))
		{
			sizes.insert({s.width, s.height});
		}
		for(const auto& s : m_sensorModes)
		{
			sizes.insert({s.width, s.height});
		}
		sizes.insert({sc.size.width, sc.size.height}); // always include the active one
		for(const auto& [w, h] : sizes)
		{
			std::stringstream ss;
			ss << w << " " << h;
			config["controls_info"]["resolutions"].push_back(ss.str());
		}
	}
	for(auto const& control : m_controlsInfo)
	{
		const unsigned int id = control.first->id();
		auto name = control.first->name();
		if(name.empty())
		{
			continue;
		}

		const bool isArray = control.first->isArray();
		config["controls_info"][name]["name"] = control.first->name();
		config["controls_info"][name]["id"] = id;
		config["controls_info"]["vendor"] = control.first->vendor();
		config["controls_info"][name]["isArray"] = isArray;
		config["controls_info"][name]["isInput"] = control.first->isInput();
		config["controls_info"][name]["isOutput"] = control.first->isOutput();
		config["controls_info"][name]["type"] = control.first->type();

		const auto type = control.first->type();
		if(LibCameraControlTypeToString.find(type) != LibCameraControlTypeToString.end())
		{
			config["controls_info"][name]["type_str"] = LibCameraControlTypeToString.at(type);
		}
		else
		{
			config["controls_info"][name]["type_str"] = "Unknown";
		}

		const auto& defValue = control.second.def();
		const auto& minValue = control.second.min();
		const auto& maxValue = control.second.max();

		config["controls_info"][name]["min"] = minValue.toString();
		config["controls_info"][name]["max"] = maxValue.toString();
		config["controls_info"][name]["def"] = defValue.toString();

		const bool applied = m_controlList.contains(id);
		const libcamera::ControlValue& reported = applied ? m_controlList.get(id) : defValue;
		if(isArray)
		{
			// arrays/rectangles: only report a value that was actually applied - the default
			// is usually "full frame" and echoing it would force it back on every apply.
			config["picamera"][name] = applied ? reportArrayValue(reported, type) : nlohmann::json();
		}
		else if(type == libcamera::ControlTypeRectangle)
		{
			config["picamera"][name] = applied ? reportValue(reported, type) : nlohmann::json();
		}
		else
		{
			config["picamera"][name] = reportValue(reported, type);
		}
	}

	return config;
}

} // namespace mandeye
