#include "../src/tree/rbtree.h"

int main(int argc, char** argv){


	if (argc != 2) {

		exit(0);
	}

	// build a binary tree
	std::cerr << "Create btree.\n";

	std::string bgptable(argv[1]);

	RBTree<32, 6>* rbt = new RBTree<32, 6>();
	
	rbt->build(bgptable);

	rbt->report();

	// search the tree
	std::cerr << "Search btree.\n";
	std::ifstream fin(bgptable, std::ios_base::binary);

	std::string line; 

	ipv4_type prefix;

	uint8 length;				

	while (getline(fin, line)) {	

		// retrieve prefix and length
		utility::retrieveInfo(line, prefix, length);

		rbt->search(prefix);

//		std::cerr<< "prefix: " << prefix << " length: " << (uint32)length << " nexthop: " << rbt->search(prefix) << std::endl;
	}

	// map to a linear pipeline
	rbt->scatterToPipeline(0);

	rbt->scatterToPipeline(1, 25); // W - U + 1 = 32 - 8 + 1 = 25

	rbt->scatterToPipeline(2, 25);

//	// delete the binary tree	
	std::cerr << "Delete rbtree.\n";

	std::ifstream fin2(bgptable, std::ios_base::binary);

	while (getline(fin2, line)) {

		utility::retrieveInfo(line, prefix, length);
		
		rbt->del(prefix, length);
	}

	rbt->report();
	
	// traverse after deletion
	std::cerr << "Traverse after deletion.\n";

	rbt->traverse();


	return 0;
}
