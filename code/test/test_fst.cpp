#include "../src/tree/fstree.h"

int main(int argc, char** argv){


	if (argc != 2) {

		exit(0);
	}

	// build a fstree
	std::cerr << "Create fstree.\n";

	std::string bgptable(argv[1]);

	// even
	std::cerr << "----------------even:\n";

	FSTree<32, 6, 2>* fst1 = new FSTree<32, 6, 2>();
	
	fst1->build(bgptable);


	// CPE
	std::cerr << "\n---------------CPE:\n";

	FSTree<32, 6, 0>* fst2 = new FSTree<32, 6, 0>();
	
	fst2->build(bgptable);


	// MINMAX
	std::cerr << "\n---------------MINMAX:\n";

	FSTree<32, 6, 1>* fst3 = new FSTree<32, 6, 1>();
	
	fst3->build(bgptable);


	// map to a linear pipeline
		

///	// search the tree
///	std::cerr << "Search fstree.\n";

//	std::ifstream fin(bgptable, std::ios_base::binary);
//
//	std::string line; 

//	ipv4_type prefix;

//	uint8 length;				

//	while (getline(fin, line)) {	

//		// retrieve prefix and length
//		utility::retrieveInfo(line, prefix, length);

//		fst->search(prefix);
//	}
//
	// map to a linear pipeline
//	fst->scatterToPipeline(0);

//	fst->scatterToPipeline(1, 25); // W - U + 1 = 32 - 8 + 1 = 25

//	fst->scatterToPipeline(2, 25);

//	// delete the binary tree	
//	std::cerr << "Delete fstree.\n";

//	std::ifstream fin2(bgptable, std::ios_base::binary);

//	while (getline(fin2, line)) {

//		utility::retrieveInfo(line, prefix, length);
		
//		fst->del(prefix, length);
//	}
//
//	fst->report();
//	
//	// traverse after deletion
//	std::cerr << "Traverse after deletion.\n";

//	fst->traverse();


	return 0;
}
