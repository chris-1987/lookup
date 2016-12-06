#include "../src/tree/rmptree.h"
#include "../src/common/utility.h"
#include "../src/scheduler/linsched.h"
#include "../src/scheduler/ransched.h"
#include "../src/scheduler/cirsched.h"

// note: only support random pipeline 

static const size_t RN = 1024 * 1024 * 1; ///< number of requests
static const int PL = 32; ///< 32 or 128 for IPv4 or IPv6, respectively
static const int PT = 8; ///< threshold for short & long prefixes
static const int ST = 2; ///< stride of MPT
static const int SN = 10; ///< number of pipe stages

int main(int argc, char** argv){

	if (argc != 4) {

		std::cerr << "This program takes two parameters:\n";

		std::cerr << "The 1st parameter specifies the file of the BGP table. We reuse the table to generate search requests.\n";
	
		std::cerr << "The 2nd parameter specifies the file prefix for storing lookup trace. The trace is used for simulation.\n";

		std::cerr << "The 3rd parameter specifies the update file.\n"; 

		exit(0);
	}

	// generate search requests
	std::string bgptable(argv[1]);

	std::cerr << "-----Generate search requests.\n";

	std::string reqFile = std::string(argv[2]).append("_req.dat");

	utility::generateSearchRequest<PL>(bgptable, RN, reqFile);

	{ // random pipeline
	
		// step 1: build the index
		std::cerr << "-----Create the index.\n";

		// RMPT<W, K, U>
		// W = 32 or 128 for IPv4/IPv6
		RMPTree<PL, ST, PT>* rmpt = new RMPTree<PL, ST, PT>();
	
		rmpt->build(bgptable);

		// step 2: generate trace 
		std::cerr << "-----Scatter to random pipeline.\n";

		rmpt->scatterToPipeline(1, SN);
		
		std::string ranTraceFile = std::string(argv[2]).append("_ran.dat");

		rmpt->generateTrace(reqFile, ranTraceFile);

		// step 3: schedule, number of stages is given in SN	
		std::cerr << "-----Schedule in a random pipeline.\n";
	
		RanSched<PL - PT + 1, SN>* ransched = new RanSched<PL - PT + 1, SN>();
	
		ransched->searchRun(ranTraceFile);

		delete ransched;

		// step 4: update
		std::cerr << "-----Update in random pipeline.\n";

		std::string updateFile(argv[3]);

		rmpt->update(updateFile, SN);

		delete rmpt;
	}


	return 0;
}
