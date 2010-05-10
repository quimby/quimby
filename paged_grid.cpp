/*
 * paged_grid.cpp
 *
 *  Created on: 06.05.2010
 *      Author: gmueller
 */

#include "arguments.hpp"

#include "gadget/PagedGrid.hpp"
#include "gadget/SmoothParticle.hpp"
#include "gadget/GadgetFile.hpp"

#include <ctime>
#include <limits>
#include <omp.h>

class PagedSPVisitor: public PagedGrid<Vector3f>::Visitor {
public:
	SmoothParticle particle;
	size_t lastX, lastY, lastZ;
	Vector3f b;
	float cellLength;

	PagedSPVisitor() :
		lastX(-1), lastY(-1), lastZ(-1) {
	}

	void visit(PagedGrid<Vector3f> &grid, size_t x, size_t y, size_t z,
			Vector3f &value) {
		if (x != lastX) {
			b.x = SmoothParticle::kernel(particle.bfield.x, toCellCenter(x),
					particle.position.x, particle.smoothingLength);
		}
		lastX = x;

		if (y != lastY) {
			b.y = SmoothParticle::kernel(particle.bfield.y, toCellCenter(y),
					particle.position.y, particle.smoothingLength);
		}
		lastY = y;

		if (z != lastZ) {
			b.z = SmoothParticle::kernel(particle.bfield.z, toCellCenter(z),
					particle.position.z, particle.smoothingLength);
		}
		lastZ = z;

#pragma omp critical
		value += b;
	}

	float toCellCenter(size_t x) {
		float a = (float) x + 0.5f;
		return cellLength * a;
	}
};

inline size_t limit(const float &x, const size_t &lower, const size_t &upper) {
	size_t r;

	if (x < 0)
		r = 0;
	else
		r = x;

	if (r < lower)
		r = lower;
	else if (r > upper)
		r = upper;

	return r;
}

size_t dround(double d) {
	return size_t(d + 0.5);
}

