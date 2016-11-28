#include "../src/tree/rbtree.h"
#include "../src/scheduler/linsched.h"
#include "../src/common/utility.h"

static const int SN = 5; // number of pipe stages
static const size_t RN = 1024 * 1024 * 1; // number of lookups
static const int PL = 32; // prefix length, 32 or 128
static const int PT = 4; // threshold for short & long prefixes

int main(int argc, char** argv){

	if (argc != 3) {
		
		std::cerr << "This program takes two parameters:\n";

		std::cerr << "The 1st parameter specifies the file of the BGP table. We reuse the table to generate search requests.\n";
	
		std::cerr << "The 2nd parameter specifies the file prefix for storing lookup trace. The trace is used for simulation.\n";

		exit(0);
	}

	// step 1: build the index
	std::cerr << "Create the index.\n";

	std::string bgptable(argv[1]);
	
	// RBTree<W, U>
	// W = 32/128 for IPv4/IPv6 
	// U = threshold of short/long prefixes
	RBTree<PL, PT>* rbt = new RBTree<PL, PT>();
	
	rbt->build(bgptable);

	// step 2: generate search requests
	std::cerr << "Generate search requests.\n";

	std::string reqFile = std::string(argv[2]).append("_req.dat");

	utility::generateSearchRequest<PL>(bgptable, RN, reqFile);

	// step 3: scatter nodes into a linear/random/circular pipeline and
	// perform IP lookup to record search trace
	std::cerr << "Generate search trace.\n";

	// linear pipeline
	rbt->scatterToPipeline(0);

	std::string linTraceFile = std::string(argv[2]).append("_lin.dat");

	rbt->generateTrace(reqFile, linTraceFile);

	// random pipeline 
	rbt->scatterToPipeline(1, SN);

	std::string ranTraceFile = std::string(argv[2]).append("_ran.dat");

	rbt->generateTrace(reqFile, ranTraceFile);

	// circular pipeline
	rbt->scatterToPipeline(2);

	std::string cirTraceFile = std::string(argv[2]).append("_cir.dat");
	
	rbt->generateTrace(reqFile, cirTraceFile);

	// step 4: schedule requests in a linear/random/circular pipeline.
	std::cerr << "Schedule search requests\n";

	// linear pipeline, number of stages is PL - PT + 1
	// a new arrival comes at the beginning of each time slot
	LinSched<PL - PT + 1>* linsched = new LinSched<PL - PT + 1>();
	
	linsched->searchRun(linTraceFile);
	
	delete linsched;



	// step 5: Delete prefixes.
	std::cerr << "Delete prefixes.\n";

	std::ifstream fin2(bgptable, std::ios_base::binary);

	std::string line;

	ipv4_type prefix;

	uint8 length;

	while (getline(fin2, line)) {

		utility::retrieveInfo(line, prefix, length);
		
		rbt->del(prefix, length);
	}

	rbt->report();
	
	// traverse after deletion
	std::cerr << "Traverse after deletion.\n";

	rbt->traverse();

	delete rbt;

	return 0;
}
