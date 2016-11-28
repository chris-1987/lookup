#include "../src/tree/rmptree.h"

int main(int argc, char** argv){

	if (argc != 2) {

		exit(0);
	}

	// build the index
	std::cerr << "Build index.\n";

	std::string bgptable(argv[1]);
	
	// W = 32 (ip address length), K = 2 (stride) and U = 4 (short/long prefix threshold)
	RMPTree<32, 2, 8>* rmpt = new RMPTree<32, 2, 8>();

	rmpt->build(bgptable);

	rmpt->report();

	// search in the index
	std::cerr << "Search index.\n";

	std::ifstream fin(bgptable, std::ios_base::binary);

	std::string line;

	ipv4_type prefix;

	uint8 length;

	while (getline(fin, line)) {

		utility::retrieveInfo(line, prefix, length);

		rmpt->search(prefix);

	//	std::cerr << "prefix: " << prefix << " length: " << (uint32)length << " nexthop: " << rmpt->search(prefix) << std::endl;
	}		

	// map to a linear pipeline <lin == 0, pipestage = H2>
	rmpt->scatterToPipeline(0);

	// map to a random pipeline <ran == 1, pipestage>
	rmpt->scatterToPipeline(1, 6);

	// map to a circular pipeline <cir == 2, pipestage>
	rmpt->scatterToPipeline(2, 6);	


	// delete the binary tree
	std::cerr << "Delete index.\n";

	std::ifstream fin2(bgptable, std::ios_base::binary);

	while (getline(fin2, line)) {

		utility::retrieveInfo(line, prefix, length);

		rmpt->del(prefix, length);	
	}		

	rmpt->report();

	return 0;
}