int paged_grid(Arguments &arguments) {
	float pageSize = arguments.getInt("-pageSize", 100);
	std::cout << "PageSize:       " << pageSize << " kpc " << std::endl;

	float res = arguments.getFloat("-res", 50);
	std::cout << "Resolution:     " << res << " kpc" << std::endl;

	int pageLength = pageSize / res;
	std::cout << "PageLength:     " << pageLength << std::endl;

	float size = arguments.getFloat("-size", 40000);
	std::cout << "Size:           " << size << " kpc" << std::endl;

	BinaryPageIO<Vector3f> io;
	io.prefix = arguments.getString("-prefix", "paged_grid");
	std::cout << "Output Prefix:  " << io.prefix << std::endl;

	size3_t lowerLimit, upperLimit;
	lowerLimit.x = dround(arguments.getFloat("-lx", 0.0) / res);
	lowerLimit.y = dround(arguments.getFloat("-ly", 0.0) / res);
	lowerLimit.z = dround(arguments.getFloat("-lz", 0.0) / res);

	upperLimit.x = dround(arguments.getFloat("-ux", size) / res);
	upperLimit.y = dround(arguments.getFloat("-uy", size) / res);
	upperLimit.z = dround(arguments.getFloat("-uz", size) / res);

	std::cout << "Lower limit:    " << lowerLimit * res << " kpc" << std::endl;
	std::cout << "Upper limit:    " << upperLimit * res << " kpc" << std::endl;

	int memory = arguments.getInt("-memory", 512);
	size_t page_byte_size = pageLength * pageLength * pageLength
			* sizeof(Vector3f);
	int pageCount = (memory * 1024 * 1024) / page_byte_size;
	std::cout << "Memory:         " << memory << " MiB -> " << pageCount
			<< " pages" << std::endl;

	io.defaultValue = Vector3f(0.0f);

	size_t fileSizeKpc = arguments.getFloat("-fileSize", 10000);
	io.fileSize = fileSizeKpc / pageSize;
	size_t pages_per_file = std::pow(io.fileSize, 3);
	std::cout << "FileSize:       " << fileSizeKpc << " kpc "
			<< (pages_per_file * page_byte_size / 1024 / 1024) << " MiB -> "
			<< pages_per_file << " pages" << std::endl;

	LeastAccessPagingStrategy<Vector3f> strategy;
	PagedGrid<Vector3f> grid(size3_t(size / res, size / res, size / res),
			pageLength);
	grid.setStrategy(&strategy);
	grid.setIO(&io);
	grid.setPageCount(pageCount);

	Vector3f offset(arguments.getFloat("-offX", 0), arguments.getFloat("-offY",
			0), arguments.getFloat("-offZ", 0));
	std::cout << "Offset:         " << offset << std::endl;

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

		std::cout << "  Read POS block" << std::endl;
		std::vector<float> pos;
		if (file.readFloatBlock("POS ", pos) == false) {
			std::cerr << "Failed to read POS block" << std::endl;
			return 1;
		}

		std::cout << "  Read BFLD block" << std::endl;
		std::vector<float> bfld;
		if (file.readFloatBlock("BFLD", bfld) == false) {
			std::cerr << "Failed to read BFLD block" << std::endl;
			return 1;
		}

		std::cout << "  Read HSML block" << std::endl;
		std::vector<float> hsml;
		if (file.readFloatBlock("HSML", hsml) == false) {
			std::cerr << "Failed to read HSML block" << std::endl;
			return 1;
		}

		std::cout << "  Fill grid" << std::endl;

		time_t start = std::time(0);
		time_t last = std::time(0);
		size_t lastN = 0;
		for (int iP = 0; iP < pn; iP++) {
			time_t now = std::time(0);
			if ((now - last > 1) && verbose && iP) {
				time_t elapsed = now - last;
				size_t n = iP - lastN;
				float pps = (float) n / elapsed;
				float pps_average = (float) iP / (now - start);
				time_t total = pn / pps;
				time_t eta = (pn * (pps + pps_average) / 2 - now + start) / 60;
				std::cout << "\r  " << iP << ": " << (iP * 100) / pn
						<< "%, active pages: " << grid.getActivePageCount()
						<< ", pages loaded: " << io.loadedPages
						<< ", throughput: " << pps << ", eta: " << eta
						<< " minutes";
				std::cout.flush();

				last = now;
				lastN = iP;
			}

			PagedSPVisitor v;
			v.particle.smoothingLength = hsml[iP];
			v.particle.position.x = pos[iP * 3] - offset.x;
			v.particle.position.y = pos[iP * 3 + 1] - offset.y;
			v.particle.position.z = pos[iP * 3 + 2] - offset.z;

			float norm = 1.0 / M_PI / pow(v.particle.smoothingLength, 3);
			v.particle.bfield.x = bfld[iP * 3] * norm;
			v.particle.bfield.y = bfld[iP * 3 + 1] * norm;
			v.particle.bfield.z = bfld[iP * 3 + 2] * norm;

			v.cellLength = res;

			size3_t lower, upper;
			lower.x
					= std::max(lowerLimit.x,
							(size_t) std::floor((v.particle.position.x
									- v.particle.smoothingLength) / res));
			lower.y
					= std::max(lowerLimit.y,
							(size_t) std::floor((v.particle.position.y
									- v.particle.smoothingLength) / res));
			lower.z
					= std::max(lowerLimit.z,
							(size_t) std::floor((v.particle.position.z
									- v.particle.smoothingLength) / res));

			upper.x
					= std::min(upperLimit.x,
							(size_t) std::ceil((v.particle.position.x
									+ v.particle.smoothingLength) / res));
			upper.y
					= std::min(upperLimit.y,
							(size_t) std::ceil((v.particle.position.y
									+ v.particle.smoothingLength) / res));
			upper.z
					= std::min(upperLimit.z,
							(size_t) std::ceil((v.particle.position.z
									+ v.particle.smoothingLength) / res));

			grid.acceptZYX(v, lower, upper);
		}

		std::cout << "  Done                 " << std::endl;
	}

	std::cout << "Write output" << std::endl;
	grid.flush();

	return 0;
}