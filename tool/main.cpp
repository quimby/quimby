#include "arguments.h"

#include "quimby/GadgetFile.h"
#include "quimby/Grid.h"
#include "quimby/Octree.h"
#include "quimby/SmoothParticle.h"
#include "quimby/PagedGrid.h"
#include "quimby/Database.h"
#include "quimby/HCube.h"

#include <cmath>
#include <ctime>
#include <cstdlib>
#include <string>
#include <ctime>
#include <sstream>

#include <omp.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

using namespace quimby;

int sph(Arguments& arguments);
int sph_dump(Arguments& arguments);
int paged_grid(Arguments& arguments);
int bfieldtest(Arguments& arguments);
int mass(Arguments& arguments);
int database(Arguments& arguments);

class DumpMagnitudeGridVisitor: public Grid<Vector3f>::Visitor {
private:
	std::ofstream outfile;
	bool logarithm;
public:
	DumpMagnitudeGridVisitor(const std::string& filename, bool logarithm) {
		outfile.open(filename.c_str(), std::ios::binary);
		this->logarithm = logarithm;
	}

	void visit(Grid<Vector3f>& grid, size_t x, size_t y, size_t z,
	           Vector3f& value) {
		float mag = std::sqrt(
		                value.x * value.x + value.y * value.y + value.z * value.z);

		if (logarithm) {
			mag = std::log(mag + 1e-32);
		}

		outfile.write((char*) &mag, sizeof(float));
	}
};

class DumpVectorGridVisitor: public Grid<Vector3f>::Visitor {
public:
	std::ofstream outfile;
	DumpVectorGridVisitor(const std::string& filename) {
		outfile.open(filename.c_str(), std::ios::binary);
	}

	void visit(Grid<Vector3f>& grid, size_t x, size_t y, size_t z,
	           Vector3f& value) {
		outfile.write((char*) &value, sizeof(Vector3f));
	}
};

int hc(Arguments& arguments) {
	size_t n = arguments.getInt("-n", 4);
	std::cout << "N: " << n << std::endl;

	float sizeKpc = arguments.getFloat("-size", 10);
	std::cout << "Size: " << sizeKpc << std::endl;

	size_t depth = arguments.getInt("-depth", 2);
	std::cout << "Depth: " << depth << std::endl;

	size_t target_depth = arguments.getInt("-target-depth", depth);
	std::cout << "Target Depth: " << target_depth << std::endl;

	std::string output = arguments.getString("-o", "hcube.hc4");
	std::cout << "Output: " << output << std::endl;

	std::string input = arguments.getString("-i", "field.raw");
	std::cout << "Input: " << input << std::endl;

	unsigned int bins = arguments.getInt("-bins", 10);
	std::cout << "Bins: " << bins << std::endl;

	float threshold = arguments.getFloat("-t", 1e-15);
	std::cout << "Threshold: " << threshold << std::endl;

	float error = arguments.getFloat("-e", 0.01);
	std::cout << "Error: " << error << std::endl;

	Vector3f offsetKpc;
	offsetKpc.x = arguments.getFloat("-ox", 0);
	offsetKpc.y = arguments.getFloat("-oy", 0);
	offsetKpc.z = arguments.getFloat("-oz", 0);
	std::cout << "Offset: " << offsetKpc << std::endl;

	std::ifstream in(input.c_str(), std::ios::binary);
	std::ofstream out(output.c_str(), std::ios::binary);

	switch (n) {
	case 2:
		HCube<2>::create(out, in, bins, sizeKpc, offsetKpc, sizeKpc, error,
		                 threshold, depth);
		break;

	case 4:
		HCube<4>::create(out, in, bins, sizeKpc, offsetKpc, sizeKpc, error,
		                 threshold, depth);
		break;

	case 8:
		HCube<8>::create(out, in, bins, sizeKpc, offsetKpc, sizeKpc, error,
		                 threshold, depth);
		break;

	case 16:
		HCube<16>::create(out, in, bins, sizeKpc, offsetKpc, sizeKpc, error,
		                  threshold, depth);
		break;

	case 32:
		HCube<32>::create(out, in, bins, sizeKpc, offsetKpc, sizeKpc, error,
		                  threshold, depth);
		break;

	case 64:
		HCube<64>::create(out, in, bins, sizeKpc, offsetKpc, sizeKpc, error,
		                  threshold, depth);
		break;

	case 128:
		HCube<128>::create(out, in, bins, sizeKpc, offsetKpc, sizeKpc, error,
		                   threshold, depth);
		break;

	default:
		std::cout << "Invalid n: " << n << std::endl;
		break;
	}

	return 0;
}

