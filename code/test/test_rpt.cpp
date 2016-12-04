#include "../src/tree/rptree.h"
#include "../src/common/utility.h"
#include "../src/scheduler/linsched.h"
#include "../src/scheduler/ransched.h"
#include "../src/scheduler/cirsched.h"

static const size_t RN = 1024 * 1024 * 1; ///< number of requests
static const int PL = 128; ///< 32 or 128 for Ipv4 or IPv6, respectively
static const int PT = 8; ///< threshold for short & long prefixes
static const int SN = 40; ///< number of pipe stages


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
	
	// RPTree<W, U>
	// W = 32/128 for IPv4/IPv6 
	// U = threshold of short/long prefixes
	RPTree<PL, PT>* rpt = new RPTree<PL, PT>();
	
	rpt->build(bgptable);

 	// step 2: generate search requests
 	std::cerr << "Generate search requests.\n";
 
 	std::string reqFile = std::string(argv[2]).append("_req.dat");
 
 	utility::generateSearchRequest<PL>(bgptable, RN, reqFile);
 
 	// step 3: scatter nodes into a linear/random/circular pipeline and
 	// perform IP lookup to record search trace
 	std::cerr << "Generate search trace.\n";
 
 	// linear pipeline
 	std::cerr << "Generate in a linear pipeline\n";
 
 	rpt->scatterToPipeline(0);
 
 	std::string linTraceFile = std::string(argv[2]).append("_lin.dat");
 
 	rpt->generateTrace(reqFile, linTraceFile);
 
 	// circular pipeline
 	std::cerr << "Generate in a circular pipeline\n";
 
 	rpt->scatterToPipeline(2, SN);
 
 	std::string cirTraceFile = std::string(argv[2]).append("_cir.dat");
 	
 	rpt->generateTrace(reqFile, cirTraceFile);
 
 	// random pipeline 
 	std::cerr << "Generate in a random pipeline\n";
 
 	rpt->scatterToPipeline(1, SN);
 
 	std::string ranTraceFile = std::string(argv[2]).append("_ran.dat");
 
 	rpt->generateTrace(reqFile, ranTraceFile);
 
 	// step 4: schedule requests in a linear/random/circular pipeline.
 	std::cerr << "Schedule search requests\n";
 
 	// linear pipeline, number of stages is PL - PT + 1
 	// a new arrival comes at the beginning of each time slot
 	std::cerr << "schedule in a linear pipeline\n";
 
 	LinSched<PL - PT + 1>* linsched = new LinSched<PL - PT + 1>();
 	
 	linsched->searchRun(linTraceFile);
 	
 	delete linsched;
 
 	// circular pipeline
 	// new arrivals comes at the beginning of each time slot
 	// comply with the Bernoulli distribution (benign or burst)
 	std::cerr << "schedule in a circular pipeline\n";
 
 	// !!note that number of stages >= search depth
 	CirSched<PL - PT + 1, SN>* cirsched = new CirSched<PL - PT + 1, SN>();
 
 	cirsched->searchRun(cirTraceFile);
 
	delete cirsched;

 	// random pipeline
 	// new arrivals comes at the beginning of each time slot
 	// submitting to the Bernoulli distribution (benign or burst)
 	std::cerr << "schedule in a random pipeline\n";
 
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
 		
 		rpt->del(prefix, length);
 	}
 
 	rpt->report();
 	
 	// traverse after deletion
 	std::cerr << "Traverse after deletion.\n";
 
 	rpt->traverse();
 
 	delete rpt;

	return 0;
}
