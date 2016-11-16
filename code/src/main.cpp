#include "btree.h"
#include "fstree.h"
#include "ptree.h"
#include "scheduler.h"

//#define TEST_SCHEDULER
//#define TEST_BTREE
//#define TEST_FSTREE
#define TEST_PTREE

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

#ifdef TEST_BTREE

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

#ifdef TEST_FSTREE

	if (argc != 2) {

		exit(0);
	}


	// build a fixed-stride tree
	std::cerr << "------------Create fstree with CPE.\n";

	std::string bgptable(argv[1]);

	FSTree<32, 8, 0>* fst_cpe = FSTree<32, 8, 0>::getInstance();
	
	fst_cpe->build(bgptable);
	
	// search the fixed-stride tree
	std::cerr << "Search fixed-stride tree.\n";

	std::ifstream fin(bgptable, std::ios_base::binary);

	std::string line; 

	ipv4_type prefix;

	uint8 length;					

	while (getline(fin, line)) {	

		// retrieve prefix and length
		utility::retrieveInfo(line, prefix, length);

		fst_cpe->search(prefix);
	//	std::cerr << "prefix: " << prefix << " " << " nexthop: " << fst_cpe->search(prefix) << std::endl;
	}

	std::cerr << "------------Create fstree with MinMax.\n";

	FSTree<32, 8, 1>* fst_mm = FSTree<32, 8, 1>::getInstance();

	fst_mm->build(bgptable);

	// search the fixed-stride tree
	std::cerr << "Search fixed-stride tree.\n";

	std::ifstream fin2(bgptable, std::ios_base::binary);

	while(getline(fin2, line)) {

		// retrieve prefix and length
		utility::retrieveInfo(line, prefix, length);

		fst_mm->search(prefix);

	//	std::cerr << "prefix: " << prefix << " " << " nexthop: " << fst_mm->search(prefix) << std::endl;
	}

		
	std::cerr << "------------Create fstree with Even.\n";

	FSTree<32, 9, 2>* fst_even = FSTree<32, 9, 2>::getInstance();

	fst_even->build(bgptable);

	// search the fixed-stride tree
	std::cerr << "Search fixed-stride tree.\n";

	std::ifstream fin3(bgptable, std::ios_base::binary);

	while(getline(fin3, line)) {

		// retrieve prefix and length
		utility::retrieveInfo(line, prefix, length);

		fst_even->search(prefix);

//		std::cerr << "prefix: " << prefix << " " << " nexthop: " << fst_mm->search(prefix) << std::endl;
	}
	
#endif


#ifdef TEST_PTREE

	if (argc != 3) {

		exit(0);
	}


	// build a binary tree
	std::cerr << "Create ptree.\n";

	std::string bgptable(argv[1]);

	PTree<32>* pt = PTree<32>::getInstance();
	
	pt->build(bgptable);

	// delete the prefix tree	
	std::string line; 

	ipv4_type prefix;

	uint8 length;					

	std::cerr << "Delete ptree.\n";

	std::string deltable(argv[2]);

	std::ifstream fin2(deltable, std::ios_base::binary);

	while (getline(fin2, line)) {

		utility::retrieveInfo(line, prefix, length);
	
		pt->del(prefix, length, pt->getRoot(), 0);
	}

	// traverse after deletion
	pt->traverse();

	// search the tree
	std::cerr << "Search ptree.\n";

	std::ifstream fin(bgptable, std::ios_base::binary);

	while (getline(fin, line)) {	

		// retrieve prefix and length
		utility::retrieveInfo(line, prefix, length);

		pt->search(prefix);

	//	std::cerr << "prefix: " << prefix << " length:" << (uint32)length << " nexthop: " << std::endl;
	//	std::cerr <<  pt->search(prefix) << std::endl;

	}





#endif

	return 0;

}
