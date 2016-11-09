#include "btree.h"
#include "scheduler.h"

//#define TEST_SCHEDULER
#define TEST_TREE

int main(int argc, char** argv){

#ifdef TEST_SCHEDULER
	
	std::string str = std::string("lala");

	SearchScheduler<1, 1, 5>* ss = new SearchScheduler<1, 1, 5>();

	ss->print();

	ss->schedule(str);

	UpdateScheduler<1, 1, 5>* us = new UpdateScheduler<1, 1, 5>();

	us->print();

	us->schedule(str);
	
#endif

#ifdef TEST_TREE

	if (argc != 2) {

		exit(0);
	}


	// build a tree
	std::cerr << "Create btree.\n";
	std::string bgptable(argv[1]);

	BTree* bt = BTree::getInstance();
	
	bt->build4(bgptable);

//	bt->traverse();

	// search the tree
	std::cerr << "Search btree.\n";
	std::ifstream fin(bgptable, std::ios_base::binary);

	std::string line; 

	uint32 prefix;

	uint8 length;
						

	while (getline(fin, line)) {	

		// retrieve prefix and length
		utility::retrieveInfo4(line, prefix, length);

		bt->search4(prefix);
	}

	// delete the tree	
	std::cerr << "Delete btree.\n";

	std::ifstream fin2(bgptable, std::ios_base::binary);

	getline(fin2, line); //skip */0

	while (getline(fin2, line)) {

		utility::retrieveInfo4(line, prefix, length);
		
		bt->delete4(prefix, length);

	}
	
	// traverse after deletion
	std::cerr << "Traverse after deletion.\n";

	bt->traverse();

#endif

	return 0;

}
