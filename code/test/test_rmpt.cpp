#include "../src/tree/rmptree.h"
#include "../src/common/utility.h"
#include "../src/scheduler/linsched.h"
#include "../src/scheduler/ransched.h"
#include "../src/scheduler/cirsched.h"


static const size_t RN = 1024 * 1024 * 1; ///< number of requests
static const int PL = 128; ///< 32 or 128 for IPv4 or IPv6, respectively
static const int PT = 8; ///< threshold for short & long prefixes
static const int ST = 2; ///< stride of MPT
static const int SN = 10; ///< number of pipe stages

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
	
	// RMPT<W, K, U>
	// W = 32 or 128 for IPv4/IPv6
	RMPTree<PL, ST, PT>* rmpt = new RMPTree<PL, ST, PT>();

	rmpt->build(bgptable);

	// step 2: search in the index
	std::cerr << "Generate search requests.\n";

	std::string reqFile = std::string(argv[2]).append("_req.dat");

	utility::generateSearchRequest<PL>(bgptable, RN, reqFile);

	// step 3: scatter nodes into a linear/random/circular pipeline and
	// perform IP lookup to record search trace
	// note that, only support random pipeline
	std::cerr << "Generate search trace in a random pipeline\n";

	rmpt->scatterToPipeline(1, SN);
		
	std::string ranTraceFile = std::string(argv[2]).append("_ran.dat");

	rmpt->generateTrace(reqFile, ranTraceFile);

	
	// step 4: schedule requests in a random pipeline
	std::cerr << "Schedule search requests in a random pipeline.\n";

	RanSched<PL - PT + 1, SN>* ransched = new RanSched<PL - PT + 1, SN>();

	ransched->searchRun(ranTraceFile);

	delete ransched;

	// step 5: Delete prefixes.
	std::cerr << "Delete prefixes.\n";

	std::ifstream fin2(bgptable, std::ios_base::binary);

	std::string line;

	choose_ip_type<PL>::ip_type prefix;

	uint8 length;

	while (getline(fin2, line)) {

		utility::retrieveInfo(line, prefix, length);

		rmpt->del(prefix, length);
	}

	rmpt->report();

	// traverse after deletion
	std::cerr << "Traverse after deletion.\n";

	delete rmpt;

	return 0;
}
