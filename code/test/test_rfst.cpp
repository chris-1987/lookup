#include "../src/tree/rfstree.h"
#include "../src/common/utility.h"
#include "../src/scheduler/linsched.h"
#include "../src/scheduler/ransched.h"
#include "../src/scheduler/cirsched.h"


static const size_t RN = 1024 * 1024 * 1; ///< number of requests
static const int PL = 128; ///< 32 or 128 for IPv4 or IPv6, respectively.
static const int PT = 16; ///< threshold for short & long prefixes
static const int FM = 2; ///< CPE:0, MIMAX:1, EVEN:2
static const int EL = 16; ///< expansion level
static const int SN = 6; ///< number of pipe stages

int main(int argc, char** argv){

	if (argc != 3) {
		
		std::cerr << "This program takes two parameters:\n";

		std::cerr << "The 1st parameter specifies the file of the BGP table. We reuse the table to generate search requests.\n";
	
		std::cerr << "The 2nd parameter specifies the file prefix for storing lookup trace. The trace is used for simulation.\n";

		exit(0);
	}


	// step 1: create the index
	std::string bgptable(argv[1]);

	std::string fmn; // name of fixed-stride method in use

	switch(FM) {

	case 0: 
		fmn = std::string("CPE"); break;

	case 1: 
		fmn = std::string("MIMMAX"); break;

	case 2: 
		fmn = std::string("EVEN"); break;
	}

	std::cerr << "Create the index using " << fmn << std::endl;
		
	// RFSTree<W, K, M, U>
	// W = 32 or 128 for IPv4 or IPv6, respectively.
	// K number of expansion level
	// M fixed-stride method, 0, 1, or 2 for CPE, MINMAX or EVEN
	// U threshold for classifying short & long prefixes
	RFSTree<PL, EL, FM, PT>* rfst = new RFSTree<PL, EL, FM, PT>();
		
	rfst->build(bgptable);

	// step 2: generate search requests
	std::cerr << "Generate search requests.\n";

	std::string reqFile = std::string(argv[2]).append("_req.dat");

	utility::generateSearchRequest<PL>(bgptable, RN, reqFile);
	
	// step 3: scatter nodes into a linear/random/circular pipeline,
	// perform IP lookup to record search trace,
	// and schedule requests in a linear/random/circular pipeline
	std::cerr << "Generate search trace.\n";
		
	// linear pipeline
	// number of pipe stages is EL
	if (32 == PL) {

		// generate trace
		std::cerr << "Generate in a linear pipeline\n";
		
		rfst->scatterToPipeline(0);
		
		std::string linTraceFile = std::string(argv[2]).append("_lin.dat");
	
		rfst->generateTrace(reqFile, linTraceFile);
	
		// schedule
		std::cerr << "Schedule in a linear pipeline\n";
		
		LinSched<EL>* linsched = new LinSched<EL>();
		
		linsched->searchRun(linTraceFile);
		
		delete linsched;
	}

	// circular pipeline
	// generate trace
	std::cerr << "Generate in a circular pipeline\n";
	
	rfst->scatterToPipeline(2, SN);
		
	std::string cirTraceFile = std::string(argv[2]).append("_cir.dat");
		
	rfst->generateTrace(reqFile, cirTraceFile);

	// schedule
	std::cerr << "Schedule in a circular pipeline\n";
	
	CirSched<EL, SN>* cirsched = new CirSched<EL, SN>();

	cirsched->searchRun(cirTraceFile);
		
	delete cirsched;

	// random pipeline
	// generate trace
	std::cerr << "Generate in a random pipeline\n";
	
	rfst->scatterToPipeline(1, SN); // W - U + 1 = 32 - 8 + 1 = 25
	
	std::string ranTraceFile = std::string(argv[2]).append("_ran.dat");
		
	rfst->generateTrace(reqFile, ranTraceFile);
	
	std::cerr << "Schedule search requests\n";

	// schedule
	std::cerr << "Schedule in a random pipeline\n";

	RanSched<EL, SN>* ransched = new RanSched<EL, SN>();

	ransched->searchRun(ranTraceFile);

	delete ransched;

	// step 4: Delete prefixes.
	std::cerr << "Delete the index.\n";

	delete rfst;
	
	return 0;
}
