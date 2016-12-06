#include "../src/tree/rptree.h"
#include "../src/common/utility.h"
#include "../src/scheduler/linsched.h"
#include "../src/scheduler/ransched.h"
#include "../src/scheduler/cirsched.h"

static const size_t RN = 1024 * 1024 * 1; ///< number of requests
static const int PL = 32; ///< 32 or 128 for Ipv4 or IPv6, respectively
static const int PT = 10; ///< threshold for short & long prefixes
static const int SN = 15; ///< number of pipe stages


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
 
	{ // linear pipeline

 		// step 1: build the index
		std::cerr << "-----Create the index.\n";

		// RPTree<W, U>
		// W = 32/128 for IPv4/IPv6 
		// U = threshold of short/long prefixes
		RPTree<PL, PT>* rpt = new RPTree<PL, PT>();
	
		rpt->build(bgptable);

		// step 2: generate trace
		std::cerr << "-----Scatter to linear pipeline.\n";

	 	rpt->scatterToPipeline(0);
	 
	 	std::string linTraceFile = std::string(argv[2]).append("_lin.dat");
	 
	 	rpt->generateTrace(reqFile, linTraceFile);
 

 		// step 3: schedule, number of stages is PL - PT + 1		
		std::cerr << "-----Schedule in a linear pipeline.\n";

	 	LinSched<PL - PT + 1>* linsched = new LinSched<PL - PT + 1>();
 	
	 	linsched->searchRun(linTraceFile);
 	
	 	delete linsched;
 
		// step 4: update
		std::cerr << "-----Update in linear pipeline.\n";

		std::string updateFile(argv[3]);

		rpt->update(updateFile, 0);			

		delete rpt;
 	}


	{ // random pipeline
		
		// step 1: build the index
	 	std::cerr << "-----Create the index.\n";

		//RPTree<W, U>
		// W = 32/128 for Ipv4/Ipv6
		// U = threshold of short/long prefixes 
		RPTree<PL, PT>* rpt = new RPTree<PL, PT>();
	
		rpt->build(bgptable);

		// step 2: generate trace
		std::cerr << "-----Scatter to random pipeline.\n";
 
	 	rpt->scatterToPipeline(1, SN);
	 
	 	std::string ranTraceFile = std::string(argv[2]).append("_ran.dat");
	 
	 	rpt->generateTrace(reqFile, ranTraceFile);

		// step 3: schedule, number of stages is given in SN 
 		std::cerr << "-----Schedule in a random pipeline\n";
 
 		RanSched<PL - PT + 1, SN>* ransched = new RanSched<PL - PT + 1, SN>();
	 
 		ransched->searchRun(ranTraceFile);
	 
		delete ransched;

		// step 4: update
		std::cerr << "-----Update in random pipeline.\n";

		std::string updateFile(argv[3]);

		rpt->update(updateFile, 1, SN);

		delete rpt;
	}

	{ // circular pipeline

		// step 1: build the index
		std::cerr << "-----Create the index.\n";

		//RPTree<W, U>
		// W = 32/128 for Ipv4/Ipv5
		// U = threshold of short/long prefixes 
		RPTree<PL, PT>* rpt = new RPTree<PL, PT>();
	
		rpt->build(bgptable);

		// step 2: generate trace
		std::cerr << "-----Scatter to circular pipeline.\n";

		rpt->scatterToPipeline(2, SN);

		std::string cirTraceFile = std::string(argv[2]).append("_cir.dat");

 		rpt->generateTrace(reqFile, cirTraceFile);
 
 		// step 3: schedule, number of stages is given in SN
		std::cerr << "-----Schedule in circular pipeline.\n";
 
 		CirSched<PL - PT + 1, SN>* cirsched = new CirSched<PL - PT + 1, SN>();
 
	 	cirsched->searchRun(cirTraceFile);
 
		delete cirsched;

 		// step 4: update
		std::cerr << "-----Update in circular pipeline.\n";

		std::string updateFile(argv[3]);

		rpt->update(updateFile, 2, SN);

	 	delete rpt;
	}

	return 0;
}
