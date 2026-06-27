#include "save_laz.h"
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <laszip/laszip_api.h>
#include <sys/mman.h>
#include <tracy/Tracy.hpp>
#include <unistd.h>

namespace {
int getEnvInt(const char* name, int def)
{
	const char* v = std::getenv(name);
	if(!v)
		return def;
	return std::stoi(v);
}
} // namespace

nlohmann::json mandeye::LazStats::produceStatus() const
{
	nlohmann::json status;
	status["filename"] = m_filename;
	status["points_count"] = m_pointsCount;
	status["save_duration_sec1"] = m_saveDurationSec1;
	status["save_duration_sec2"] = m_saveDurationSec2;
	status["size_mb"] = m_sizeMb;
	status["decimation_step"] = m_decimationStep;
	return status;
}
std::optional<mandeye::LazStats> mandeye::saveLaz(const std::string& filename, LidarPointsBufferPtr buffer)
{
	ZoneScoped;
	TracyPlot("laz_buffer_points", (int64_t)buffer->size());

	mandeye::LazStats stats;
	stats.m_filename = filename;
	stats.m_pointsCount = buffer->size();
	constexpr float scale = 0.0001f; // one tenth of milimeter

	// heuristically determine the decimation step
	const int lazDecimationThreshold = getEnvInt("MANDEYE_LAZ_DECIMATION_THRESHOLD", 4000000);
	const int lazDecimationTarget = getEnvInt("MANDEYE_LAZ_DECIMATION_TARGET", 2000000);
	int step = 1;
	if((int)buffer->size() > lazDecimationThreshold)
	{
		step = (int)ceil((double)buffer->size() / lazDecimationTarget);
	}
	if(step < 1)
	{
		step = 1;
	}
	stats.m_decimationStep = step;

	// Compute bounds and point count only over the points that will actually be written.
	double max_x{std::numeric_limits<double>::lowest()};
	double max_y{std::numeric_limits<double>::lowest()};
	double max_z{std::numeric_limits<double>::lowest()};
	double min_x{std::numeric_limits<double>::max()};
	double min_y{std::numeric_limits<double>::max()};
	double min_z{std::numeric_limits<double>::max()};
	int num_points = 0;

	{
		ZoneScopedN("find_bounds");
		for(int i = 0; i < (int)buffer->size(); i += step)
		{
			const auto& p = buffer->at(i);
			max_x = std::max(max_x, (double)p.x);
			max_y = std::max(max_y, (double)p.y);
			max_z = std::max(max_z, (double)p.z);
			min_x = std::min(min_x, (double)p.x);
			min_y = std::min(min_y, (double)p.y);
			min_z = std::min(min_z, (double)p.z);
			num_points++;
		}
	}

	std::cout << "processing: " << filename << " points " << buffer->size()
			  << " -> " << num_points << " (step=" << step << ")" << std::endl;

	laszip_POINTER laszip_writer;
	if(laszip_create(&laszip_writer))
	{
		fprintf(stderr, "DLL ERROR: creating laszip writer\n");
		return nullopt;
	}

	laszip_header* header;
	if(laszip_get_header_pointer(laszip_writer, &header))
	{
		fprintf(stderr, "DLL ERROR: getting header pointer from laszip writer\n");
		return nullopt;
	}

	header->file_source_ID = 4711;
	header->global_encoding = (1 << 0);
	header->version_major = 1;
	header->version_minor = 2;
	header->point_data_format = 1;
	header->point_data_record_length = 28;
	header->number_of_point_records = num_points;
	header->number_of_points_by_return[0] = num_points;
	header->number_of_points_by_return[1] = 0;
	header->x_scale_factor = scale;
	header->y_scale_factor = scale;
	header->z_scale_factor = scale;
	header->max_x = max_x;
	header->min_x = min_x;
	header->max_y = max_y;
	header->min_y = min_y;
	header->max_z = max_z;
	header->min_z = min_z;

	// Write compressed LAZ into an anonymous in-memory fd so that all I/O
	// during compression goes to RAM. Afterwards we flush it to disk in a
	// single write() + fsync(), keeping SD-card traffic sequential and bounded.
	int memfd = memfd_create("laz_buffer", 0);
	if(memfd < 0)
	{
		fprintf(stderr, "ERROR: memfd_create failed: %s\n", strerror(errno));
		laszip_destroy(laszip_writer);
		return nullopt;
	}
	char memfd_path[64];
	snprintf(memfd_path, sizeof(memfd_path), "/proc/self/fd/%d", memfd);

	laszip_BOOL compress = (strstr(filename.c_str(), ".laz") != nullptr);
	const auto start = std::chrono::high_resolution_clock::now();

	if(laszip_open_writer(laszip_writer, memfd_path, compress))
	{
		fprintf(stderr, "DLL ERROR: opening laszip writer for '%s'\n", filename.c_str());
		close(memfd);
		laszip_destroy(laszip_writer);
		return nullopt;
	}

	fprintf(stderr, "writing '%s' %scompressed via memfd\n", filename.c_str(), (compress ? "" : "un"));

	laszip_point* point;
	if(laszip_get_point_pointer(laszip_writer, &point))
	{
		fprintf(stderr, "DLL ERROR: getting point pointer from laszip writer\n");
		close(memfd);
		laszip_destroy(laszip_writer);
		return nullopt;
	}

	laszip_I64 p_count = 0;
	laszip_F64 coordinates[3];

	{
		ZoneScopedN("write_points");
		for(int i = 0; i < (int)buffer->size(); i += step)
		{
			const auto& p = buffer->at(i);
			point->intensity = p.intensity;
			point->gps_time = p.timestamp * 1e-9;
			point->classification = p.tag;
			point->user_data = p.laser_id;
			coordinates[0] = p.x;
			coordinates[1] = p.y;
			coordinates[2] = p.z;
			if(laszip_set_coordinates(laszip_writer, coordinates))
			{
				fprintf(stderr, "DLL ERROR: setting coordinates for point %lld\n", (long long)p_count);
				close(memfd);
				laszip_destroy(laszip_writer);
				return nullopt;
			}
			if(laszip_write_point(laszip_writer))
			{
				fprintf(stderr, "DLL ERROR: writing point %lld\n", (long long)p_count);
				close(memfd);
				laszip_destroy(laszip_writer);
				return nullopt;
			}
			p_count++;
		}
	}

	if(laszip_get_point_count(laszip_writer, &p_count))
	{
		fprintf(stderr, "DLL ERROR: getting point count\n");
		close(memfd);
		laszip_destroy(laszip_writer);
		return nullopt;
	}
	fprintf(stderr, "successfully written %lld points\n", (long long)p_count);
	stats.m_pointsCount = p_count;

	if(laszip_close_writer(laszip_writer))
	{
		fprintf(stderr, "DLL ERROR: closing laszip writer\n");
		close(memfd);
		laszip_destroy(laszip_writer);
		return nullopt;
	}
	if(laszip_destroy(laszip_writer))
	{
		fprintf(stderr, "DLL ERROR: destroying laszip writer\n");
		close(memfd);
		return nullopt;
	}

	const auto end_compress = std::chrono::high_resolution_clock::now();
	stats.m_saveDurationSec1 = std::chrono::duration<float>(end_compress - start).count();

	// Read compressed data from memfd into RAM buffer.
	const off_t laz_size = lseek(memfd, 0, SEEK_END);
	lseek(memfd, 0, SEEK_SET);

	std::vector<char> laz_buffer(laz_size);
	{
		char* dst = laz_buffer.data();
		off_t remaining = laz_size;
		while(remaining > 0)
		{
			ssize_t n = read(memfd, dst, remaining);
			if(n <= 0)
			{
				fprintf(stderr, "ERROR: reading from memfd failed\n");
				close(memfd);
				return nullopt;
			}
			dst += n;
			remaining -= n;
		}
	}
	close(memfd);

	stats.m_sizeMb = static_cast<float>(laz_size) / (1024.f * 1024.f);

	// Single sequential write to the SD card followed by fsync.
	{
		ZoneScopedN("flush_to_disk");
		int out_fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if(out_fd < 0)
		{
			fprintf(stderr, "ERROR: opening output file '%s': %s\n", filename.c_str(), strerror(errno));
			return nullopt;
		}
		const char* src = laz_buffer.data();
		off_t remaining = laz_size;
		while(remaining > 0)
		{
			ssize_t n = write(out_fd, src, remaining);
			if(n <= 0)
			{
				fprintf(stderr, "ERROR: writing to '%s': %s\n", filename.c_str(), strerror(errno));
				close(out_fd);
				return nullopt;
			}
			src += n;
			remaining -= n;
		}
		fsync(out_fd);
		close(out_fd);
	}

	std::cout << "exportLaz DONE" << std::endl;

	const auto end_write = std::chrono::high_resolution_clock::now();
	stats.m_saveDurationSec2 = std::chrono::duration<float>(end_write - end_compress).count();

	TracyPlot("laz_file_size_mb", (double)stats.m_sizeMb);
	TracyPlot("laz_save_duration_compress_sec", (double)stats.m_saveDurationSec1);
	TracyPlot("laz_save_duration_write_sec", (double)stats.m_saveDurationSec2);

	return stats;
}