int hcdb(Arguments& arguments) {
	size_t n = arguments.getInt("-n", 4);
	std::cout << "N: " << n << std::endl;

	float sizeKpc = arguments.getFloat("-size", 10);
	std::cout << "Size: " << sizeKpc << std::endl;

	size_t depth = arguments.getInt("-depth", 2);
	size_t samples = pow(n, depth + 1);
	std::cout << "Depth: " << depth << " (" << (sizeKpc / samples) << " kpc)" << std::endl;


	size_t target_depth = arguments.getInt("-target-depth", depth);
	size_t target_samples = pow(n, target_depth + 1);
	std::cout << "Target Depth: " << target_depth << " (" << (sizeKpc / target_samples) << " kpc)" << std::endl;

	std::string output = arguments.getString("-o", "hcube.hc4");
	std::cout << "Output: " << output << std::endl;

	std::vector<std::string> databases;
	arguments.getVector("-db", databases);

	Databases db;
	std::cout << "Open databases: " << std::endl;

	for (size_t iDB = 0; iDB < databases.size(); iDB++) {
		ref_ptr<FileDatabase> fdb = new FileDatabase();

		if (!fdb->open(databases[iDB])) {
			std::cout << "Error: Could not open database: " << databases[iDB]
			          << std::endl;
			return 1;
		} else {
			std::cout << "  " << databases[iDB] << ", particles: "
			          << fdb->getCount() << std::endl;
		}

		db.add(fdb);
	}

	std::cout << "Database lower: " << db.getLowerBounds() << "\n";
	std::cout << "Database upper: " << db.getUpperBounds() << "\n";

	float threshold = arguments.getFloat("-t", 1e-15);
	std::cout << "Threshold: " << threshold << std::endl;

	float error = arguments.getFloat("-e", 0.01);
	std::cout << "Error: " << error << std::endl;

	Vector3f offsetKpc;
	offsetKpc.x = arguments.getFloat("-ox", 0);
	offsetKpc.y = arguments.getFloat("-oy", 0);
	offsetKpc.z = arguments.getFloat("-oz", 0);
	std::cout << "Offset: " << offsetKpc << std::endl;

	const int invalid_idx = -1;
	const int invalid_levels = -1;
	int idx = arguments.getInt("-i", invalid_idx);
	int levels = arguments.getInt("-l", invalid_levels);
	const size_t n2 = n * n;
	const size_t n3 = n2 * n;

	if (idx != invalid_idx && levels != invalid_levels) {
		size_t indices = pow(n3, levels + 1);
		std::cout << "Index: " << idx << " / " << (indices - 1) << std::endl;
		std::cout << "Levels: " << levels << std::endl;

		if (idx >= indices) {
			std::cout << "Invalid index!" << std::endl;
			return 1;
		}

		size_t t = idx;

		for (size_t i = 0; i <= levels; i++) {
			sizeKpc /= n;
			indices /= n3;
			size_t m = t / indices;
			size_t x = m / n2;
			size_t y = (m % n2) / n;
			size_t z = m % n;
			offsetKpc += Vector3f(x, y, z) * sizeKpc;
			t = t % indices;
		}

		std::cout << "New Offset: " << offsetKpc << std::endl;
		std::cout << "New Size: " << sizeKpc << std::endl;
		depth -= levels + 1;
		target_depth -= levels + 1;
		std::ostringstream str;
		str << output << "." << levels << "." << idx;
		output = str.str();
	}

	if (idx == invalid_idx && levels != invalid_levels) {
		size_t indices = pow(n3, levels + 1);
		std::vector<std::string> srcs(indices);

		for (size_t i = 0; i < indices; i++) {
			std::ostringstream str;
			str << output << "." << levels << "." << i;
			srcs[i] = str.str();
		}

		switch (n) {
		case 2:
			HCubeFile<2>::create(srcs, levels, error, threshold, depth, target_depth, output);
			break;

		case 4:
			HCubeFile<4>::create(srcs, levels, error, threshold, depth, target_depth, output);
			break;

		case 8:
			HCubeFile<8>::create(srcs, levels, error, threshold, depth, target_depth, output);
			break;

		case 16:
			HCubeFile<16>::create(srcs, levels, error, threshold, depth, target_depth, output);
			break;

		case 32:
			HCubeFile<32>::create(srcs, levels, error, threshold, depth, target_depth, output);
			break;

		case 64:
			HCubeFile<64>::create(srcs, levels, error, threshold, depth, target_depth, output);
			break;

		case 128:
			HCubeFile<128>::create(srcs, levels, error, threshold, depth, target_depth, output);
			break;

		default:
			std::cout << "Invalid n: " << n << std::endl;
			break;
		}
	} else {


		switch (n) {
		case 2:
			HCubeFile<2>::create(&db, offsetKpc, sizeKpc, error, threshold, depth, target_depth, output);
			break;

		case 4:
			HCubeFile<4>::create(&db, offsetKpc, sizeKpc, error, threshold, depth, target_depth, output);
			break;

		case 8:
			HCubeFile<8>::create(&db, offsetKpc, sizeKpc, error, threshold, depth, target_depth, output);
			break;

		case 16:
			HCubeFile<16>::create(&db, offsetKpc, sizeKpc, error, threshold, depth, target_depth, output);
			break;

		case 32:
			HCubeFile<32>::create(&db, offsetKpc, sizeKpc, error, threshold, depth, target_depth, output);
			break;

		case 64:
			HCubeFile<64>::create(&db, offsetKpc, sizeKpc, error, threshold, depth, target_depth, output);
			break;

		case 128:
			HCubeFile<128>::create(&db, offsetKpc, sizeKpc, error, threshold, depth, target_depth, output);
			break;

		default:
			std::cout << "Invalid n: " << n << std::endl;
			break;
		}
	}

	if (idx == invalid_idx) {
		size_t d = output.find_last_of(".");
		std::string cfgname = output.substr(0, d) + ".cfg";
		std::ofstream o(cfgname.c_str());

		size_t s = output.find_last_of("/");
		std::string filename = output.substr(s == std::string::npos ? 0 : (s + 1));


		o << "{\n";
		o << "  \"databases\": [\n";

		for (size_t iDB = 0; iDB < databases.size(); iDB++) {
			o << "    \"" << databases[iDB] << "\"";

			if (iDB + 1 == databases.size())
				o << "\n";
			else
				o << ",\n";

		}

		o << "  ],\n";
		o << "  \"n\": " << n << ",\n";
		o << "  \"depth\": " << depth << ",\n";
		o << "  \"filename\": \"" << filename << "\",\n";
		o << "  \"offset\": [" << offsetKpc.x << ", " << offsetKpc.y << ", " << offsetKpc.z << "],\n";
		o << "  \"size\": " << sizeKpc << ",\n";
		o << "  \"threshold\": " << threshold << ",\n";
		o << "  \"error\": " << error << ",\n";
		o << "  \"samples\": " << samples << ",\n";
		o << "  \"resolution\": " << (sizeKpc / target_samples) << ",\n";
		o << "  \"sampling_resolution\": " << (sizeKpc / samples) << "\n";
		o << "}\n";
	}

	return 0;
}

