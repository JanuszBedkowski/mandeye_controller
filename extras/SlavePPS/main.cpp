#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <minmea.h>
#include <optional>
#include <string>
#include <sys/time.h>
#include <sys/timepps.h>
#include <termios.h>
#include <unistd.h>
#include <unordered_map>

#include "mandeye_utils.h"
#include <gpios.h>
#include <hardware_config/mandeye.h>

static const std::unordered_map<int, speed_t> baud_map = {
	{4800, B4800},
	{9600, B9600},
	{19200, B19200},
	{38400, B38400},
	{57600, B57600},
	{115200, B115200},
	{230400, B230400},
};

static std::optional<speed_t> baudrate_to_constant(int baud)
{
	auto it = baud_map.find(baud);
	if(it != baud_map.end())
		return it->second;
	return std::nullopt;
}

static void setup_uart(int fd, speed_t baud)
{
	struct termios tty
	{ };
	if(tcgetattr(fd, &tty) != 0)
	{
		perror("tcgetattr");
		return;
	}
	cfsetispeed(&tty, baud);
	cfsetospeed(&tty, baud);
	tty.c_cflag |= (CLOCAL | CREAD);
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	tty.c_cflag &= ~PARENB;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;
	tty.c_lflag = 0;
	tty.c_iflag = 0;
	tty.c_oflag = 0;
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 0;
	tcsetattr(fd, TCSANOW, &tty);
}

std::string getEnvString(const std::string& env, const std::string& def)
{
	const char* env_p = std::getenv(env.c_str());
	if(env_p == nullptr)
		return def;
	return std::string{env_p};
}

int main()
{

	const std::string uartPort =
		getEnvString("SLAVE_PPS_UART_PORT", mandeye::GetLidarSyncPorts().empty() ? "/dev/ttyAMA1" : mandeye::GetLidarSyncPorts()[0]);
	const int baudInt = std::stoi(getEnvString("SLAVE_PPS_UART_BAUD_RATE", "9600"));
	const std::string ppsDevice = getEnvString("SLAVE_PPS_DEVICE", "/dev/pps0");

	const auto baud = baudrate_to_constant(baudInt);
	if(!baud)
	{
		std::cerr << "Invalid baud rate: " << baudInt << std::endl;
		return 1;
	}

	int uart = open(uartPort.c_str(), O_RDONLY | O_NOCTTY);
	if(uart < 0)
	{
		perror("UART open");
		return 1;
	}
	setup_uart(uart, *baud);
	tcflush(uart, TCIFLUSH); // discard bytes buffered before we started
	FILE* gps = fdopen(uart, "r");

	int ppsfd = open(ppsDevice.c_str(), O_RDONLY);
	if(ppsfd < 0)
	{
		perror("PPS open");
		return 1;
	}

	pps_handle_t pps;
	time_pps_create(ppsfd, &pps);
	pps_params_t params;
	time_pps_getparams(pps, &params);
	params.mode |= PPS_CAPTUREASSERT;
	time_pps_setparams(pps, &params);

	std::cout << "GPS+PPS sync started" << " uart=" << uartPort << " baud=" << baudInt << " pps=" << ppsDevice << std::endl;

	char buf[256];
	while(true)
	{
		if(!fgets(buf, sizeof(buf), gps))
			continue;

		if(minmea_sentence_id(buf, false) != MINMEA_SENTENCE_RMC)
			continue;

		struct minmea_sentence_rmc rmc
		{ };
		if(!minmea_parse_rmc(&rmc, buf) || !rmc.valid)
			continue;

		// Only sync on whole seconds
		if(rmc.time.microseconds != 0)
			continue;

		std::cout << "RMC: " << buf;

		struct tm tm
		{ };
		tm.tm_hour = rmc.time.hours;
		tm.tm_min = rmc.time.minutes;
		tm.tm_sec = rmc.time.seconds;
		tm.tm_mday = rmc.date.day;
		tm.tm_mon = rmc.date.month - 1;
		tm.tm_year = rmc.date.year + 100;

		const time_t gps_time = timegm(&tm);
		std::cout << "GPS second: " << gps_time << std::endl;

		pps_info_t info;
		if(time_pps_fetch(pps, PPS_TSFMT_TSPEC, &info, nullptr) < 0)
		{
			perror("pps_fetch");
			continue;
		}

		// Set clock to the next whole second (aligned with next PPS pulse)
		timespec ts{};
		ts.tv_sec = gps_time + 1;
		ts.tv_nsec = 0;

		if(clock_settime(CLOCK_REALTIME, &ts) < 0)
			perror("clock_settime");
		else
			std::cout << "Time set to " << ts.tv_sec << std::endl;
	}
}