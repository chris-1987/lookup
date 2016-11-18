#include "../src/mptree.h"


int main(int argc, char** argv){


#ifdef TEST
	if (argc != 2) {

		exit(0);
	}

	// build a binary tree
	std::cerr << "Create btree.\n";

	std::string bgptable(argv[1]);

	BTree<32>* bt = BTree<32>::getInstance();
	
	bt->build(bgptable);

	// search the tree
	std::cerr << "Search btree.\n";
	std::ifstream fin(bgptable, std::ios_base::binary);

	std::string line; 

	ipv4_type prefix;

	uint8 length;					

	while (getline(fin, line)) {	

		// retrieve prefix and length
		utility::retrieveInfo(line, prefix, length);

		bt->search(prefix);
	}

	// delete the binary tree	
	std::cerr << "Delete btree.\n";

	std::ifstream fin2(bgptable, std::ios_base::binary);

	getline(fin2, line); //skip */0

	while (getline(fin2, line)) {

		utility::retrieveInfo(line, prefix, length);
		
		bt->del(prefix, length);
	}
	
	// traverse after deletion
	std::cerr << "Traverse after deletion.\n";

	bt->traverse();


#endif

	return 0;
}
