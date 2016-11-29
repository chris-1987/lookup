#ifndef LINSCHED_H
#define LINSCHED_H

////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University,
/// All rights reserved
/// \file linsched.h
/// \brief Definition of the scheduler for a linear pipeline.
///
/// Schedule lookup tasks in a linear pipeline.
///
/// \author Yi Wu
/// \date 2016.11
///////////////////////////////////////////////////////////

#include <string>
#include <sstream>

static const double LAMBDA = 0.7; // packet arriving probability

static const int BURSTSIZE = 1; // number of packets arriving at the same time

static const int QUEUESIZE = 128; // size of queue

/// \brief schedule lookup tasks in a linear pipeline
///
/// In a linear pipeline, each lookup task starts at the initial pipe stage and steps through the remaining pipe stages from left to right.
/// Therefore, the throughput of a linear pipeline is one task per time slot.
/// 
/// \param K number of pipe stages.
template<int K>
class LinSched{
private:

	size_t mSlotNum;  // number of time slots in total

	size_t mRequestNum; // number of request
	
	size_t mBusySlotNumStage[K]; // number of busy time slots for each pipe stage
	
	size_t mBusySlotNumAvg; // average number of busy time slots over all pipe stages

public:

	/// \brief structure of a task
	struct Request{

		int stagelist[K]; ///< list of stages to be visited
	
		int stepnum; ///< number of steps in the task

		int curstep; ///< index of step in execution 

		Request() : curstep(0) {}
	};

	/// \brief structure of a scheduler
	///
	/// Each pipe stage has a scheduler. During each time slot, a scheduler receives at most one task from the upstream pipe stage
	/// and delivers the task to the downstream pipe stage for a further processing (if necessary).
	struct Stage{
			
		Request* req;

		Stage() : req(nullptr){}

		// performs a lookup
		void execute() {

			if (req != nullptr) {

				req->curstep++;
			}

			return;
		}

		// has a request
		bool exist() {

			return req != nullptr;
		}

		// check if current request is finished
		bool isFinished() {
			
			return req->curstep == req->stepnum;
		}

	};

	Stage stages[K]; ///< one scheduler per stage

	/// \brief default ctor
	LinSched () : mSlotNum(0), mRequestNum(0), mBusySlotNumAvg(0){

		for (int i = 0; i < K; ++i) {

			mBusySlotNumStage[i] = 0;
		}
	}	

	/// \brief current pipeline is empty
	bool isEmpty() {

		for (int i = 0; i < K; ++i) {

			if (stages[i].exist()) {

				return false;	
			}
		}

		return true;
	}

	/// \brief perform lookup 
	///
	/// A new arrival comes at the beginning of each time slot.
	void searchRun(const std::string& _traceFile) {

		// step 1: count number of traces 
		std::ifstream fin(_traceFile, std::ios_base::binary);

		std::string line;

		size_t linenum = 0;

		while (getline(fin, line)) linenum++;

		fin.close();
		
		mRequestNum = linenum;

		// step 2: scheduling
		std::ifstream fin2(_traceFile, std::ios_base::binary);

		std::string line2;

		mSlotNum = 0;

		while (linenum > 0 || !isEmpty()) {

			mSlotNum++;
	
			// step 1: here comes a new arrival
			if (linenum > 0) {

				getline(fin2, line2);

				std::stringstream ss(line2);
		
				Request* newReq = new Request();

				ss >> newReq->stepnum;
		
				for (int i = 0; i < newReq->stepnum; ++i) {

					ss >> newReq->stagelist[i];		
				}				

				--linenum;
				
				// deliver it to the initial stage
				if (0 == newReq->stepnum) {

					delete newReq;
				}
				else {

					stages[0].req = newReq; 
				}
			}	

			// step 2: performs one lookup step
			for (int i = 0; i < K; ++i) {

				Stage& curstage = stages[i];

				if (curstage.exist()) { // exists a request 

					curstage.execute();
		
					mBusySlotNumStage[i]++;

					if (curstage.isFinished()) {

						delete curstage.req;

						curstage.req = nullptr;
					}
				}
			}
			
			// step 3: transfer
			for (int i = K - 1; i >= 1; --i) {

				if (nullptr != stages[i - 1].req) {

					stages[i].req = stages[i - 1].req;
				}
				else {

					stages[i].req = nullptr;
				}
			} 

			stages[0].req = nullptr;
		}		
		
		searchReport();

		return;
	}

	void searchReport() {

		std::cerr << "request num: " << mRequestNum << std::endl;

		std::cerr << "slot num: " << mSlotNum << std::endl;

		mBusySlotNumAvg = 0;

		for (int i = 0; i < K; ++i) {

			std::cerr << "busy slot for stage " << i << ": " <<  mBusySlotNumStage[i] << std::endl;			

			mBusySlotNumAvg += mBusySlotNumStage[i];
		}

		mBusySlotNumAvg /= K;

		std::cerr << "busy slot num in average (per stage): " << mBusySlotNumAvg << std::endl;

		std::cerr << "usage ratio: (busy slot/ total slot): " << static_cast<double>(mBusySlotNumAvg) / mSlotNum << std::endl;
	}


	void updateRun(const std::string& _taskFile) {

		return;
	}

};




#endif
