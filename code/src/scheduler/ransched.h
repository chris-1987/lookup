#ifndef RANSCHED_H
#define RANSCHED_H

////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University,
/// All rights reserved
/// \file ransched.h
/// \brief Definition of the scheduler for a random pipeline.
///
/// Schedule lookup tasks in a random pipeline.
///
/// \author Yi Wu
/// \date 2016.11
///////////////////////////////////////////////////////////


#include "../common/common.h"

#include <string>
#include <sstream>
#include <queue>

/// \brief Schedule lookup requests in a random pipeline
///
/// In a random pipeline, a search request jump over all the pipestages to perform the IP lookup task.
///
/// \param W at most W search steps for a request
/// \param K number of pipe stages
/// \param S size of request queue 
template<int W, int K, int S = QUEUESIZE>
class RanSched{

private:

	size_t mSlotNum;  // number of time slots in total

	size_t mRequestNum; // number of request
	
	size_t mBusySlotNumStage[K]; // number of busy time slots for each pipe stage
	
	size_t mBusySlotNumAvg; // average number of busy time slots over all pipe stages

	bool mIsUsed[K]; ///< a pipe stage is occupied during the time slot

	int mMaxQueueLength; ///< maximum length of the request queue

	double mAvgQueueLength; ///< average length of the request queue

public:

	/// \brief structure of a requeset
	struct Request{

		int stagelist[W]; ///< list of stages

		int stepnum; ///< number of steps

		int curstep; ///< current step (start numbering from 0)

		Request() : curstep (0) {}

		int getTargetStage() {

			return stagelist[curstep];
		}

		bool isFinished() {

			return stepnum == curstep;
		}

		void toNext() {

			curstep++;
		}

	};

	/// \brief request queue
	struct ReqQue {

		std::vector<Request*> mData; ///< payload
		
		/// \brief check if queue is empty
		bool isEmpty() {

			return 0 == mData.size();
		}

		/// \brief check if queue is overflow
		bool isOverflow() {

			return S < mData.size();
		}

		/// \brief append a request to the end of the queue
		void append(Request* _req) {

			mData.push_back(_req);

			return;			
		}
	};

	ReqQue mReqQue; ///< queue of requests

	/// \brief default ctor
	RanSched() : mSlotNum(0), mRequestNum(0), mBusySlotNumAvg(0), mMaxQueueLength(0), mAvgQueueLength(0) {

		for (int i = 0; i < K; ++i) {

			mBusySlotNumStage[i] = 0;
		}

		for (int i = 0; i < K; ++i) {

			mIsUsed[i] = false;
		}
	}		
	
	/// \brief execute a search request on each pipe stage
	void execute() {
	
		// reset status of each pipe stage to unused	
		for (int i = 0; i < K; ++i) {

			mIsUsed[i] = false;
		}

		for (int i = 0; i < mReqQue.mData.size(); ++i) {

			int targetStage = mReqQue.mData[i]->getTargetStage();

			if (false == mIsUsed[targetStage]) {
		
				// used 	
				mIsUsed[targetStage] = true;
		
				mBusySlotNumStage[targetStage]++; 		

				// point to next target stage
				mReqQue.mData[i]->toNext();
			}
		} 
	}

	/// \brief dispatch requests from the queue after finishing the search task
	void dispatch() {
	
		int initialSize = mReqQue.mData.size();
	
		for (int i = initialSize - 1; i >= 0; --i) {

			if (mReqQue.mData[i]->isFinished()) {

				delete mReqQue.mData[i];

				mReqQue.mData[i] = nullptr;

				mReqQue.mData.erase(mReqQue.mData.begin() + i);
			}
		}

		return;
	}

	/// \brief a run of executing search requests
	void searchRun(const std::string& _traceFile) {

		// step 1: count number of traces
		std::ifstream fin(_traceFile, std::ios_base::binary);

		std::string line;

		size_t linenum = 0;

		while (getline(fin, line)) linenum++;

		fin.close();	

		mRequestNum = linenum;

		
		// step 2: scheduling
		// packet arrivals submit to bernoulli distribution
		unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

		std::default_random_engine generator(seed);
	
		std::bernoulli_distribution distribution(LAMBDA); // LAMBDA is a consexpr

		auto genreq = std::bind(distribution, generator);
	
		// start simulation
		std::ifstream fin2(_traceFile, std::ios_base::binary);

		std::string line2;

		mSlotNum = 0; 

		while (linenum > 0 || !mReqQue.isEmpty()) {

			mSlotNum++;
		
			// generate requests
			if (genreq()) { 

				for (int i = 0; i < BURSTSIZE; ++i) {

					if (linenum > 0) {

						// generate a request and insert into the queue
						getline(fin2, line2);

						std::stringstream ss(line2);

						Request* newReq = new Request();

						ss >> newReq->stepnum;

						for (int j = 0; j < newReq->stepnum; ++j) {

							ss >> newReq->stagelist[j];
						}						

//						std::cerr << "queue size: " << mReqQue.mData.size() << std::endl;

						if (0 == newReq->stepnum) {
							
							delete newReq;
						}
						else {
					
							mReqQue.append(newReq);

							// collect max length of the queue
							if (mReqQue.mData.size() > mMaxQueueLength) {
	
								mMaxQueueLength = mReqQue.mData.size();
							}
						}

	
						--linenum;
					}
				}	
			}

			// collect avg length of the requets queue
			mAvgQueueLength += mReqQue.mData.size();

			// scheduling and executes a search step
			execute();
			
			// dispatch requests that are finished
			dispatch();
		}

		searchReport();
	}

	
	/// \brief print search report
	void searchReport() {

		std::cerr << "lamda: " << LAMBDA << std::endl;

		std::cerr << "burst size: " << BURSTSIZE << std::endl;

		std::cerr << "queue size: " << QUEUESIZE << std::endl; 

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

		std::cerr << "max queue length: " << mMaxQueueLength << std::endl;

		std::cerr << "avg queue length (per slot): " << mAvgQueueLength / mSlotNum << std::endl;
	}
};


#endif
