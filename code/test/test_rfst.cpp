#include "../src/tree/rfstree.h"

int main(int argc, char** argv){


	if (argc != 2) {

		exit(0);
	}

	// build a rfstree
	std::cerr << "Create rfstree.\n";

	std::string bgptable(argv[1]);

	// CPE
	std::cerr << "\nCPE:\n";

	RFSTree<32, 6, 0, 6>* rfst1 = new RFSTree<32, 6, 0, 6>();
	
	rfst1->build(bgptable);

	rfst1->report();

	// MINMAX
	std::cerr << "\nMINMAX:\n";

	RFSTree<32, 6, 1, 6>* rfst2 = new RFSTree<32, 6, 1, 6>();
	
	rfst2->build(bgptable);

	rfst2->report();

	// even
	std::cerr << "even:\n";

	RFSTree<32, 6, 2, 6>* rfst3 = new RFSTree<32, 6, 2, 6>();
	
	rfst3->build(bgptable);

	rfst3->report();

	// search in the tree
	std::cerr << "Search rfstree.\n";

	std::ifstream fin(bgptable, std::ios_base::binary);

	std::string line; 

	ipv4_type prefix;

	uint8 length;				

	while (getline(fin, line)) {	

		// retrieve prefix and length
		utility::retrieveInfo(line, prefix, length);

//		std::cerr << "prefix: " << prefix << " length: " << (uint32)length << " nexthop: " << rfst3->search(prefix) << std::endl;

//		std::cin.get();

		rfst3->search(prefix);
	}

	// test rfst1
	std::cerr << "--------------CPE scatter\n";
	std::cerr << "scatter to linear pipeline\n";
	rfst1->scatterToPipeline(0);

	std::cerr << "scatter to random pipeline\n";
	rfst1->scatterToPipeline(1, 15); // W - U + 1 = 32 - 8 + 1 = 25

	std::cerr << "scatter to circular pipeline\n";
	rfst1->scatterToPipeline(2, 15);

	// test rfst2
	std::cerr << "--------------MINMAX scatter\n";
	std::cerr << "scatter to linear pipeline\n";
	rfst2->scatterToPipeline(0);

	std::cerr << "scatter to random pipeline\n";
	rfst2->scatterToPipeline(1, 15); // W - U + 1 = 32 - 8 + 1 = 25

	std::cerr << "scatter to circular pipeline\n";
	rfst2->scatterToPipeline(2, 15);

	// test rfst3
	std::cerr<< "---------------EVEN scatter\n";
	std::cerr << "scatter to linear pipeline\n";
	rfst3->scatterToPipeline(0);

	std::cerr << "scatter to random pipeline\n";
	rfst3->scatterToPipeline(1, 15); // W - U + 1 = 32 - 8 + 1 = 25

	std::cerr << "scatter to circular pipeline\n";
	rfst3->scatterToPipeline(2, 15);

	
	return 0;
}