int bigfield(Arguments& arguments) {
	uint64_t bins = arguments.getInt("-bins", 64);
	std::cout << "Bins: " << bins << std::endl;

	unsigned int chunks = arguments.getInt("-c", 1);
	std::cout << "Chunks: " << chunks << std::endl;

	float sizeKpc = arguments.getFloat("-size", 10);
	std::cout << "Size: " << sizeKpc << std::endl;

	Vector3f offsetKpc;
	offsetKpc.x = arguments.getFloat("-ox", 0);
	offsetKpc.y = arguments.getFloat("-oy", 0);
	offsetKpc.z = arguments.getFloat("-oz", 0);
	std::cout << "Offset: " << offsetKpc << std::endl;

	std::string output = arguments.getString("-o", "field.raw");
	std::cout << "Output: " << output << std::endl;

	std::vector<std::string> databases;
	arguments.getVector("-db", databases);

	if (bins % chunks) {
		std::cout << "Bins must be dividable by chunks!" << std::endl;
		return 1;
	}

	float chunkSizeKpc = sizeKpc / chunks;
	size_t chunkSizeBins = bins / chunks;

	Databases db;
	std::cout << "Open databases: " << std::endl;

	for (size_t iDB; iDB < databases.size(); iDB++) {
		ref_ptr<FileDatabase> fdb = new FileDatabase();

		if (!fdb->open(databases[iDB])) {
			std::cout << "Error: Could not open database: " << databases[iDB]
			          << std::endl;
			return 1;
		} else {
			std::cout << "  " << databases[iDB] << ", particles: "
			          << fdb->getCount() << std::endl;
		}

		db.add(fdb);
	}

	// calculate max size
	uint64_t n2 = bins * bins;
	uint64_t n3 = bins * bins * bins;

	FILE* fd = ::fopen(output.c_str(), "wb");

	Vector3f* data = new Vector3f[n2 * chunkSizeBins];

	std::cout << "Sample data..." << std::endl;

	for (size_t iX = 0; iX < chunks; iX++) {
		std::cout << "Chunk: " << iX << std::endl;

		// zero
		for (size_t cx = 0; cx < chunkSizeBins; cx++)
			for (size_t cy = 0; cy < bins; cy++)
				for (size_t cz = 0; cz < bins; cz++)
					data[cx * bins * bins + cy * bins + cz] = Vector3f(0, 0, 0);

		// create visitor
		Vector3f lower = offsetKpc + Vector3f(iX * chunkSizeKpc, 0, 0);
		Vector3f upper = lower + Vector3f(chunkSizeKpc, sizeKpc, sizeKpc);
		std::cout << "  Lower: " << lower << "\n";
		std::cout << "  Upper: " << upper << std::endl;
		SimpleSamplingVisitor v(data, bins, lower, sizeKpc);
		v.showProgress(true);

		v.limit(0, chunkSizeBins - 1, 0, bins, 0, bins);
		db.accept(v);
		std::cout << std::endl;
		::fwrite(data, n2 * sizeof(Vector3f), chunkSizeBins, fd);
	}

	delete[] data;

	::fclose(fd);

}
#if 0
int bigfield(Arguments& arguments) {
	uint64_t bins = arguments.getInt("-bins", 10);
	std::cout << "Bins: " << bins << std::endl;

	unsigned int chunks = arguments.getInt("-c", 1);
	std::cout << "Chunks: " << bins << std::endl;

	float sizeKpc = arguments.getFloat("-size", 10);
	std::cout << "Size: " << sizeKpc << std::endl;

	Vector3f offsetKpc;
	offsetKpc.x = arguments.getFloat("-ox", 0);
	offsetKpc.y = arguments.getFloat("-oy", 0);
	offsetKpc.z = arguments.getFloat("-oz", 0);
	std::cout << "Offset: " << offsetKpc << std::endl;

	std::string output = arguments.getString("-o", "field.raw");
	std::cout << "Output: " << output << std::endl;

	std::string database = arguments.getString("-db", "field.db");
	std::cout << "Database: " << database << std::endl;

	if (bins % chunks) {
		std::cout << "Bins must be dividable by chunks!" << std::endl;
		return 1;
	}

	float chunkSizeKpc = sizeKpc / chunks;
	size_t chunkSizeBins = bins / chunks;

	FileDatabase db;
	db.open(database);

	// calculate max size
	uint64_t n = bins * bins * bins;
	std::cout << "N: " << n << std::endl;

	off_t fsize = n * sizeof(Vector3f);

	std::cout << "Size: " << fsize << " bytes, " << fsize / 1000000000
	          << " Gbytes" << std::endl;
	int fd = ::open(output.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
	ftruncate(fd, fsize);
	void* buffer = ::mmap(NULL, fsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
	                      0);

	if (buffer == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	Vector3f* data = (Vector3f*) buffer;
	std::cout << "Sample data..." << std::endl;
	SimpleSamplingVisitor v(data, bins, offsetKpc, sizeKpc);
	v.showProgress(true);

	for (size_t iX = 0; iX < chunks; iX++) {
		std::cout << "Chunk: " << iX << std::endl;
		size_t xmin = iX * chunkSizeBins, xmax = xmin + chunkSizeBins;

		for (size_t cx = xmin; cx < xmax; cx++)
			for (size_t cy = 0; cy < bins; cy++)
				for (size_t cz = 0; cz < bins; cz++)
					data[cx * bins * bins + cy * bins + cz] = Vector3f(0, 0, 0);

		v.limit(xmin, xmax - 1, 0, bins, 0, bins);
		Vector3f lower = offsetKpc + Vector3f(iX, 0, 0) * chunkSizeKpc;
		Vector3f upper = lower + Vector3f(chunkSizeKpc, sizeKpc, sizeKpc);
		db.accept(lower, upper, v);
		std::cout << std::endl;
		::msync(buffer, fsize, MS_SYNC);

	}

	if (::munmap(buffer, fsize) < 0) {
		perror("munmap");
		exit(1);
	}

	::close(fd);

}
#endif
int bfield(Arguments& arguments) {
	unsigned int bins = arguments.getInt("-bins", 10);
	std::cout << "Bins: " << bins << std::endl;

	float size = arguments.getFloat("-size", 10);
	std::cout << "Size: " << size << std::endl;

	std::string output = arguments.getString("-o", "bfield.raw");
	std::cout << "Output: " << output << std::endl;

	std::cout << "Create Grid" << std::endl;
	Grid<Vector3f> grid;
	grid.create(bins, size);
	Vector3f zero;
	zero.x = 0.0;
	zero.y = 0.0;
	zero.z = 0.0;
	grid.reset(zero);

	bool verbose = arguments.hasFlag("-v");

	std::vector<std::string> files;
	arguments.getVector("-f", files);

	for (size_t iArg = 0; iArg < files.size(); iArg++) {
		std::cout << "Open " << files[iArg] << std::endl;

		GadgetFile file;
		file.open(files[iArg]);

		if (file.good() == false) {
			std::cerr << "Failed to open file " << files[iArg] << std::endl;
			return 1;
		}

		file.readHeader();
		int pn = file.getHeader().particleNumberList[0];
		std::cout << "Number of SmoothParticles: " << pn << std::endl;

		std::cerr << "Read POS block" << std::endl;
		std::vector<float> pos;

		if (file.readFloatBlock("POS ", pos) == false) {
			std::cerr << "Failed to read POS block" << std::endl;
			return 1;
		}

		std::cerr << "Read BFLD block" << std::endl;
		std::vector<float> bfld;

		if (file.readFloatBlock("BFLD", bfld) == false) {
			std::cerr << "Failed to read BFLD block" << std::endl;
			return 1;
		}

		std::cerr << "Read HSML block" << std::endl;
		std::vector<float> hsml;

		if (file.readFloatBlock("HSML", hsml) == false) {
			std::cerr << "Failed to read HSML block" << std::endl;
			return 1;
		}

		std::cout << "Fill grid" << std::endl;

		#pragma omp parallel for schedule(dynamic, 10000)

		for (int iP = 0; iP < pn; iP++) {
			if (iP % (pn / 100) == 0 && verbose) {
				std::cerr << clock() << " ";
#ifdef _OPENMP
				std::cerr << "[" << omp_get_thread_num() << "] ";
#endif
				std::cerr << iP << " / " << pn << std::endl;
			}

			float sl = hsml[iP];
			float vol = M_PI / sl / sl / sl;
			float pX = pos[iP * 3];
			float pY = pos[iP * 3 + 1];
			float pZ = pos[iP * 3 + 2];
			float bX = bfld[iP * 3] * vol;
			float bY = bfld[iP * 3 + 1] * vol;
			float bZ = bfld[iP * 3 + 2] * vol;
			int steps = ceil(hsml[iP] * 2 / grid.getCellLength());
			int iX = pX / grid.getCellLength();
			int iY = pY / grid.getCellLength();
			int iZ = pZ / grid.getCellLength();

			for (int iStepX = -steps; iStepX <= steps; iStepX++) {
				float x = iStepX * grid.getCellLength() / sl;
				float vx = SmoothParticle::kernel(fabs(x)) * bX;

				for (int iStepY = -steps; iStepY <= steps; iStepY++) {
					float y = iStepY * grid.getCellLength() / sl;
					float vy = SmoothParticle::SmoothParticle::kernel(fabs(y))
					           * bY;

					for (int iStepZ = -steps; iStepZ <= steps; iStepZ++) {
						float z = iStepZ * grid.getCellLength() / sl;
						float vz = SmoothParticle::kernel(fabs(z)) * bZ;

						Vector3f& v = grid.get(iX + iStepX, iY + iStepY,
						                       iZ + iStepZ);
						#pragma omp critical
						{
							v.x += vx;
							v.y += vy;
							v.z += vz;
						}
					}
				}

			}
		}
	}

	if (arguments.hasFlag("-mag")) {
		std::cout << "Dump ZYX magnitude grid" << std::endl;
		DumpMagnitudeGridVisitor visitor(output, arguments.hasFlag("-log"));

		if (arguments.hasFlag("-zyx"))
			grid.acceptZYX(visitor);
		else
			grid.acceptXYZ(visitor);
	} else {
		DumpVectorGridVisitor visitor(output);

		if (arguments.hasFlag("-header")) {
			int intbins = bins;
			visitor.outfile.write((char*) &intbins, sizeof(intbins));
			visitor.outfile.write((char*) &intbins, sizeof(intbins));
			visitor.outfile.write((char*) &intbins, sizeof(intbins));
			double d = grid.getCellLength();
			visitor.outfile.write((char*) &d, sizeof(d));
			d = 0.0;
			visitor.outfile.write((char*) &d, sizeof(double));
			visitor.outfile.write((char*) &d, sizeof(double));
			visitor.outfile.write((char*) &d, sizeof(double));
		}

		if (arguments.hasFlag("-zyx")) {
			std::cout << "Dump ZYX vector grid" << std::endl;
			grid.acceptZYX(visitor);
		} else {
			std::cout << "Dump XYZ vector grid" << std::endl;
			grid.acceptXYZ(visitor);
		}
	}

	return 0;
}

struct GridCell {
	GridCell() :
		ref_count(0) {
	}
	std::vector<float> data;
	int64_t ref_count;
};

#if 0
void ranges(size_t& start, size_t& end, size_t n, float x, float h, float s) {
	int lowerX = floor((x - 2 * h) / s);

	if (lowerX < 0)
		lowerX = 0;
	else if (lowerX > n)
		lowerX = n;

	int upperX = ceil((x + 2 * h) / s);

	if (upperX < 0)
		upperX = 0;
	else if (upperX > n)
		upperX = n;

	start = lowerX;
	end = upperX;
}
void TFloatMagField::readCellData(GridCell& cell, const Vector3<float>& offset, std::ifstream& in, unsigned int count) const {
	Vector3<float> pos;
	Vector3<float> bfld;
	float hsml;

	for (size_t i = 0; i < count; i++) {
		in.read((char*)&pos.x, sizeof(float));
		in.read((char*)&pos.y, sizeof(float));
		in.read((char*)&pos.z, sizeof(float));
		in.read((char*)&bfld.x, sizeof(float));
		in.read((char*)&bfld.y, sizeof(float));
		in.read((char*)&bfld.z, sizeof(float));
		in.read((char*)&hsml, sizeof(float));
		float vol = M_PI / hsml / hsml / hsml;
		pos = pos - offset;
		bfld /= vol;
		size_t bX, eX, bY, eY, bZ, eZ;
		ranges(bX, eX, _fN, pos.x, hsml, _fStepsize);
		ranges(bY, eY, _fN, pos.y, hsml, _fStepsize);
		ranges(bZ, eZ, _fN, pos.z, hsml, _fStepsize);

		for (size_t x = bX; x < eX; x++) {
			float fx = x * _fStepsize - pos.x;
			float bx = kernel(fabs(fx) / hsml) * bfld.x;

			for (size_t y = bY; y < eY; y++) {
				float fy = y * _fStepsize - pos.y;
				float by = kernel(fabs(fy) / hsml) * bfld.y;

				for (size_t z = bZ; z < eZ; z++) {
					float fz = z * _fStepsize - pos.z;
					float bz = kernel(fabs(fz) / hsml) * bfld.z;
					size_t n = x * _fN * _fN + y * _fN + z;
					cell.data[n * 3] += bx;
					cell.data[n * 3 + 1] += by;
					cell.data[n * 3 + 2] += bz;
				}
			}
		}
	}
}

#endif

class SPVisitor: public Grid<Vector3f>::Visitor {
public:
	SmoothParticle particle;
	size_t lastX, lastY, lastZ;
	Vector3f b;

	SPVisitor() :
		lastX(-1), lastY(-1), lastZ(-1) {
	}

	void visit(Grid<Vector3f>& grid, size_t x, size_t y, size_t z,
	           Vector3f& value) {
		if (x != lastX) {
			b.x = SmoothParticle::kernel(particle.bfield.x,
			                             grid.toCellCenter(x), particle.position.x,
			                             particle.smoothingLength);
		}

		lastX = x;

		if (y != lastY) {
			b.y = SmoothParticle::kernel(particle.bfield.y,
			                             grid.toCellCenter(y), particle.position.y,
			                             particle.smoothingLength);
		}

		lastY = y;

		if (z != lastZ) {
			b.z = SmoothParticle::kernel(particle.bfield.z,
			                             grid.toCellCenter(z), particle.position.z,
			                             particle.smoothingLength);
		}

		lastZ = z;

		#pragma omp critical
		value += b;
	}
};

class DumpBFieldGridVisitor: public Grid<Vector3f>::Visitor {
private:
	std::ostream& out;
public:

	DumpBFieldGridVisitor(std::ostream& out) :
		out(out) {
	}

	void visit(Grid<Vector3f>& grid, size_t x, size_t y, size_t z,
	           Vector3f& value) {
		out.write((char*) &value.x, sizeof(float));
		out.write((char*) &value.y, sizeof(float));
		out.write((char*) &value.z, sizeof(float));
	}
};

void bfield_grid(std::vector<float>& pos, std::vector<float>& bfld,
                 std::vector<float>& hsml, Vector3f& offset, Grid<Vector3f>& grid,
                 size_t iP) {

}

int block(Arguments& arguments) {
	unsigned int bins = arguments.getInt("-bins", 100);
	std::cout << "Bins: " << bins << std::endl;

	float size = arguments.getFloat("-size", 40000);
	std::cout << "Size: " << size << std::endl;

	std::string output = arguments.getString("-o", "block");
	std::cout << "Output: " << output << std::endl;

	std::cout << "Create Grid" << std::endl;
	Grid<Vector3f> grid;
	grid.create(bins, size);
	grid.reset(Vector3f(0, 0, 0));

	std::cout << "Cell Size: " << grid.getCellLength() << std::endl;

	Vector3f offset(arguments.getFloat("-offX", 0),
	                arguments.getFloat("-offY", 0), arguments.getFloat("-offZ", 0));
	std::cout << "Offset: " << offset << std::endl;

	bool verbose = arguments.hasFlag("-v");

	std::vector<std::string> files;
	arguments.getVector("-f", files);

	for (size_t iArg = 0; iArg < files.size(); iArg++) {
		std::cout << "Open " << files[iArg] << " (" << (iArg + 1) << "/"
		          << files.size() << ")" << std::endl;

		GadgetFile file;
		file.open(files[iArg]);

		if (file.good() == false) {
			std::cerr << "Failed to open file " << files[iArg] << std::endl;
			return 1;
		}

		file.readHeader();
		int pn = file.getHeader().particleNumberList[0];
		std::cout << "  Number of SmoothParticles: " << pn << std::endl;

		std::cerr << "  Read POS block" << std::endl;
		std::vector<float> pos;

		if (file.readFloatBlock("POS ", pos) == false) {
			std::cerr << "Failed to read POS block" << std::endl;
			return 1;
		}

		std::cerr << "  Read BFLD block" << std::endl;
		std::vector<float> bfld;

		if (file.readFloatBlock("BFLD", bfld) == false) {
			std::cerr << "Failed to read BFLD block" << std::endl;
			return 1;
		}

		std::cerr << "  Read HSML block" << std::endl;
		std::vector<float> hsml;

		if (file.readFloatBlock("HSML", hsml) == false) {
			std::cerr << "Failed to read HSML block" << std::endl;
			return 1;
		}

		#pragma omp parallel for schedule(dynamic, 10000)

		for (int iP = 0; iP < pn; iP++) {
			if (iP == 0) {
				std::cout << "  Fill grid";
#ifdef _OPENMP
				std::cout << " (" << omp_get_num_threads() << " threads)";
#endif
				std::cout << std::endl;

			}

			if (iP % (pn / 100) == 0 && verbose) {
				std::cout << "  ";

				std::cout << iP << ": " << (iP * 100) / pn << "%\r";
				std::cout.flush();
			}

			SPVisitor v;
			v.particle.smoothingLength = hsml[iP];
			v.particle.position.x = pos[iP * 3] - offset.x;
			v.particle.position.y = pos[iP * 3 + 1] - offset.y;
			v.particle.position.z = pos[iP * 3 + 2] - offset.z;

			float norm = 1.0 / M_PI / pow(v.particle.smoothingLength, 3);
			v.particle.bfield.x = bfld[iP * 3] * norm;
			v.particle.bfield.y = bfld[iP * 3 + 1] * norm;
			v.particle.bfield.z = bfld[iP * 3 + 2] * norm;

			AABC<float> aabc(v.particle.position,
			                 v.particle.smoothingLength * 2);
			grid.acceptZYX(v, aabc);
		}

		std::cout << "  Done                 " << std::endl;
	}

	std::cout
	        << grid.get((size_t) bins / 2, (size_t) bins / 2, (size_t) bins / 2)
	        << std::endl;

	std::cout << "Write output" << std::endl;

	std::ofstream outfile;
	outfile.open(output.c_str(), std::ios::binary);
	outfile.write((const char*) &bins, sizeof(bins));
	outfile.write((const char*) &size, sizeof(size));
	outfile.write((const char*) &offset.x, sizeof(float));
	outfile.write((const char*) &offset.y, sizeof(float));
	outfile.write((const char*) &offset.z, sizeof(float));
	DumpBFieldGridVisitor visitor(outfile);
	grid.acceptZYX(visitor);

	return 0;
}

struct file_sphs {
	std::vector<float> pos;
	std::vector<float> bfld;
	std::vector<float> hsml;
};

void extract(file_sphs& fs, const std::string& filename) {
	std::cout << "Open " << filename << std::endl;

	GadgetFile file;
	file.open(filename);

	if (file.good() == false) {
		std::cerr << "Failed to open file " << filename << std::endl;
		return;
	}

	file.readHeader();
	int pn = file.getHeader().particleNumberList[0];
	std::cout << " Number of SmoothParticles: " << pn << std::endl;

	std::cout << " Read POS block" << std::endl;

	if (file.readFloatBlock("POS ", fs.pos) == false) {
		std::cerr << "Failed to read POS block" << std::endl;
		return;
	}

	std::cout << " Read BFLD block" << std::endl;

	if (file.readFloatBlock("BFLD", fs.bfld) == false) {
		std::cerr << "Failed to read BFLD block" << std::endl;
		return;
	}

	std::cout << " Read HSML block" << std::endl;

	if (file.readFloatBlock("HSML", fs.hsml) == false) {
		std::cerr << "Failed to read HSML block" << std::endl;
		return;
	}
}

void dump(const std::vector<file_sphs>& fs, size_t x, size_t y, size_t z,
          float cellSize, std::ostream& output) {
	std::cout << "dump " << x << "/" << y << "/" << z << std::endl;
	std::vector<std::vector<size_t> > indices;
	indices.resize(fs.size());

	// find intersecting particles
	Vector3<float> center(x * cellSize + cellSize / 2,
	                      y * cellSize + cellSize / 2, z * cellSize + cellSize / 2);
	AABC<float> aabc(center, cellSize / 2);
	std::cout << " bounding box: " << aabc << std::endl;

	for (size_t iFile = 0; iFile < fs.size(); iFile++) {
		for (size_t iSPH = 0; iSPH < fs[iFile].pos.size() / 3; iSPH++) {
			Vector3<float> v(fs[iFile].pos[iSPH * 3],
			                 fs[iFile].pos[iSPH * 3 + 1], fs[iFile].pos[iSPH * 3 + 2]);
			float l = fs[iFile].hsml[iSPH];
			AABC<float> bb(v, l);

			if (aabc.intersects(bb)) {
				indices[iFile].push_back(iSPH);
			}
		}
	}

	std::cout << " write to file" << std::endl;

	// dump particles
	unsigned int tmp = x;
	output.write((const char*) &tmp, sizeof(unsigned int));
	tmp = y;
	output.write((const char*) &tmp, sizeof(unsigned int));
	tmp = z;
	output.write((const char*) &tmp, sizeof(unsigned int));

	unsigned int total = 0;

	for (size_t iFile = 0; iFile < fs.size(); iFile++) {
		total += indices[iFile].size();
	}

	std::cout << " " << x << "/" << y << "/" << z << ": " << total << std::endl;
	tmp = total;
	output.write((const char*) &tmp, sizeof(unsigned int));

	for (size_t iFile = 0; iFile < fs.size(); iFile++) {
		for (size_t iSPH = 0; iSPH < indices[iFile].size(); iSPH++) {
			size_t index = indices[iFile][iSPH];
			output.write((const char*) &fs[iFile].pos[index * 3],
			             sizeof(float));
			output.write((const char*) &fs[iFile].pos[index * 3 + 1],
			             sizeof(float));
			output.write((const char*) &fs[iFile].pos[index * 3 + 2],
			             sizeof(float));
			output.write((const char*) &fs[iFile].bfld[index * 3],
			             sizeof(float));
			output.write((const char*) &fs[iFile].bfld[index * 3 + 1],
			             sizeof(float));
			output.write((const char*) &fs[iFile].bfld[index * 3 + 2],
			             sizeof(float));
			output.write((const char*) &fs[iFile].hsml[index], sizeof(float));
		}
	}

}

int info(Arguments& arguments) {
	for (size_t i = 2; i < (size_t) arguments.getCount(); i++) {
		std::cout << "Print info for file: " << arguments.get(i) << std::endl;
		GadgetFile file;
		file.open(arguments.get(i));
		file.printBlocks();
	}

	return 0;
}

int pp(Arguments& arguments) {
	std::string filename = arguments.getString("-o", "prepared.sph");
	std::cout << "Output: " << filename << std::endl;

	std::ofstream output(filename.c_str());

	unsigned int bins = arguments.getInt("-bins", 10);
	std::cout << "Bins: " << bins << std::endl;

	float size = arguments.getFloat("-size", 10);
	std::cout << "Size: " << size << std::endl;

	output.write((const char*) &size, sizeof(float));
	output.write((const char*) &bins, sizeof(unsigned int));

	std::vector<std::string> files;
	arguments.getVector("-f", files);

	std::vector<file_sphs> fs;
	fs.resize(files.size());

	for (size_t iFile = 0; iFile < files.size(); iFile++) {
		extract(fs[iFile], files[iFile]);
	}

	for (size_t x = 0; x < bins; x++) {
		for (size_t y = 0; y < bins; y++) {
			for (size_t z = 0; z < bins; z++) {
				dump(fs, x, y, z, size / bins, output);
			}
		}
	}

	return 0;
}

typedef int (*function_pointer)(Arguments& arguments);

struct function {
	function(const std::string& name, const std::string& desc,
	         function_pointer f) :
		name(name), description(desc), f(f) {
	}
	std::string name, description;
	function_pointer f;
};

int main(int argc, const char** argv) {
	std::vector<function> functions;
	functions.push_back(
	    function("bigfield", "Create raw magnetic field from database",
	             &bigfield));

	try {
		Arguments arguments(argc, argv);

		if (arguments.getCount() < 2) {
			std::cout << "Functions:" << std::endl;
			std::cout << "  mass        mass grid" << std::endl;
			std::cout << "  bfield      bfield grid" << std::endl;
			std::cout << "  av          average bfield" << std::endl;
			std::cout << "  pp          preprocess for use in CRPRopa"
			          << std::endl;
			std::cout << "  writetest   grid write test" << std::endl;
			std::cout << "  readtest    grid read test" << std::endl;
			return 1;
		}

		std::string function = argv[1];

		if (function == "mass")
			return mass(arguments);
		else if (function == "bfield")
			return bfield(arguments);
		else if (function == "bigfield")
			return bigfield(arguments);
		else if (function == "hc")
			return hc(arguments);
		else if (function == "hcdb")
			return hcdb(arguments);
		else if (function == "bfieldtest")
			return bfieldtest(arguments);
		else if (function == "pp")
			return pp(arguments);
		else if (function == "pg")
			return paged_grid(arguments);
		else if (function == "sph")
			return sph(arguments);
		else if (function == "sph-dump")
			return sph_dump(arguments);
		else if (function == "block")
			return block(arguments);
		else if (function == "info")
			return info(arguments);
		else if (function == "db")
			return database(arguments);
		else if (function == "writetest") {
			if (arguments.hasFlag("-float")) {
				Grid<float> fg;
				fg.create(2, 1.0);
				fg.save("ls_float.dat");
			} else if (arguments.hasFlag("-vector")) {
				Grid<Vector3<float> > fg;
				fg.create(2, 1.0);
				fg.save("ls_vector.dat");
			}
		} else if (function == "readtest") {
			if (arguments.hasFlag("-float")) {
				Grid<float> fg;
				fg.load("ls_float.dat");
				std::cout << "Bins: " << fg.getBins() << std::endl;
				std::cout << "Size: " << fg.getSize() << std::endl;
			} else if (arguments.hasFlag("-vector")) {
				Grid<Vector3<float> > fg;
				fg.load("ls_vector.dat");
				std::cout << "Bins: " << fg.getBins() << std::endl;
				std::cout << "Size: " << fg.getSize() << std::endl;
			}
		}
	} catch (std::exception& e) {
		std::cerr << "EXCEPTION: " << e.what() << std::endl;
	}

	return 0;
}

