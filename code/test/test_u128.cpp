#include "../src/common/common.h"
#include "../src/common/utility.h"
#include <sstream>

const int W = 128;

int main() {

{
	
	choose_ip_type<W>::ip_type x1;

	std::cerr << "x1: " << x1 << std::endl;

	uint64 x = static_cast<uint64>(1) << 63;

	std::cerr << std::hex << "hex x: " << x << std::endl;

	std::cerr << std::dec << "dec x: " << x << std::endl;

	choose_ip_type<W>::ip_type y1(6532916048470455938llu, 8382767142366093585llu);

	x1 = y1;

	std::cerr << "x1: " << x1 << std::endl;

	std::cerr << "y1: " << y1 << std::endl;
}


{	
	// test 1: read/write
	// write an element into file	
//	typename choose_ip_type<W>::ip_type x = std::numeric_limits<uint64>::max();

	typename choose_ip_type<W>::ip_type x(std::numeric_limits<uint64>::max(), std::numeric_limits<uint64>::max());

	std::ofstream fout("test.dat", std::ios_base::binary);

	fout << x;
	
	std::cerr << "x: " << x << std::endl;

	fout << "\n";

	fout.close();

	// read an element from file
	std::string str;
	
	std::ifstream fin("test.dat", std::ios_base::binary);

	getline(fin, str);

	std::stringstream ss(str);

	typename choose_ip_type<W>::ip_type y;

	ss >> y;

	std::cerr << "y: " << y << std::endl;

	fin.close();	
}



{
	// test 2: shift
	typename choose_ip_type<W>::ip_type x(
		6532916048470455938llu, 
		8382767142366093585llu);

	std::cerr << std::dec << x << std::endl;

	std::cerr << std::hex << x << std::endl;

	std::cerr << "-----\n";

	for (size_t i = 0; i < 128; ++i) {

		std::cerr << std::dec << i << " " <<  x.getBitValue(i) << "\n";

		//std::cin.get();
	}

	std::cerr << std::endl;	

	uint32 d = 10;

	for (uint32 j = 0; j < 128 - d; ++j) {

		uint32 beg = j;

		uint32 end = beg + d - 1;

		std::cerr << "beg: " << beg << " end: " << end << std::endl;

		std::cerr << std::dec << x.getBitsValue(beg, end) << std::endl;

		std::cin.get();		
	}
}
	

	return 0;
}
