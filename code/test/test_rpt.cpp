#include "../src/tree/rptree.h"

int main(int argc, char** argv){

	if (argc != 2) {

		exit(0);
	}

	// build a binary tree
	std::cerr << "Create rptree.\n";

	std::string bgptable(argv[1]);

	RPTree<32, 4>* rpt = new RPTree<32, 4>();
	
	rpt->build(bgptable);

	rpt->report();

	// search the tree
	std::cerr << "Search btree.\n";
	std::ifstream fin(bgptable, std::ios_base::binary);

	std::string line; 

	ipv4_type prefix;

	uint8 length;				

	while (getline(fin, line)) {	

		// retrieve prefix and length
		utility::retrieveInfo(line, prefix, length);

		rpt->search(prefix);

	//	std::cerr<< "prefix: " << prefix << " length: " << (uint32)length << " nexthop: " << rpt->search(prefix) << std::endl;
	}

	// map to a linear pipeline
	rpt->scatterToPipeline(0);

	rpt->scatterToPipeline(1, 6); // W - U + 1 = 32 - 8 + 1 = 25

	rpt->scatterToPipeline(2, 6);


	// delete the binary tree	
	std::cerr << "Delete rptree.\n";

	std::ifstream fin2(bgptable, std::ios_base::binary);

	while (getline(fin2, line)) {

		utility::retrieveInfo(line, prefix, length);
		
		rpt->del(prefix, length);
	}

	rpt->report();
	
	// traverse after deletion
	std::cerr << "Traverse after deletion.\n";

//	rpt->traverse();

	return 0;
}
