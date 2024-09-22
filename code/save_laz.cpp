#include "save_laz.h"
#include <iostream>
#include <laszip/laszip_api.h>

bool mandeye::saveLaz(const std::string& filename, LivoxPointsBufferPtr buffer)
{

	constexpr float scale = 0.0001f; // one tenth of milimeter
	// find max
	double max_x{std::numeric_limits<double>::lowest()};
	double max_y{std::numeric_limits<double>::lowest()};
	double max_z{std::numeric_limits<double>::lowest()};

	double min_x{std::numeric_limits<double>::max()};
	double min_y{std::numeric_limits<double>::max()};
	double min_z{std::numeric_limits<double>::max()};

	for(auto& p : *buffer)
	{
		double x = 0.001 * p.x;
		double y = 0.001 * p.y;
		double z = 0.001 * p.z;

		max_x = std::max(max_x, x);
		max_y = std::max(max_y, y);
		max_z = std::max(max_z, z);

		min_x = std::min(min_x, x);
		min_y = std::min(min_y, y);
		min_z = std::min(min_z, z);
	}

	std::cout << "processing: " << filename << "points " << buffer->size() << std::endl;

	laszip_POINTER laszip_writer;
	if(laszip_create(&laszip_writer))
	{
		fprintf(stderr, "DLL ERROR: creating laszip writer\n");
		return false;
	}

	// get a pointer to the header of the writer so we can populate it

	laszip_header* header;

	if(laszip_get_header_pointer(laszip_writer, &header))
	{
		fprintf(stderr, "DLL ERROR: getting header pointer from laszip writer\n");
		return false;
	}

	// populate the header
	int step = 1;
	if(buffer->size() > 4000000){
		step = ceil((double)buffer->size() / 2000000.0);
	}
	if(step < 1){
		step = 1;
	}

	int num_points = 0;
	for(int i = 0; i < buffer->size(); i += step){
		num_points ++;
	}

	header->file_source_ID = 4711;
	header->global_encoding = (1 << 0); // see LAS specification for details
	header->version_major = 1;
	header->version_minor = 2;
	//    header->file_creation_day = 120;
	//    header->file_creation_year = 2013;
	header->point_data_format = 1;
	header->point_data_record_length = 0;
	header->number_of_point_records = num_points;//buffer->size();
	header->number_of_points_by_return[0] = num_points;//buffer->size();
	header->number_of_points_by_return[1] = 0;
	header->point_data_record_length = 28;
	header->x_scale_factor = scale;
	header->y_scale_factor = scale;
	header->z_scale_factor = scale;

	header->max_x = max_x;
	header->min_x = min_x;
	header->max_y = max_y;
	header->min_y = min_y;
	header->max_z = max_z;
	header->min_z = min_z;

	// optional: use the bounding box and the scale factor to create a "good" offset
	// open the writer
	laszip_BOOL compress = (strstr(filename.c_str(), ".laz") != 0);

	if(laszip_open_writer(laszip_writer, filename.c_str(), compress))
	{
		fprintf(stderr, "DLL ERROR: opening laszip writer for '%s'\n", filename.c_str());
		return false;
	}

	fprintf(stderr, "writing file '%s' %scompressed\n", filename.c_str(), (compress ? "" : "un"));

	// get a pointer to the point of the writer that we will populate and write

	laszip_point* point;
	if(laszip_get_point_pointer(laszip_writer, &point))
	{
		fprintf(stderr, "DLL ERROR: getting point pointer from laszip writer\n");
		return false;
	}

	laszip_I64 p_count = 0;
	laszip_F64 coordinates[3];

	//for(int i = 0; i < buffer->size(); i++)
	for(int i = 0; i < buffer->size(); i += step)
	{

		const auto& p = buffer->at(i);
		point->intensity = p.reflectivity;
		point->gps_time = p.timestamp * 1e-9;
		point->user_data = p.line_id;
		point->classification = p.tag;
		point->user_data = p.laser_id;
		point->classification = p.tag;
		p_count++;
		coordinates[0] = 0.001 * p.x;
		coordinates[1] = 0.001 * p.y;
		coordinates[2] = 0.001 * p.z;
		if(laszip_set_coordinates(laszip_writer, coordinates))
		{
			fprintf(stderr, "DLL ERROR: setting coordinates for point %I64d\n", p_count);
			return false;
		}

		if(laszip_write_point(laszip_writer))
		{
			fprintf(stderr, "DLL ERROR: writing point %I64d\n", p_count);
			return false;
		}
	}

	if(laszip_get_point_count(laszip_writer, &p_count))
	{
		fprintf(stderr, "DLL ERROR: getting point count\n");
		return false;
	}

	fprintf(stderr, "successfully written %I64d points\n", p_count);

	// close the writer

	if(laszip_close_writer(laszip_writer))
	{
		fprintf(stderr, "DLL ERROR: closing laszip writer\n");
		return false;
	}

	// destroy the writer

	if(laszip_destroy(laszip_writer))
	{
		fprintf(stderr, "DLL ERROR: destroying laszip writer\n");
		return false;
	}

	std::cout << "exportLaz DONE" << std::endl;
	return true;
}